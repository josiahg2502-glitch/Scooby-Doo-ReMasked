// GameCube memory card (Scooby-Doo! Unmasked, G5DE78): the game's CARD calls run on Aurora's host card
// (lib/dolphin/card.cpp, GCI folder per slot under UserData\Saves\MemoryCard). Guest structs are big-endian
// and converted field by field. Async calls finish at once and queue the guest callback (chan, result) on the
// DVD completion drain, so it runs from the scheduler like the EXI interrupt would.
#include "hle_stubs.h"
#include "memory.h"
#include "runtime_config.h"
#include "runtime_log.h"

#include <dolphin/card.h>

#include <cstring>
#include <filesystem>
#include <string>

void QueueGuestCallback(uint32_t callbackPtr, uint32_t r3, uint32_t r4);  // hle/storage/dvd.cpp
extern "C" void GxNotifyGuestRamDmaWrite(uint32_t addr, uint32_t size);   // hle/gx/gx_objects.cpp

namespace {

void EnsureCardInit() {
    static bool done = false;
    if (done) return;
    done = true;
    const auto dir = RuntimeConfigFile::ApplicationDataDirectory() / "Saves" / "MemoryCard";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string path = dir.string();
    CARDSetBasePath(path.c_str(), 0);
    CARDSetBasePath(path.c_str(), 1);
    CARDInit("G5DE", "78");
    RT_LOGF(RT_TAG_OS, "[card] memory card folder: %s\n", path.c_str());
}

std::string GuestString(uint32_t addr) {
    std::string s;
    for (uint32_t i = 0; addr && i < CARD_FILENAME_MAX; ++i) {
        const char c = static_cast<char>(Memory::Read8(addr + i));
        if (!c) break;
        s.push_back(c);
    }
    return s;
}

CARDFileInfo LoadFileInfo(uint32_t g) {
    CARDFileInfo fi{};
    fi.chan = static_cast<s32>(Memory::Read32(g + 0x00));
    fi.fileNo = static_cast<s32>(Memory::Read32(g + 0x04));
    fi.offset = static_cast<s32>(Memory::Read32(g + 0x08));
    fi.length = static_cast<s32>(Memory::Read32(g + 0x0C));
    fi.iBlock = Memory::Read16(g + 0x10);
    return fi;
}
void StoreFileInfo(uint32_t g, const CARDFileInfo& fi) {
    Memory::Write32(g + 0x00, static_cast<uint32_t>(fi.chan));
    Memory::Write32(g + 0x04, static_cast<uint32_t>(fi.fileNo));
    Memory::Write32(g + 0x08, static_cast<uint32_t>(fi.offset));
    Memory::Write32(g + 0x0C, static_cast<uint32_t>(fi.length));
    Memory::Write16(g + 0x10, fi.iBlock);
}

CARDStat LoadStat(uint32_t g) {
    CARDStat s{};
    for (uint32_t i = 0; i < CARD_FILENAME_MAX; ++i) s.fileName[i] = static_cast<char>(Memory::Read8(g + i));
    s.length = Memory::Read32(g + 0x20);
    s.time = Memory::Read32(g + 0x24);
    for (uint32_t i = 0; i < 4; ++i) s.gameName[i] = Memory::Read8(g + 0x28 + i);
    for (uint32_t i = 0; i < 2; ++i) s.company[i] = Memory::Read8(g + 0x2C + i);
    s.bannerFormat = Memory::Read8(g + 0x2E);
    s.iconAddr = Memory::Read32(g + 0x30);
    s.iconFormat = Memory::Read16(g + 0x34);
    s.iconSpeed = Memory::Read16(g + 0x36);
    s.commentAddr = Memory::Read32(g + 0x38);
    s.offsetBanner = Memory::Read32(g + 0x3C);
    s.offsetBannerTlut = Memory::Read32(g + 0x40);
    for (uint32_t i = 0; i < CARD_ICON_MAX; ++i) s.offsetIcon[i] = Memory::Read32(g + 0x44 + i * 4);
    s.offsetIconTlut = Memory::Read32(g + 0x64);
    s.offsetData = Memory::Read32(g + 0x68);
    return s;
}
void StoreStat(uint32_t g, const CARDStat& s) {
    for (uint32_t i = 0; i < CARD_FILENAME_MAX; ++i) Memory::Write8(g + i, static_cast<uint8_t>(s.fileName[i]));
    Memory::Write32(g + 0x20, s.length);
    Memory::Write32(g + 0x24, s.time);
    for (uint32_t i = 0; i < 4; ++i) Memory::Write8(g + 0x28 + i, s.gameName[i]);
    for (uint32_t i = 0; i < 2; ++i) Memory::Write8(g + 0x2C + i, s.company[i]);
    Memory::Write8(g + 0x2E, s.bannerFormat);
    Memory::Write32(g + 0x30, s.iconAddr);
    Memory::Write16(g + 0x34, s.iconFormat);
    Memory::Write16(g + 0x36, s.iconSpeed);
    Memory::Write32(g + 0x38, s.commentAddr);
    Memory::Write32(g + 0x3C, s.offsetBanner);
    Memory::Write32(g + 0x40, s.offsetBannerTlut);
    for (uint32_t i = 0; i < CARD_ICON_MAX; ++i) Memory::Write32(g + 0x44 + i * 4, s.offsetIcon[i]);
    Memory::Write32(g + 0x64, s.offsetIconTlut);
    Memory::Write32(g + 0x68, s.offsetData);
}

// Every CARD result goes to the log (first 200): the game's memory-card screens act on these codes.
s32 Trace(const char* fn, s32 r) {
    static int n = 0;
    const bool poll = std::strcmp(fn, "CARDProbeEx") == 0 || std::strcmp(fn, "CARDGetResultCode") == 0;  // polled every frame
    if (!poll && n++ < 400) RT_LOGF(RT_TAG_OS, "[card] %s -> %d\n", fn, static_cast<int>(r));
    return r;
}

// Aurora's GCI-folder card answers a missing file with NOCARD; the SDK says NOFILE, which is what a game's
// "no save yet" path checks for (NOCARD there is a fatal error). A card is present whenever the probe says so.
s32 FileResult(s32 chan, s32 r) {
    return (r == CARD_RESULT_NOCARD && CARDProbe(chan)) ? CARD_RESULT_NOFILE : r;
}

void WriteS32(uint32_t g, s32 v) { if (g) Memory::Write32(g, static_cast<uint32_t>(v)); }

// SDK async contract: an error at issue time is returned and no callback runs; otherwise READY is returned
// and the callback gets the final result.
int32_t FinishAsync(s32 chan, s32 result, uint32_t callback) {
    if (result < 0 && result != CARD_RESULT_BROKEN && result != CARD_RESULT_ENCODING) return result;
    QueueGuestCallback(callback, static_cast<uint32_t>(chan), static_cast<uint32_t>(result));
    return CARD_RESULT_READY;
}

} // namespace

extern "C" void CARDInit_Gc() { EnsureCardInit(); }
PPC_NATIVE_OVERRIDE_VOID(8016326C, CARDInit_Gc, (), ());

extern "C" int32_t CARDGetResultCode_Gc(int32_t chan) { EnsureCardInit(); return Trace("CARDGetResultCode", CARDGetResultCode(chan)); }
PPC_NATIVE_OVERRIDE(80163474, CARDGetResultCode_Gc, int32_t, (int32_t c), (c));

extern "C" int32_t CARDFreeBlocks_Gc(int32_t chan, uint32_t bytesNotUsed, uint32_t filesNotUsed) {
    EnsureCardInit();
    s32 bytes = 0, files = 0;
    const s32 r = Trace("CARDFreeBlocks", CARDFreeBlocks(chan, &bytes, &files));
    WriteS32(bytesNotUsed, bytes);
    WriteS32(filesNotUsed, files);
    return r;
}
PPC_NATIVE_OVERRIDE(801634A4, CARDFreeBlocks_Gc, int32_t, (int32_t c, uint32_t b, uint32_t f), (c, b, f));

extern "C" int32_t CARDCheckExAsync_Gc(int32_t chan, uint32_t xferBytes, uint32_t callback) {
    EnsureCardInit();
    s32 xfer = 0;
    const s32 r = Trace("CARDCheckEx", CARDCheckEx(chan, &xfer));
    WriteS32(xferBytes, xfer);
    return FinishAsync(chan, r, callback);
}
PPC_NATIVE_OVERRIDE(80165BA8, CARDCheckExAsync_Gc, int32_t, (int32_t c, uint32_t x, uint32_t cb), (c, x, cb));

extern "C" int32_t CARDCheckAsync_Gc(int32_t chan, uint32_t callback) {
    EnsureCardInit();
    return FinishAsync(chan, Trace("CARDCheck", CARDCheck(chan)), callback);
}
PPC_NATIVE_OVERRIDE(80166138, CARDCheckAsync_Gc, int32_t, (int32_t c, uint32_t cb), (c, cb));

extern "C" int32_t CARDProbeEx_Gc(int32_t chan, uint32_t memSize, uint32_t sectorSize) {
    EnsureCardInit();
    s32 mem = 0, sector = 0;
    const s32 r = Trace("CARDProbeEx", CARDProbeEx(chan, &mem, &sector));
    { static int n = 0; if (n++ < 8) RT_LOGF(RT_TAG_OS, "[card] CARDProbeEx chan %d memSize %d sectorSize %d\n", chan, mem, sector); }
    WriteS32(memSize, mem);
    WriteS32(sectorSize, sector);
    return r;
}
PPC_NATIVE_OVERRIDE(8016622C, CARDProbeEx_Gc, int32_t, (int32_t c, uint32_t m, uint32_t s), (c, m, s));

extern "C" int32_t CARDMountAsync_Gc(int32_t chan, uint32_t workArea, uint32_t detachCallback, uint32_t attachCallback) {
    (void)workArea; (void)detachCallback;  // the host card never detaches
    EnsureCardInit();
    return FinishAsync(chan, Trace("CARDMount", CARDMount(chan, nullptr, nullptr)), attachCallback);
}
PPC_NATIVE_OVERRIDE(801668F0, CARDMountAsync_Gc, int32_t, (int32_t c, uint32_t w, uint32_t d, uint32_t a), (c, w, d, a));

extern "C" int32_t CARDMount_Gc(int32_t chan, uint32_t workArea, uint32_t detachCallback) {
    (void)workArea; (void)detachCallback;
    EnsureCardInit();
    return Trace("CARDMount", CARDMount(chan, nullptr, nullptr));
}
PPC_NATIVE_OVERRIDE(80166A90, CARDMount_Gc, int32_t, (int32_t c, uint32_t w, uint32_t d), (c, w, d));

// Aurora's CARDUnmount is a stub that always answers NOCARD; the host card needs no unmount.
extern "C" int32_t CARDUnmount_Gc(int32_t chan) { EnsureCardInit(); return Trace("CARDUnmount", (chan == 0 || chan == 1) ? CARD_RESULT_READY : CARD_RESULT_FATAL_ERROR); }
PPC_NATIVE_OVERRIDE(80166B74, CARDUnmount_Gc, int32_t, (int32_t c), (c));

extern "C" int32_t CARDFormatAsync_Gc(int32_t chan, uint32_t callback) {
    EnsureCardInit();
    return FinishAsync(chan, Trace("CARDFormat", CARDFormat(chan)), callback);
}
PPC_NATIVE_OVERRIDE(801673BC, CARDFormatAsync_Gc, int32_t, (int32_t c, uint32_t cb), (c, cb));

extern "C" int32_t CARDOpen_Gc(int32_t chan, uint32_t fileName, uint32_t fileInfo) {
    EnsureCardInit();
    CARDFileInfo fi{};
    const s32 r = Trace("CARDOpen", FileResult(chan, CARDOpen(chan, GuestString(fileName).c_str(), &fi)));
    if (r >= 0 && fileInfo) StoreFileInfo(fileInfo, fi);
    return r;
}
PPC_NATIVE_OVERRIDE(80167878, CARDOpen_Gc, int32_t, (int32_t c, uint32_t n, uint32_t f), (c, n, f));

extern "C" int32_t CARDClose_Gc(uint32_t fileInfo) {
    EnsureCardInit();
    CARDFileInfo fi = LoadFileInfo(fileInfo);
    const s32 r = Trace("CARDClose", CARDClose(&fi));
    StoreFileInfo(fileInfo, fi);
    return r;
}
PPC_NATIVE_OVERRIDE(80167994, CARDClose_Gc, int32_t, (uint32_t f), (f));

extern "C" int32_t CARDCreateAsync_Gc(int32_t chan, uint32_t fileName, uint32_t size, uint32_t fileInfo, uint32_t callback) {
    EnsureCardInit();
    CARDFileInfo fi{};
    const s32 r = Trace("CARDCreate", CARDCreate(chan, GuestString(fileName).c_str(), size, &fi));
    if (r >= 0 && fileInfo) StoreFileInfo(fileInfo, fi);
    return FinishAsync(chan, r, callback);
}
PPC_NATIVE_OVERRIDE(80167B20, CARDCreateAsync_Gc, int32_t, (int32_t c, uint32_t n, uint32_t s, uint32_t f, uint32_t cb), (c, n, s, f, cb));

extern "C" int32_t CARDReadAsync_Gc(uint32_t fileInfo, uint32_t addr, int32_t length, int32_t offset, uint32_t callback) {
    EnsureCardInit();
    CARDFileInfo fi = LoadFileInfo(fileInfo);
    void* buf = length > 0 ? Memory::GetPointer(addr, static_cast<uint32_t>(length)) : nullptr;
    const s32 r = buf ? CARDRead(&fi, buf, length, offset) : CARD_RESULT_FATAL_ERROR;
    if (r >= 0) GxNotifyGuestRamDmaWrite(addr, static_cast<uint32_t>(length));
    StoreFileInfo(fileInfo, fi);
    return FinishAsync(fi.chan, r, callback);
}
PPC_NATIVE_OVERRIDE(80168028, CARDReadAsync_Gc, int32_t, (uint32_t f, uint32_t a, int32_t l, int32_t o, uint32_t cb), (f, a, l, o, cb));

extern "C" int32_t CARDWriteAsync_Gc(uint32_t fileInfo, uint32_t addr, int32_t length, int32_t offset, uint32_t callback) {
    EnsureCardInit();
    CARDFileInfo fi = LoadFileInfo(fileInfo);
    const void* buf = length > 0 ? Memory::GetPointer(addr, static_cast<uint32_t>(length)) : nullptr;
    const s32 r = buf ? CARDWrite(&fi, buf, length, offset) : CARD_RESULT_FATAL_ERROR;
    StoreFileInfo(fileInfo, fi);
    return FinishAsync(fi.chan, r, callback);
}
PPC_NATIVE_OVERRIDE(8016838C, CARDWriteAsync_Gc, int32_t, (uint32_t f, uint32_t a, int32_t l, int32_t o, uint32_t cb), (f, a, l, o, cb));

extern "C" int32_t CARDDeleteAsync_Gc(int32_t chan, uint32_t fileName, uint32_t callback) {
    EnsureCardInit();
    return FinishAsync(chan, Trace("CARDDelete", FileResult(chan, CARDDelete(chan, GuestString(fileName).c_str()))), callback);
}
PPC_NATIVE_OVERRIDE(80168544, CARDDeleteAsync_Gc, int32_t, (int32_t c, uint32_t n, uint32_t cb), (c, n, cb));

extern "C" int32_t CARDGetStatus_Gc(int32_t chan, int32_t fileNo, uint32_t stat) {
    EnsureCardInit();
    CARDStat s{};
    const s32 r = Trace("CARDGetStatus", CARDGetStatus(chan, fileNo, &s));
    if (r >= 0 && stat) StoreStat(stat, s);
    return r;
}
PPC_NATIVE_OVERRIDE(8016884C, CARDGetStatus_Gc, int32_t, (int32_t c, int32_t n, uint32_t s), (c, n, s));

extern "C" int32_t CARDSetStatusAsync_Gc(int32_t chan, int32_t fileNo, uint32_t stat, uint32_t callback) {
    EnsureCardInit();
    const CARDStat s = LoadStat(stat);
    return FinishAsync(chan, Trace("CARDSetStatus", CARDSetStatus(chan, fileNo, &s)), callback);
}
PPC_NATIVE_OVERRIDE(80168960, CARDSetStatusAsync_Gc, int32_t, (int32_t c, int32_t n, uint32_t s, uint32_t cb), (c, n, s, cb));

extern "C" int32_t CARDSetStatus_Gc(int32_t chan, int32_t fileNo, uint32_t stat) {
    EnsureCardInit();
    const CARDStat s = LoadStat(stat);
    return Trace("CARDSetStatus", CARDSetStatus(chan, fileNo, &s));
}
PPC_NATIVE_OVERRIDE(80168AD4, CARDSetStatus_Gc, int32_t, (int32_t c, int32_t n, uint32_t s), (c, n, s));

// GameCube-only hardware the Wii runtime never needed (Scooby-Doo! Unmasked, G5DE78):
//  - the reset button (PI register 0xCC003000), polled by the game's IO error check;
//  - ARAM, the 16 MB audio/auxiliary RAM. The engine uses it as virtual memory (engine.ini GCVMSARAMSize) and
//    for streamed audio. Here it is a host buffer; AR DMA and ARQ requests copy at once and queue their
//    completion callback on the DVD drain, like the AR interrupt would.
#include "hle_stubs.h"
#include "memory.h"
#include "runtime_log.h"

#include <cstdlib>
#include <cstring>
#include <vector>

void QueueGuestCallback(uint32_t callbackPtr, uint32_t r3, uint32_t r4);  // hle/storage/dvd.cpp
extern "C" void GxNotifyGuestRamDmaWrite(uint32_t addr, uint32_t size);   // hle/gx/gx_objects.cpp

#include "guest_profiler.h"
extern "C" uint32_t OSGetResetButtonState_Gc() {
    GuestProfiler::OnFramePresented();  // diagnostic: lets NSMBW_PROFILE start before the first frame (one-shot)
    return 0;  // never pressed
}
PPC_NATIVE_OVERRIDE(8014EE80, OSGetResetButtonState_Gc, uint32_t, (), ());

namespace {
constexpr uint32_t kAramSize = 16u << 20;
constexpr uint32_t kAramUserBase = 0x4000u;  // ARInit reserves the first 16 KB for the OS/DSP
std::vector<uint8_t>& Aram() {
    static std::vector<uint8_t> aram(kAramSize, 0);
    return aram;
}
uint32_t g_arDmaCallback = 0;

// Main-memory side of an AR transfer: the SDK takes physical or cached addresses.
uint32_t MainRamAddress(uint32_t a) { return (a & 0x01FFFFFFu) | 0x80000000u; }

// type 0: main RAM -> ARAM, type 1: ARAM -> main RAM (ARStartDMA and ARQ use the same encoding).
bool AramCopy(uint32_t type, uint32_t mainAddr, uint32_t aramAddr, uint32_t length) {
    mainAddr = MainRamAddress(mainAddr);
    if (aramAddr >= kAramSize || length > kAramSize - aramAddr || !Memory::Contains(mainAddr, length)) {
        RT_LOGF(RT_TAG_OS, "[aram] bad DMA type %u main 0x%08X aram 0x%08X len 0x%X\n", type, mainAddr, aramAddr, length);
        return false;
    }
    static const bool trace = std::getenv("SCOOBY_TRACE_IO") != nullptr;  // diagnostic: ARAM transfers
    if (trace) {
        static int n = 0;
        if (n++ < 400) RT_LOGF(RT_TAG_OS, "[aram] %s main 0x%08X aram 0x%08X len 0x%X\n",
                               type == 0 ? "to-aram" : "from-aram", mainAddr, aramAddr, length);
    }
    uint8_t* main = static_cast<uint8_t*>(Memory::GetPointer(mainAddr, length));
    if (type == 0) {
        std::memcpy(Aram().data() + aramAddr, main, length);
    } else {
        std::memcpy(main, Aram().data() + aramAddr, length);
        GxNotifyGuestRamDmaWrite(mainAddr, length);
    }
    return true;
}
} // namespace

// Host ARAM for the GameCube AX mixer (hle/audio), which reads voice samples from it; 16 MB (kAramSize).
extern "C" const uint8_t* GcAramHostBase() { return Aram().data(); }

extern "C" uint32_t ARInit_Gc(uint32_t stackIndexAddr, uint32_t numEntries) {
    (void)stackIndexAddr; (void)numEntries;
    Aram();
    RT_LOGF(RT_TAG_OS, "[aram] ARInit: %u MB host ARAM\n", kAramSize >> 20);
    return kAramUserBase;
}
PPC_NATIVE_OVERRIDE(801596B4, ARInit_Gc, uint32_t, (uint32_t s, uint32_t n), (s, n));

extern "C" uint32_t ARGetBaseAddress_Gc() { return kAramUserBase; }
PPC_NATIVE_OVERRIDE(80159778, ARGetBaseAddress_Gc, uint32_t, (), ());

extern "C" uint32_t ARGetSize_Gc() { return kAramSize; }
PPC_NATIVE_OVERRIDE(80159780, ARGetSize_Gc, uint32_t, (), ());

extern "C" uint32_t ARRegisterDMACallback_Gc(uint32_t callback) {
    const uint32_t old = g_arDmaCallback;
    g_arDmaCallback = callback;
    return old;
}
PPC_NATIVE_OVERRIDE(80159544, ARRegisterDMACallback_Gc, uint32_t, (uint32_t c), (c));

extern "C" uint32_t ARGetDMAStatus_Gc() { return 0; }  // transfers finish as they start
PPC_NATIVE_OVERRIDE(80159588, ARGetDMAStatus_Gc, uint32_t, (), ());

extern "C" void ARStartDMA_Gc(uint32_t type, uint32_t mainmemAddr, uint32_t aramAddr, uint32_t length) {
    AramCopy(type, mainmemAddr, aramAddr, length);
    QueueGuestCallback(g_arDmaCallback, 0, 0);
}
PPC_NATIVE_OVERRIDE_VOID(801595C4, ARStartDMA_Gc, (uint32_t t, uint32_t m, uint32_t a, uint32_t l), (t, m, a, l));

extern "C" void ARQInit_Gc() {}
PPC_NATIVE_OVERRIDE_VOID(8015B1F4, ARQInit_Gc, (), ());

// ARQPostRequest(ARQRequest*, owner, type, priority, source, dest, length, callback): callback(request).
extern "C" void ARQPostRequest_Gc(uint32_t request, uint32_t owner, uint32_t type, uint32_t priority,
                                  uint32_t source, uint32_t dest, uint32_t length, uint32_t callback) {
    (void)priority;
    // Fill the ARQRequest as the SDK does: completion callbacks read it back (G5DE78 SOUND_UpdateStreamCB picks its
    // stream by request->owner; left unset, every refill went to stream 0 and voice streams looped their first chunk).
    const uint32_t fields[][2] = {{0x00, 0}, {0x04, owner}, {0x08, type}, {0x10, source},
                                  {0x14, dest}, {0x18, length}, {0x1C, callback}};
    for (const auto& f : fields) Memory::Write32(request + f[0], f[1]);
    if (type == 0) AramCopy(0, source, dest, length);
    else AramCopy(1, dest, source, length);
    QueueGuestCallback(callback, request, 0);
}
PPC_NATIVE_OVERRIDE_VOID(8015B264, ARQPostRequest_Gc,
    (uint32_t r, uint32_t o, uint32_t t, uint32_t p, uint32_t s, uint32_t d, uint32_t l, uint32_t c),
    (r, o, t, p, s, d, l, c));

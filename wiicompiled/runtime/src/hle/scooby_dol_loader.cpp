// Scooby-Doo: Re-Masked: the game's code and data sections come from the player's own disc, never from this exe.
//
// The translation was built from main_logic.dol: the disc's sys/main.dol plus every runtime-loaded logic module (the
// relocatable ELF objects inside each level's .ghr), pre-relocated into text section 2 at 0x81800000, with the
// loader's MMMalloc/MMFree calls patched to the native stubs in scooby_logic.cpp. This file rebuilds that image from
// the extracted disc exactly as projects/scooby/tools/build_logic_overlay.py does (keep the two in step), checks it
// against the SHA-256 the build pinned, caches it as <disc>/sys/main_logic.dol, and copies its sections into guest
// memory. It replaces the translator's embedded data_sections_init.cpp.
#include "memory.h"
#include "runtime_config.h"
#include "runtime_log.h"

#include <SDL3/SDL_messagebox.h>
#include <cryptopp/sha.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

#include "scooby_logic_dol_sha256.inc"  // kLogicDolSha256: hex SHA-256 of the main_logic.dol the build translated

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

constexpr uint32_t kOverlayBase = 0x81800000u, kOverlaySize = 0x01800000u;
constexpr uint32_t kStubAlloc = kOverlayBase + 0x00, kStubFree = kOverlayBase + 0x08;
constexpr uint32_t kVeneerMemcpy = kOverlayBase + 0x10, kVeneerMemset = kOverlayBase + 0x18;
constexpr uint32_t kFirstModule = kOverlayBase + 0x40;
constexpr uint32_t kMemcpy = 0x80011B78u, kMemset = 0x80011A8Cu;
constexpr uint32_t kCallAlloc = 0x800E7AE8u, kCallFree = 0x800E796Cu;  // bl MMMalloc / bl MMFree
constexpr uint32_t kMMMalloc = 0x80070AA0u, kMMFree = 0x800707BCu;
constexpr uint32_t kDeadFeed = 0xDEADFEEDu;

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
void Put32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }
void Put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }

Bytes ReadFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("can't read " + RuntimeConfigFile::PathToUtf8(path));
    return Bytes(std::istreambuf_iterator<char>(f), {});
}

uint64_t Fnv1a64(const Bytes& data, uint64_t h = 0xCBF29CE484222325ull) {
    for (uint8_t b : data) h = (h ^ b) * 0x100000001B3ull;
    return h;
}

std::string Sha256Hex(const Bytes& data) {
    uint8_t digest[CryptoPP::SHA256::DIGESTSIZE];
    CryptoPP::SHA256().CalculateDigest(digest, data.data(), data.size());
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (uint8_t b : digest) { out += hex[b >> 4]; out += hex[b & 15]; }
    return out;
}

struct Module {
    Bytes text, rela;
    std::vector<std::pair<std::string, uint32_t>> syms;  // name, value
};

void Need(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }

// One big-endian 32-bit PowerPC ELF object at data[i].
Module ParseElf(const Bytes& d, size_t i) {
    Need(i + 52 <= d.size(), "truncated ELF header");
    const uint32_t shoff = Be32(&d[i + 32]);
    const uint16_t shnum = Be16(&d[i + 48]), shstr = Be16(&d[i + 50]);
    Need(i + shoff + size_t(shnum) * 40 <= d.size() && shstr < shnum, "bad ELF section table");
    auto sec = [&](int k, int field) { return Be32(&d[i + shoff + size_t(k) * 40 + field * 4]); };
    auto cstr = [&](size_t at) { size_t e = at; while (e < d.size() && d[e]) ++e; return std::string(d.begin() + at, d.begin() + e); };
    const size_t names = i + sec(shstr, 4);
    std::map<std::string, int> by;
    for (int k = 0; k < shnum; ++k) by[cstr(names + sec(k, 0))] = k;
    auto get = [&](const char* name) {
        auto it = by.find(name);
        Need(it != by.end(), "ELF section missing");
        const size_t off = i + sec(it->second, 4), size = sec(it->second, 5);
        Need(off + size <= d.size(), "ELF section out of range");
        return Bytes(d.begin() + off, d.begin() + off + size);
    };
    Module m{get(".text"), get(".rela.text"), {}};
    const Bytes symtab = get(".symtab"), strtab = get(".strtab");
    for (size_t k = 0; k + 16 <= symtab.size(); k += 16) {
        const uint32_t nameOff = Be32(&symtab[k]);
        std::string name;
        for (size_t c = nameOff; c < strtab.size() && strtab[c]; ++c) name += char(strtab[c]);
        m.syms.emplace_back(name, Be32(&symtab[k + 4]));
    }
    return m;
}

// EFLogicCode::LoadFile's relocation loop (0x800E7B4C..0x800E7DA4), bit for bit.
Bytes Relocate(const Module& m, uint32_t base) {
    Bytes out = m.text;
    for (size_t k = 0; k + 12 <= m.rela.size(); k += 12) {
        const uint32_t off = Be32(&m.rela[k]), info = Be32(&m.rela[k + 4]);
        const int32_t addend = int32_t(Be32(&m.rela[k + 8]));
        const uint32_t type = info & 0xFF, sidx = info >> 8;
        Need(sidx < m.syms.size() && off + 4 <= out.size(), "bad relocation");
        const auto& [name, value] = m.syms[sidx];
        const bool isMemcpy = name == "memcpy", isMemset = name == "memset";
        const uint32_t p = base + off;
        if (type == 1 || type == 4 || type == 6) {
            const uint32_t v = (isMemcpy || isMemset) ? kDeadFeed : base + value + uint32_t(addend);
            if (type == 1) Put32(&out[off], v);
            else if (type == 4) Put16(&out[off], uint16_t(v & 0xFFFF));
            else Put16(&out[off], uint16_t(((v >> 16) + ((v >> 15) & 1)) & 0xFFFF));
        } else if (type == 10) {
            const int64_t d = isMemcpy ? int64_t(kVeneerMemcpy) - p : isMemset ? int64_t(kVeneerMemset) - p
                                       : int64_t(value) + addend - int64_t(off);
            Need(d >= -0x2000000 && d < 0x2000000, "branch out of range");
            Put32(&out[off], (Be32(&out[off]) & ~0x03FFFFFCu) | (uint32_t(d) & 0x03FFFFFCu));
        } else if (type == 26) {
            Put32(&out[off], (isMemcpy || isMemset) ? kDeadFeed : value + uint32_t(addend) - off);
        }  // every other type is ignored by the game's switch
    }
    return out;
}

uint32_t Bl(uint32_t from, uint32_t to) { return 0x48000001u | ((to - from) & 0x03FFFFFCu); }

Bytes BuildLogicDol(const fs::path& disc) {
    std::map<uint64_t, Module> modules;  // sorted by key = stable placement
    for (const auto& entry : fs::recursive_directory_iterator(disc / "files")) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (!entry.is_regular_file() || ext != ".ghr") continue;
        const Bytes data = ReadFile(entry.path());
        static const uint8_t kMagic[] = {0x7F, 'E', 'L', 'F', 1, 2, 1};
        for (auto at = std::search(data.begin(), data.end(), std::begin(kMagic), std::end(kMagic)); at != data.end();
             at = std::search(at + 4, data.end(), std::begin(kMagic), std::end(kMagic))) {
            Module m = ParseElf(data, size_t(at - data.begin()));
            const uint64_t key = Fnv1a64(m.rela, Fnv1a64(m.text));
            auto it = modules.find(key);
            Need(it == modules.end() || it->second.text == m.text, "logic module key collision");
            modules.insert_or_assign(key, std::move(m));  // like the Python tool: the last copy wins
        }
    }
    uint32_t addr = kFirstModule;
    std::vector<std::pair<uint64_t, uint32_t>> placed;
    for (const auto& [key, m] : modules) {
        placed.emplace_back(key, addr);
        addr = (addr + uint32_t(m.text.size()) + 31) & ~31u;
    }
    Need(addr <= kOverlayBase + kOverlaySize, "logic overlay full");
    Bytes image(addr - kOverlayBase, 0);
    Put32(&image[0x00], 0x4E800020u);  // LogicAlloc placeholder (blr); overridden natively
    Put32(&image[0x08], 0x4E800020u);  // LogicFree placeholder (blr)
    Put32(&image[0x10], 0x48000000u | ((kMemcpy - kVeneerMemcpy) & 0x03FFFFFCu));  // b memcpy
    Put32(&image[0x18], 0x48000000u | ((kMemset - kVeneerMemset) & 0x03FFFFFCu));  // b memset
    for (const auto& [key, base] : placed) {
        const Bytes code = Relocate(modules[key], base);
        std::copy(code.begin(), code.end(), image.begin() + (base - kOverlayBase));
    }

    Bytes dol = ReadFile(disc / "sys" / "main.dol");
    Need(dol.size() >= 0x100 && Be32(&dol[2 * 4]) == 0 && Be32(&dol[0x90 + 2 * 4]) == 0, "main.dol is not the expected game executable");
    auto fileOffset = [&](uint32_t a) -> size_t {
        for (int k = 0; k < 18; ++k) {
            const uint32_t s = Be32(&dol[0x48 + k * 4]), sz = Be32(&dol[0x90 + k * 4]);
            if (sz && s <= a && a < s + sz) return Be32(&dol[k * 4]) + (a - s);
        }
        throw std::runtime_error("address outside main.dol");
    };
    const size_t alloc = fileOffset(kCallAlloc), freeAt = fileOffset(kCallFree);
    Need(Be32(&dol[alloc]) == Bl(kCallAlloc, kMMMalloc) && Be32(&dol[freeAt]) == Bl(kCallFree, kMMFree),
         "main.dol is not the expected game executable");
    Put32(&dol[alloc], Bl(kCallAlloc, kStubAlloc));
    Put32(&dol[freeAt], Bl(kCallFree, kStubFree));
    dol.resize((dol.size() + 31) & ~size_t(31), 0);
    const uint32_t off = uint32_t(dol.size());
    dol.insert(dol.end(), image.begin(), image.end());
    Put32(&dol[2 * 4], off);
    Put32(&dol[0x48 + 2 * 4], kOverlayBase);
    Put32(&dol[0x90 + 2 * 4], uint32_t(image.size()));
    return dol;
}

[[noreturn]] void Fail(const std::string& message) {
    RT_LOG(RT_TAG_RUNTIME) << "[disc] " << message << std::endl;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Scooby-Doo: Re-Masked", message.c_str(), nullptr);
    std::exit(1);
}

}  // namespace

extern "C" void InitializeDataSections() {
    static bool done = false;
    if (done) return;
    done = true;

    const fs::path disc = RuntimeConfigFile::ResolvedDvdRoot();
    if (disc.empty() || !fs::exists(disc / "sys" / "main.dol"))
        Fail("No game disc is set up. Open \"Scooby-Doo Re-Masked.exe\" (the launcher) and choose your "
             "Scooby-Doo! Unmasked (USA, GameCube) disc image.");

    const fs::path cache = disc / "sys" / "main_logic.dol";
    Bytes dol;
    std::error_code ec;
    if (fs::exists(cache, ec)) {
        try { dol = ReadFile(cache); } catch (...) { dol.clear(); }
        if (Sha256Hex(dol) != kLogicDolSha256) dol.clear();
    }
    if (dol.empty()) {
        RT_LOG(RT_TAG_RUNTIME) << "[disc] building main_logic.dol from " << RuntimeConfigFile::PathToUtf8(disc) << std::endl;
        try { dol = BuildLogicDol(disc); }
        catch (const std::exception& e) { Fail(std::string("The game files could not be read: ") + e.what() + ". Set up the disc again from the launcher."); }
        if (Sha256Hex(dol) != kLogicDolSha256)
            Fail("This disc is not the copy this build expects (Scooby-Doo! Unmasked, USA, GameCube, G5DE78). "
                 "Set up a clean dump of that disc from the launcher.");
        std::ofstream out(cache, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(dol.data()), std::streamsize(dol.size()));  // a failed cache only costs a rebuild next start
    }

    for (int k = 0; k < 18; ++k) {
        const uint32_t off = Be32(&dol[k * 4]), addr = Be32(&dol[0x48 + k * 4]), size = Be32(&dol[0x90 + k * 4]);
        if (size == 0 || size_t(off) + size > dol.size() || !Memory::Contains(addr, size)) continue;
        std::memcpy(Memory::GetPointer(addr, size), &dol[off], size);
    }
    RT_LOG(RT_TAG_RUNTIME) << "[disc] loaded game code and data from " << RuntimeConfigFile::PathToUtf8(cache) << std::endl;
}

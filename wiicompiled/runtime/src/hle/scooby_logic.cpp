// Scooby-Doo! Unmasked logic modules: compiled PowerPC objects the game loads from each level at run time
// (EFLogicCode::LoadFile 0x800E799C). projects/scooby/tools/build_logic_overlay.py pre-relocates every unique
// module at a fixed address in the overlay above MEM1 and translates it, and patches the loader's MMMalloc /
// MMFree calls to the two stubs below. LogicAlloc recognises the module being loaded (FNV-1a 64 of its raw
// .text + .rela.text) and returns its fixed address, so the game relocates into the same bytes that were
// translated; anything unknown falls back to the game's own allocator (and would then run untranslated).
#include "hle_stubs.h"
#include "memory.h"
#include "runtime_log.h"

#include <cstdint>
#include <unordered_map>

extern "C" void func_80070AA0(CpuContext* ctx);  // MMMalloc
extern "C" void func_800707BC(CpuContext* ctx);  // MMFree

namespace {
struct LogicModule { uint64_t key; uint32_t address; uint32_t textSize; };
constexpr LogicModule kLogicModules[] = {
#include "scooby_logic_table.inc"
};
constexpr uint32_t kOverlayBase = 0x81800000u;
constexpr uint32_t kOverlayEnd = 0x83000000u;  // runtime memory.cpp MEM1_LOGIC_OVERLAY

const std::unordered_map<uint64_t, const LogicModule*>& ModulesByKey() {
    static const auto map = [] {
        std::unordered_map<uint64_t, const LogicModule*> m;
        for (const auto& mod : kLogicModules) m.emplace(mod.key, &mod);
        return m;
    }();
    return map;
}

uint64_t Fnv1a64(uint32_t addr, uint32_t size, uint64_t h) {
    const auto* p = static_cast<const uint8_t*>(Memory::GetPointer(addr, size));
    for (uint32_t i = 0; i < size; ++i) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}
} // namespace

// bl from 0x800E7AE8, in place of MMMalloc(textSize). Loader registers at that point: r28 = ELF image,
// r25 = .text section header, r31 = section header table, r26 = .rela.text section index.
extern "C" void LogicAlloc_Scooby(CpuContext* ctx) {
    const uint32_t size = ctx->gpr[3];
    try {
        const uint32_t elf = ctx->gpr[28], textHdr = ctx->gpr[25];
        const uint32_t relaHdr = ctx->gpr[31] + ctx->gpr[26] * 0x28u;
        const uint32_t textOff = Memory::Read32(textHdr + 0x10), textSize = Memory::Read32(textHdr + 0x14);
        const uint32_t relaOff = Memory::Read32(relaHdr + 0x10), relaSize = Memory::Read32(relaHdr + 0x14);
        const uint64_t key = Fnv1a64(elf + relaOff, relaSize, Fnv1a64(elf + textOff, textSize, 0xCBF29CE484222325ull));
        const auto it = ModulesByKey().find(key);
        if (it != ModulesByKey().end() && it->second->textSize == size) {
            static int logged = 0;
            if (logged++ < 64) RT_LOGF(RT_TAG_RUNTIME, "[logic] module %016llX -> 0x%08X (%u bytes)\n",
                                       static_cast<unsigned long long>(key), it->second->address, size);
            ctx->gpr[3] = it->second->address;
            return;
        }
        RT_LOGF(RT_TAG_RUNTIME, "[logic] UNKNOWN module %016llX (%u bytes): heap copy, it will not be translated\n",
                static_cast<unsigned long long>(key), size);
    } catch (const Memory::AccessViolation& e) {
        LogMemoryError(RT_TAG_RUNTIME, "LogicAlloc_Scooby", e);
    }
    func_80070AA0(ctx);
}
REGISTER_NATIVE_FUNCTION_AS(0x81800000, LogicAlloc_Scooby, "LogicAlloc_Scooby");

// bl from 0x800E796C (EFLogicCode::Free), in place of MMFree(code): overlay code is never freed.
extern "C" void LogicFree_Scooby(CpuContext* ctx) {
    const uint32_t p = ctx->gpr[3];
    if (p >= kOverlayBase && p < kOverlayEnd) return;
    func_800707BC(ctx);
}
REGISTER_NATIVE_FUNCTION_AS(0x81800008, LogicFree_Scooby, "LogicFree_Scooby");

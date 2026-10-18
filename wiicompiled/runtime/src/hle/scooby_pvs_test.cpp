// Diagnostic (env GAME_NOPVS=1): EFSpace::FindCameraRegion (0x801047DC) reports "no region", so
// EFWorld::CompileRegionsInFrustum takes every region in the frustum (FindAllRegionsInFrustum) instead of the
// camera region's PVS. Otherwise the translated original runs (copied verbatim from the generated shard).
#include <cstdint>
#include <cstdlib>
#include "ppc_runtime.h"
#include "abi_bridge.h"
#include "memory.h"
#include "recomp_mod_loader.h"
#include "hle_stubs.h"

extern "C" void func_8004A788(CpuContext*);

static void ScoobyPvs_FindCameraRegion_Original(CpuContext* MKW_RESTRICT ctx)
{
    uint32_t cr0_0 = 0;
    uint32_t r0_rot_0 = 0;
    uint32_t r0_rot_1 = 0;
    uint32_t r0_rot_2 = 0;
    uint32_t r0_rot_3 = 0;
    uint32_t r0_rot_4 = 0;
    uint32_t r29_addr_0 = 0;
    uint32_t r29_addr_1 = 0;
    uint32_t r29_addr_2 = 0;
    uint32_t r29_addr_3 = 0;
    uint32_t r29_addr_4 = 0;
    uint32_t r29_addr_5 = 0;
    uint32_t r29_addr_6 = 0;
    uint32_t r29_addr_7 = 0;
    uint32_t r29_addr_8 = 0;
    uint32_t r29_addr_9 = 0;
    uint32_t r30_rot_0 = 0;
    uint32_t r30_rot_1 = 0;
    uint32_t r30_rot_2 = 0;
    uint32_t r30_rot_3 = 0;
    uint32_t r3_rot_0 = 0;
    uint32_t r3_rot_1 = 0;
    uint32_t r3_rot_2 = 0;
    uint8_t* guest_range_0 = nullptr;

    uint32_t r0 = ctx->gpr[0];
    uint32_t r1 = ctx->gpr[1];
    uint32_t r3 = ctx->gpr[3];
    uint32_t r4 = ctx->gpr[4];
    uint32_t r5 = ctx->gpr[5];
    uint32_t r26 = ctx->gpr[26];
    uint32_t r27 = ctx->gpr[27];
    uint32_t r28 = ctx->gpr[28];
    uint32_t r29 = ctx->gpr[29];
    uint32_t r30 = ctx->gpr[30];
    uint32_t r31 = ctx->gpr[31];
    uint32_t cr = ctx->cr;
    uint32_t xer = ctx->xer;

    goto loc_801047DC;

loc_801047DC:
{
    guest_range_0 = MemoryInline::ResolveRangeHost((r1 + -224), 0, 232u, false, true);
    MemoryInline::WriteResolved32(guest_range_0, 0u, (r1 + -224), r1);
    r1 = (r1 + -224);
    r0 = ctx->lr;
    MemoryInline::WriteResolved32(guest_range_0, 228u, (r1 + 228), r0);
    if (!MemoryInline::WriteResolvedPair32(guest_range_0, 200u, ((static_cast<uint64_t>(static_cast<uint32_t>(r26)) << 32) | static_cast<uint32_t>(r27)))) {
        MemoryInline::WriteResolved32(guest_range_0, 200u, (r1 + 200), r26);
        MemoryInline::WriteResolved32(guest_range_0, 204u, (r1 + 204), r27);
    }
    if (!MemoryInline::WriteResolvedPair32(guest_range_0, 208u, ((static_cast<uint64_t>(static_cast<uint32_t>(r28)) << 32) | static_cast<uint32_t>(r29)))) {
        MemoryInline::WriteResolved32(guest_range_0, 208u, (r1 + 208), r28);
        MemoryInline::WriteResolved32(guest_range_0, 212u, (r1 + 212), r29);
    }
    if (!MemoryInline::WriteResolvedPair32(guest_range_0, 216u, ((static_cast<uint64_t>(static_cast<uint32_t>(r30)) << 32) | static_cast<uint32_t>(r31)))) {
        MemoryInline::WriteResolved32(guest_range_0, 216u, (r1 + 216), r30);
        MemoryInline::WriteResolved32(guest_range_0, 220u, (r1 + 220), r31);
    }
    r26 = (r5 | r5);
    SetCRResident(cr, xer, 0, static_cast<int32_t>(r26), static_cast<int32_t>(0));
    r27 = r3;
    r31 = r4;
    if (((cr & 0x20000000u) != 0)) {
        goto loc_80104814;
    }
}

loc_801047FC:
{
    r3 = (r26 + 16);
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->cr = cr;
    func_8004A788(ctx);
    r0 = ctx->gpr[0];
    r3 = ctx->gpr[3];
    cr = ctx->cr;
    SetCRResident(cr, xer, 0, static_cast<uint32_t>(r3), static_cast<uint32_t>(0));
}

loc_80104808:
{
    if (((cr & 0x20000000u) == 0)) {
        goto loc_80104814;
    }
}

loc_8010480C:
{
    r3 = r26;
    goto loc_801048C8;
}

loc_80104814:
{
    r4 = MemoryInline::FlatRead32((r27 + 72));
    r28 = 0;
    r3_rot_1 = PpcRotl32Inline(static_cast<uint32_t>(r28), static_cast<uint32_t>(2));
    r3 = (r3_rot_1 & -4);
    r29 = (r1 + 8);
    r4 = MemoryInline::FlatRead32((r4 + 44));
    r28 = 1;
    r0_rot_1 = PpcRotl32Inline(static_cast<uint32_t>(r28), static_cast<uint32_t>(2));
    r0 = (r0_rot_1 & -4);
    r29_addr_1 = (r29 + r3);
    MemoryInline::FlatWrite32(r29_addr_1, r4);
    r3 = (r4 + 384);
    r28 = 2;
    r29_addr_2 = (r29 + r0);
    MemoryInline::FlatWrite32(r29_addr_2, r3);
    goto loc_801048BC;
}

loc_80104844:
{
    r28 = (r28 + -1);
    r0_rot_3 = PpcRotl32Inline(static_cast<uint32_t>(r28), static_cast<uint32_t>(2));
    r0 = (r0_rot_3 & -4);
    r28 = (r28 + -1);
    r29_addr_4 = (r29 + r0);
    r26 = MemoryInline::FlatRead32(r29_addr_4);
    r30_rot_2 = PpcRotl32Inline(static_cast<uint32_t>(r28), static_cast<uint32_t>(2));
    r30 = (r30_rot_2 & -4);
    r29_addr_5 = (r29 + r30);
    r27 = MemoryInline::FlatRead32(r29_addr_5);
    goto loc_801048B4;
}

loc_80104860:
{
    r4 = r31;
    r3 = (r27 + 16);
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->cr = cr;
    func_8004A788(ctx);
    r0 = ctx->gpr[0];
    r3 = ctx->gpr[3];
    cr = ctx->cr;
}

loc_80104870:
{
    if ((static_cast<uint32_t>(r3) != static_cast<uint32_t>(0))) {
        goto loc_801048B0;
    }
}

loc_80104874:
{
    r0 = MemoryInline::FlatRead16((r27 + 40));
    r0 = (r0 & 1);
    SetCRResident(cr, xer, 0, static_cast<int32_t>(r0), static_cast<int32_t>(0));
}

loc_8010487C:
{
    if (((cr & 0x20000000u) != 0)) {
        goto loc_80104888;
    }
}

loc_80104880:
{
    r3 = r27;
    goto loc_801048C8;
}

loc_80104888:
{
    r0 = (r27 + 48);
    r3 = MemoryInline::FlatRead32((r27 + 44));
    r29_addr_7 = (r29 + r30);
    MemoryInline::FlatWrite32(r29_addr_7, r0);
    r30 = (r30 + 4);
    r27 = r3;
    r28 = (r28 + 2);
    r29_addr_8 = (r29 + r30);
    MemoryInline::FlatWrite32(r29_addr_8, r26);
    r26 = (r3 + 384);
    r30 = (r30 + 4);
    goto loc_801048B4;
}

loc_801048B0:
{
    r27 = (r27 + 48);
}

loc_801048B4:
{
    SetCRResident(cr, xer, 0, static_cast<uint32_t>(r27), static_cast<uint32_t>(r26));
}

loc_801048B8:
{
    if (((cr & 0x80000000u) != 0)) {
        goto loc_80104860;
    }
}

loc_801048BC:
{
    SetCRResident(cr, xer, 0, static_cast<uint32_t>(r28), static_cast<uint32_t>(0));
}

loc_801048C0:
{
    if (((cr & 0x20000000u) == 0)) {
        goto loc_80104844;
    }
}

loc_801048C4:
{
    r3 = 0;
}

loc_801048C8:
{
    r26 = MemoryInline::FlatRead32((r1 + 200));
    r27 = MemoryInline::FlatRead32((r1 + 204));
    r28 = MemoryInline::FlatRead32((r1 + 208));
    r29 = MemoryInline::FlatRead32((r1 + 212));
    r30 = MemoryInline::FlatRead32((r1 + 216));
    r31 = MemoryInline::FlatRead32((r1 + 220));
    r0 = MemoryInline::FlatRead32((r1 + 228));
    ctx->lr = r0;
    r1 = (r1 + 224);
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[26] = r26;
    ctx->gpr[27] = r27;
    ctx->gpr[28] = r28;
    ctx->gpr[29] = r29;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    ctx->cr = cr;
    return;
}

}


extern "C" void ScoobyPvs_FindCameraRegion(CpuContext* ctx) {
    if (static const bool noPvs = std::getenv("GAME_NOPVS") != nullptr; noPvs) {
        ctx->gpr[3] = 0;
        return;
    }
    ScoobyPvs_FindCameraRegion_Original(ctx);
}
PPC_NATIVE_OVERRIDE_VOID(801047DC, ScoobyPvs_FindCameraRegion, (CpuContext* ctx), (ctx));

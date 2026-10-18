// Diagnostic (env GAME_NOCULL=1): IsSphereInFrustrum__FUlP10DTVector3df (0x80090D30) answers "visible" for everything,
// to test whether the game's own frustum culling hides walls and floors. Otherwise the translated original runs
// (copied verbatim from the generated shard).
#include <cstdint>
#include <cstdlib>
#include "ppc_runtime.h"
#include "abi_bridge.h"
#include "memory.h"
#include "recomp_mod_loader.h"
#include "hle_stubs.h"

extern "C" void func_8004A2E0(CpuContext*);
extern "C" void func_8007BC54(CpuContext*);
extern "C" void func_800F1658(CpuContext*);

static void ScoobyCull_IsSphereInFrustrum_Original(CpuContext* MKW_RESTRICT ctx)
{
    uint32_t cr0_0 = 0;
    uint32_t r1_psq_tmp_0 = 0;

    uint32_t r0 = ctx->gpr[0];
    uint32_t r1 = ctx->gpr[1];
    uint32_t r3 = ctx->gpr[3];
    uint32_t r4 = ctx->gpr[4];
    uint32_t r5 = ctx->gpr[5];
    uint32_t r6 = ctx->gpr[6];
    uint32_t r7 = ctx->gpr[7];
    uint32_t r13 = ctx->gpr[13];
    uint32_t r30 = ctx->gpr[30];
    uint32_t r31 = ctx->gpr[31];
    PPC_FPR f0 = ctx->fpr[0];
    PPC_FPR f1 = ctx->fpr[1];
    PPC_FPR f2 = ctx->fpr[2];
    PPC_FPR f3 = ctx->fpr[3];
    PPC_FPR f4 = ctx->fpr[4];
    PPC_FPR f5 = ctx->fpr[5];
    PPC_FPR f6 = ctx->fpr[6];
    PPC_FPR f7 = ctx->fpr[7];
    PPC_FPR f8 = ctx->fpr[8];
    PPC_FPR f31 = ctx->fpr[31];
    uint32_t cr = ctx->cr;
    uint32_t ctr = ctx->ctr;
    uint32_t xer = ctx->xer;

    goto loc_80090D30;

loc_80090D30:
{
    MemoryInline::FlatWriteRam32((r1 + -64), r1);
    r1 = (r1 + -64);
    r0 = ctx->lr;
    MemoryInline::FlatWriteRam32((r1 + 68), r0);
    MemoryInline::FlatWriteRamFloat64((r1 + 48), f31.d);
    // psq_store w=0 quant=0 (using PPC_PsqSt)
    r1_psq_tmp_0 = (r1 + 56);
    PPC_PsqStStackInline<0u, 0u>(ctx, r1_psq_tmp_0, PPC_PsFromScalarInline(f31.d));
    MemoryInline::FlatWriteRam32((r1 + 44), r31);
    MemoryInline::FlatWriteRam32((r1 + 40), r30);
    r5 = 0x80180000u;
    r6 = r3;
    r0 = (r5 + -6872);
    f31.d = f1.d;
    MemoryInline::FlatWrite32((r13 + -23188), r0);
    r30 = r4;
    r3 = MemoryInline::FlatRead32((r13 + -23056));
    r4 = r6;
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->cr = cr;
    ctx->xer = xer;
    func_800F1658(ctx);
    r0 = ctx->gpr[0];
    r3 = ctx->gpr[3];
    r4 = ctx->gpr[4];
    cr = ctx->cr;
    r0 = MemoryInline::FlatRead32((r3 + 844));
    r31 = r3;
    SetCRResident(cr, xer, 0, static_cast<uint32_t>(r0), static_cast<uint32_t>(0));
}

loc_80090D7C:
{
    if (((cr & 0x20000000u) != 0)) {
        goto loc_80090D84;
    }
}

loc_80090D80:
{
    ctx->lr = 0x80090D84u;
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[31] = r31;
    ctx->fpr[0] = f0;
    ctx->fpr[1] = f1;
    ctx->fpr[2] = f2;
    ctx->fpr[3] = f3;
    ctx->fpr[4] = f4;
    ctx->fpr[5] = f5;
    ctx->fpr[6] = f6;
    ctx->fpr[31] = f31;
    ctx->cr = cr;
    ctx->ctr = ctr;
    ctx->xer = xer;
    InvokeDirectCpu<0x80090F00u>(ctx);
    r0 = ctx->gpr[0];
    r1 = ctx->gpr[1];
    r3 = ctx->gpr[3];
    r4 = ctx->gpr[4];
    r5 = ctx->gpr[5];
    r31 = ctx->gpr[31];
    f0 = ctx->fpr[0];
    f1 = ctx->fpr[1];
    f2 = ctx->fpr[2];
    f3 = ctx->fpr[3];
    f4 = ctx->fpr[4];
    f5 = ctx->fpr[5];
    f6 = ctx->fpr[6];
    f31 = ctx->fpr[31];
    cr = ctx->cr;
    ctr = ctx->ctr;
}

loc_80090D84:
{
    r3 = r31;
    // inline leaf 0x80090EF8 (2 guest instruction(s))
    r3 = MemoryInline::FlatRead32((r3 + 16));
    // end of inlined leaf 0x80090EF8
    ctx->lr = 0x80090D90u;
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[6] = r6;
    ctx->gpr[7] = r7;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    ctx->fpr[0] = f0;
    ctx->fpr[1] = f1;
    ctx->fpr[2] = f2;
    ctx->fpr[3] = f3;
    ctx->fpr[4] = f4;
    ctx->fpr[5] = f5;
    ctx->fpr[6] = f6;
    ctx->fpr[7] = f7;
    ctx->fpr[8] = f8;
    ctx->cr = cr;
    ctx->xer = xer;
    func_8007BC54(ctx);
    r0 = ctx->gpr[0];
    r1 = ctx->gpr[1];
    r3 = ctx->gpr[3];
    r4 = ctx->gpr[4];
    r5 = ctx->gpr[5];
    r6 = ctx->gpr[6];
    r30 = ctx->gpr[30];
    r31 = ctx->gpr[31];
    f0 = ctx->fpr[0];
    f1 = ctx->fpr[1];
    f2 = ctx->fpr[2];
    f3 = ctx->fpr[3];
    f4 = ctx->fpr[4];
    f5 = ctx->fpr[5];
    cr = ctx->cr;
    xer = ctx->xer;
    r4 = r3;
    r3 = (r31 + 720);
    r5 = (r31 + 848);
    ctx->lr = 0x80090DA0u;
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    ctx->fpr[0] = f0;
    ctx->fpr[1] = f1;
    ctx->fpr[2] = f2;
    ctx->fpr[3] = f3;
    ctx->fpr[4] = f4;
    ctx->cr = cr;
    ctx->xer = xer;
    func_8004A2E0(ctx);
    r0 = ctx->gpr[0];
    r1 = ctx->gpr[1];
    r3 = ctx->gpr[3];
    r4 = ctx->gpr[4];
    r5 = ctx->gpr[5];
    r30 = ctx->gpr[30];
    r31 = ctx->gpr[31];
    f0 = ctx->fpr[0];
    f1 = ctx->fpr[1];
    f2 = ctx->fpr[2];
    f3 = ctx->fpr[3];
    f4 = ctx->fpr[4];
    cr = ctx->cr;
    r3 = r31;
    ctx->lr = 0x80090DA8u;
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[6] = r6;
    ctx->gpr[7] = r7;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    ctx->fpr[0] = f0;
    ctx->fpr[1] = f1;
    ctx->fpr[2] = f2;
    ctx->fpr[3] = f3;
    ctx->fpr[4] = f4;
    ctx->fpr[5] = f5;
    ctx->fpr[6] = f6;
    ctx->fpr[7] = f7;
    ctx->fpr[8] = f8;
    ctx->fpr[31] = f31;
    ctx->cr = cr;
    ctx->ctr = ctr;
    ctx->xer = xer;
    InvokeDirectCpu<0x800F080Cu>(ctx);
    r0 = ctx->gpr[0];
    r1 = ctx->gpr[1];
    r3 = ctx->gpr[3];
    r4 = ctx->gpr[4];
    r5 = ctx->gpr[5];
    r6 = ctx->gpr[6];
    r30 = ctx->gpr[30];
    r31 = ctx->gpr[31];
    f0 = ctx->fpr[0];
    f1 = ctx->fpr[1];
    f2 = ctx->fpr[2];
    f3 = ctx->fpr[3];
    f4 = ctx->fpr[4];
    f5 = ctx->fpr[5];
    f6 = ctx->fpr[6];
    f31 = ctx->fpr[31];
    cr = ctx->cr;
    ctr = ctx->ctr;
    xer = ctx->xer;
    f8.d = static_cast<double>(PpcForceSingleValueInline(f31.d));
    r5 = MemoryInline::FlatRead32(r30);
    r4 = MemoryInline::FlatRead32((r30 + 4));
    r6 = 0;
    r3 = MemoryInline::FlatRead32((r30 + 8));
    r0 = 3;
    MemoryInline::FlatWriteRam32((r1 + 12), r5);
    f7.d = (-(f8.d));
    r5 = r6;
    r7 = (r31 + 848);
    MemoryInline::FlatWriteRam32((r1 + 16), r4);
    f3.d = MemoryInline::FlatReadFloat32((r1 + 12));
    MemoryInline::FlatWriteRam32((r1 + 20), r3);
    f5.d = MemoryInline::FlatReadFloat32((r1 + 16));
    MemoryInline::FlatWriteRamFloat32((r1 + 24), f31.d);
    f6.d = MemoryInline::FlatReadFloat32((r1 + 20));
    ctr = r0;
}

loc_80090DEC:
{
    f0.d = MemoryInline::FlatReadFloat32((r7 + 4));
    f2.d = MemoryInline::FlatReadFloat32(r7);
    f1.d = PpcFmulsInline(f5.d, f0.d);
    f4.d = MemoryInline::FlatReadFloat32((r7 + 8));
    f0.d = MemoryInline::FlatReadFloat32((r7 + 12));
    f1.d = PPC_Fmadds(f3.d, f2.d, f1.d);
    f1.d = PPC_Fmadds(f6.d, f4.d, f1.d);
    f0.d = static_cast<double>(PpcForceSingleValueInline(f1.d - f0.d));
    SetCRFloatResident(cr, 0, f0.d, f8.d);
}

loc_80090E10:
{
    if (((cr & 0x40000000u) == 0)) {
        goto loc_80090E1C;
    }
}

loc_80090E14:
{
    r0 = 0;
    goto loc_80090E30;
}

loc_80090E1C:
{
    SetCRFloatResident(cr, 0, f0.d, f7.d);
}

loc_80090E20:
{
    if (((cr & 0x80000000u) == 0)) {
        goto loc_80090E2C;
    }
}

loc_80090E24:
{
    r0 = 1;
    goto loc_80090E30;
}

loc_80090E2C:
{
    r0 = 2;
}

loc_80090E30:
{
}

loc_80090E34:
{
    if ((static_cast<int32_t>(r0) != static_cast<int32_t>(0))) {
        goto loc_80090E40;
    }
}

loc_80090E38:
{
    r3 = 0;
    goto loc_80090EC8;
}

loc_80090E40:
{
}

loc_80090E44:
{
    if ((static_cast<int32_t>(r0) != static_cast<int32_t>(2))) {
        goto loc_80090E4C;
    }
}

loc_80090E48:
{
    r6 = 1;
}

loc_80090E4C:
{
    f0.d = MemoryInline::FlatReadFloat32((r7 + 20));
    f2.d = MemoryInline::FlatReadFloat32((r7 + 16));
    f1.d = PpcFmulsInline(f5.d, f0.d);
    f4.d = MemoryInline::FlatReadFloat32((r7 + 24));
    f0.d = MemoryInline::FlatReadFloat32((r7 + 28));
    f1.d = PPC_Fmadds(f3.d, f2.d, f1.d);
    f1.d = PPC_Fmadds(f6.d, f4.d, f1.d);
    f0.d = static_cast<double>(PpcForceSingleValueInline(f1.d - f0.d));
    SetCRFloatResident(cr, 0, f0.d, f8.d);
}

loc_80090E70:
{
    if (((cr & 0x40000000u) == 0)) {
        goto loc_80090E7C;
    }
}

loc_80090E74:
{
    r0 = 0;
    goto loc_80090E90;
}

loc_80090E7C:
{
    SetCRFloatResident(cr, 0, f0.d, f7.d);
}

loc_80090E80:
{
    if (((cr & 0x80000000u) == 0)) {
        goto loc_80090E8C;
    }
}

loc_80090E84:
{
    r0 = 1;
    goto loc_80090E90;
}

loc_80090E8C:
{
    r0 = 2;
}

loc_80090E90:
{
}

loc_80090E94:
{
    if ((static_cast<int32_t>(r0) != static_cast<int32_t>(0))) {
        goto loc_80090EA0;
    }
}

loc_80090E98:
{
    r3 = 0;
    goto loc_80090EC8;
}

loc_80090EA0:
{
}

loc_80090EA4:
{
    if ((static_cast<int32_t>(r0) != static_cast<int32_t>(2))) {
        goto loc_80090EAC;
    }
}

loc_80090EA8:
{
    r6 = 1;
}

loc_80090EAC:
{
    r7 = (r7 + 32);
    r5 = (r5 + 1);
    ctr = (ctr + -1);
    if ((ctr != 0)) {
        goto loc_80090DEC;
    }
}

loc_80090EB8:
{
}

loc_80090EBC:
{
    r3 = 1;
    if ((static_cast<uint32_t>(r6) == static_cast<uint32_t>(0))) {
        goto loc_80090EC8;
    }
}

loc_80090EC4:
{
    r3 = 2;
}

loc_80090EC8:
{
    r0 = (r1 + 8);
    {
        const uint64_t ppcCarryWide = static_cast<uint64_t>(static_cast<uint32_t>(r1)) + static_cast<uint64_t>(static_cast<uint32_t>(8)) + (static_cast<uint64_t>(static_cast<uint32_t>(0)) & 1u);
        xer = (xer & 0xDFFFFFFFu) | (static_cast<uint32_t>((ppcCarryWide >> 32) & 1u) << 29);
    }
    SetCRResident(cr, xer, 0, static_cast<int32_t>(r0), static_cast<int32_t>(0));
}

loc_80090ECC:
{
    if (((cr & 0x20000000u) != 0)) {
        goto loc_80090ED8;
    }
}

loc_80090ED0:
{
    r0 = 0;
    MemoryInline::FlatWrite32((r13 + -23188), r0);
}

loc_80090ED8:
{
    // psq_load w=0 quant=0 (using PPC_PsqL)
    r0 = MemoryInline::FlatRead32((r1 + 68));
    f31.d = MemoryInline::FlatReadFloat64((r1 + 48));
    r31 = MemoryInline::FlatRead32((r1 + 44));
    r30 = MemoryInline::FlatRead32((r1 + 40));
    ctx->lr = r0;
    r1 = (r1 + 64);
    ctx->gpr[0] = r0;
    ctx->gpr[1] = r1;
    ctx->gpr[3] = r3;
    ctx->gpr[4] = r4;
    ctx->gpr[5] = r5;
    ctx->gpr[6] = r6;
    ctx->gpr[7] = r7;
    ctx->gpr[30] = r30;
    ctx->gpr[31] = r31;
    ctx->fpr[0] = f0;
    ctx->fpr[1] = f1;
    ctx->fpr[2] = f2;
    ctx->fpr[3] = f3;
    ctx->fpr[4] = f4;
    ctx->fpr[5] = f5;
    ctx->fpr[6] = f6;
    ctx->fpr[7] = f7;
    ctx->fpr[8] = f8;
    ctx->fpr[31] = f31;
    ctx->cr = cr;
    ctx->ctr = ctr;
    ctx->xer = xer;
    return;
}

}


extern "C" void ScoobyCull_IsSphereInFrustrum(CpuContext* ctx) {
    if (static const bool noCull = std::getenv("GAME_NOCULL") != nullptr; noCull) {
        ctx->gpr[3] = 1;
        return;
    }
    ScoobyCull_IsSphereInFrustrum_Original(ctx);
}
PPC_NATIVE_OVERRIDE_VOID(80090D30, ScoobyCull_IsSphereInFrustrum, (CpuContext* ctx), (ctx));

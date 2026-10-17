// gx_indirect.cpp - Indirect Texture / Bump Mapping
#include "gx_internal.h"

// ============================================================================
// Indirect Texture Stages
// ============================================================================

extern "C" void GX__SetNumIndStages_80171b38(uint32_t n) { GXSetNumIndStages((u8)n); }
PPC_NATIVE_OVERRIDE_VOID(8016dd9c, GX__SetNumIndStages_80171b38, (uint32_t n), (n));

extern "C" void GX__SetIndTexOrder_80171a6c(uint32_t s, uint32_t c, uint32_t m) {
    GXSetIndTexOrder((GXIndTexStageID)s, (GXTexCoordID)(c==0xFFu?0:c), (GXTexMapID)(m==0xFFu?0:m));
}
#if 0  // SCOOBY: GXSetIndTexOrder: name not in Scooby
PPC_NATIVE_OVERRIDE_VOID(8028fc78, GX__SetIndTexOrder_80171a6c, (uint32_t s, uint32_t c, uint32_t m), (s, c, m));
#endif

extern "C" void GX__SetIndTexCoordScale_80171968(uint32_t s, uint32_t ss, uint32_t ts) {
    GXSetIndTexCoordScale((GXIndTexStageID)s, (GXIndTexScale)ss, (GXIndTexScale)ts);
}
PPC_NATIVE_OVERRIDE_VOID(8016DC58, GX__SetIndTexCoordScale_80171968, (uint32_t s, uint32_t ss, uint32_t ts), (s, ss, ts));

extern "C" void GX__SetIndTexMtx_80171814(uint32_t id, uint32_t ma, uint32_t se) {
    float m[6]; for(int i=0; i<6; ++i) m[i]=Memory::ReadFloat32(ma+i*4);
    GXSetIndTexMtx((GXIndTexMtxID)id, m, (s8)se);
}
#if 0  // SCOOBY: GXSetIndTexMtx: name not in Scooby
PPC_NATIVE_OVERRIDE_VOID(8028FA20, GX__SetIndTexMtx_80171814, (uint32_t id, uint32_t ma, uint32_t se), (id, ma, se));
#endif

// ============================================================================
// TEV Indirect Texture Control
// ============================================================================

extern "C" void GX__SetTevDirect_80171b58(uint32_t s) { GXSetTevDirect((GXTevStageID)s); }
PPC_NATIVE_OVERRIDE_VOID(8016ddc0, GX__SetTevDirect_80171b58, (uint32_t s), (s));

extern "C" void GX__SetTevIndWarp_80171ba0(uint32_t ts, uint32_t is, uint32_t so, uint32_t rm, uint32_t ms) {
    GXSetTevIndWarp((GXTevStageID)std::min(ts, 15u), (GXIndTexStageID)std::min(is, 3u),
        (GXBool)so, (GXBool)rm, (GXIndTexMtxID)ms);
}
#if 0  // SCOOBY: GXSetTevIndWarp: name not in Scooby
PPC_NATIVE_OVERRIDE_VOID(8028fdac, GX__SetTevIndWarp_80171ba0, (uint32_t ts, uint32_t is, uint32_t so, uint32_t rm, uint32_t ms), (ts, is, so, rm, ms));
#endif

extern "C" void GX__SetTevIndirect_801717ac(uint32_t ts, uint32_t is, uint32_t f, uint32_t bs, uint32_t ms, uint32_t ws, uint32_t wt, uint32_t ap, uint32_t il, uint32_t as) {
    GXSetTevIndirect((GXTevStageID)std::min(ts,15u), (GXIndTexStageID)std::min(is,3u), (GXIndTexFormat)f,
        (GXIndTexBiasSel)bs, (GXIndTexMtxID)ms, (GXIndTexWrap)ws, (GXIndTexWrap)wt,
        (GXBool)ap, (GXBool)il, (GXIndTexAlphaSel)as);
}
PPC_NATIVE_OVERRIDE_VOID(8016dbec, GX__SetTevIndirect_801717ac, (uint32_t ts, uint32_t is, uint32_t f, uint32_t bs, uint32_t ms, uint32_t ws, uint32_t wt, uint32_t ap, uint32_t il, uint32_t as), (ts, is, f, bs, ms, ws, wt, ap, il, as));

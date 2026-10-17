extern "C" unsigned g_spmAudit[8];
extern "C" unsigned g_spmGameClear;
extern "C" int g_spmSweepDump;
#include "spm_map_sweep.h"
#include "abi_bridge.h"
#include "memory.h"
#include "ppc_runtime.h"
#include "runtime_log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace SpmMapSweep {
namespace {
using Clock = std::chrono::steady_clock;
// Diagnostic (env SPM_MTX_SELFTEST): runs the game's own matrix/vector routines through the recompiled code and compares
// with plain C++ math. Any "MISMATCH" line names a routine whose recompiled result is wrong.
void RunMtxSelfTest(CpuContext* cpu) {
    CpuContext a = *cpu;
    a.gpr[3] = 0;
    a.gpr[4] = 1024;
    InvokeIndirectCpu(0x801A5634, &a);  // __memAlloc(HEAP_MAIN, 1024)
    const uint32_t base = a.gpr[3];
    if (!base || !Memory::Contains(base, 1024)) {
        RT_LOGF(RT_TAG_RUNTIME, "[mtxtest] alloc failed\n");
        return;
    }
    auto put = [&](uint32_t off, const float* v, int n) { for (int i = 0; i < n; ++i) Memory::WriteFloat32(base + off + i * 4, v[i]); };
    auto get = [&](uint32_t off, float* v, int n) { for (int i = 0; i < n; ++i) v[i] = Memory::ReadFloat32(base + off + i * 4); };
    auto call = [&](uint32_t fn, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, std::initializer_list<double> f) {
        CpuContext c = *cpu;
        c.gpr[3] = r3; c.gpr[4] = r4; c.gpr[5] = r5; c.gpr[6] = r6;
        int i = 1;
        for (double d : f) c.fpr[i++].d = d;
        InvokeIndirectCpu(fn, &c);
    };
    int bad = 0;
    auto cmp = [&](const char* name, uint32_t off, const float* exp, int n) {
        float got[16] = {};
        get(off, got, n);
        float md = 0, mx = 1;
        for (int i = 0; i < n; ++i) { md = std::max(md, std::fabs(got[i] - exp[i])); mx = std::max(mx, std::fabs(exp[i])); }
        const bool ok = md <= 1e-3f * mx;
        bad += !ok;
        RT_LOGF(RT_TAG_RUNTIME, "[mtxtest] %s %s maxdiff=%g got=[%g %g %g %g] exp=[%g %g %g %g]\n", ok ? "ok      " : "MISMATCH", name, md,
                got[0], got[1], got[2], got[3], exp[0], exp[1], exp[2], exp[3]);
    };
    const uint32_t A = 0x000, B = 0x040, R = 0x080, V = 0x0C0, VO = 0x0D0, CAM = 0x100, UP = 0x110, TGT = 0x120, M4 = 0x140;
    const float ma[12] = {0.8f, 0.1f, -0.3f, 5.f, 0.2f, 0.9f, 0.4f, -7.f, -0.1f, 0.3f, 1.1f, 2.f};
    const float mb[12] = {1.2f, -0.4f, 0.5f, 3.f, 0.3f, 0.7f, -0.2f, 11.f, 0.6f, 0.1f, 0.9f, -4.f};
    put(A, ma, 12); put(B, mb, 12);
    float e[16] = {};
    // Concat
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j) { float v = j == 3 ? ma[i * 4 + 3] : 0; for (int k = 0; k < 3; ++k) v += ma[i * 4 + k] * mb[k * 4 + j]; e[i * 4 + j] = v; }
    call(0x80151244, base + A, base + B, base + R, 0, {});
    cmp("PSMTXConcat", R, e, 12);
    // Inverse
    {
        const float* m = ma;
        float c00 = m[5] * m[10] - m[6] * m[9], c01 = m[2] * m[9] - m[1] * m[10], c02 = m[1] * m[6] - m[2] * m[5];
        float c10 = m[6] * m[8] - m[4] * m[10], c11 = m[0] * m[10] - m[2] * m[8], c12 = m[2] * m[4] - m[0] * m[6];
        float c20 = m[4] * m[9] - m[5] * m[8], c21 = m[1] * m[8] - m[0] * m[9], c22 = m[0] * m[5] - m[1] * m[4];
        const float det = m[0] * c00 + m[1] * c10 + m[2] * c20;
        const float r[9] = {c00 / det, c01 / det, c02 / det, c10 / det, c11 / det, c12 / det, c20 / det, c21 / det, c22 / det};
        for (int i = 0; i < 3; ++i) { for (int j = 0; j < 3; ++j) e[i * 4 + j] = r[i * 3 + j]; e[i * 4 + 3] = -(r[i * 3] * m[3] + r[i * 3 + 1] * m[7] + r[i * 3 + 2] * m[11]); }
        call(0x80151310, base + A, base + R, 0, 0, {});
        cmp("PSMTXInverse", R, e, 12);
    }
    // MultVec
    {
        const float v[3] = {3.f, -2.f, 7.f};
        put(V, v, 3);
        for (int i = 0; i < 3; ++i) e[i] = ma[i * 4] * v[0] + ma[i * 4 + 1] * v[1] + ma[i * 4 + 2] * v[2] + ma[i * 4 + 3];
        call(0x801514D0, base + A, base + V, base + VO, 0, {});
        cmp("PSMTXMultVec", VO, e, 3);
        const float cr[3] = {v[1] * 4.f - v[2] * 0.5f, v[2] * 1.f - v[0] * 4.f, v[0] * 0.5f - v[1] * 1.f};
        const float w[3] = {1.f, 0.5f, 4.f};
        put(R, w, 3);
        call(0x80278978, base + V, base + R, base + VO, 0, {});
        const float ce[3] = {v[1] * w[2] - v[2] * w[1], v[2] * w[0] - v[0] * w[2], v[0] * w[1] - v[1] * w[0]};
        (void)cr;
        cmp("PSVECCrossProduct", VO, ce, 3);
        call(0x802788B8, base + V, base + VO, 0, 0, {});
        const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        const float ne[3] = {v[0] / len, v[1] / len, v[2] / len};
        cmp("PSVECNormalize", VO, ne, 3);
    }
    // LookAt
    {
        const float cam[3] = {10.f, 20.f, 830.f}, up[3] = {0.f, 1.f, 0.f}, tgt[3] = {10.f, 20.f, 30.f};
        put(CAM, cam, 3); put(UP, up, 3); put(TGT, tgt, 3);
        float look[3] = {cam[0] - tgt[0], cam[1] - tgt[1], cam[2] - tgt[2]};
        auto norm = [](float* v) { const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); v[0] /= l; v[1] /= l; v[2] /= l; };
        norm(look);
        float right[3] = {up[1] * look[2] - up[2] * look[1], up[2] * look[0] - up[0] * look[2], up[0] * look[1] - up[1] * look[0]};
        norm(right);
        const float u[3] = {look[1] * right[2] - look[2] * right[1], look[2] * right[0] - look[0] * right[2], look[0] * right[1] - look[1] * right[0]};
        const float* rows[3] = {right, u, look};
        for (int i = 0; i < 3; ++i) { for (int j = 0; j < 3; ++j) e[i * 4 + j] = rows[i][j]; e[i * 4 + 3] = -(rows[i][0] * cam[0] + rows[i][1] * cam[1] + rows[i][2] * cam[2]); }
        call(0x80277FBC, base + R, base + CAM, base + UP, base + TGT, {});
        cmp("C_MTXLookAt", R, e, 12);
    }
    // Trans / Scale / RotRad(y)
    {
        call(0x80277E18, base + R, 0, 0, 0, {12.5, -34.25, 800.0});
        const float te[12] = {1, 0, 0, 12.5f, 0, 1, 0, -34.25f, 0, 0, 1, 800.f};
        cmp("PSMTXTrans", R, te, 12);
        call(0x80277E98, base + R, 0, 0, 0, {2.0, 3.0, 0.5});
        const float se[12] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0.5f, 0};
        cmp("PSMTXScale", R, se, 12);
        const double ang = 0.6;
        call(0x80277BC0, base + R, 'y', 0, 0, {ang});
        const float c = std::cos(static_cast<float>(ang)), sn = std::sin(static_cast<float>(ang));
        const float re[12] = {c, 0, sn, 0, 0, 1, 0, 0, -sn, 0, c, 0};
        cmp("PSMTXRotRad(y)", R, re, 12);
    }
    // Ortho (4x4): t,b,l,r,n,f
    {
        call(0x801515C0, base + M4, 0, 0, 0, {143.0, -143.0, -250.0, 250.0, 0.0, 10000.0});
        const float oe[16] = {2.f / 500.f, 0, 0, 0, 0, 2.f / 286.f, 0, 0, 0, 0, -1.f / 10000.f, -1.f, 0, 0, 0, 1};
        float tmp[16] = {2.f / 500.f, 0, 0, 0, 0, 2.f / 286.f, 0, 0, 0, 0, -1.f / 10000.f, -0.f, 0, 0, 0, 1};
        (void)tmp;
        float got[16];
        get(M4, got, 16);
        const float exp[16] = {oe[0], 0, 0, 0, 0, oe[5], 0, 0, 0, 0, oe[10], -0.f, 0, 0, 0, 1};
        RT_LOGF(RT_TAG_RUNTIME, "[mtxtest] C_MTXOrtho raw: %g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g (expect m00=%g m11=%g m22=%g m23=%g)\n",
                got[0], got[1], got[2], got[3], got[4], got[5], got[6], got[7], got[8], got[9], got[10], got[11], got[12], got[13], got[14], got[15],
                exp[0], exp[5], exp[10], -0.f);
    }
    RT_LOGF(RT_TAG_RUNTIME, "[mtxtest] DONE mismatches=%d\n", bad);
}


// US0 seqSetSeq (0x8017B3C8) stores seq/map/entry; seq_gameInit reads gp->mapName at +0x44.
constexpr uint32_t kNowSeq = 0x8056D0D8, kNextSeq = 0x8056D0DC, kGp = 0x8056D198;

struct Sweep {
    bool enabled = false, pending = false, changed = false, sampling = false;
    std::vector<std::string> maps;
    size_t next = 0;
    uint32_t buffer = 0, firstFrame = 0;
    int dwellSeconds = 3;
    Clock::time_point started = Clock::now(), requested{}, stable{};

    Sweep() {
        const char* path = std::getenv("SPM_MAP_SWEEP_FILE");
        if (!path || !*path) return;
        enabled = true;
        std::ifstream in(path);
        std::string name;
        while (std::getline(in, name)) {
            if (!name.empty() && name.back() == '\r') name.pop_back();
            if (name.empty() || name.size() >= 32 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos) {
                RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] invalid-map-list\n");
                Finish(3);
            }
            maps.push_back(name);
        }
        if (!in.eof() || maps.empty()) {
            RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] unreadable-or-empty-map-list\n");
            Finish(3);
        }
        if (const char* value = std::getenv("SPM_MAP_SWEEP_SECONDS")) {
            const int seconds = std::atoi(value);
            if (seconds >= 1 && seconds <= 60) dwellSeconds = seconds;
        }
        RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] ready %zu maps dwell=%d\n", maps.size(), dwellSeconds);
    }

    [[noreturn]] static void Finish(int code) {
        std::cerr.flush();
        std::fflush(nullptr);
        std::_Exit(code);
    }

    static std::string CurrentMap() {
        uint32_t gp = 0;
        if (!Memory::TryRead32(kGp, gp) || gp < 0x80011B78 || !Memory::Contains(gp, 0x64)) return {};
        std::string name;
        for (uint32_t i = 0; i < 32; ++i) {
            const char c = static_cast<char>(Memory::Read8(gp + 0x44 + i));
            if (!c) return name;
            name += c;
        }
        return {};
    }

    static bool EffectsReady() {
        uint32_t effects = 0, pending = 0, ready = 0, data = 0, a = 0, b = 0;
        Memory::TryRead32(0x8056CFB0, effects);
        if (!effects || !Memory::Contains(effects, 16)) return false;
        Memory::TryRead32(effects + 8, pending);
        Memory::TryRead32(effects + 12, ready);
        Memory::TryRead32(0x8056D804, data);  // effSync's final check: r13-0x771C.
        if (!pending || !ready || !data || !Memory::Contains(data, 20)) return false;
        Memory::TryRead32(data + 12, a);
        Memory::TryRead32(data + 16, b);
        return a && b;
    }

    void Tick(CpuContext* cpu, uint32_t frames) {
        if (!enabled || !cpu) return;
        uint32_t now = 0, after = 0;
        if (!Memory::TryRead32(kNowSeq, now) || !Memory::TryRead32(kNextSeq, after)) return;
        const auto time = Clock::now();
        static Clock::time_point holdUntil{};
        if (!pending) {
            if (now != 2 || after != 2 || CurrentMap().empty() || (!buffer && !EffectsReady())) {
                if (time - started > std::chrono::seconds(90)) {
                    RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] boot-timeout seq=%u next=%u\n", now, after);
                    Finish(2);
                }
                return;
            }
            if (next == maps.size()) {
                if (time < holdUntil) return;  // the last map's screenshot needs the window too
                RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] complete\n");
                Finish(0);
            }
            if (!buffer) {
                CpuContext call = *cpu;
                call.gpr[3] = 0;  // __memAlloc(HEAP_MAIN, 64), persistent across map changes.
                call.gpr[4] = 64;
                InvokeIndirectCpu(0x801A5634, &call);
                buffer = call.gpr[3];
                if (!buffer || !Memory::Contains(buffer, 64)) {
                    RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] allocation-failed\n");
                    Finish(3);
                }
            }
            { static bool mtxDone = false; if (!mtxDone && std::getenv("SPM_MTX_SELFTEST")) { mtxDone = true; RunMtxSelfTest(cpu); } }
            if (time < holdUntil) return;
            const auto& name = maps[next++];
            for (uint32_t i = 0; i < 32; ++i) Memory::Write8(buffer + i, i < name.size() ? name[i] : 0);
            pending = true;
            changed = sampling = false;
            requested = time;
            RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] attempt %s\n", name.c_str());
            if (const char* g0 = std::getenv("SPM_SWEEP_GSW0")) {  // story progress (gp->gsw0) the map scripts branch on
                uint32_t gp = 0;
                if (Memory::TryRead32(kGp, gp) && Memory::Contains(gp, 0x144)) Memory::Write32(gp + 0x140, uint32_t(std::atoi(g0)));
            }
            CpuContext call = *cpu;
            call.gpr[3] = 3;  // seqSetSeq(SEQ_MAPCHANGE, mapName, nullptr).
            call.gpr[4] = buffer;
            call.gpr[5] = 0;
            InvokeIndirectCpu(0x8017B3C8, &call);
            return;
        }
        const auto& name = maps[next - 1];
        changed = changed || now == 3;
        if (changed && now == 2 && after == 2) {
            const std::string actual = CurrentMap();
            if (actual == name) {
                if (!sampling) {
                    stable = time;
                    firstFrame = frames;
                    sampling = true;
                    for (unsigned& c : g_spmAudit) c = 0;
                    RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] loaded %s\n", name.c_str());
                }
                {   // draw dump of 2 frames, 200 frames after load, for the map named in SPM_SWEEP_DUMP
                    static const char* dumpMap = std::getenv("SPM_SWEEP_DUMP");
                    const unsigned rel = frames - firstFrame;
                    g_spmSweepDump = (dumpMap && name == dumpMap && rel >= 200 && rel < 202) ? 1 : 0;
                }
                if (time - stable >= std::chrono::seconds(dwellSeconds) && frames - firstFrame >= 10) {
                    // fadeIsFinish(): 0 means a screen fade/curtain is still active, so a black shot is not a render bug.
                    CpuContext fadeCall = *cpu;
                    InvokeIndirectCpu(0x800676E0, &fadeCall);
                    RT_LOGF(RT_TAG_RUNTIME, "[spmaudit] %s draws=%u dropped=%u nohandle=%u badtexmap=%u clear=%06X fade=%u xfskip=%u nesteddl=%u texnull=%u palnull=%u\n", name.c_str(),
                            g_spmAudit[0], g_spmAudit[1], g_spmAudit[2], g_spmAudit[3], g_spmGameClear, fadeCall.gpr[3] & 0xFF,
                            g_spmAudit[4], g_spmAudit[5], g_spmAudit[6], g_spmAudit[7]);
                    RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] sampled %s frames=%u\n", name.c_str(), frames - firstFrame);
                    { uint32_t g = 0, gsw0 = 0; if (Memory::TryRead32(kGp, g) && Memory::Contains(g, 0x144) && Memory::TryRead32(g + 0x140, gsw0)) RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] gsw0 %d\n", int32_t(gsw0)); }
                    { // MapObj flag0 of interesting objects (mapdrv_wp at r13-0x7F10; spm-headers MapEntry/MapObj layout)
                      uint32_t wp = 0, hidden = 0;
                      if (Memory::TryRead32(0x8056D010u, wp) && Memory::Contains(wp, 0x6AC)) {
                          const uint32_t grp = Memory::Read32(wp + 0x698) & 1u;
                          const uint32_t e = wp + grp * 0x34C;
                          const uint32_t n = Memory::Read32(e + 0x180), objs = Memory::Read32(e + 0x184);
                          for (uint32_t k = 0; k < n && k < 4096 && Memory::Contains(objs + k * 0x144, 0x144); ++k) {
                              const uint32_t o = objs + k * 0x144, f0 = Memory::Read32(o), j = Memory::Read32(o + 0xC);
                              uint32_t np = 0; if (!Memory::Contains(j, 4) || !Memory::TryRead32(j, np) || !Memory::Contains(np, 16)) continue;
                              std::string nm; for (int c = 0; c < 31; ++c) { const char ch = char(Memory::Read8(np + c)); if (!ch || ch < 0x20 || ch > 0x7e) break; nm += ch; }
                              if (f0 & 1u) ++hidden;
                              if (nm == "sky_msk" || nm == "group4" || nm == "dai_k_ani_01" || nm == "sc_doa_02" || nm == "polySurface62_q00197" || nm == "mae_kusa_10")
                                  RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] mapobj %s f0 %08X\n", nm.c_str(), f0);
                          }
                          RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] mapobjs %u hidden %u\n", n, hidden);
                          const uint32_t na = Memory::Read32(e + 0x190), anims = Memory::Read32(e + 0x194);
                          for (uint32_t k = 0; k < na && k < 64 && Memory::Contains(anims + k * 0x28, 0x28); ++k) {
                              const uint32_t a = anims + k * 0x28, fd = Memory::Read32(a + 0x18);
                              uint32_t np = 0; std::string nm;
                              if (Memory::Contains(fd, 4) && Memory::TryRead32(fd, np) && Memory::Contains(np, 16))
                                  for (int c = 0; c < 31; ++c) { const char ch = char(Memory::Read8(np + c)); if (!ch || ch < 0x20 || ch > 0x7e) break; nm += ch; }
                              RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] anim %s flags %04X time %.1f rate %.2f\n", nm.c_str(), Memory::Read16(a), Memory::ReadFloat32(a + 0x10), Memory::ReadFloat32(a + 0x14));
                          }
                      } }
                    holdUntil = time + std::chrono::milliseconds(4000);  // let the audit screenshot see this map (the runner polls the log, then PrintWindows a full frame)
                    pending = false;
                    return;
                }
            } else if (!actual.empty()) {
                RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] redirected %s actual=%s\n", name.c_str(), actual.c_str());
                pending = false;
                return;
            }
        } else {
            sampling = false;
        }
        if (time - requested > std::chrono::seconds(30 + dwellSeconds)) {
            uint32_t stage = 0, rel = 0, effects = 0, effectPending = 0, effectReady = 0;
            Memory::TryRead32(0x804CF364, stage);
            Memory::TryRead32(0x8056D1C0, rel);  // isRelLoaded: r13-0x7D60, byte +8.
            Memory::TryRead32(0x8056CFB0, effects);  // effSync: r13-0x7F70, words +8/+12.
            if (effects && Memory::Contains(effects, 16)) {
                Memory::TryRead32(effects + 8, effectPending);
                Memory::TryRead32(effects + 12, effectReady);
            }
            const uint32_t loaded = rel && Memory::Contains(rel, 9) ? Memory::Read8(rel + 8) : 0;
            RT_LOGF(RT_TAG_RUNTIME, "[spmsweep] timeout %s seq=%u next=%u stage=%u rel=%u effects=%u/%u\n",
                    name.c_str(), now, after, stage, loaded, effectPending, effectReady);
            Finish(2);  // Restart externally; forcing another warp during a failed transition is unsafe.
        }
    }
};
} // namespace

void Tick(CpuContext* cpu, uint32_t presentedFrames) {
    static Sweep sweep;
    sweep.Tick(cpu, presentedFrames);
}
} // namespace SpmMapSweep

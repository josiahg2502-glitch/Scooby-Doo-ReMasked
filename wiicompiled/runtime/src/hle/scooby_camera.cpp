// Scooby-Doo! Unmasked true widescreen. HGCamera_BC::SetViewWindow(x, y) (0x80058860) sizes every camera's view, and
// the game's cameras are 4:3 (y = 0.75 * x) even in its 16:9 mode, so widescreen was the 4:3 view stretched. In a
// widened picture mode the stored window is widened to the picture's aspect, so the game projects and culls for the
// wide view itself. GetViewWindow (0x8005884C) hands back the window as the game set it: the game reads a camera's
// window to keep its aspect, and seeing the widened one fed back until the view collapsed. Square/taller (shadow) and
// pixel-sized screen windows (640 x 480) are left alone.
//
// Native in full: an override's address is excluded from translation. The original bodies are
// camera->viewWindow (+228, +232) = (x, y), r3 = 0; and *r4 = viewWindow.x, *r5 = viewWindow.y.
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "ppc_runtime.h"
#include "abi_bridge.h"
#include "memory.h"
#include "hle_stubs.h"
#include "runtime_log.h"

extern std::atomic<float> g_gameCameraAspect;  // display_settings.cpp

namespace {
float ReadF32(uint32_t a) { const uint32_t w = Memory::Read32(a); float f; std::memcpy(&f, &w, 4); return f; }
void WriteF32(uint32_t a, float f) { uint32_t w; std::memcpy(&w, &f, 4); Memory::Write32(a, w); }
// How much a 4:3-or-wider camera window is widened (1 = not at all).
float Widen() {
    const float aspect = g_gameCameraAspect.load(std::memory_order_relaxed);
    return aspect > 0.0f ? aspect * 0.75f : 1.0f;
}
}

extern "C" void Scooby_CameraSetViewWindow(CpuContext* ctx) {
    if (static const bool log = std::getenv("CAMERA_LOG") != nullptr; log)
        if (static int n = 0; n++ < 60) RT_LOGF(RT_TAG_GX, "[camera] set %.4f x %.4f\n", ctx->fpr[1].d, ctx->fpr[2].d);
    const uint32_t camera = ctx->gpr[3];
    float x = static_cast<float>(ctx->fpr[1].d);
    const float y = static_cast<float>(ctx->fpr[2].d);
    if (x > 0.0f && x < 8.0f && y <= 0.7508f * x) x *= Widen();
    WriteF32(camera + 228, x);
    WriteF32(camera + 232, y);
    ctx->gpr[3] = 0;
}
PPC_NATIVE_OVERRIDE_VOID(80058860, Scooby_CameraSetViewWindow, (CpuContext* ctx), (ctx));

extern "C" void Scooby_CameraGetViewWindow(CpuContext* ctx) {
    const uint32_t camera = ctx->gpr[3];
    const float k = Widen();
    float x = ReadF32(camera + 228);
    const float y = ReadF32(camera + 232);
    if (x > 0.0f && x < 8.0f * k && y <= 0.7508f * x / k) x /= k;  // the inverse of SetViewWindow's widening
    WriteF32(ctx->gpr[4], x);
    WriteF32(ctx->gpr[5], y);
    ctx->fpr[0].d = y;
}
PPC_NATIVE_OVERRIDE_VOID(8005884C, Scooby_CameraGetViewWindow, (CpuContext* ctx), (ctx));

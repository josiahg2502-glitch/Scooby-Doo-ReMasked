#pragma once
#include <cstdint>
struct CpuContext;

namespace SpmMapSweep {
void Tick(CpuContext* cpu, uint32_t presentedFrames);
}

#pragma once

#include <cstdint>

struct CpuContext;

namespace AxDspHle {

void Init();
void InitForAXOut(CpuContext* ctx);
void Stop();

uint32_t CheckInit();
uint32_t AddTask(uint32_t taskPtr);
void SendMailToDSP(uint32_t mail);
uint32_t CheckMailToDSP();
uint32_t CheckMailFromDSP();
uint32_t ReadMailFromDSP();
uint32_t AssertTask(uint32_t taskPtr);
void ServiceDeferredCallbacks();

// INVARIANT: no guest code may observe mix output before this returns. Called from
// Audio_HLE_Tick before the AI DMA block reaches the backend and before the AI callback
// runs __AXOutNewFrame, the two places guest code reads the mix's PB/output/aux data.
// Cheap when the mix already finished (the normal case).
void JoinMixWorker();

// Joins and tears down the mix worker thread. Call before the guest memory map
// is destroyed or re-created.
void ShutdownMixWorker();

void SetMixWorkerEnabled(bool enabled);

void InitAram();

// The rate the backend plays at: 48 kHz when the GameCube mix renders its own 48 kHz output, else the AI rate.
uint32_t OutputSampleRate(uint32_t aiSampleRate);

// The 48 kHz frame (big-endian right/left pairs) the last OUTPUT made for this AI buffer; nullptr when 48 kHz
// output is off or no mix has targeted the buffer yet.
const uint8_t* HiRateFrame(uint32_t aiAddr, uint32_t& bytes);

}

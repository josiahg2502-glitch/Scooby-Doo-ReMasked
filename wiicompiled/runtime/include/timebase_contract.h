#pragma once

#include <chrono>
#include <cstdint>

namespace TimeBaseContract {

// GameCube: Gekko's time base runs at one quarter of the 162 MHz bus clock (the Wii's Broadway runs at 243 MHz).
// Code that hard-codes the GameCube rate (NO100F's Bink movies) ran 1.5x fast on the Wii clock.
// Reduced against one billion nanoseconds per second, that is exactly
// 81 time-base ticks per 2000 nanoseconds (40.5 MHz).
inline constexpr uint64_t kBusClockHz = 162'000'000u;
inline constexpr uint64_t kTimeBaseDivider = 4u;
inline constexpr uint64_t kTicksPerSecond = kBusClockHz / kTimeBaseDivider;
inline constexpr uint64_t kNanosecondsPerSecond = 1'000'000'000u;
inline constexpr uint64_t kTickRatioNumerator = 81u;
inline constexpr uint64_t kTickRatioDenominator = 2'000u;

static_assert(kTicksPerSecond == 40'500'000u);
static_assert(kTicksPerSecond * kTickRatioDenominator ==
              kNanosecondsPerSecond * kTickRatioNumerator);

// Split the rational conversion around the division so the intermediate
// product cannot overflow. The result is floor(nanoseconds * 243 / 4000).
constexpr uint64_t NanosecondsToTicks(uint64_t nanoseconds) noexcept
{
    return (nanoseconds / kTickRatioDenominator) * kTickRatioNumerator +
           ((nanoseconds % kTickRatioDenominator) * kTickRatioNumerator) /
               kTickRatioDenominator;
}

// Convert guest ticks to a host duration without applying scheduling policy.
// Oversized durations retain the existing zero-duration failure behavior.
constexpr std::chrono::nanoseconds TicksToDuration(uint64_t ticks) noexcept
{
    constexpr uint64_t kMaxNanoseconds =
        static_cast<uint64_t>(std::chrono::nanoseconds::max().count());
    constexpr uint64_t kMaxTicks = NanosecondsToTicks(kMaxNanoseconds);

    if (ticks == 0 || ticks > kMaxTicks) {
        return std::chrono::nanoseconds::zero();
    }

    const uint64_t nanoseconds =
        (ticks / kTickRatioNumerator) * kTickRatioDenominator +
        ((ticks % kTickRatioNumerator) * kTickRatioDenominator) /
            kTickRatioNumerator;
    return std::chrono::nanoseconds(static_cast<int64_t>(nanoseconds));
}

} // namespace TimeBaseContract

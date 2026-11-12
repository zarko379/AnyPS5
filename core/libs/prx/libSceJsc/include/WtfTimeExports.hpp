#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::WtfAbi {

enum class ClockType : int {
    Wall = 0,
    Monotonic = 1,
};

struct Seconds {
    double value;
};

struct TimeWithDynamicClockType {
    double value;
    ClockType type;
};

struct MediaTime {
    union {
        std::int64_t timeValue;
        double timeValueAsDouble;
    };
    std::uint32_t timeScale;
    std::uint8_t timeFlags;
};

inline constexpr std::uint8_t MediaTimeValid = 1 << 0;
inline constexpr std::uint8_t MediaTimeHasBeenRounded = 1 << 1;
inline constexpr std::uint8_t MediaTimePositiveInfinite = 1 << 2;
inline constexpr std::uint8_t MediaTimeNegativeInfinite = 1 << 3;
inline constexpr std::uint8_t MediaTimeIndefinite = 1 << 4;
inline constexpr std::uint8_t MediaTimeDoubleValue = 1 << 5;

static_assert(sizeof(Seconds) == 8);
static_assert(sizeof(TimeWithDynamicClockType) == 16);
static_assert(offsetof(TimeWithDynamicClockType, value) == 0);
static_assert(offsetof(TimeWithDynamicClockType, type) == 8);
static_assert(std::is_trivially_copyable_v<TimeWithDynamicClockType>);
static_assert(sizeof(MediaTime) == 16);
static_assert(offsetof(MediaTime, timeScale) == 8);
static_assert(offsetof(MediaTime, timeFlags) == 12);

} // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(const Jsc::WtfAbi::TimeWithDynamicClockType* self, const Jsc::WtfAbi::TimeWithDynamicClockType* other);
Jsc::WtfAbi::TimeWithDynamicClockType APS5_VABI _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(const Jsc::WtfAbi::Seconds* self, const Jsc::WtfAbi::TimeWithDynamicClockType* other);
bool APS5_VABI _ZNK3WTF9MediaTimeneERKS0_(const Jsc::WtfAbi::MediaTime* self, const Jsc::WtfAbi::MediaTime* rhs);

}

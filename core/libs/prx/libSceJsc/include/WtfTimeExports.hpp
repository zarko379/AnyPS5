#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

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

static_assert(sizeof(MediaTime) == 16);
static_assert(offsetof(MediaTime, timeScale) == 8);
static_assert(offsetof(MediaTime, timeFlags) == 12);

} // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZNK3WTF9MediaTimeneERKS0_(const Jsc::WtfAbi::MediaTime* self, const Jsc::WtfAbi::MediaTime* rhs);

}

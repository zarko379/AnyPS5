// Derived from FreeType src/base/fttrigon.c (FreeType 2.4.11, commit c6a66b49e64e9a21c013f23c867d238cf1aa98cb).
// Copyright 2001-2005, 2012 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Trigonometry.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

using FreeType::GuestVector;

namespace {

using Downscale = std::int64_t (*)(std::int64_t);

constexpr std::int64_t AnglePi = std::int64_t{180} << 16;
constexpr std::int64_t AnglePi2 = AnglePi / 2;
constexpr std::uint64_t TrigScale = 0x9B74EDA8u;
constexpr std::int64_t ComponentLimit = 0xFFFFFFFF;
constexpr std::int64_t ArctanTable[] = {
    2949120, 1740967, 919879, 466945, 234379, 117304, 58666, 29335, 14668, 7334, 3667, 1833,
    917, 458, 229, 115, 57, 29, 14, 7, 4, 2, 1,
};

struct Normalized {
    GuestVector vector;
    std::int32_t shift;
};

std::int64_t ShiftLeft(std::int64_t value, std::int32_t shift) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(value) << shift);
}

std::int64_t DownscaleLong64(std::int64_t value) {
    const auto magnitude = static_cast<std::uint64_t>(value < 0 ? -value : value);
    const auto scaled = static_cast<std::int64_t>((magnitude * TrigScale + 0x100000000u) >> 32);
    return value >= 0 ? scaled : -scaled;
}

std::int64_t DownscaleInt32(std::int64_t value) {
    const std::int64_t magnitude = value < 0 ? -value : value;
    const std::uint32_t v1 = static_cast<std::uint32_t>(magnitude) >> 16;
    const auto v2 = static_cast<std::uint32_t>(magnitude & 0xFFFF);
    const std::uint32_t k1 = static_cast<std::uint32_t>(TrigScale) >> 16;
    const auto k2 = static_cast<std::uint32_t>(TrigScale & 0xFFFF);
    std::uint32_t hi = k1 * v1;
    std::uint32_t lo1 = k1 * v2 + k2 * v1;
    const std::uint32_t lo2 = (k2 * v2) >> 16;
    const std::uint32_t lo3 = lo1 >= lo2 ? lo1 : lo2;
    lo1 += lo2;
    hi += lo1 >> 16;
    if (lo1 < lo3) hi += 0x10000u;
    const auto scaled = static_cast<std::int64_t>(hi);
    return value >= 0 ? scaled : -scaled;
}

void CheckComponents(const GuestVector& vector, const char* function) {
    if (vector.x < -ComponentLimit || vector.x > ComponentLimit || vector.y < -ComponentLimit || vector.y > ComponentLimit) {
        NotImplemented_nid_no_patch(function);
    }
}

Normalized Prenorm(const GuestVector& vector) {
    const std::int64_t x = vector.x;
    const std::int64_t y = vector.y;
    std::int64_t z = (x >= 0 ? x : -x) | (y >= 0 ? y : -y);
    std::int32_t shift = 0;
    for (const std::int32_t step : {16, 8, 4, 2, 1}) {
        if (z >= (std::int64_t{1} << step)) {
            z >>= step;
            shift += step;
        }
    }
    if (shift <= 27) {
        shift = 27 - shift;
        return {{ShiftLeft(x, shift), ShiftLeft(y, shift)}, shift};
    }
    shift -= 27;
    return {{x >> shift, y >> shift}, -shift};
}

std::int64_t ReduceAngle(std::int64_t theta, bool* negate) {
    __int128 turns = 0;
    __int128 reduced = theta;
    if (theta <= -AnglePi2) {
        turns = (static_cast<__int128>(-AnglePi2) - theta) / AnglePi + 1;
        reduced += turns * AnglePi;
    } else if (theta > AnglePi2) {
        turns = (static_cast<__int128>(theta) - AnglePi2 + AnglePi - 1) / AnglePi;
        reduced -= turns * AnglePi;
    }
    *negate = (turns & 1) != 0;
    return static_cast<std::int64_t>(reduced);
}

GuestVector PseudoRotate(const GuestVector& vector, std::int64_t angle) {
    bool negate = false;
    std::int64_t theta = ReduceAngle(angle, &negate);
    std::int64_t x = negate ? -vector.x : vector.x;
    std::int64_t y = negate ? -vector.y : vector.y;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(std::size(ArctanTable)); ++i) {
        const std::int64_t xtemp = theta < 0 ? x + (y >> i) : x - (y >> i);
        y = theta < 0 ? y - (x >> i) : y + (x >> i);
        x = xtemp;
        theta += theta < 0 ? ArctanTable[i] : -ArctanTable[i];
    }
    return {x, y};
}

GuestVector PseudoPolarize(const GuestVector& vector) {
    std::int64_t x = vector.x;
    std::int64_t y = vector.y;
    std::int64_t theta = 0;
    if (x < 0) {
        x = -x;
        y = -y;
        theta = 2 * AnglePi2;
    }
    if (y > 0) theta = -theta;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(std::size(ArctanTable)); ++i) {
        const bool positive = y > 0;
        const std::int64_t xtemp = positive ? x + (y >> i) : x - (y >> i);
        y = positive ? y - (x >> i) : y + (x >> i);
        x = xtemp;
        theta += positive ? ArctanTable[i] : -ArctanTable[i];
    }
    theta = theta >= 0 ? (theta + 16) & ~std::int64_t{31} : -((-theta + 16) & ~std::int64_t{31});
    return {x, theta};
}

template <typename TFinish>
auto BothBuilds(TFinish finish, const char* function) {
    const auto long64 = finish(DownscaleLong64);
    const auto int32 = finish(DownscaleInt32);
    if (long64 != int32) NotImplemented_nid_no_patch(function);
    return long64;
}

GuestVector Rotated(const GuestVector& vector, std::int64_t angle, const char* function) {
    CheckComponents(vector, function);
    const Normalized normalized = Prenorm(vector);
    const GuestVector rotated = PseudoRotate(normalized.vector, angle);
    const std::int32_t shift = normalized.shift;
    const auto finish = [&](Downscale downscale) {
        const std::int64_t x = downscale(rotated.x);
        const std::int64_t y = downscale(rotated.y);
        if (shift > 0) {
            const std::int64_t half = std::int64_t{1} << (shift - 1);
            return std::pair{(x + half + (x >> 63)) >> shift, (y + half + (y >> 63)) >> shift};
        }
        return std::pair{ShiftLeft(x, -shift), ShiftLeft(y, -shift)};
    };
    const auto [x, y] = BothBuilds(finish, function);
    return {x, y};
}

}

extern "C" {

std::int64_t APS5_VABI sceFreeTypeOtVectorLength_nid_no_patch(const GuestVector* vector) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    const GuestVector value = *vector;
    if (value.x == 0 || value.y == 0) {
        const std::int64_t axis = value.x == 0 ? value.y : value.x;
        if (axis == std::numeric_limits<std::int64_t>::min()) NotImplemented_nid_no_patch(__func__);
        return axis >= 0 ? axis : -axis;
    }
    CheckComponents(value, __func__);
    const Normalized normalized = Prenorm(value);
    const GuestVector polar = PseudoPolarize(normalized.vector);
    const std::int32_t shift = normalized.shift;
    return BothBuilds([&](Downscale downscale) {
        const std::int64_t length = downscale(polar.x);
        return shift > 0 ? (length + (std::int64_t{1} << (shift - 1))) >> shift : ShiftLeft(length, -shift);
    }, __func__);
}
APS5_EXPORT("1jmC2QcpRl0", sceFreeTypeOtVectorLength_nid_no_patch);

void APS5_VABI sceFreeTypeOtVectorRotate_nid_no_patch(GuestVector* vector, std::int64_t angle) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    const GuestVector value = *vector;
    if (angle == 0 || (value.x == 0 && value.y == 0)) return;
    *vector = Rotated(value, angle, __func__);
}
APS5_EXPORT("6wJwUIVwAxg", sceFreeTypeOtVectorRotate_nid_no_patch);

void APS5_VABI sceFreeTypeOtVectorPolarize_nid_no_patch(const GuestVector* vector, std::int64_t* length, std::int64_t* angle) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    const GuestVector value = *vector;
    if (value.x == 0 && value.y == 0) return;
    if (!length || !angle) NotImplemented_nid_no_patch(__func__);
    CheckComponents(value, __func__);
    const Normalized normalized = Prenorm(value);
    const GuestVector polar = PseudoPolarize(normalized.vector);
    const std::int32_t shift = normalized.shift;
    *length = BothBuilds([&](Downscale downscale) {
        const std::int64_t scaled = downscale(polar.x);
        return shift >= 0 ? scaled >> shift : ShiftLeft(scaled, -shift);
    }, __func__);
    *angle = polar.y;
}
APS5_EXPORT("HMcNe70D9TI", sceFreeTypeOtVectorPolarize_nid_no_patch);

void APS5_VABI sceFreeTypeOtVectorFromPolar_nid_no_patch(GuestVector* vector, std::int64_t length, std::int64_t angle) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    GuestVector value{length, 0};
    if (angle != 0 && length != 0) value = Rotated(value, angle, __func__);
    *vector = value;
}
APS5_EXPORT("2rmvbvOCyjY", sceFreeTypeOtVectorFromPolar_nid_no_patch);

}

/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Derived from Source/WTF/wtf/TimeWithDynamicClockType.cpp, TimeWithDynamicClockType.h and Seconds.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Derived from Source/WTF/wtf/MediaTime.cpp and MediaTime.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/WtfTimeExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

using namespace Jsc::WtfAbi;

enum class Comparison { LessThan = -1, EqualTo = 0, GreaterThan = 1 };

template<typename T> const T& Require(const T* value, const char* function) {
    if (!value) throw std::invalid_argument(function);
    return *value;
}

double ToDouble(const MediaTime& time) {
    if (time.timeFlags & MediaTimeDoubleValue) return time.timeValueAsDouble;
    return static_cast<double>(time.timeValue) / time.timeScale;
}

Comparison CompareRational(const MediaTime& lhs, const MediaTime& rhs, const char* function) {
    const std::int64_t lhsValue = lhs.timeValue;
    const std::int64_t rhsValue = rhs.timeValue;
    const std::int64_t lhsScale = lhs.timeScale;
    const std::int64_t rhsScale = rhs.timeScale;
    if ((lhsValue < 0) != (rhsValue < 0)) return lhsValue < 0 ? Comparison::LessThan : Comparison::GreaterThan;
    if (!lhsValue && !rhsValue) return Comparison::EqualTo;
    if (lhsScale == rhsScale) {
        if (lhsValue == rhsValue) return Comparison::EqualTo;
        return lhsValue < rhsValue ? Comparison::LessThan : Comparison::GreaterThan;
    }
    if (lhsValue == rhsValue) return lhsScale < rhsScale ? Comparison::GreaterThan : Comparison::LessThan;
    if (lhsValue >= 0) {
        if (lhsValue < rhsValue && lhsScale > rhsScale) return Comparison::LessThan;
        if (lhsValue > rhsValue && lhsScale < rhsScale) return Comparison::GreaterThan;
    } else {
        if (lhsValue < rhsValue && lhsScale < rhsScale) return Comparison::LessThan;
        if (lhsValue > rhsValue && lhsScale > rhsScale) return Comparison::GreaterThan;
    }
    std::int64_t lhsFactor;
    std::int64_t rhsFactor;
    if (!__builtin_mul_overflow(lhsValue, rhsScale, &lhsFactor) && !__builtin_mul_overflow(rhsValue, lhsScale, &rhsFactor)) {
        if (lhsFactor == rhsFactor) return Comparison::EqualTo;
        return lhsFactor < rhsFactor ? Comparison::LessThan : Comparison::GreaterThan;
    }
    if (!rhsScale || !lhsScale) throw std::domain_error(function);
    const std::int64_t rhsWhole = rhsValue / rhsScale;
    const std::int64_t lhsWhole = lhsValue / lhsScale;
    if (lhsWhole > rhsWhole) return Comparison::GreaterThan;
    if (lhsWhole < rhsWhole) return Comparison::LessThan;
    const std::int64_t rhsRemain = rhsValue % rhsScale;
    const std::int64_t lhsRemain = lhsValue % lhsScale;
    if (__builtin_mul_overflow(lhsRemain, rhsScale, &lhsFactor) || __builtin_mul_overflow(rhsRemain, lhsScale, &rhsFactor)) throw std::overflow_error(function);
    if (lhsFactor == rhsFactor) return Comparison::EqualTo;
    return lhsFactor > rhsFactor ? Comparison::GreaterThan : Comparison::LessThan;
}

Comparison Compare(const MediaTime& lhs, const MediaTime& rhs, const char* function) {
    const unsigned andFlags = lhs.timeFlags & rhs.timeFlags;
    if (andFlags & (MediaTimePositiveInfinite | MediaTimeNegativeInfinite | MediaTimeIndefinite)) return Comparison::EqualTo;
    const unsigned orFlags = lhs.timeFlags | rhs.timeFlags;
    if (!(orFlags & MediaTimeValid)) return Comparison::EqualTo;
    if (!(andFlags & MediaTimeValid)) return !(lhs.timeFlags & MediaTimeValid) ? Comparison::GreaterThan : Comparison::LessThan;
    if (orFlags & MediaTimeNegativeInfinite) return (lhs.timeFlags & MediaTimeNegativeInfinite) ? Comparison::LessThan : Comparison::GreaterThan;
    if (orFlags & MediaTimePositiveInfinite) return (lhs.timeFlags & MediaTimePositiveInfinite) ? Comparison::GreaterThan : Comparison::LessThan;
    if (orFlags & MediaTimeIndefinite) return (lhs.timeFlags & MediaTimeIndefinite) ? Comparison::GreaterThan : Comparison::LessThan;
    if (andFlags & MediaTimeDoubleValue) {
        if (lhs.timeValueAsDouble == rhs.timeValueAsDouble) return Comparison::EqualTo;
        return lhs.timeValueAsDouble < rhs.timeValueAsDouble ? Comparison::LessThan : Comparison::GreaterThan;
    }
    if (orFlags & MediaTimeDoubleValue) {
        const double a = ToDouble(lhs);
        const double b = ToDouble(rhs);
        if (a > b) return Comparison::GreaterThan;
        if (a < b) return Comparison::LessThan;
        return Comparison::EqualTo;
    }
    return CompareRational(lhs, rhs, function);
}

} // namespace

extern "C" {

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(const TimeWithDynamicClockType* self, const TimeWithDynamicClockType* other) {
    const TimeWithDynamicClockType& lhs = Require(self, __func__);
    const TimeWithDynamicClockType& rhs = Require(other, __func__);
    if (lhs.type != rhs.type) throw std::invalid_argument(__func__);
    return lhs.value < rhs.value;
}

TimeWithDynamicClockType APS5_VABI _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(const Seconds* self, const TimeWithDynamicClockType* other) {
    const Seconds& seconds = Require(self, __func__);
    const TimeWithDynamicClockType& time = Require(other, __func__);
    return {time.value + seconds.value, time.type};
}

bool APS5_VABI _ZNK3WTF9MediaTimeneERKS0_(const MediaTime* self, const MediaTime* rhs) {
    return Compare(Require(self, __func__), Require(rhs, __func__), __func__) != Comparison::EqualTo;
}

}

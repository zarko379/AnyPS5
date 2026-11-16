// Derived from Source/JavaScriptCore/bytecode/SpeculatedType.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
 */

#include "prx/libSceJsc/include/JscRuntimeExports.hpp"
#include "prx/libc/include/General.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace {

constexpr std::uint64_t SpecBoolInt32 = 1ull << 30;
constexpr std::uint64_t SpecNonBoolInt32 = 1ull << 31;
constexpr std::uint64_t SpecAnyIntAsDouble = 1ull << 34;
constexpr std::uint64_t SpecNonIntAsDouble = 1ull << 35;
constexpr std::uint64_t SpecDoublePureNaN = 1ull << 36;
constexpr std::uint64_t SpecBoolean = 1ull << 38;
constexpr std::uint64_t SpecOther = 1ull << 39;
constexpr std::uint64_t SpecEmpty = 1ull << 40;

constexpr std::uint64_t DoubleEncodeOffset = 1ull << 49;
constexpr std::uint64_t NumberTag = 0xfffe000000000000ull;
constexpr std::uint64_t OtherTag = 0x2;
constexpr std::uint64_t ValueFalse = 0x6;
constexpr std::uint64_t NotCellMask = NumberTag | OtherTag;
constexpr double Int52Limit = 2251799813685248.0;

bool IsInt52(double number) {
    if (std::isnan(number)) return false;
    if (!(number >= -Int52Limit && number < Int52Limit)) return false;
    if (std::trunc(number) != number) return false;
    return !(number == 0 && std::signbit(number));
}

}

extern "C" {

std::uint64_t APS5_VABI _ZN3JSC20speculationFromValueENS_7JSValueE(std::uint64_t value) {
    if (!value) return SpecEmpty;
    if ((value & NumberTag) == NumberTag) return (static_cast<std::int32_t>(value) & ~1) ? SpecNonBoolInt32 : SpecBoolInt32;
    if (value & NumberTag) {
        const double number = std::bit_cast<double>(value - DoubleEncodeOffset);
        if (number != number) return SpecDoublePureNaN;
        return IsInt52(number) ? SpecAnyIntAsDouble : SpecNonIntAsDouble;
    }
    if (!(value & NotCellMask)) NotImplemented_nid_no_patch(__func__);
    if ((value & ~1ull) == ValueFalse) return SpecBoolean;
    return SpecOther;
}

}

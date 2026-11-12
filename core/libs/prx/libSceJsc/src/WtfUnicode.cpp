/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 * Derived from Source/WTF/wtf/unicode/UTF8Conversion.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 * Copyright (C) 1999-2015, International Business Machines Corporation and others. All Rights Reserved.
 * Derived from Source/WTF/icu/unicode/utf8.h and utf16.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/WtfUnicodeExports.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::ConversionResult;

constexpr std::ptrdiff_t IntMax = std::numeric_limits<int>::max();
constexpr const char* Lead3TrailBits = "\x20\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x10\x30\x30";
constexpr const char* Lead4TrailBits = "\x00\x00\x00\x00\x00\x00\x00\x00\x1E\x0F\x0F\x0F\x00\x00\x00\x00";
constexpr std::uint32_t SurrogateOffset = (0xD800u << 10) + 0xDC00u - 0x10000u;

template<typename T> std::ptrdiff_t Distance(const T* begin, const T* end) {
    const auto bytes = static_cast<std::ptrdiff_t>(reinterpret_cast<std::uintptr_t>(end) - reinterpret_cast<std::uintptr_t>(begin));
    return bytes / static_cast<std::ptrdiff_t>(sizeof(T));
}

template<typename T> bool Before(const T* position, const T* end) {
    return reinterpret_cast<std::uintptr_t>(position) < reinterpret_cast<std::uintptr_t>(end);
}

void Reserve(const void* target, std::ptrdiff_t offset, std::ptrdiff_t count, const char* function) {
    if (offset > IntMax - count) throw std::length_error(function);
    if (!target) throw std::invalid_argument(function);
}

bool AppendUTF8(char* target, std::ptrdiff_t& offset, std::ptrdiff_t capacity, std::uint32_t character, const char* function) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(target);
    if (character <= 0x7F) {
        if (offset >= capacity) throw std::out_of_range(function);
        Reserve(target, offset, 1, function);
        bytes[offset++] = static_cast<std::uint8_t>(character);
    } else if (character <= 0x7FF && offset + 1 < capacity) {
        Reserve(target, offset, 2, function);
        bytes[offset++] = static_cast<std::uint8_t>((character >> 6) | 0xC0);
        bytes[offset++] = static_cast<std::uint8_t>((character & 0x3F) | 0x80);
    } else if ((character <= 0xD7FF || (character >= 0xE000 && character <= 0xFFFF)) && offset + 2 < capacity) {
        Reserve(target, offset, 3, function);
        bytes[offset++] = static_cast<std::uint8_t>((character >> 12) | 0xE0);
        bytes[offset++] = static_cast<std::uint8_t>(((character >> 6) & 0x3F) | 0x80);
        bytes[offset++] = static_cast<std::uint8_t>((character & 0x3F) | 0x80);
    } else if (character > 0xFFFF && character <= 0x10FFFF && offset + 3 < capacity) {
        Reserve(target, offset, 4, function);
        bytes[offset++] = static_cast<std::uint8_t>((character >> 18) | 0xF0);
        bytes[offset++] = static_cast<std::uint8_t>(((character >> 12) & 0x3F) | 0x80);
        bytes[offset++] = static_cast<std::uint8_t>(((character >> 6) & 0x3F) | 0x80);
        bytes[offset++] = static_cast<std::uint8_t>((character & 0x3F) | 0x80);
    } else {
        return false;
    }
    return true;
}

bool AppendUTF16(char16_t* target, std::ptrdiff_t& offset, std::ptrdiff_t capacity, std::uint32_t character, const char* function) {
    if (character <= 0xFFFF) {
        if (offset >= capacity) throw std::out_of_range(function);
        Reserve(target, offset, 1, function);
        target[offset++] = static_cast<char16_t>(character);
        return true;
    }
    if (character <= 0x10FFFF && offset + 1 < capacity) {
        Reserve(target, offset, 2, function);
        target[offset++] = static_cast<char16_t>((character >> 10) + 0xD7C0);
        target[offset++] = static_cast<char16_t>((character & 0x3FF) | 0xDC00);
        return true;
    }
    return false;
}

std::int32_t NextUTF8(const std::uint8_t* source, std::ptrdiff_t& offset, std::ptrdiff_t length, std::int32_t substitute) {
    std::int32_t character = source[offset++];
    if (!(character & 0x80)) return character;
    bool valid = false;
    if (offset != length) {
        std::uint8_t trail = 0;
        bool prefix;
        if (character >= 0xE0) {
            if (character < 0xF0) {
                character &= 0xF;
                trail = source[offset];
                prefix = (Lead3TrailBits[character] & (1 << (trail >> 5))) != 0;
                if (prefix) trail &= 0x3F;
            } else {
                character -= 0xF0;
                prefix = character <= 4;
                if (prefix) {
                    trail = source[offset];
                    prefix = (Lead4TrailBits[trail >> 4] & (1 << character)) != 0;
                }
                if (prefix) {
                    character = (character << 6) | (trail & 0x3F);
                    prefix = ++offset != length;
                }
                if (prefix) {
                    trail = static_cast<std::uint8_t>(source[offset] - 0x80);
                    prefix = trail <= 0x3F;
                }
            }
            if (prefix) {
                character = (character << 6) | trail;
                prefix = ++offset != length;
            }
        } else {
            prefix = character >= 0xC2;
            if (prefix) character &= 0x1F;
        }
        if (prefix) {
            trail = static_cast<std::uint8_t>(source[offset] - 0x80);
            if (trail <= 0x3F) {
                character = (character << 6) | trail;
                ++offset;
                valid = true;
            }
        }
    }
    return valid ? character : substitute;
}

template<bool ReplaceInvalidSequences>
bool ConvertUTF8ToUTF16(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllASCII, const char* function) {
    const std::ptrdiff_t length = Distance(source, sourceEnd);
    if (length > IntMax) throw std::length_error(function);
    if (!targetStart) throw std::invalid_argument(function);
    char16_t* target = *targetStart;
    const std::ptrdiff_t capacity = Distance<char16_t>(target, targetEnd);
    if (capacity > IntMax) throw std::length_error(function);
    if (length > 0 && !source) throw std::invalid_argument(function);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(source);
    std::int32_t orAllData = 0;
    std::ptrdiff_t targetOffset = 0;
    for (std::ptrdiff_t sourceOffset = 0; sourceOffset < length;) {
        const std::int32_t character = NextUTF8(bytes, sourceOffset, length, ReplaceInvalidSequences ? 0xFFFD : -1);
        if (character < 0) return false;
        if (!AppendUTF16(target, targetOffset, capacity, static_cast<std::uint32_t>(character), function)) return false;
        orAllData |= character;
    }
    *targetStart = target + targetOffset;
    if (sourceAllASCII) *sourceAllASCII = !(orAllData & ~0x7F);
    return true;
}

} // namespace

extern "C" {

ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict) {
    if (!sourceStart) throw std::invalid_argument(__func__);
    const char16_t* source = *sourceStart;
    if (!targetStart) throw std::invalid_argument(__func__);
    char* target = *targetStart;
    if (!source && Before(source, sourceEnd)) throw std::invalid_argument(__func__);
    const std::ptrdiff_t capacity = Distance(target, targetEnd);
    ConversionResult result = ConversionResult::ConversionOK;
    std::ptrdiff_t offset = 0;
    while (Before(source, sourceEnd)) {
        const std::ptrdiff_t length = Distance(source, sourceEnd);
        std::ptrdiff_t consumed = 0;
        std::uint32_t character = source[consumed++];
        if ((character & 0xFC00) == 0xD800 && consumed != length) {
            const std::uint32_t trail = source[consumed];
            if ((trail & 0xFC00) == 0xDC00) {
                ++consumed;
                character = (character << 10) + trail - SurrogateOffset;
            }
        }
        if ((character & 0xFFFFF800) == 0xD800) {
            if (consumed == length && !(character & 0x400)) {
                result = ConversionResult::SourceExhausted;
                break;
            }
            if (strict) {
                result = ConversionResult::SourceIllegal;
                break;
            }
            character = 0xFFFD;
        }
        if (!AppendUTF8(target, offset, capacity, character, __func__)) {
            result = ConversionResult::TargetExhausted;
            break;
        }
        source += consumed;
    }
    *sourceStart = source;
    *targetStart = target + offset;
    return result;
}

bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* isSourceAllASCII) {
    return ConvertUTF8ToUTF16<false>(source, sourceEnd, targetStart, targetEnd, isSourceAllASCII, __func__);
}

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd) {
    if (!targetStart) throw std::invalid_argument(__func__);
    char* target = *targetStart;
    if (!sourceStart) throw std::invalid_argument(__func__);
    const unsigned char* source = *sourceStart;
    if (!source && Before(source, sourceEnd)) throw std::invalid_argument(__func__);
    const std::ptrdiff_t capacity = Distance(target, targetEnd);
    std::ptrdiff_t offset = 0;
    for (; Before(source, sourceEnd); ++source) {
        if (!AppendUTF8(target, offset, capacity, *source, __func__)) return false;
    }
    *sourceStart = source;
    *targetStart = target + offset;
    return true;
}

bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* isSourceAllASCII) {
    return ConvertUTF8ToUTF16<true>(source, sourceEnd, targetStart, targetEnd, isSourceAllASCII, __func__);
}

}

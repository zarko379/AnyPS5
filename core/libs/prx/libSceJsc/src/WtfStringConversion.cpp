// Derived from Source/WTF/wtf/text/WTFString.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/*
 * Derived from Source/WTF/wtf/text/StringImpl.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/*
 * Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Derived from Source/WTF/wtf/text/StringHasher.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "prx/libSceJsc/include/WtfStringConversionExports.hpp"
#include "prx/libSceJsc/include/WtfUnicodeExports.hpp"
#include "prx/libSceJsc/src/WtfCharacters.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using namespace Jsc::WtfAbi;

constexpr char16_t ReplacementCharacter = 0xFFFD;

bool IsASCIIDigit(char16_t character) {
    return character >= '0' && character <= '9';
}

bool IsASCIIAlpha(char16_t character) {
    return (character | 0x20) >= 'a' && (character | 0x20) <= 'z';
}

bool IsCharacterAllowedInBase(char16_t character, int base) {
    if (character > 0x7F) return false;
    if (IsASCIIDigit(character)) return character - '0' < base;
    if (IsASCIIAlpha(character)) {
        if (base > 36) base = 36;
        return (character >= 'a' && character < 'a' + base - 10) || (character >= 'A' && character < 'A' + base - 10);
    }
    return false;
}

template<typename CharacterType>
unsigned ToUIntStrict(const CharacterType* data, std::size_t length, bool* ok, int base) {
    if (!base) throw std::invalid_argument("WTF::String::toUIntStrict: base 0 divides by zero");
    constexpr unsigned integralMax = std::numeric_limits<unsigned>::max();
    const unsigned maxMultiplier = integralMax / static_cast<unsigned>(base);
    const unsigned maxRemainder = integralMax % static_cast<unsigned>(base);

    unsigned value = 0;
    bool isOk = false;

    if (data) {
        while (length && IsSpaceOrNewline(*data)) {
            --length;
            ++data;
        }
        if (length && *data == '+') {
            --length;
            ++data;
        }
        if (length && IsCharacterAllowedInBase(*data, base)) {
            bool overflow = false;
            while (length && IsCharacterAllowedInBase(*data, base)) {
                --length;
                const char16_t character = *data;
                unsigned digitValue;
                if (IsASCIIDigit(character))
                    digitValue = character - '0';
                else if (character >= 'a')
                    digitValue = character - 'a' + 10;
                else
                    digitValue = character - 'A' + 10;
                if (value > maxMultiplier || (value == maxMultiplier && digitValue > maxRemainder)) {
                    overflow = true;
                    break;
                }
                value = static_cast<unsigned>(base) * value + digitValue;
                ++data;
            }
            if (!overflow) {
                while (length && IsSpaceOrNewline(*data)) {
                    --length;
                    ++data;
                }
                isOk = !length;
            }
        }
    }

    if (ok) *ok = isOk;
    return isOk ? value : 0;
}

void PutUTF8Triple(char*& buffer, const char* bufferEnd, char16_t character) {
    if (bufferEnd - buffer < 3) throw std::length_error("WTF::StringImpl::utf8Impl: buffer too small");
    *buffer++ = static_cast<char>(((character >> 12) & 0x0F) | 0xE0);
    *buffer++ = static_cast<char>(((character >> 6) & 0x3F) | 0x80);
    *buffer++ = static_cast<char>((character & 0x3F) | 0x80);
}

ConversionResult Convert(const char16_t** characters, const char16_t* charactersEnd, char** buffer, char* bufferEnd, bool strict) {
    const ConversionResult result = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(characters, charactersEnd, buffer, bufferEnd, strict);
    if (result == ConversionResult::TargetExhausted) throw std::length_error("WTF::StringImpl::utf8Impl: buffer too small");
    return result;
}


constexpr unsigned StringHashingStartValue = 0x9E3779B9u;
constexpr unsigned StringHashMask = 0x00FFFFFFu;
constexpr unsigned StringFlagCount = 8;
constexpr unsigned StringImplFlagDidReportCost = 1u << 3;
constexpr unsigned StringImplFlagAtom = 1u << 4;
constexpr unsigned StringImplRefCountStatic = 1;

constexpr unsigned AvalancheBits(unsigned hash) {
    unsigned result = hash;
    result ^= result << 3;
    result += result >> 5;
    result ^= result << 2;
    result += result >> 15;
    result ^= result << 10;
    return result;
}

constexpr unsigned EmptyHashAndMaskTop8Bits() {
    const unsigned hash = AvalancheBits(StringHashingStartValue) & StringHashMask;
    return hash ? hash : 0x80000000u >> StringFlagCount;
}

constinit const unsigned char EmptyCharacters[1] = {0};
constinit StringImpl EmptyAtomString{StringImplRefCountStatic, 0, EmptyCharacters,
                                     StringImplFlag8Bit | StringImplFlagDidReportCost | StringImplFlagAtom | (EmptyHashAndMaskTop8Bits() << StringFlagCount)};
}

extern "C" {

const String* APS5_VABI _ZN3WTF11emptyStringEv() {
    static constinit const String emptyString{&EmptyAtomString};
    return &emptyString;
}

const String* APS5_VABI _ZN3WTF10nullStringEv() {
    static constinit const String nullString{nullptr};
    return &nullString;
}

unsigned APS5_VABI _ZNK3WTF6String12toUIntStrictEPbi(const String* self, bool* ok, int base) {
    if (!self) throw std::invalid_argument("WTF::String::toUIntStrict requires an object");
    const StringImpl* impl = self->impl;
    if (!impl) {
        if (ok) *ok = false;
        return 0;
    }
    if (impl->hashAndFlags & StringImplFlag8Bit) return ToUIntStrict(static_cast<const std::uint8_t*>(impl->data), impl->length, ok, base);
    return ToUIntStrict(static_cast<const char16_t*>(impl->data), impl->length, ok, base);
}

UTF8ConversionError APS5_VABI _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(const char16_t* characters, unsigned length, char*& buffer,
                                                                                        std::size_t bufferSize, ConversionMode mode) {
    const char16_t* charactersEnd = characters + length;
    char* bufferEnd = buffer + bufferSize;
    if (mode == StrictConversionReplacingUnpairedSurrogatesWithFFFD) {
        while (characters < charactersEnd) {
            if (Convert(&characters, charactersEnd, &buffer, bufferEnd, true) != ConversionResult::ConversionOK) {
                PutUTF8Triple(buffer, bufferEnd, ReplacementCharacter);
                ++characters;
            }
        }
        return UTF8ConversionError::None;
    }
    const bool strict = mode == StrictConversion;
    const ConversionResult result = Convert(&characters, charactersEnd, &buffer, bufferEnd, strict);
    if (result == ConversionResult::SourceIllegal) return UTF8ConversionError::IllegalSource;
    if (result == ConversionResult::SourceExhausted) {
        if (strict) return UTF8ConversionError::SourceExhausted;
        PutUTF8Triple(buffer, bufferEnd, *characters);
    }
    return UTF8ConversionError::None;
}

}

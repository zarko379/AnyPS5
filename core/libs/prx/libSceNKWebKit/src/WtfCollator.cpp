// Derived from Source/WTF/wtf/unicode/icu/CollatorICU.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2008, 2014 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/CollatorExports.hpp"
#include "prx/libSceIcu/Collation.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::Collator;
using NKWebKit::WtfAbi::StringView;

constexpr std::int32_t UCOL_CASE_FIRST = 2;
constexpr std::int32_t UCOL_NORMALIZATION_MODE = 4;
constexpr std::int32_t UCOL_ON = 17;
constexpr std::int32_t UCOL_LOWER_FIRST = 24;
constexpr std::int32_t UCOL_UPPER_FIRST = 25;
constexpr std::int32_t U_SENTINEL = -1;

UCollator* cachedCollator;
char* cachedCollatorLocale;
bool cachedCollatorShouldSortLowercaseFirst;
std::mutex cachedCollatorMutex;

bool Failed(std::int32_t status) {
    return status > 0;
}

bool LocalesMatch(const char* a, const char* b) {
    return a == b || (a && b && !std::strcmp(a, b));
}

char* DuplicateLocale(const char* locale) {
    if (!locale) return nullptr;
    const auto size = std::strlen(locale) + 1;
    auto* copy = static_cast<char*>(_ZN3WTF10fastMallocEm(size));
    std::memcpy(copy, locale, size);
    return copy;
}

Collator& Checked(Collator* self) {
    if (!self) throw std::invalid_argument("null WTF::Collator");
    return *self;
}

const Collator& Checked(const Collator* self) {
    if (!self) throw std::invalid_argument("null WTF::Collator");
    if (!self->collator) throw std::invalid_argument("WTF::Collator without a UCollator");
    return *self;
}

std::int32_t APS5_VABI GetIndexLatin1(UCharIterator* iterator, UCharIteratorOrigin origin) {
    switch (origin) {
    case UITER_START:
        return iterator->start;
    case UITER_CURRENT:
        return iterator->index;
    case UITER_LIMIT:
        return iterator->limit;
    case UITER_ZERO:
        return 0;
    case UITER_LENGTH:
        return iterator->length;
    }
    return U_SENTINEL;
}

std::int32_t APS5_VABI MoveLatin1(UCharIterator* iterator, std::int32_t delta, UCharIteratorOrigin origin) {
    return iterator->index = GetIndexLatin1(iterator, origin) + delta;
}

std::int8_t APS5_VABI HasNextLatin1(UCharIterator* iterator) {
    return iterator->index < iterator->limit;
}

std::int8_t APS5_VABI HasPreviousLatin1(UCharIterator* iterator) {
    return iterator->index > iterator->start;
}

std::int32_t APS5_VABI CurrentLatin1(UCharIterator* iterator) {
    if (iterator->index >= iterator->limit) return U_SENTINEL;
    return static_cast<const unsigned char*>(iterator->context)[iterator->index];
}

std::int32_t APS5_VABI NextLatin1(UCharIterator* iterator) {
    if (iterator->index >= iterator->limit) return U_SENTINEL;
    return static_cast<const unsigned char*>(iterator->context)[iterator->index++];
}

std::int32_t APS5_VABI PreviousLatin1(UCharIterator* iterator) {
    if (iterator->index <= iterator->start) return U_SENTINEL;
    return static_cast<const unsigned char*>(iterator->context)[--iterator->index];
}

std::uint32_t APS5_VABI GetStateLatin1(const UCharIterator* iterator) {
    return static_cast<std::uint32_t>(iterator->index);
}

void APS5_VABI SetStateLatin1(UCharIterator* iterator, std::uint32_t state, std::int32_t*) {
    iterator->index = static_cast<std::int32_t>(state);
}

std::int32_t Length(unsigned length) {
    if (length > static_cast<unsigned>(INT_MAX)) throw std::length_error("WTF::StringView too long for an ICU iterator");
    return static_cast<std::int32_t>(length);
}

UCharIterator CreateIterator(StringView string) {
    const auto length = Length(string.length);
    if (length && !string.data) throw std::invalid_argument("null WTF::StringView characters");
    UCharIterator iterator{};
    if (string.is8Bit) {
        iterator.context = string.data;
        iterator.length = length;
        iterator.start = 0;
        iterator.index = 0;
        iterator.limit = length;
        iterator.reservedField = 0;
        iterator.getIndex = GetIndexLatin1;
        iterator.move = MoveLatin1;
        iterator.hasNext = HasNextLatin1;
        iterator.hasPrevious = HasPreviousLatin1;
        iterator.current = CurrentLatin1;
        iterator.next = NextLatin1;
        iterator.previous = PreviousLatin1;
        iterator.reservedFn = nullptr;
        iterator.getState = GetStateLatin1;
        iterator.setState = SetStateLatin1;
        return iterator;
    }
    uiter_setString_nid_postfix(&iterator, static_cast<const char16_t*>(string.data), length);
    return iterator;
}

UCharIterator CreateIteratorUTF8(const char* string) {
    if (!string) throw std::invalid_argument("null UTF-8 string");
    const auto length = std::strlen(string);
    if (length > static_cast<std::size_t>(INT_MAX)) throw std::length_error("UTF-8 string too long for an ICU iterator");
    UCharIterator iterator{};
    uiter_setUTF8_nid_postfix(&iterator, string, static_cast<std::int32_t>(length));
    return iterator;
}

int Compare(const Collator& collator, UCharIterator& a, UCharIterator& b) {
    std::int32_t status = 0;
    const auto result = ucol_strcollIter_nid_postfix(collator.collator, &a, &b, &status);
    if (Failed(status)) throw std::runtime_error("ucol_strcollIter failed");
    return result;
}

void Construct(Collator* self, const char* locale, bool shouldSortLowercaseFirst) {
    auto& collator = Checked(self);
    {
        std::lock_guard lock(cachedCollatorMutex);
        if (cachedCollator && LocalesMatch(cachedCollatorLocale, locale) && cachedCollatorShouldSortLowercaseFirst == shouldSortLowercaseFirst) {
            collator.collator = cachedCollator;
            collator.locale = cachedCollatorLocale;
            collator.shouldSortLowercaseFirst = shouldSortLowercaseFirst;
            cachedCollator = nullptr;
            cachedCollatorLocale = nullptr;
            return;
        }
    }
    std::int32_t status = 0;
    auto* opened = ucol_open_nid_postfix(locale, &status);
    if (Failed(status)) {
        status = 0;
        opened = ucol_open_nid_postfix("", &status);
    }
    if (Failed(status) || !opened) throw std::runtime_error("WTF::Collator: ucol_open failed");
    ucol_setAttribute_nid_postfix(opened, UCOL_CASE_FIRST, shouldSortLowercaseFirst ? UCOL_LOWER_FIRST : UCOL_UPPER_FIRST, &status);
    ucol_setAttribute_nid_postfix(opened, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    if (Failed(status)) {
        ucol_close_nid_postfix(opened);
        throw std::runtime_error("WTF::Collator: ucol_setAttribute failed");
    }
    collator.collator = opened;
    collator.locale = DuplicateLocale(locale);
    collator.shouldSortLowercaseFirst = shouldSortLowercaseFirst;
}

void Destroy(Collator* self) {
    auto& collator = Checked(self);
    std::lock_guard lock(cachedCollatorMutex);
    if (cachedCollator) {
        ucol_close_nid_postfix(cachedCollator);
        _ZN3WTF8fastFreeEPv(cachedCollatorLocale);
    }
    cachedCollator = collator.collator;
    cachedCollatorLocale = collator.locale;
    cachedCollatorShouldSortLowercaseFirst = collator.shouldSortLowercaseFirst;
}

}

extern "C" {

void APS5_VABI _ZN3WTF8CollatorC1EPKcb(Collator* self, const char* locale, bool shouldSortLowercaseFirst) {
    Construct(self, locale, shouldSortLowercaseFirst);
}

void APS5_VABI _ZN3WTF8CollatorC2EPKcb(Collator* self, const char* locale, bool shouldSortLowercaseFirst) {
    Construct(self, locale, shouldSortLowercaseFirst);
}

void APS5_VABI _ZN3WTF8CollatorD1Ev(Collator* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF8CollatorD2Ev(Collator* self) {
    Destroy(self);
}

int APS5_VABI _ZNK3WTF8Collator7collateENS_10StringViewES1_(const Collator* self, StringView a, StringView b) {
    const auto& collator = Checked(self);
    auto iteratorA = CreateIterator(a);
    auto iteratorB = CreateIterator(b);
    return Compare(collator, iteratorA, iteratorB);
}

int APS5_VABI _ZNK3WTF8Collator11collateUTF8EPKcS2_(const Collator* self, const char* a, const char* b) {
    const auto& collator = Checked(self);
    auto iteratorA = CreateIteratorUTF8(a);
    auto iteratorB = CreateIteratorUTF8(b);
    return Compare(collator, iteratorA, iteratorB);
}

UCharIterator APS5_VABI _ZN3WTF14createIteratorENS_10StringViewE(StringView string) {
    return CreateIterator(string);
}

}

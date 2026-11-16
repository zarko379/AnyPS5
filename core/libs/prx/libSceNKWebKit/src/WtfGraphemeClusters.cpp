// Derived from Source/WTF/wtf/text/TextBreakIterator.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/TextBreakIteratorExports.hpp"
#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceNKWebKit/include/WtfPlatformDefaultsExports.hpp"

#include <atomic>
#include <climits>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::StringView;

constexpr std::int32_t UBRK_CHARACTER = 0;
constexpr std::int32_t UBRK_DONE = -1;
constexpr std::int32_t UBRK_WORD_NONE = 0;

std::atomic<UBreakIterator*> nonSharedCharacterBreakIterator{nullptr};

const char16_t* Characters16(StringView string) {
    if (string.length > static_cast<unsigned>(INT_MAX)) throw std::length_error("WTF::StringView too long for an ICU break iterator");
    if (string.length && !string.data) throw std::invalid_argument("null WTF::StringView characters");
    return static_cast<const char16_t*>(string.data);
}

const unsigned char* Characters8(StringView string) {
    if (string.length && !string.data) throw std::invalid_argument("null WTF::StringView characters");
    return static_cast<const unsigned char*>(string.data);
}

class CharacterBreakIterator {
public:
    explicit CharacterBreakIterator(StringView string) {
        const auto* characters = Characters16(string);
        m_iterator = nonSharedCharacterBreakIterator.exchange(nullptr, std::memory_order_acquire);
        if (!m_iterator) {
            std::int32_t status = 0;
            m_iterator = ubrk_open_nid_postfix(UBRK_CHARACTER, _ZN3WTF24currentTextBreakLocaleIDEv(), nullptr, 0, &status);
            if (status > 0 || !m_iterator) throw std::runtime_error("ubrk_open of a character break iterator failed");
        }
        std::int32_t status = 0;
        ubrk_setText_nid_postfix(m_iterator, characters, static_cast<std::int32_t>(string.length), &status);
        if (status > 0) {
            Release();
            throw std::runtime_error("ubrk_setText failed");
        }
    }

    ~CharacterBreakIterator() {
        Release();
    }

    CharacterBreakIterator(const CharacterBreakIterator&) = delete;
    CharacterBreakIterator& operator=(const CharacterBreakIterator&) = delete;

    UBreakIterator* Get() const {
        return m_iterator;
    }

private:
    void Release() {
        if (!m_iterator) return;
        if (auto* old = nonSharedCharacterBreakIterator.exchange(m_iterator, std::memory_order_release)) ubrk_close_nid_postfix(old);
        m_iterator = nullptr;
    }

    UBreakIterator* m_iterator = nullptr;
};

}

extern "C" {

bool APS5_VABI _ZN3WTF15isWordTextBreakEP14UBreakIterator(UBreakIterator* iterator) {
    if (!iterator) throw std::invalid_argument("null UBreakIterator");
    return ubrk_getRuleStatus_nid_postfix(iterator) != UBRK_WORD_NONE;
}

unsigned APS5_VABI _ZN3WTF19numGraphemeClustersENS_10StringViewE(StringView string) {
    const unsigned stringLength = string.length;
    if (!stringLength) return 0;
    if (string.is8Bit) {
        const auto* characters = Characters8(string);
        unsigned numCRLF = 0;
        for (unsigned i = 1; i < stringLength; ++i) numCRLF += characters[i - 1] == '\r' && characters[i] == '\n';
        return stringLength - numCRLF;
    }
    CharacterBreakIterator iterator(string);
    unsigned numGraphemeClusters = 0;
    while (ubrk_next_nid_postfix(iterator.Get()) != UBRK_DONE) ++numGraphemeClusters;
    return numGraphemeClusters;
}

unsigned APS5_VABI _ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj(StringView string, unsigned numGraphemeClusters) {
    const unsigned stringLength = string.length;
    if (stringLength <= numGraphemeClusters) return stringLength;
    if (string.is8Bit) {
        const auto* characters = Characters8(string);
        unsigned i = 0;
        unsigned j = 0;
        for (; i < numGraphemeClusters && j + 1 < stringLength; ++i, ++j) j += characters[j] == '\r' && characters[j + 1] == '\n';
        return j + (i < numGraphemeClusters);
    }
    CharacterBreakIterator iterator(string);
    for (unsigned i = 0; i < numGraphemeClusters; ++i) {
        if (ubrk_next_nid_postfix(iterator.Get()) == UBRK_DONE) return stringLength;
    }
    return static_cast<unsigned>(ubrk_current_nid_postfix(iterator.Get()));
}

}

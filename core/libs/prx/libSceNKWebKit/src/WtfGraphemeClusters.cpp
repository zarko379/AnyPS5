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

#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::int32_t UBRK_WORD_NONE = 0;

}

extern "C" {

bool APS5_VABI _ZN3WTF15isWordTextBreakEP14UBreakIterator(UBreakIterator* iterator) {
    if (!iterator) throw std::invalid_argument("null UBreakIterator");
    return ubrk_getRuleStatus_nid_postfix(iterator) != UBRK_WORD_NONE;
}

}

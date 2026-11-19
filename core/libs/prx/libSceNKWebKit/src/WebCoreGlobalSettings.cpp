// Derived from Source/WebCore/platform/graphics/FontCascade.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreProcessSwitchesExports.hpp"

namespace {

bool g_shouldUseFontSmoothing = true;

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(bool shouldUseSmoothing) {
    g_shouldUseFontSmoothing = shouldUseSmoothing;
}

bool APS5_VABI _ZN7WebCore11FontCascade18shouldUseSmoothingEv() {
    return g_shouldUseFontSmoothing;
}

}

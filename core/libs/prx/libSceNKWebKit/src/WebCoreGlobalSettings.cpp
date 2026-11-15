// Derived from Source/WebCore/page/DeprecatedGlobalSettings.cpp and Source/WebCore/page/DeprecatedGlobalSettings.h@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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
 */

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

bool g_mockScrollbarsEnabled = false;
bool g_usesOverlayScrollbars = false;
bool g_mockScrollAnimatorEnabled = false;
bool g_shouldRespectPriorityInCSSAttributeSetters = false;
bool g_lowPowerVideoAudioBufferSizeEnabled = false;
bool g_resourceLoadStatisticsEnabled = false;
bool g_allowsAnySSLCertificate = false;
bool g_shouldUseFontSmoothing = true;

}  // namespace

namespace NKWebKit::WebCoreAbi {

DeprecatedGlobalSettingsState CurrentDeprecatedGlobalSettings() {
    return DeprecatedGlobalSettingsState{
        g_mockScrollbarsEnabled,
        g_usesOverlayScrollbars,
        g_mockScrollAnimatorEnabled,
        g_shouldRespectPriorityInCSSAttributeSetters,
        g_lowPowerVideoAudioBufferSizeEnabled,
        g_resourceLoadStatisticsEnabled,
        g_allowsAnySSLCertificate,
    };
}

}

extern "C" {

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(bool flag) {
    g_mockScrollbarsEnabled = flag;
}

bool APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv() {
    return g_mockScrollbarsEnabled;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb(bool flag) {
    g_usesOverlayScrollbars = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings25setUsesMockScrollAnimatorEb(bool flag) {
    g_mockScrollAnimatorEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings45setShouldRespectPriorityInCSSAttributeSettersEb(bool flag) {
    g_shouldRespectPriorityInCSSAttributeSetters = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings38setLowPowerVideoAudioBufferSizeEnabledEb(bool flag) {
    g_lowPowerVideoAudioBufferSizeEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings32setResourceLoadStatisticsEnabledEb(bool flag) {
    g_resourceLoadStatisticsEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb(bool allowAnySSLCertificate) {
    g_allowsAnySSLCertificate = allowAnySSLCertificate;
}

void APS5_VABI _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(bool shouldUseSmoothing) {
    g_shouldUseFontSmoothing = shouldUseSmoothing;
}

bool APS5_VABI _ZN7WebCore11FontCascade18shouldUseSmoothingEv() {
    return g_shouldUseFontSmoothing;
}

}

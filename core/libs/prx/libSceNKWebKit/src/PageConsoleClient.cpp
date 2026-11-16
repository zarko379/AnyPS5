// Derived from Source/WebCore/page/PageConsoleClient.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreProcessSwitchesExports.hpp"

#include <limits>
#include <stdexcept>

namespace {

int g_muteCount = 0;
bool g_printExceptions = false;

}  // namespace

namespace NKWebKit::WebCoreAbi {

int PageConsoleClientMuteCount() {
    return g_muteCount;
}

}

extern "C" {

bool APS5_VABI _ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv() {
    return g_printExceptions;
}

void APS5_VABI _ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb(bool print) {
    g_printExceptions = print;
}

void APS5_VABI _ZN7WebCore17PageConsoleClient4muteEv() {
    if (g_muteCount == std::numeric_limits<int>::max()) throw std::overflow_error("WebCore::PageConsoleClient::mute: mute count overflow");
    g_muteCount++;
}

void APS5_VABI _ZN7WebCore17PageConsoleClient6unmuteEv() {
    if (g_muteCount == std::numeric_limits<int>::min()) throw std::overflow_error("WebCore::PageConsoleClient::unmute: mute count overflow");
    g_muteCount--;
}

}

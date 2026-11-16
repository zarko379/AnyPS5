// Derived from Source/WebCore/loader/FrameLoaderClient.h@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/Api.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore17FrameLoaderClient11allowScriptEb(void*, bool enabledPerSettings) {
    return enabledPerSettings;
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient15didCreateWindowERNS_9DOMWindowE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient15didDisownOpenerEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient17dispatchDidLayoutEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient19finishedLoadingIconEmPNS_12SharedBufferE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient20willInjectUserScriptERNS_15DOMWrapperWorldE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient21didChangeScrollOffsetEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient22dispatchDidReceiveIconEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient23dispatchDidExplicitOpenERKN3WTF3URLERKNS1_6StringE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient23getLoadDecisionForIconsERKN3WTF6VectorISt4pairIRNS_8LinkIconEmELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient24didRestoreScrollPositionEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient26dispatchWillChangeDocumentERKN3WTF3URLES4_() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient27registerForIconNotificationEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient28didLoadFromRegistrableDomainEONS_17RegistrableDomainE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient28notifyPageOfAppBoundBehaviorEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient29dispatchDidChangeMainDocumentEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient29dispatchDidNavigateWithinPageEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient29dispatchGlobalObjectAvailableERNS_15DOMWrapperWorldE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient30completePageTransitionIfNeededEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient31dispatchDidChangeProvisionalURLEv() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient31dispatchDidReachLayoutMilestoneEN3WTF9OptionSetINS_15LayoutMilestoneEEE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient52dispatchDidReconnectDOMWindowExtensionToGlobalObjectEPNS_18DOMWindowExtensionE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient52dispatchWillDestroyGlobalObjectForDOMWindowExtensionEPNS_18DOMWindowExtensionE() {
}

void APS5_VABI _ZN7WebCore17FrameLoaderClient56dispatchWillDisconnectDOMWindowExtensionFromGlobalObjectEPNS_18DOMWindowExtensionE() {
}

}

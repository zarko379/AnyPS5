// Derived from Source/WebKit/UIProcess/API/C/WKPage.cpp, Source/WebKit/UIProcess/WebPageProxy.cpp, Source/WebKit/UIProcess/WebPageProxy.h and Source/WebKit/UIProcess/WebPageProxyInternals.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdint>

namespace {

constexpr std::uint32_t kWKPaginationModeBottomToTop = 4;
constexpr std::uint32_t kWKScrollPinningBehaviorDoNotPin = 0;
constexpr std::uint32_t kWKScrollPinningBehaviorPinToBottom = 2;

NKWebKit::Page* PageFrom(const void* pageRef, const char* function) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, function);
}

}  // namespace

extern "C" {

bool APS5_VABI WKPageRubberBandsAtLeft(const void* pageRef) {
    return PageFrom(pageRef, __func__)->rubberBandsAtLeft;
}

void APS5_VABI WKPageSetRubberBandsAtLeft(const void* pageRef, bool rubberBandsAtLeft) {
    PageFrom(pageRef, __func__)->rubberBandsAtLeft = rubberBandsAtLeft;
}

bool APS5_VABI WKPageRubberBandsAtRight(const void* pageRef) {
    return PageFrom(pageRef, __func__)->rubberBandsAtRight;
}

void APS5_VABI WKPageSetRubberBandsAtRight(const void* pageRef, bool rubberBandsAtRight) {
    PageFrom(pageRef, __func__)->rubberBandsAtRight = rubberBandsAtRight;
}

bool APS5_VABI WKPageRubberBandsAtTop(const void* pageRef) {
    return PageFrom(pageRef, __func__)->rubberBandsAtTop;
}

void APS5_VABI WKPageSetRubberBandsAtTop(const void* pageRef, bool rubberBandsAtTop) {
    PageFrom(pageRef, __func__)->rubberBandsAtTop = rubberBandsAtTop;
}

bool APS5_VABI WKPageRubberBandsAtBottom(const void* pageRef) {
    return PageFrom(pageRef, __func__)->rubberBandsAtBottom;
}

void APS5_VABI WKPageSetRubberBandsAtBottom(const void* pageRef, bool rubberBandsAtBottom) {
    PageFrom(pageRef, __func__)->rubberBandsAtBottom = rubberBandsAtBottom;
}

bool APS5_VABI WKPageVerticalRubberBandingIsEnabled(const void* pageRef) {
    return PageFrom(pageRef, __func__)->enableVerticalRubberBanding;
}

void APS5_VABI WKPageSetEnableVerticalRubberBanding(const void* pageRef, bool enableVerticalRubberBanding) {
    PageFrom(pageRef, __func__)->enableVerticalRubberBanding = enableVerticalRubberBanding;
}

bool APS5_VABI WKPageHorizontalRubberBandingIsEnabled(const void* pageRef) {
    return PageFrom(pageRef, __func__)->enableHorizontalRubberBanding;
}

void APS5_VABI WKPageSetEnableHorizontalRubberBanding(const void* pageRef, bool enableHorizontalRubberBanding) {
    PageFrom(pageRef, __func__)->enableHorizontalRubberBanding = enableHorizontalRubberBanding;
}

void APS5_VABI WKPageSetBackgroundExtendsBeyondPage(const void* pageRef, bool backgroundExtendsBeyondPage) {
    PageFrom(pageRef, __func__)->backgroundExtendsBeyondPage = backgroundExtendsBeyondPage;
}

bool APS5_VABI WKPageBackgroundExtendsBeyondPage(const void* pageRef) {
    return PageFrom(pageRef, __func__)->backgroundExtendsBeyondPage;
}

void APS5_VABI WKPageSetPaginationMode(const void* pageRef, std::uint32_t paginationMode) {
    if (paginationMode > kWKPaginationModeBottomToTop) {
        return;
    }
    PageFrom(pageRef, __func__)->paginationMode = paginationMode;
}

std::uint32_t APS5_VABI WKPageGetPaginationMode(const void* pageRef) {
    return PageFrom(pageRef, __func__)->paginationMode;
}

void APS5_VABI WKPageSetPaginationBehavesLikeColumns(const void* pageRef, bool behavesLikeColumns) {
    PageFrom(pageRef, __func__)->paginationBehavesLikeColumns = behavesLikeColumns;
}

bool APS5_VABI WKPageGetPaginationBehavesLikeColumns(const void* pageRef) {
    return PageFrom(pageRef, __func__)->paginationBehavesLikeColumns;
}

void APS5_VABI WKPageSetPageLength(const void* pageRef, double pageLength) {
    PageFrom(pageRef, __func__)->pageLength = pageLength;
}

double APS5_VABI WKPageGetPageLength(const void* pageRef) {
    return PageFrom(pageRef, __func__)->pageLength;
}

void APS5_VABI WKPageSetGapBetweenPages(const void* pageRef, double gap) {
    PageFrom(pageRef, __func__)->gapBetweenPages = gap;
}

double APS5_VABI WKPageGetGapBetweenPages(const void* pageRef) {
    return PageFrom(pageRef, __func__)->gapBetweenPages;
}

void APS5_VABI WKPageSetSuppressScrollbarAnimations(const void* pageRef, bool suppressAnimations) {
    PageFrom(pageRef, __func__)->suppressScrollbarAnimations = suppressAnimations;
}

bool APS5_VABI WKPageAreScrollbarAnimationsSuppressed(const void* pageRef) {
    return PageFrom(pageRef, __func__)->suppressScrollbarAnimations;
}

std::uint32_t APS5_VABI WKPageGetScrollPinningBehavior(const void* pageRef) {
    return PageFrom(pageRef, __func__)->scrollPinningBehavior;
}

void APS5_VABI WKPageSetScrollPinningBehavior(const void* pageRef, std::uint32_t pinning) {
    PageFrom(pageRef, __func__)->scrollPinningBehavior = pinning <= kWKScrollPinningBehaviorPinToBottom ? pinning : kWKScrollPinningBehaviorDoNotPin;
}

}

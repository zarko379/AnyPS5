// Derived from Source/WebKit/UIProcess/API/C/WKPage.cpp, Source/WebKit/Shared/API/c/WKSharedAPICast.h, Source/WebKit/Shared/API/APIString.h, Source/WebKit/UIProcess/API/APIPageConfiguration.h, Source/WebKit/UIProcess/WebPageProxy.cpp, Source/WebKit/UIProcess/WebPageProxy.h and Source/WebKit/UIProcess/WebPageProxyInternals.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
#include <optional>
#include <stdexcept>
#include <string>

namespace {

NKWebKit::Page* PageFrom(const void* pageRef, const char* function) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, function);
}

std::int32_t ToInt(double value, const char* function) {
    if (!(value > -2147483649.0 && value < 2147483648.0)) {
        throw std::out_of_range(std::string(function) + ": size component outside the int range");
    }
    return static_cast<std::int32_t>(value);
}

std::optional<std::u16string> ToWTFString(const void* stringRef, const char* function) {
    if (!stringRef) {
        return std::nullopt;
    }
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, function);
    if (string->null) {
        return std::nullopt;
    }
    return string->characters;
}

}  // namespace

extern "C" {






double APS5_VABI WKPageGetBackingScaleFactor(const void* pageRef) {
    const auto* page = PageFrom(pageRef, __func__);
    return page->customDeviceScaleFactor.value_or(page->intrinsicDeviceScaleFactor);
}

void APS5_VABI WKPageSetCustomBackingScaleFactor(const void* pageRef, double customScaleFactor) {
    auto* page = PageFrom(pageRef, __func__);
    float scaleFactor = static_cast<float>(customScaleFactor);
    if (scaleFactor) {
        page->customDeviceScaleFactor = scaleFactor;
    } else {
        page->customDeviceScaleFactor = std::nullopt;
    }
}

void APS5_VABI WKPageSetScaleFactor(const void* pageRef, double scale, NKWebKit::PointValue) {
    PageFrom(pageRef, __func__)->pageScaleFactor = scale;
}

double APS5_VABI WKPageGetScaleFactor(const void* pageRef) {
    return PageFrom(pageRef, __func__)->pageScaleFactor;
}

void APS5_VABI WKPageSetUseFixedLayout(const void* pageRef, bool fixed) {
    auto* page = PageFrom(pageRef, __func__);
    if (fixed == page->useFixedLayout) {
        return;
    }
    page->useFixedLayout = fixed;
    if (!fixed) {
        page->fixedLayoutWidth = 0;
        page->fixedLayoutHeight = 0;
    }
}

void APS5_VABI WKPageSetFixedLayoutSize(const void* pageRef, NKWebKit::SizeValue size) {
    auto* page = PageFrom(pageRef, __func__);
    std::int32_t width = ToInt(size.width, __func__);
    std::int32_t height = ToInt(size.height, __func__);
    page->fixedLayoutWidth = width;
    page->fixedLayoutHeight = height;
}

bool APS5_VABI WKPageUseFixedLayout(const void* pageRef) {
    return PageFrom(pageRef, __func__)->useFixedLayout;
}

NKWebKit::SizeValue APS5_VABI WKPageFixedLayoutSize(const void* pageRef) {
    const auto* page = PageFrom(pageRef, __func__);
    return NKWebKit::SizeValue{static_cast<double>(page->fixedLayoutWidth), static_cast<double>(page->fixedLayoutHeight)};
}

bool APS5_VABI WKPageGetIsControlledByAutomation(const void* pageRef) {
    return PageFrom(pageRef, __func__)->controlledByAutomation;
}

void APS5_VABI WKPageSetControlledByAutomation(const void* pageRef, bool controlled) {
    PageFrom(pageRef, __func__)->controlledByAutomation = controlled;
}

void APS5_VABI WKPageSetMediaCaptureEnabled(const void* pageRef, bool enabled) {
    PageFrom(pageRef, __func__)->mediaCaptureEnabled = enabled;
}

bool APS5_VABI WKPageGetMediaCaptureEnabled(const void* pageRef) {
    return PageFrom(pageRef, __func__)->mediaCaptureEnabled;
}

const void* APS5_VABI WKPageCopyCustomTextEncodingName(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(PageFrom(pageRef, __func__)->customTextEncodingName);
}

void APS5_VABI WKPageSetCustomTextEncodingName(const void* pageRef, const void* encodingNameRef) {
    auto* page = PageFrom(pageRef, __func__);
    page->customTextEncodingName = ToWTFString(encodingNameRef, __func__);
}

}

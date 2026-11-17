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

namespace {

NKWebKit::Page* PageFrom(const void* pageRef, const char* function) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, function);
}

}  // namespace

extern "C" {

void APS5_VABI WKPageSetBackgroundExtendsBeyondPage(const void* pageRef, bool backgroundExtendsBeyondPage) {
    PageFrom(pageRef, __func__)->backgroundExtendsBeyondPage = backgroundExtendsBeyondPage;
}

bool APS5_VABI WKPageBackgroundExtendsBeyondPage(const void* pageRef) {
    return PageFrom(pageRef, __func__)->backgroundExtendsBeyondPage;
}

}

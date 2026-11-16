/* Derived from api/video_codecs/video_decoder_factory.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/video_decoder_factory.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t GetSupportedFormatsSlot = 2;

struct FormatVector {
    WebRtcSdpVideoFormat* begin;
    WebRtcSdpVideoFormat* end;
    WebRtcSdpVideoFormat* capacity;
};

using GetSupportedFormats = FormatVector* (APS5_VABI*)(FormatVector* result, const void* factory);

std::size_t Count(const FormatVector& formats) {
    const auto first = reinterpret_cast<std::uintptr_t>(formats.begin);
    const auto last = reinterpret_cast<std::uintptr_t>(formats.end);
    const auto limit = reinterpret_cast<std::uintptr_t>(formats.capacity);
    if (last < first || limit < last || (!first && limit) || (last - first) % sizeof(WebRtcSdpVideoFormat) ||
        (limit - first) % sizeof(WebRtcSdpVideoFormat))
        throw std::invalid_argument("VideoDecoderFactory::QueryCodecSupport requires a valid supported format vector");
    return (last - first) / sizeof(WebRtcSdpVideoFormat);
}

void Destroy(FormatVector& formats) {
    Count(formats);
    while (formats.end != formats.begin)
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(--formats.end);
    if (formats.begin)
        _ZdlPv_nid_postfix(formats.begin);
}

}

extern "C" {

WebRtcCodecSupport APS5_VABI _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(const void* factory, const WebRtcSdpVideoFormat* format, bool referenceScaling) {
    if (!factory)
        throw std::invalid_argument("VideoDecoderFactory::QueryCodecSupport requires a factory");
    if (!format)
        throw std::invalid_argument("VideoDecoderFactory::QueryCodecSupport requires a format");
    WebRtcCodecSupport support {false, false};
    if (referenceScaling)
        return support;
    const void* const* vtable = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtable, factory, sizeof(vtable)) || !vtable || !vtable[GetSupportedFormatsSlot])
        throw std::invalid_argument("VideoDecoderFactory::QueryCodecSupport requires a factory with GetSupportedFormats");
    FormatVector formats {};
    reinterpret_cast<GetSupportedFormats>(const_cast<void*>(vtable[GetSupportedFormatsSlot]))(&formats, factory);
    try {
        support.isSupported = _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(format, formats.begin, Count(formats));
    } catch (...) {
        try {
            Destroy(formats);
        } catch (...) {
        }
        throw;
    }
    Destroy(formats);
    return support;
}

}

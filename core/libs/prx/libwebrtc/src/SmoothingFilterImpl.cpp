/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/libc.hpp"

#include <math.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {

void APS5_VABI DeleteSmoothingFilter_nid_no_patch(WebRtcSmoothingFilterImpl* filter) {
    _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(filter);
    _ZdlPv_nid_postfix(filter);
}

void UpdateAlpha(WebRtcSmoothingFilterImpl& filter, int timeConstantMs) {
    filter.alpha = timeConstantMs == 0 ? 0.0f : std::exp(-1.0f / timeConstantMs);
}

void ExtrapolateLastSample(WebRtcSmoothingFilterImpl& filter, std::int64_t timeMs) {
    float multiplier = 0.0f;
    const std::int64_t initEndTimeMs = filter.initEndTimeMs.value;
    if (timeMs <= initEndTimeMs) {
        if (filter.initTimeMs == 0) {
            multiplier = 0.0f;
        } else if (filter.initTimeMs == 1) {
            multiplier = static_cast<float>(std::exp(filter.lastStateTimeMs - timeMs));
        } else {
            multiplier = std::exp(-(powf(filter.initFactor, static_cast<float>(filter.lastStateTimeMs - initEndTimeMs)) -
                powf(filter.initFactor, static_cast<float>(timeMs - initEndTimeMs))) / filter.initConst);
        }
    } else {
        if (filter.lastStateTimeMs < initEndTimeMs)
            ExtrapolateLastSample(filter, initEndTimeMs);
        multiplier = powf(filter.alpha, static_cast<float>(timeMs - filter.lastStateTimeMs));
    }
    filter.state = multiplier * filter.state + (1.0f - multiplier) * filter.lastSample;
    filter.lastStateTimeMs = timeMs;
}

void APS5_VABI AddSample_nid_no_patch(WebRtcSmoothingFilterImpl* filter, float sample) {
    const std::int64_t nowMs = _ZN3rtc10TimeMillisEv_nid_postfix();
    if (!filter->initEndTimeMs.engaged) {
        filter->state = filter->lastSample = sample;
        filter->initEndTimeMs.value = nowMs + filter->initTimeMs;
        filter->initEndTimeMs.engaged = true;
        filter->lastStateTimeMs = nowMs;
        return;
    }
    ExtrapolateLastSample(*filter, nowMs);
    filter->lastSample = sample;
}

WebRtcOptionalFloat APS5_VABI GetAverage_nid_no_patch(WebRtcSmoothingFilterImpl* filter) {
    WebRtcOptionalFloat average{};
    if (!filter->initEndTimeMs.engaged)
        return average;
    ExtrapolateLastSample(*filter, _ZN3rtc10TimeMillisEv_nid_postfix());
    average.storage.value = filter->state;
    average.engaged = true;
    return average;
}

bool APS5_VABI SetTimeConstantMs_nid_no_patch(WebRtcSmoothingFilterImpl* filter, int timeConstantMs) {
    if (!filter->initEndTimeMs.engaged || filter->lastStateTimeMs < filter->initEndTimeMs.value)
        return false;
    UpdateAlpha(*filter, timeConstantMs);
    return true;
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteSmoothingFilter_nid_no_patch),
    reinterpret_cast<void*>(&AddSample_nid_no_patch),
    reinterpret_cast<void*>(&GetAverage_nid_no_patch),
    reinterpret_cast<void*>(&SetTimeConstantMs_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(WebRtcSmoothingFilterImpl* filter, int initTimeMs) {
    if (!filter)
        throw std::invalid_argument("SmoothingFilterImpl::SmoothingFilterImpl requires a filter");
    filter->vtable = Vtable + 2;
    filter->initTimeMs = initTimeMs;
    filter->initFactor = initTimeMs == 0 ? 0.0f : powf(static_cast<float>(initTimeMs), -1.0f / initTimeMs);
    filter->initConst = initTimeMs == 0 ? 0.0f : initTimeMs - powf(static_cast<float>(initTimeMs), 1.0f - 1.0f / initTimeMs);
    filter->initEndTimeMs.engaged = false;
    UpdateAlpha(*filter, initTimeMs);
}

void APS5_VABI _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(WebRtcSmoothingFilterImpl*) {}

}

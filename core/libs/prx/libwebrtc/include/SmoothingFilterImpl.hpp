#pragma once

#include "prx/libwebrtc/include/VideoFrame.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcSmoothingFilterImpl {
    void* const* vtable;
    std::int32_t initTimeMs;
    float initFactor;
    float initConst;
    WebRtcOptionalInt64 initEndTimeMs;
    float lastSample;
    float alpha;
    float state;
    std::int64_t lastStateTimeMs;
};

static_assert(sizeof(WebRtcSmoothingFilterImpl) == 64);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initTimeMs) == 8);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initFactor) == 12);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initConst) == 16);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initEndTimeMs) == 24);
static_assert(offsetof(WebRtcSmoothingFilterImpl, lastSample) == 40);
static_assert(offsetof(WebRtcSmoothingFilterImpl, alpha) == 44);
static_assert(offsetof(WebRtcSmoothingFilterImpl, state) == 48);
static_assert(offsetof(WebRtcSmoothingFilterImpl, lastStateTimeMs) == 56);

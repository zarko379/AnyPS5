#pragma once

#include "prx/libwebrtc/include/AudioOptions.hpp"
#include "prx/libwebrtc/include/CodecSpecificInfo.hpp"
#include "prx/libwebrtc/include/SdpAudioFormat.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcScalabilityModeCount = 34;

struct WebRtcSdpVideoFormat {
    WebRtcGuestString name;
    WebRtcStringMap parameters;
    WebRtcInlinedVector<WebRtcScalabilityModeCount> scalabilityModes;
};

static_assert(sizeof(WebRtcInlinedVector<WebRtcScalabilityModeCount>) == 48);
static_assert(sizeof(WebRtcSdpVideoFormat) == 112);
static_assert(offsetof(WebRtcSdpVideoFormat, parameters) == 40);
static_assert(offsetof(WebRtcSdpVideoFormat, scalabilityModes) == 64);

struct WebRtcCodecSupport {
    bool isSupported;
    bool isPowerEfficient;
};

static_assert(sizeof(WebRtcCodecSupport) == 2);

struct WebRtcFeedbackParam {
    WebRtcGuestString id;
    WebRtcGuestString param;
};

struct WebRtcCodec {
    const void* vtable;
    std::int32_t type;
    std::int32_t id;
    WebRtcGuestString name;
    std::int32_t clockrate;
    std::int32_t bitrate;
    std::uint64_t channels;
    WebRtcOptionalGuestString packetization;
    WebRtcInlinedVector<WebRtcScalabilityModeCount> scalabilityModes;
    WebRtcOptionalGuestString txMode;
    WebRtcStringMap params;
    WebRtcFeedbackParam* feedbackBegin;
    WebRtcFeedbackParam* feedbackEnd;
    WebRtcFeedbackParam* feedbackCapacity;
};

static_assert(sizeof(WebRtcOptionalGuestString) == 48);
static_assert(sizeof(WebRtcFeedbackParam) == 80);
static_assert(offsetof(WebRtcCodec, name) == 16);
static_assert(offsetof(WebRtcCodec, clockrate) == 56);
static_assert(offsetof(WebRtcCodec, channels) == 64);
static_assert(offsetof(WebRtcCodec, packetization) == 72);
static_assert(offsetof(WebRtcCodec, scalabilityModes) == 120);
static_assert(offsetof(WebRtcCodec, txMode) == 168);
static_assert(offsetof(WebRtcCodec, params) == 216);
static_assert(offsetof(WebRtcCodec, feedbackBegin) == 240);
static_assert(sizeof(WebRtcCodec) == 264);

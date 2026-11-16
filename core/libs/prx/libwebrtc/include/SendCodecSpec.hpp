#pragma once

#include "prx/libwebrtc/include/SdpAudioFormat.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcOptionalInt32 {
    std::int32_t value;
    bool engaged;
};

struct WebRtcSendCodecSpec {
    std::int32_t payloadType;
    WebRtcSdpAudioFormat format;
    bool flags[3];
    WebRtcOptionalInt32 cngPayloadType;
    WebRtcOptionalInt32 redPayloadType;
    WebRtcOptionalInt32 targetBitrateBps;
};

static_assert(sizeof(WebRtcOptionalInt32) == 8);
static_assert(offsetof(WebRtcOptionalInt32, engaged) == 4);
static_assert(sizeof(WebRtcSendCodecSpec) == 120);
static_assert(offsetof(WebRtcSendCodecSpec, format) == 8);
static_assert(offsetof(WebRtcSendCodecSpec, flags) == 88);
static_assert(offsetof(WebRtcSendCodecSpec, cngPayloadType) == 92);
static_assert(offsetof(WebRtcSendCodecSpec, redPayloadType) == 100);
static_assert(offsetof(WebRtcSendCodecSpec, targetBitrateBps) == 108);

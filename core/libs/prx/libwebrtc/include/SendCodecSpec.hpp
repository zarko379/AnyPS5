#pragma once

#include "prx/libwebrtc/include/AudioEncoder.hpp"
#include "prx/libwebrtc/include/AudioOptions.hpp"
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

struct WebRtcOptionalDouble {
    double value;
    bool engaged;
};

struct WebRtcAudioProcessingStats {
    WebRtcOptionalBool voiceDetected;
    WebRtcOptionalDouble echoReturnLoss;
    WebRtcOptionalDouble echoReturnLossEnhancement;
    WebRtcOptionalDouble divergentFilterFraction;
    WebRtcOptionalInt32 delayMedianMs;
    WebRtcOptionalInt32 delayStandardDeviationMs;
    WebRtcOptionalDouble residualEchoLikelihood;
    WebRtcOptionalDouble residualEchoLikelihoodRecentMax;
    WebRtcOptionalInt32 delayMs;
};

struct WebRtcAudioSendStreamStats {
    std::uint32_t localSsrc;
    std::int64_t payloadBytesSent;
    std::int64_t headerAndPaddingBytesSent;
    std::uint64_t retransmittedBytesSent;
    std::int32_t packetsSent;
    std::int64_t totalPacketSendDelayUs;
    std::uint64_t retransmittedPacketsSent;
    std::int32_t packetsLost;
    float fractionLost;
    WebRtcGuestString codecName;
    WebRtcOptionalInt32 codecPayloadType;
    std::int32_t jitterMs;
    std::int64_t rttMs;
    std::int16_t audioLevel;
    double totalInputEnergy;
    double totalInputDuration;
    WebRtcAnaStats anaStatistics;
    WebRtcAudioProcessingStats apmStatistics;
    std::int64_t targetBitrateBps;
    void* reportBlockDatasBegin;
    void* reportBlockDatasEnd;
    void* reportBlockDatasCapacity;
    std::uint32_t nacksReceived;
};

static_assert(sizeof(WebRtcOptionalDouble) == 16);
static_assert(sizeof(WebRtcAudioProcessingStats) == 112);
static_assert(offsetof(WebRtcAudioProcessingStats, echoReturnLoss) == 8);
static_assert(offsetof(WebRtcAudioProcessingStats, delayMedianMs) == 56);
static_assert(offsetof(WebRtcAudioProcessingStats, residualEchoLikelihood) == 72);
static_assert(offsetof(WebRtcAudioProcessingStats, delayMs) == 104);
static_assert(offsetof(WebRtcAudioSendStreamStats, codecName) == 64);
static_assert(offsetof(WebRtcAudioSendStreamStats, codecPayloadType) == 104);
static_assert(offsetof(WebRtcAudioSendStreamStats, rttMs) == 120);
static_assert(offsetof(WebRtcAudioSendStreamStats, anaStatistics) == 152);
static_assert(offsetof(WebRtcAudioSendStreamStats, apmStatistics) == 208);
static_assert(offsetof(WebRtcAudioSendStreamStats, targetBitrateBps) == 320);
static_assert(offsetof(WebRtcAudioSendStreamStats, reportBlockDatasBegin) == 328);
static_assert(offsetof(WebRtcAudioSendStreamStats, nacksReceived) == 352);
static_assert(sizeof(WebRtcAudioSendStreamStats) == 360);

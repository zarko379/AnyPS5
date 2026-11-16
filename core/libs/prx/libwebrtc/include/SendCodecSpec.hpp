#pragma once

#include "prx/libwebrtc/include/AudioEncoder.hpp"
#include "prx/libwebrtc/include/AudioOptions.hpp"
#include "prx/libwebrtc/include/CryptoOptions.hpp"
#include "prx/libwebrtc/include/RtpExtension.hpp"
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

struct WebRtcAudioSendStreamConfigRtp {
    std::uint32_t ssrc;
    WebRtcGuestString rid;
    WebRtcGuestString mid;
    bool extmapAllowMixed;
    WebRtcRtpExtension* extensionsBegin;
    WebRtcRtpExtension* extensionsEnd;
    WebRtcRtpExtension* extensionsCapacity;
    WebRtcGuestString cName;
    std::int32_t rtcpMode;
};

struct WebRtcOptionalSendCodecSpec {
    WebRtcSendCodecSpec value;
    bool engaged;
};

struct WebRtcOptionalCodecPairId {
    std::uint64_t value;
    bool engaged;
};

struct WebRtcAudioSendStreamConfig {
    WebRtcAudioSendStreamConfigRtp rtp;
    std::int32_t rtcpReportIntervalMs;
    void* sendTransport;
    std::int32_t minBitrateBps;
    std::int32_t maxBitrateBps;
    double bitratePriority;
    bool hasDscp;
    WebRtcOptionalGuestString audioNetworkAdaptorConfig;
    WebRtcOptionalSendCodecSpec sendCodecSpec;
    void* encoderFactory;
    WebRtcOptionalCodecPairId codecPairId;
    WebRtcGuestString trackId;
    WebRtcCryptoOptions cryptoOptions;
    void* frameEncryptor;
    void* frameTransformer;
};

static_assert(sizeof(WebRtcAudioSendStreamConfigRtp) == 168);
static_assert(offsetof(WebRtcAudioSendStreamConfigRtp, mid) == 48);
static_assert(offsetof(WebRtcAudioSendStreamConfigRtp, extmapAllowMixed) == 88);
static_assert(offsetof(WebRtcAudioSendStreamConfigRtp, extensionsBegin) == 96);
static_assert(offsetof(WebRtcAudioSendStreamConfigRtp, cName) == 120);
static_assert(offsetof(WebRtcAudioSendStreamConfigRtp, rtcpMode) == 160);
static_assert(sizeof(WebRtcOptionalSendCodecSpec) == 128);
static_assert(offsetof(WebRtcOptionalSendCodecSpec, engaged) == 120);
static_assert(sizeof(WebRtcOptionalCodecPairId) == 16);
static_assert(offsetof(WebRtcAudioSendStreamConfig, rtcpReportIntervalMs) == 168);
static_assert(offsetof(WebRtcAudioSendStreamConfig, sendTransport) == 176);
static_assert(offsetof(WebRtcAudioSendStreamConfig, bitratePriority) == 192);
static_assert(offsetof(WebRtcAudioSendStreamConfig, hasDscp) == 200);
static_assert(offsetof(WebRtcAudioSendStreamConfig, audioNetworkAdaptorConfig) == 208);
static_assert(offsetof(WebRtcAudioSendStreamConfig, sendCodecSpec) == 256);
static_assert(offsetof(WebRtcAudioSendStreamConfig, encoderFactory) == 384);
static_assert(offsetof(WebRtcAudioSendStreamConfig, codecPairId) == 392);
static_assert(offsetof(WebRtcAudioSendStreamConfig, trackId) == 408);
static_assert(offsetof(WebRtcAudioSendStreamConfig, cryptoOptions) == 448);
static_assert(offsetof(WebRtcAudioSendStreamConfig, frameEncryptor) == 456);
static_assert(offsetof(WebRtcAudioSendStreamConfig, frameTransformer) == 464);
static_assert(sizeof(WebRtcAudioSendStreamConfig) == 472);

struct WebRtcDecoderMapNode {
    WebRtcDecoderMapNode* left;
    WebRtcDecoderMapNode* right;
    WebRtcDecoderMapNode* parent;
    bool isBlack;
    std::int32_t payloadType;
    WebRtcSdpAudioFormat format;
};

struct WebRtcDecoderMap {
    WebRtcDecoderMapNode* beginNode;
    WebRtcDecoderMapNode* root;
    std::uint64_t size;
};

struct WebRtcAudioReceiveStreamConfig {
    std::uint32_t remoteSsrc;
    std::uint32_t localSsrc;
    std::int32_t nackRtpHistoryMs;
    std::int32_t rtcpMode;
    bool enableNonSenderRtt;
    void* rtcpSendTransport;
    std::uint64_t jitterBufferMaxPackets;
    bool jitterBufferFastAccelerate;
    std::int32_t jitterBufferMinDelayMs;
    WebRtcGuestString syncGroup;
    WebRtcDecoderMap decoderMap;
    void* decoderFactory;
    WebRtcOptionalCodecPairId codecPairId;
    WebRtcCryptoOptions cryptoOptions;
    void* frameDecryptor;
    void* frameTransformer;
};

static_assert(sizeof(WebRtcDecoderMapNode) == 120);
static_assert(offsetof(WebRtcDecoderMapNode, payloadType) == 32);
static_assert(offsetof(WebRtcDecoderMapNode, format) == 40);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, enableNonSenderRtt) == 16);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, rtcpSendTransport) == 24);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, jitterBufferMinDelayMs) == 44);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, syncGroup) == 48);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, decoderMap) == 88);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, decoderFactory) == 112);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, codecPairId) == 120);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, cryptoOptions) == 136);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, frameDecryptor) == 144);
static_assert(sizeof(WebRtcAudioReceiveStreamConfig) == 160);

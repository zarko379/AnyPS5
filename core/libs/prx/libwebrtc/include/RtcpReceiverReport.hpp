#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcpReportBlock {
    std::uint32_t sourceSsrc;
    std::uint8_t fractionLost;
    std::int32_t cumulativeLost;
    std::uint32_t extendedHighSeqNum;
    std::uint32_t jitter;
    std::uint32_t lastSr;
    std::uint32_t delaySinceLastSr;
};

struct WebRtcRtcpReceiverReport {
    const void* vtable;
    std::uint32_t senderSsrc;
    WebRtcRtcpReportBlock* blocksBegin;
    WebRtcRtcpReportBlock* blocksEnd;
    WebRtcRtcpReportBlock* blocksCapacity;
};

static_assert(sizeof(WebRtcRtcpReportBlock) == 28);
static_assert(offsetof(WebRtcRtcpReportBlock, fractionLost) == 4);
static_assert(offsetof(WebRtcRtcpReportBlock, cumulativeLost) == 8);
static_assert(offsetof(WebRtcRtcpReportBlock, extendedHighSeqNum) == 12);
static_assert(offsetof(WebRtcRtcpReportBlock, jitter) == 16);
static_assert(offsetof(WebRtcRtcpReportBlock, lastSr) == 20);
static_assert(offsetof(WebRtcRtcpReportBlock, delaySinceLastSr) == 24);
static_assert(sizeof(WebRtcRtcpReceiverReport) == 40);
static_assert(offsetof(WebRtcRtcpReceiverReport, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpReceiverReport, blocksBegin) == 16);
static_assert(offsetof(WebRtcRtcpReceiverReport, blocksEnd) == 24);
static_assert(offsetof(WebRtcRtcpReceiverReport, blocksCapacity) == 32);

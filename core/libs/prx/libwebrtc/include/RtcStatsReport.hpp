#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

struct WebRtcRtcStatsNode {
    WebRtcRtcStatsNode* left;
    WebRtcRtcStatsNode* right;
    WebRtcRtcStatsNode* parent;
    bool isBlack;
    WebRtcGuestString id;
    const void* stats;
};

struct WebRtcRtcStatsReport {
    std::atomic<std::int32_t> refCount;
    std::int64_t timestampUs;
    WebRtcRtcStatsNode* beginNode;
    WebRtcRtcStatsNode* root;
    std::uint64_t size;
};

struct WebRtcRtcStatsReportIterator {
    WebRtcRtcStatsReport* report;
    WebRtcRtcStatsNode* node;
};

static_assert(sizeof(WebRtcRtcStatsNode) == 80);
static_assert(offsetof(WebRtcRtcStatsNode, isBlack) == 24);
static_assert(offsetof(WebRtcRtcStatsNode, id) == 32);
static_assert(offsetof(WebRtcRtcStatsNode, stats) == 72);
static_assert(sizeof(WebRtcRtcStatsReport) == 40);
static_assert(offsetof(WebRtcRtcStatsReport, timestampUs) == 8);
static_assert(offsetof(WebRtcRtcStatsReport, beginNode) == 16);
static_assert(offsetof(WebRtcRtcStatsReport, root) == 24);
static_assert(offsetof(WebRtcRtcStatsReport, size) == 32);
static_assert(sizeof(WebRtcRtcStatsReportIterator) == 16);
static_assert(offsetof(WebRtcRtcStatsReportIterator, node) == 8);

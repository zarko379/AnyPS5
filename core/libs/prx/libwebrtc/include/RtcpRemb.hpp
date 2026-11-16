#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcpRemb {
    const void* vtable;
    std::uint32_t senderSsrc;
    std::uint32_t mediaSsrc;
    std::int64_t bitrateBps;
    std::uint32_t* ssrcsBegin;
    std::uint32_t* ssrcsEnd;
    std::uint32_t* ssrcsCapacity;
};

static_assert(sizeof(WebRtcRtcpRemb) == 48);
static_assert(offsetof(WebRtcRtcpRemb, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpRemb, mediaSsrc) == 12);
static_assert(offsetof(WebRtcRtcpRemb, bitrateBps) == 16);
static_assert(offsetof(WebRtcRtcpRemb, ssrcsBegin) == 24);
static_assert(offsetof(WebRtcRtcpRemb, ssrcsEnd) == 32);
static_assert(offsetof(WebRtcRtcpRemb, ssrcsCapacity) == 40);

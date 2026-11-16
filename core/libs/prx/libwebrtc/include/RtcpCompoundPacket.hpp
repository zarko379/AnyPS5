#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcpCompoundPacket {
    const void* vtable;
    std::uint32_t senderSsrc;
    void** packetsBegin;
    void** packetsEnd;
    void** packetsCapacity;
};

static_assert(sizeof(WebRtcRtcpCompoundPacket) == 40);
static_assert(offsetof(WebRtcRtcpCompoundPacket, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpCompoundPacket, packetsBegin) == 16);
static_assert(offsetof(WebRtcRtcpCompoundPacket, packetsEnd) == 24);
static_assert(offsetof(WebRtcRtcpCompoundPacket, packetsCapacity) == 32);

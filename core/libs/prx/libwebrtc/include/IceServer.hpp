#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcGuestStringVector {
    WebRtcGuestString* begin;
    WebRtcGuestString* end;
    WebRtcGuestString* capacity;
};

struct WebRtcIceServer {
    WebRtcGuestString uri;
    WebRtcGuestStringVector urls;
    WebRtcGuestString username;
    WebRtcGuestString password;
    std::int32_t tlsCertPolicy;
    WebRtcGuestString hostname;
    WebRtcGuestStringVector tlsAlpnProtocols;
    WebRtcGuestStringVector tlsEllipticCurves;
};

static_assert(sizeof(WebRtcGuestStringVector) == 24);
static_assert(sizeof(WebRtcIceServer) == 240);
static_assert(offsetof(WebRtcIceServer, urls) == 40);
static_assert(offsetof(WebRtcIceServer, username) == 64);
static_assert(offsetof(WebRtcIceServer, password) == 104);
static_assert(offsetof(WebRtcIceServer, tlsCertPolicy) == 144);
static_assert(offsetof(WebRtcIceServer, hostname) == 152);
static_assert(offsetof(WebRtcIceServer, tlsAlpnProtocols) == 192);
static_assert(offsetof(WebRtcIceServer, tlsEllipticCurves) == 216);

struct WebRtcRtpTransceiverInit {
    std::int32_t direction;
    WebRtcGuestStringVector streamIds;
    void* sendEncodingsBegin;
    void* sendEncodingsEnd;
    void* sendEncodingsCapacity;
};

static_assert(sizeof(WebRtcRtpTransceiverInit) == 56);
static_assert(offsetof(WebRtcRtpTransceiverInit, streamIds) == 8);
static_assert(offsetof(WebRtcRtpTransceiverInit, sendEncodingsBegin) == 32);

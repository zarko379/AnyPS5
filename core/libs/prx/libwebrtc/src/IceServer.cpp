/* Derived from api/peer_connection_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/peer_connection_interface.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/rtp_transceiver_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/rtp_transceiver_interface.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

constexpr std::int32_t TlsCertPolicySecure = 0;
constexpr std::int32_t RtpTransceiverDirectionSendRecv = 0;

void InitEmpty(WebRtcGuestString& string) {
    string.reserved = 0;
    string.buffer[0] = '\0';
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    InitEmpty(string);
}

void ValidateString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity > WebRtcGuestStringInlineCapacity && !string.pointer)
        throw std::invalid_argument(message);
}

void CopyString(const WebRtcGuestString& source, WebRtcGuestString& destination) {
    InitEmpty(destination);
    if (source.size <= WebRtcGuestStringInlineCapacity) {
        if (source.size > 0)
            std::memcpy(destination.buffer, WebRtcGuestStringData(source), source.size);
        destination.buffer[source.size] = '\0';
        destination.size = source.size;
        return;
    }
    const std::size_t capacity = source.size | WebRtcGuestStringInlineCapacity;
    if (capacity > std::numeric_limits<std::size_t>::max() - 1)
        throw std::bad_alloc();
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, source.pointer, source.size);
    data[source.size] = '\0';
    destination.pointer = data;
    destination.size = source.size;
    destination.capacity = capacity;
}

std::size_t Count(const WebRtcGuestStringVector& vector, const char* message) {
    const auto first = reinterpret_cast<std::uintptr_t>(vector.begin);
    const auto last = reinterpret_cast<std::uintptr_t>(vector.end);
    const auto limit = reinterpret_cast<std::uintptr_t>(vector.capacity);
    if (last < first || limit < last || (!first && limit) || first % alignof(WebRtcGuestString) ||
        (last - first) % sizeof(WebRtcGuestString) || (limit - first) % sizeof(WebRtcGuestString))
        throw std::invalid_argument(message);
    return (last - first) / sizeof(WebRtcGuestString);
}

void ValidateVector(const WebRtcGuestStringVector& vector, const char* message) {
    const std::size_t count = Count(vector, message);
    for (std::size_t index = 0; index < count; ++index)
        ValidateString(vector.begin[index], message);
}

void ReleaseVector(WebRtcGuestStringVector& vector) {
    if (!vector.begin)
        return;
    while (vector.end != vector.begin)
        ReleaseString(*--vector.end);
    _ZdlPv_nid_postfix(vector.begin);
    vector = {};
}

struct VectorGuard {
    WebRtcGuestStringVector vector {};
    ~VectorGuard() { ReleaseVector(vector); }
};

struct StringGuard {
    WebRtcGuestString string {};
    StringGuard() { InitEmpty(string); }
    ~StringGuard() { ReleaseString(string); }
};

void CopyVector(const WebRtcGuestStringVector& source, VectorGuard& destination) {
    const std::size_t count = static_cast<std::size_t>(source.end - source.begin);
    if (count == 0)
        return;
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(WebRtcGuestString))
        throw std::bad_alloc();
    auto* storage = static_cast<WebRtcGuestString*>(_Znwm_nid_postfix(count * sizeof(WebRtcGuestString)));
    destination.vector = {storage, storage, storage + count};
    for (std::size_t index = 0; index < count; ++index) {
        CopyString(source.begin[index], storage[index]);
        ++destination.vector.end;
    }
}

void ValidateServer(const WebRtcIceServer* server, const char* message) {
    ValidateString(server->uri, message);
    ValidateVector(server->urls, message);
    ValidateString(server->username, message);
    ValidateString(server->password, message);
    ValidateString(server->hostname, message);
    ValidateVector(server->tlsAlpnProtocols, message);
    ValidateVector(server->tlsEllipticCurves, message);
}

WebRtcGuestString Take(StringGuard& guard) {
    WebRtcGuestString string = guard.string;
    InitEmpty(guard.string);
    return string;
}

WebRtcGuestStringVector Take(VectorGuard& guard) {
    WebRtcGuestStringVector vector = guard.vector;
    guard.vector = {};
    return vector;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(WebRtcIceServer* server) {
    if (!server)
        throw std::invalid_argument("PeerConnectionInterface::IceServer constructor requires a server");
    InitEmpty(server->uri);
    server->urls = {};
    InitEmpty(server->username);
    InitEmpty(server->password);
    server->tlsCertPolicy = TlsCertPolicySecure;
    InitEmpty(server->hostname);
    server->tlsAlpnProtocols = {};
    server->tlsEllipticCurves = {};
}

void APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(WebRtcIceServer* server, const WebRtcIceServer* other) {
    constexpr const char* message = "PeerConnectionInterface::IceServer copy constructor requires valid servers";
    if (!server || !other)
        throw std::invalid_argument(message);
    ValidateServer(other, message);
    StringGuard uri;
    CopyString(other->uri, uri.string);
    VectorGuard urls;
    CopyVector(other->urls, urls);
    StringGuard username;
    CopyString(other->username, username.string);
    StringGuard password;
    CopyString(other->password, password.string);
    StringGuard hostname;
    CopyString(other->hostname, hostname.string);
    VectorGuard alpn;
    CopyVector(other->tlsAlpnProtocols, alpn);
    VectorGuard curves;
    CopyVector(other->tlsEllipticCurves, curves);
    const std::int32_t policy = other->tlsCertPolicy;
    server->uri = Take(uri);
    server->urls = Take(urls);
    server->username = Take(username);
    server->password = Take(password);
    server->tlsCertPolicy = policy;
    server->hostname = Take(hostname);
    server->tlsAlpnProtocols = Take(alpn);
    server->tlsEllipticCurves = Take(curves);
}

void APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(WebRtcIceServer* server) {
    constexpr const char* message = "PeerConnectionInterface::IceServer destructor requires a valid server";
    if (!server)
        throw std::invalid_argument(message);
    ValidateServer(server, message);
    ReleaseVector(server->tlsEllipticCurves);
    ReleaseVector(server->tlsAlpnProtocols);
    ReleaseString(server->hostname);
    ReleaseString(server->password);
    ReleaseString(server->username);
    ReleaseVector(server->urls);
    ReleaseString(server->uri);
}

void APS5_VABI _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(WebRtcRtpTransceiverInit* init) {
    if (!init)
        throw std::invalid_argument("RtpTransceiverInit constructor requires an object");
    init->direction = RtpTransceiverDirectionSendRecv;
    init->streamIds = {};
    init->sendEncodingsBegin = nullptr;
    init->sendEncodingsEnd = nullptr;
    init->sendEncodingsCapacity = nullptr;
}

void APS5_VABI _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(WebRtcRtpTransceiverInit* init) {
    constexpr const char* message = "RtpTransceiverInit destructor requires a valid object";
    if (!init)
        throw std::invalid_argument(message);
    const auto first = reinterpret_cast<std::uintptr_t>(init->sendEncodingsBegin);
    const auto last = reinterpret_cast<std::uintptr_t>(init->sendEncodingsEnd);
    const auto limit = reinterpret_cast<std::uintptr_t>(init->sendEncodingsCapacity);
    if (last < first || limit < last || (!first && limit))
        throw std::invalid_argument(message);
    ValidateVector(init->streamIds, message);
    if (last != first)
        NotImplemented_nid_no_patch("RtpTransceiverInit destructor with send encodings: the RtpEncodingParameters layout is not modelled");
    if (init->sendEncodingsBegin)
        _ZdlPv_nid_postfix(init->sendEncodingsBegin);
    init->sendEncodingsBegin = nullptr;
    init->sendEncodingsEnd = nullptr;
    init->sendEncodingsCapacity = nullptr;
    ReleaseVector(init->streamIds);
}

}

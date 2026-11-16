/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;
constexpr std::size_t CertificateSlot = 2;
constexpr std::size_t ExpirationTimeSlot = 7;
constexpr std::uint64_t MillisecondsPerSecond = 1000;

void* Slot(const void* object, std::size_t slot, const char* message) {
    const void* const* vtable;
    std::memcpy(&vtable, object, sizeof(vtable));
    if (!vtable || !vtable[slot])
        throw std::invalid_argument(message);
    return const_cast<void*>(vtable[slot]);
}

}

extern "C" {

void APS5_VABI _ZN3rtc14RTCCertificateD1Ev_nid_postfix(WebRtcRtcCertificate* certificate) {
    if (!certificate)
        throw std::invalid_argument("RTCCertificate destructor requires a certificate");
    void* identity = certificate->identity;
    if (!identity)
        return;
    const void* const* vtable;
    std::memcpy(&vtable, identity, sizeof(vtable));
    if (!vtable || !vtable[DeletingDestructorSlot])
        throw std::invalid_argument("RTCCertificate destructor requires an identity with a virtual destructor");
    using Destructor = void(APS5_VABI*)(void*);
    reinterpret_cast<Destructor>(const_cast<void*>(vtable[DeletingDestructorSlot]))(identity);
}


bool APS5_VABI _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(const WebRtcRtcCertificate* certificate, std::uint64_t now) {
    if (!certificate)
        throw std::invalid_argument("RTCCertificate::HasExpired requires a certificate");
    const void* identity = certificate->identity;
    if (!identity)
        throw std::invalid_argument("RTCCertificate::HasExpired requires an identity");
    using CertificateGetter = const void*(APS5_VABI*)(const void*);
    const void* sslCertificate = reinterpret_cast<CertificateGetter>(Slot(identity, CertificateSlot, "RTCCertificate::HasExpired requires an identity with certificate()"))(identity);
    if (!sslCertificate)
        throw std::invalid_argument("RTCCertificate::HasExpired requires an identity certificate");
    using ExpirationTime = std::int64_t(APS5_VABI*)(const void*);
    const std::int64_t expires = reinterpret_cast<ExpirationTime>(Slot(sslCertificate, ExpirationTimeSlot, "RTCCertificate::HasExpired requires a certificate with CertificateExpirationTime()"))(sslCertificate);
    const std::uint64_t expiresMs = expires != -1 ? static_cast<std::uint64_t>(expires) * MillisecondsPerSecond : 0;
    return expiresMs <= now;
}

}

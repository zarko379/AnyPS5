/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
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
#include "prx/libc/libc.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

const WebRtcIpAddress& Checked(const WebRtcIpAddress* address, const char* function) {
    if (!address)
        throw std::invalid_argument(function);
    return *address;
}

std::uint32_t BigEndianWord(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) | (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

std::uint32_t HostOrderIpv4(const WebRtcIpAddress& address) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &address.address.ip4, sizeof(bytes));
    return BigEndianWord(bytes);
}

void StoreBigEndianWord(std::uint8_t* bytes, std::uint32_t word) {
    bytes[0] = static_cast<std::uint8_t>(word >> 24);
    bytes[1] = static_cast<std::uint8_t>(word >> 16);
    bytes[2] = static_cast<std::uint8_t>(word >> 8);
    bytes[3] = static_cast<std::uint8_t>(word);
}

void APS5_VABI DestroyIpAddress_nid_no_patch(WebRtcIpAddress* address);

void APS5_VABI DeleteIpAddress_nid_no_patch(WebRtcIpAddress* address) {
    DestroyIpAddress_nid_no_patch(address);
    _ZdlPv_nid_postfix(address);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&DestroyIpAddress_nid_no_patch),
    reinterpret_cast<void*>(&DeleteIpAddress_nid_no_patch),
};

void APS5_VABI DestroyIpAddress_nid_no_patch(WebRtcIpAddress* address) {
    address->vtable = Vtable + 2;
}

void WriteAddress(WebRtcIpAddress* result, std::int32_t family, const std::uint8_t* bytes) {
    result->vtable = Vtable + 2;
    result->family = family;
    std::memcpy(&result->address, bytes, sizeof(result->address));
}

}

const void* WebRtcIpAddressVtable() {
    return Vtable + 2;
}

void WebRtcConstructIpAddress(WebRtcIpAddress* address) {
    const std::uint8_t zero[sizeof(address->address)] = {};
    WriteAddress(address, WebRtcAfUnspec, zero);
}

extern "C" {

WebRtcInAddr APS5_VABI _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(const WebRtcIpAddress* address) {
    return Checked(address, "IPAddress::ipv4_address requires an address").address.ip4;
}

WebRtcIn6Addr APS5_VABI _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(const WebRtcIpAddress* address) {
    return Checked(address, "IPAddress::ipv6_address requires an address").address.ip6;
}

std::uint32_t APS5_VABI _ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(const WebRtcIpAddress* address) {
    const WebRtcIpAddress& checked = Checked(address, "IPAddress::v4AddressAsHostOrderInteger requires an address");
    return checked.family == WebRtcAfInet ? HostOrderIpv4(checked) : 0;
}

bool APS5_VABI _ZNK3rtc9IPAddresseqERKS0__nid_postfix(const WebRtcIpAddress* address, const WebRtcIpAddress* other) {
    const WebRtcIpAddress& left = Checked(address, "IPAddress::operator== requires an address");
    const WebRtcIpAddress& right = Checked(other, "IPAddress::operator== requires another address");
    if (left.family != right.family)
        return false;
    if (left.family == WebRtcAfInet)
        return std::memcmp(&left.address.ip4, &right.address.ip4, sizeof(WebRtcInAddr)) == 0;
    if (left.family == WebRtcAfInet6)
        return std::memcmp(&left.address.ip6, &right.address.ip6, sizeof(WebRtcIn6Addr)) == 0;
    return left.family == WebRtcAfUnspec;
}

int APS5_VABI _ZN3rtc15CountIPMaskBitsERKNS_9IPAddressE_nid_postfix(const WebRtcIpAddress* mask) {
    const WebRtcIpAddress& checked = Checked(mask, "CountIPMaskBits requires a mask");
    std::uint32_t word = 0;
    int bits = 0;
    if (checked.family == WebRtcAfInet) {
        word = HostOrderIpv4(checked);
    } else if (checked.family == WebRtcAfInet6) {
        const std::uint8_t* bytes = checked.address.ip6.bytes;
        while (bits < 128 && BigEndianWord(bytes + bits / 8) == 0xFFFFFFFFu)
            bits += 32;
        if (bits < 128)
            word = BigEndianWord(bytes + bits / 8);
    } else {
        return 0;
    }
    if (word == 0)
        return bits;
    return bits + 32 - std::countr_zero(word);
}

WebRtcIpAddress* APS5_VABI _ZN3rtc10TruncateIPERKNS_9IPAddressEi_nid_postfix(WebRtcIpAddress* result, const WebRtcIpAddress* ip, int length) {
    if (!result)
        throw std::invalid_argument("TruncateIP requires a result");
    const WebRtcIpAddress& source = Checked(ip, "TruncateIP requires an address");
    std::uint8_t bytes[sizeof(source.address)] = {};
    std::int32_t family = WebRtcAfUnspec;
    if (length >= 0 && source.family == WebRtcAfInet) {
        family = WebRtcAfInet;
        if (length > 31) {
            std::memcpy(bytes, &source.address, sizeof(bytes));
        } else if (length != 0) {
            const std::uint32_t mask = 0xFFFFFFFFu << (32 - length);
            StoreBigEndianWord(bytes, HostOrderIpv4(source) & mask);
        }
    } else if (length >= 0 && source.family == WebRtcAfInet6) {
        family = WebRtcAfInet6;
        if (length > 127) {
            std::memcpy(bytes, &source.address, sizeof(bytes));
        } else if (length != 0) {
            std::memcpy(bytes, source.address.ip6.bytes, sizeof(bytes));
            const int position = length / 32;
            const int innerLength = 32 - (length - position * 32);
            const auto innerMask = static_cast<std::uint32_t>(0xFFFFFFFFull << innerLength);
            for (int i = 0; i < 4; ++i) {
                if (i == position)
                    StoreBigEndianWord(bytes + 4 * i, BigEndianWord(bytes + 4 * i) & innerMask);
                else if (i > position)
                    StoreBigEndianWord(bytes + 4 * i, 0);
            }
        }
    }
    WriteAddress(result, family, bytes);
    return result;
}

}

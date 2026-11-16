/* Derived from modules/rtp_rtcp/source/rtcp_packet/compound_packet.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;
constexpr std::size_t BlockLengthSlot = 2;
constexpr std::size_t CreateSlot = 3;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using DeletingDestructor = void (APS5_VABI*)(void* packet);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const void* packet);
using CreateFunction = bool (APS5_VABI*)(const void* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);

std::size_t PacketCount(const WebRtcRtcpCompoundPacket* compound) {
    return compound->packetsBegin ? static_cast<std::size_t>(compound->packetsEnd - compound->packetsBegin) : 0;
}

void* Slot(const void* packet, std::size_t slot, const char* message) {
    const void* const* vtable = nullptr;
    if (!packet || !GuestCopyIn_nid_no_patch(&vtable, packet, sizeof(vtable)) || !vtable || !vtable[slot])
        throw std::invalid_argument(message);
    return const_cast<void*>(vtable[slot]);
}

std::size_t APS5_VABI CompoundPacketBlockLength_nid_no_patch(const WebRtcRtcpCompoundPacket* compound) {
    if (!compound)
        throw std::invalid_argument("CompoundPacket::BlockLength requires a packet object");
    std::size_t blockLength = 0;
    const std::size_t count = PacketCount(compound);
    for (std::size_t index = 0; index < count; ++index) {
        const void* appended = compound->packetsBegin[index];
        blockLength += reinterpret_cast<BlockLengthFunction>(Slot(appended, BlockLengthSlot, "CompoundPacket::BlockLength requires appended packets with BlockLength"))(appended);
    }
    return blockLength;
}

bool APS5_VABI CompoundPacketCreate_nid_no_patch(const WebRtcRtcpCompoundPacket* compound, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    if (!compound)
        throw std::invalid_argument("CompoundPacket::Create requires a packet object");
    const std::size_t count = PacketCount(compound);
    for (std::size_t appendedIndex = 0; appendedIndex < count; ++appendedIndex) {
        const void* appended = compound->packetsBegin[appendedIndex];
        const auto create = reinterpret_cast<CreateFunction>(Slot(appended, CreateSlot, "CompoundPacket::Create requires appended packets with Create"));
        if (!create(appended, packet, index, maxLength, function, call))
            return false;
    }
    return true;
}

void APS5_VABI DeleteCompoundPacket_nid_no_patch(WebRtcRtcpCompoundPacket* compound) {
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(compound);
    _ZdlPv_nid_postfix(compound);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteCompoundPacket_nid_no_patch),
    reinterpret_cast<void*>(&CompoundPacketBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&CompoundPacketCreate_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(WebRtcRtcpCompoundPacket* compound) {
    if (!compound)
        throw std::invalid_argument("CompoundPacket constructor requires a packet object");
    compound->vtable = Vtable + 2;
    compound->senderSsrc = 0;
    compound->packetsBegin = nullptr;
    compound->packetsEnd = nullptr;
    compound->packetsCapacity = nullptr;
}

void APS5_VABI _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(WebRtcRtcpCompoundPacket* compound) {
    if (!compound)
        throw std::invalid_argument("CompoundPacket destructor requires a packet object");
    const std::size_t count = PacketCount(compound);
    for (std::size_t index = 0; index < count; ++index) {
        void* appended = compound->packetsBegin[index];
        if (appended)
            Slot(appended, DeletingDestructorSlot, "CompoundPacket destructor requires appended packets with a deleting destructor");
    }
    while (compound->packetsEnd != compound->packetsBegin) {
        void* appended = *--compound->packetsEnd;
        if (appended)
            reinterpret_cast<DeletingDestructor>(Slot(appended, DeletingDestructorSlot, "CompoundPacket destructor requires appended packets with a deleting destructor"))(appended);
    }
    if (compound->packetsBegin)
        _ZdlPv_nid_postfix(compound->packetsBegin);
}

}

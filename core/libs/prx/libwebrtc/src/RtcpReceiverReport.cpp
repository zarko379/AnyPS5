/* Derived from modules/rtp_rtcp/source/rtcp_packet/receiver_report.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/rtp_rtcp/source/rtcp_packet/report_block.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/rtp_rtcp/source/rtcp_packet.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
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

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t HeaderLength = 4;
constexpr std::size_t ReceiverReportBaseLength = 4;
constexpr std::size_t ReportBlockLength = 24;
constexpr std::uint8_t PacketType = 201;
constexpr int BlockLengthSlot = 2;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

void WriteBigEndian(std::uint8_t* bytes, std::uint32_t value, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index)
        bytes[index] = static_cast<std::uint8_t>(value >> (8 * (size - 1 - index)));
}

std::size_t BlockCount(const WebRtcRtcpReceiverReport* report) {
    return report->blocksBegin ? static_cast<std::size_t>(report->blocksEnd - report->blocksBegin) : 0;
}

void CreateReportBlock(const WebRtcRtcpReportBlock& block, std::uint8_t* buffer) {
    WriteBigEndian(buffer, block.sourceSsrc, 4);
    buffer[4] = block.fractionLost;
    WriteBigEndian(buffer + 5, static_cast<std::uint32_t>(block.cumulativeLost), 3);
    WriteBigEndian(buffer + 8, block.extendedHighSeqNum, 4);
    WriteBigEndian(buffer + 12, block.jitter, 4);
    WriteBigEndian(buffer + 16, block.lastSr, 4);
    WriteBigEndian(buffer + 20, block.delaySinceLastSr, 4);
}

std::size_t APS5_VABI ReceiverReportBlockLength_nid_no_patch(const WebRtcRtcpReceiverReport* report) {
    if (!report)
        throw std::invalid_argument("ReceiverReport::BlockLength requires a packet object");
    return HeaderLength + ReceiverReportBaseLength + BlockCount(report) * ReportBlockLength;
}

std::size_t BlockLength(const WebRtcRtcpReceiverReport* report) {
    using Function = std::size_t (APS5_VABI*)(const WebRtcRtcpReceiverReport*);
    return reinterpret_cast<Function>(static_cast<void* const*>(const_cast<void*>(report->vtable))[BlockLengthSlot])(report);
}

bool APS5_VABI ReceiverReportCreate_nid_no_patch(const WebRtcRtcpReceiverReport* report, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    if (!report || !packet || !index)
        throw std::invalid_argument("ReceiverReport::Create requires a packet object, a buffer and an index");
    while (*index + BlockLength(report) > maxLength) {
        if (*index == 0)
            return false;
        if (!call)
            throw std::invalid_argument("ReceiverReport::Create needs a callback to send a full buffer");
        call(function, packet, *index);
        *index = 0;
    }
    const std::size_t count = BlockCount(report);
    const std::size_t words = (BlockLength(report) - HeaderLength) / 4;
    packet[*index] = static_cast<std::uint8_t>((2 << 6) | static_cast<std::uint8_t>(count));
    packet[*index + 1] = PacketType;
    packet[*index + 2] = static_cast<std::uint8_t>((words >> 8) & 0xff);
    packet[*index + 3] = static_cast<std::uint8_t>(words & 0xff);
    *index += HeaderLength;
    WriteBigEndian(packet + *index, report->senderSsrc, 4);
    *index += ReceiverReportBaseLength;
    for (std::size_t block = 0; block < count; ++block) {
        CreateReportBlock(report->blocksBegin[block], packet + *index);
        *index += ReportBlockLength;
    }
    return true;
}

void APS5_VABI DestroyReceiverReport_nid_no_patch(WebRtcRtcpReceiverReport* report) {
    if (!report)
        throw std::invalid_argument("ReceiverReport destructor requires a packet object");
    if (report->blocksBegin)
        _ZdlPv_nid_postfix(report->blocksBegin);
}

void APS5_VABI DeleteReceiverReport_nid_no_patch(WebRtcRtcpReceiverReport* report) {
    DestroyReceiverReport_nid_no_patch(report);
    _ZdlPv_nid_postfix(report);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&DestroyReceiverReport_nid_no_patch),
    reinterpret_cast<void*>(&DeleteReceiverReport_nid_no_patch),
    reinterpret_cast<void*>(&ReceiverReportBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&ReceiverReportCreate_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(WebRtcRtcpReceiverReport* report) {
    if (!report)
        throw std::invalid_argument("ReceiverReport constructor requires a packet object");
    report->vtable = Vtable + 2;
    report->senderSsrc = 0;
    report->blocksBegin = nullptr;
    report->blocksEnd = nullptr;
    report->blocksCapacity = nullptr;
}

}

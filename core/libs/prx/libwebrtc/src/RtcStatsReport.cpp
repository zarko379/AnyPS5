/* Derived from stats/rtc_stats_report.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/ref_counted_base.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from rtc_base/ref_counter.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;

using DeletingDestructor = void (APS5_VABI*)(const void* stats);

WebRtcRtcStatsNode* EndNode(WebRtcRtcStatsReport* report) {
    return reinterpret_cast<WebRtcRtcStatsNode*>(&report->root);
}

DeletingDestructor StatsDeleter(const void* stats, const char* message) {
    const void* const* vtable = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtable, stats, sizeof(vtable)) || !vtable || !vtable[DeletingDestructorSlot])
        throw std::invalid_argument(message);
    return reinterpret_cast<DeletingDestructor>(const_cast<void*>(vtable[DeletingDestructorSlot]));
}

void ValidateId(const WebRtcGuestString& id, const char* message) {
    if (id.capacity < WebRtcGuestStringInlineCapacity || id.size > id.capacity)
        throw std::invalid_argument(message);
    if (id.capacity > WebRtcGuestStringInlineCapacity && !id.pointer)
        throw std::invalid_argument(message);
}

std::vector<WebRtcRtcStatsNode*> PostOrderNodes(WebRtcRtcStatsReport* report, const char* message) {
    std::vector<WebRtcRtcStatsNode*> preorder;
    WebRtcRtcStatsNode* const end = EndNode(report);
    if (report->size == 0) {
        if (report->root || report->beginNode != end)
            throw std::invalid_argument(message);
        return preorder;
    }
    if (!report->root || !report->beginNode || report->beginNode == end)
        throw std::invalid_argument(message);
    struct Entry {
        WebRtcRtcStatsNode* node;
        WebRtcRtcStatsNode* parent;
    };
    std::vector<Entry> pending {{report->root, end}};
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (preorder.size() >= report->size || current.node->parent != current.parent)
            throw std::invalid_argument(message);
        ValidateId(current.node->id, message);
        if (current.node->stats)
            StatsDeleter(current.node->stats, message);
        preorder.push_back(current.node);
        if (current.node->left)
            pending.push_back({current.node->left, current.node});
        if (current.node->right)
            pending.push_back({current.node->right, current.node});
    }
    if (preorder.size() != report->size)
        throw std::invalid_argument(message);
    return std::vector<WebRtcRtcStatsNode*>(preorder.rbegin(), preorder.rend());
}

void DeleteReport(WebRtcRtcStatsReport* report, const std::vector<WebRtcRtcStatsNode*>& nodes) {
    for (WebRtcRtcStatsNode* node : nodes) {
        if (node->stats)
            StatsDeleter(node->stats, "RTCStatsReport::ConstIterator destructor requires stats with a deleting destructor")(node->stats);
        if (node->id.capacity > WebRtcGuestStringInlineCapacity)
            GuestStringStorage::Deallocate(node->id.pointer, node->id.capacity + 1);
        _ZdlPv_nid_postfix(node);
    }
    _ZdlPv_nid_postfix(report);
}

void Release(WebRtcRtcStatsReport* report) {
    constexpr const char* message = "RTCStatsReport::ConstIterator destructor requires a valid stats report";
    if (report->refCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    std::vector<WebRtcRtcStatsNode*> nodes;
    try {
        nodes = PostOrderNodes(report, message);
    } catch (...) {
        report->refCount.fetch_add(1, std::memory_order_relaxed);
        throw;
    }
    DeleteReport(report, nodes);
}

WebRtcRtcStatsReportIterator* Iterator(WebRtcRtcStatsReportIterator* result, const WebRtcRtcStatsReport* report, bool begin, const char* message) {
    if (!result || !report)
        throw std::invalid_argument(message);
    auto* mutableReport = const_cast<WebRtcRtcStatsReport*>(report);
    mutableReport->refCount.fetch_add(1, std::memory_order_relaxed);
    result->report = mutableReport;
    result->node = begin ? mutableReport->beginNode : EndNode(mutableReport);
    return result;
}

const WebRtcRtcStatsNode* Dereferenceable(const WebRtcRtcStatsReportIterator* iterator, const char* message) {
    if (!iterator || !iterator->node)
        throw std::invalid_argument(message);
    if (iterator->report && iterator->node == EndNode(iterator->report))
        throw std::invalid_argument(message);
    return iterator->node;
}

WebRtcRtcStatsReportIterator* Increment(WebRtcRtcStatsReportIterator* iterator) {
    WebRtcRtcStatsNode* node = const_cast<WebRtcRtcStatsNode*>(Dereferenceable(iterator, "RTCStatsReport::ConstIterator::operator++ requires an iterator before the end"));
    if (node->right) {
        node = node->right;
        while (node->left)
            node = node->left;
    } else {
        while (!node->parent || node != node->parent->left) {
            if (!node->parent)
                throw std::invalid_argument("RTCStatsReport::ConstIterator::operator++ requires a valid stats map");
            node = node->parent;
        }
        node = node->parent;
    }
    iterator->node = node;
    return iterator;
}

}

extern "C" {

WebRtcRtcStatsReportIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(WebRtcRtcStatsReportIterator* result, const WebRtcRtcStatsReport* report) {
    return Iterator(result, report, true, "RTCStatsReport::begin requires a result and a report");
}

WebRtcRtcStatsReportIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(WebRtcRtcStatsReportIterator* result, const WebRtcRtcStatsReport* report) {
    return Iterator(result, report, false, "RTCStatsReport::end requires a result and a report");
}

bool APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(const WebRtcRtcStatsReportIterator* iterator, const WebRtcRtcStatsReportIterator* other) {
    if (!iterator || !other)
        throw std::invalid_argument("RTCStatsReport::ConstIterator::operator!= requires two iterators");
    return iterator->node != other->node;
}

WebRtcRtcStatsReportIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(WebRtcRtcStatsReportIterator* iterator) {
    return Increment(iterator);
}

WebRtcRtcStatsReportIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(WebRtcRtcStatsReportIterator* iterator, int) {
    return Increment(iterator);
}

const void* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(const WebRtcRtcStatsReportIterator* iterator) {
    const WebRtcRtcStatsNode* node = Dereferenceable(iterator, "RTCStatsReport::ConstIterator::operator* requires an iterator before the end");
    if (!node->stats)
        throw std::invalid_argument("RTCStatsReport::ConstIterator::operator* requires stats");
    return node->stats;
}

const void* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(const WebRtcRtcStatsReportIterator* iterator) {
    return Dereferenceable(iterator, "RTCStatsReport::ConstIterator::operator-> requires an iterator before the end")->stats;
}

void APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(WebRtcRtcStatsReportIterator* iterator) {
    if (!iterator)
        throw std::invalid_argument("RTCStatsReport::ConstIterator destructor requires an iterator");
    if (iterator->report)
        Release(iterator->report);
}

}

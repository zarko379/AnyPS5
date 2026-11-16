/* Derived from api/audio_codecs/audio_format.cc and api/audio_codecs/audio_format.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/sdp_video_format.cc and api/video_codecs/sdp_video_format.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/h264_profile_level_id.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/vp9_profile.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/av1_profile.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/video_codec.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from rtc_base/string_to_number.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from media/base/codec.cc and media/base/codec.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireWritable(void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(address, bytes))
        throw std::invalid_argument(message);
}

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void ProbeReadableChunks(const void* address, std::size_t totalBytes, const char* message) {
    std::array<char, 4096> scratch{};
    const auto* cursor = static_cast<const char*>(address);
    std::size_t remaining = totalBytes;
    while (remaining > 0) {
        const std::size_t chunk = std::min(remaining, scratch.size());
        CopyInChecked(scratch.data(), cursor, chunk, message);
        cursor += chunk;
        remaining -= chunk;
    }
}

WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

const WebRtcStringMapNode* EndNode(const WebRtcStringMap& map) {
    return reinterpret_cast<const WebRtcStringMapNode*>(&map.root);
}

void InitStringEmpty(WebRtcGuestString& string) {
    string.reserved = 0;
    string.buffer[0] = '\0';
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    InitStringEmpty(string);
}

void ValidateString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    if (!string.pointer || string.size > std::numeric_limits<std::size_t>::max() - 1)
        throw std::invalid_argument(message);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
    }
    ProbeReadableChunks(string.pointer, string.size + 1, message);
}

struct StringGuard {
    WebRtcGuestString value{};
    bool active = false;

    StringGuard() { InitStringEmpty(value); }
    ~StringGuard() {
        if (active)
            ReleaseString(value);
    }
};

void CopyString(const WebRtcGuestString& source, WebRtcGuestString& destination) {
    InitStringEmpty(destination);
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

void MoveString(WebRtcGuestString& destination, WebRtcGuestString& source) {
    if (source.capacity > WebRtcGuestStringInlineCapacity) {
        destination = source;
        InitStringEmpty(source);
        return;
    }
    InitStringEmpty(destination);
    if (source.size > 0)
        std::memcpy(destination.buffer, source.buffer, source.size);
    destination.buffer[source.size] = '\0';
    destination.size = source.size;
    InitStringEmpty(source);
}

void ValidateMapNodes(const WebRtcStringMap& map, const char* message, std::vector<WebRtcStringMapNode*>& outNodes) {
    outNodes.clear();
    const WebRtcStringMapNode* const end = EndNode(map);
    if (map.size == 0) {
        if (map.root != nullptr || map.beginNode != end)
            throw std::invalid_argument(message);
        return;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(message);
    struct Entry {
        WebRtcStringMapNode* node;
        const WebRtcStringMapNode* expectedParent;
    };
    std::vector<Entry> pending;
    pending.push_back({map.root, end});
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (outNodes.size() >= map.size)
            throw std::invalid_argument(message);
        WebRtcStringMapNode nodeCopy{};
        CopyInChecked(&nodeCopy, current.node, sizeof(nodeCopy), message);
        if (nodeCopy.links.parent != current.expectedParent)
            throw std::invalid_argument(message);
        ValidateString(nodeCopy.key, message);
        ValidateString(nodeCopy.value, message);
        outNodes.push_back(current.node);
        if (nodeCopy.links.right)
            pending.push_back({nodeCopy.links.right, current.node});
        if (nodeCopy.links.left)
            pending.push_back({nodeCopy.links.left, current.node});
    }
    if (outNodes.size() != map.size)
        throw std::invalid_argument(message);
    const WebRtcStringMapNode* leftmost = map.root;
    while (leftmost->links.left)
        leftmost = leftmost->links.left;
    if (leftmost != map.beginNode)
        throw std::invalid_argument(message);
}

void DestroyMapNodes(const std::vector<WebRtcStringMapNode*>& nodes, WebRtcStringMap& map) {
    for (WebRtcStringMapNode* node : nodes) {
        ReleaseString(node->value);
        ReleaseString(node->key);
        _ZdlPv_nid_postfix(node);
    }
    map.beginNode = EndNode(map);
    map.root = nullptr;
    map.size = 0;
}

struct NodeGuard {
    std::vector<WebRtcStringMapNode*> nodes;
    bool active = true;

    ~NodeGuard() {
        if (!active)
            return;
        for (WebRtcStringMapNode* node : nodes) {
            ReleaseString(node->value);
            ReleaseString(node->key);
            _ZdlPv_nid_postfix(node);
        }
    }
};

void CloneMap(const WebRtcStringMap& source, WebRtcStringMap& destination, const char* message) {
    std::vector<WebRtcStringMapNode*> validated;
    ValidateMapNodes(source, message, validated);
    destination.beginNode = EndNode(destination);
    destination.root = nullptr;
    destination.size = 0;
    if (source.size == 0)
        return;

    NodeGuard guard;
    struct CloneTask {
        const WebRtcStringMapNode* source;
        WebRtcStringMapNode** slot;
        WebRtcStringMapNode* parent;
    };
    std::vector<CloneTask> tasks;
    tasks.push_back({source.root, &destination.root, EndNode(destination)});
    while (!tasks.empty()) {
        const CloneTask task = tasks.back();
        tasks.pop_back();
        void* storage = _Znwm_nid_postfix(sizeof(WebRtcStringMapNode));
        auto* node = new (storage) WebRtcStringMapNode{};
        node->links.parent = task.parent;
        node->links.isBlack = task.source->links.isBlack;
        InitStringEmpty(node->key);
        InitStringEmpty(node->value);
        guard.nodes.push_back(node);
        CopyString(task.source->key, node->key);
        CopyString(task.source->value, node->value);
        *task.slot = node;
        if (task.source == source.beginNode)
            destination.beginNode = node;
        if (task.source->links.right)
            tasks.push_back({task.source->links.right, &node->links.right, node});
        if (task.source->links.left)
            tasks.push_back({task.source->links.left, &node->links.left, node});
    }
    destination.size = source.size;
    guard.active = false;
}

void ValidateFormat(const WebRtcSdpAudioFormat* format, const char* message, std::vector<WebRtcStringMapNode*>& outNodes) {
    ValidateString(format->name, message);
    ValidateMapNodes(format->parameters, message, outNodes);
}

void ValidateModes(const WebRtcInlinedVector<WebRtcScalabilityModeCount>& modes, const char* message) {
    const std::uint64_t size = modes.metadata >> 1;
    if (modes.metadata & 1) {
        if (!modes.allocated.data || size > modes.allocated.capacity)
            throw std::invalid_argument(message);
        ProbeReadableChunks(modes.allocated.data, static_cast<std::size_t>(size), message);
        return;
    }
    if (size > WebRtcScalabilityModeCount)
        throw std::invalid_argument(message);
}

void ReleaseModes(WebRtcInlinedVector<WebRtcScalabilityModeCount>& modes) {
    if ((modes.metadata & 1) && modes.allocated.data)
        _ZdlPv_nid_postfix(modes.allocated.data);
    modes.metadata = 0;
}

void ValidateVideoFormat(const WebRtcSdpVideoFormat* format, const char* message, std::vector<WebRtcStringMapNode*>& outNodes) {
    ValidateString(format->name, message);
    ValidateMapNodes(format->parameters, message, outNodes);
    ValidateModes(format->scalabilityModes, message);
}

void AttachMap(WebRtcStringMap& destination, const WebRtcStringMap& cloned) {
    destination = cloned;
    if (destination.size == 0)
        destination.beginNode = EndNode(destination);
    else
        destination.root->links.parent = EndNode(destination);
}

enum class VideoCodecKind {
    Generic,
    Vp8,
    Vp9,
    Av1,
    H264,
    H265,
};

struct ByteView {
    const char* data;
    std::size_t size;
};

ByteView View(const WebRtcGuestString& string) {
    return {WebRtcGuestStringData(string), static_cast<std::size_t>(string.size)};
}

bool EqualsIgnoreCase(ByteView left, ByteView right) {
    if (left.size != right.size)
        return false;
    for (std::size_t index = 0; index < left.size; ++index) {
        auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        if (lower(left.data[index]) != lower(right.data[index]))
            return false;
    }
    return true;
}

bool Equals(ByteView left, ByteView right) {
    return left.size == right.size && (left.size == 0 || std::memcmp(left.data, right.data, left.size) == 0);
}

ByteView Literal(const char* text) {
    return {text, std::strlen(text)};
}

VideoCodecKind CodecKind(ByteView name) {
    if (EqualsIgnoreCase(name, Literal("VP8")))
        return VideoCodecKind::Vp8;
    if (EqualsIgnoreCase(name, Literal("VP9")))
        return VideoCodecKind::Vp9;
    if (EqualsIgnoreCase(name, Literal("AV1")) || EqualsIgnoreCase(name, Literal("AV1X")))
        return VideoCodecKind::Av1;
    if (EqualsIgnoreCase(name, Literal("H264")))
        return VideoCodecKind::H264;
    if (EqualsIgnoreCase(name, Literal("H265")))
        return VideoCodecKind::H265;
    return VideoCodecKind::Generic;
}

const WebRtcGuestString* FindParameter(const std::vector<WebRtcStringMapNode*>& nodes, const char* key) {
    const ByteView wanted = Literal(key);
    for (const WebRtcStringMapNode* node : nodes)
        if (Equals(View(node->key), wanted))
            return &node->value;
    return nullptr;
}

ByteView ParameterOrDefault(const std::vector<WebRtcStringMapNode*>& nodes, const char* key, const char* fallback) {
    const WebRtcGuestString* value = FindParameter(nodes, key);
    return value ? View(*value) : Literal(fallback);
}

std::optional<long long> StringToInt(ByteView text) {
    if (text.size == 0)
        return std::nullopt;
    const bool startsWithDigit = text.data[0] >= '0' && text.data[0] <= '9';
    if (!startsWithDigit && text.data[0] != '-')
        return std::nullopt;
    const bool negative = text.data[0] == '-';
    std::size_t index = negative ? 1 : 0;
    if (index == text.size)
        return std::nullopt;
    unsigned long long magnitude = 0;
    const unsigned long long limit = negative ? static_cast<unsigned long long>(std::numeric_limits<long long>::max()) + 1 : static_cast<unsigned long long>(std::numeric_limits<long long>::max());
    for (; index < text.size; ++index) {
        const char c = text.data[index];
        if (c < '0' || c > '9')
            return std::nullopt;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (magnitude > (limit - digit) / 10)
            return std::nullopt;
        magnitude = magnitude * 10 + digit;
    }
    if (negative)
        return magnitude == static_cast<unsigned long long>(std::numeric_limits<long long>::max()) + 1 ? std::numeric_limits<long long>::min() : -static_cast<long long>(magnitude);
    return static_cast<long long>(magnitude);
}

std::optional<int> ParseProfile(const std::vector<WebRtcStringMapNode*>& nodes, const char* key, int maximum) {
    const WebRtcGuestString* value = FindParameter(nodes, key);
    if (!value)
        return 0;
    const std::optional<long long> number = StringToInt(View(*value));
    if (!number || *number < 0 || *number > maximum)
        return std::nullopt;
    return static_cast<int>(*number);
}

bool SameProfile(const std::vector<WebRtcStringMapNode*>& left, const std::vector<WebRtcStringMapNode*>& right, const char* key, int maximum) {
    const std::optional<int> profile = ParseProfile(left, key, maximum);
    const std::optional<int> otherProfile = ParseProfile(right, key, maximum);
    return profile && otherProfile && *profile == *otherProfile;
}

bool IsHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

unsigned HexValue(char c) {
    if (c >= '0' && c <= '9')
        return static_cast<unsigned>(c - '0');
    if (c >= 'a' && c <= 'f')
        return static_cast<unsigned>(c - 'a' + 10);
    return static_cast<unsigned>(c - 'A' + 10);
}

std::uint32_t StrtolHex(const char* text, std::size_t length) {
    std::size_t index = 0;
    while (index < length && (text[index] == ' ' || (text[index] >= '\t' && text[index] <= '\r')))
        ++index;
    bool negative = false;
    if (index < length && (text[index] == '+' || text[index] == '-')) {
        negative = text[index] == '-';
        ++index;
    }
    if (index + 2 < length && text[index] == '0' && (text[index + 1] == 'x' || text[index + 1] == 'X') && IsHexDigit(text[index + 2]))
        index += 2;
    std::uint64_t value = 0;
    for (; index < length && IsHexDigit(text[index]); ++index)
        value = value * 16 + HexValue(text[index]);
    return static_cast<std::uint32_t>(negative ? 0 - value : value);
}

constexpr int H264LevelValues[] = {10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 52};

struct H264ProfilePattern {
    std::uint8_t profileIdc;
    std::uint8_t mask;
    std::uint8_t maskedValue;
    int profile;
};

constexpr H264ProfilePattern H264ProfilePatterns[] = {
    {0x42, 0x4F, 0x40, 0},
    {0x4D, 0x8F, 0x80, 0},
    {0x58, 0xCF, 0xC0, 0},
    {0x42, 0x4F, 0x00, 1},
    {0x58, 0xCF, 0x80, 1},
    {0x4D, 0xAF, 0x00, 2},
    {0x64, 0xFF, 0x00, 4},
    {0x64, 0xFF, 0x0C, 3},
    {0xF4, 0xFF, 0x00, 5},
};

std::optional<int> ParseH264Profile(const std::vector<WebRtcStringMapNode*>& nodes) {
    const WebRtcGuestString* value = FindParameter(nodes, "profile-level-id");
    if (!value)
        return 0;
    const char* text = WebRtcGuestStringData(*value);
    const std::size_t length = static_cast<std::size_t>(std::find(text, text + value->size, '\0') - text);
    if (length != 6)
        return std::nullopt;
    const std::uint32_t numeric = StrtolHex(text, length);
    if (numeric == 0)
        return std::nullopt;
    const int levelIdc = static_cast<int>(numeric & 0xFF);
    const auto profileIop = static_cast<std::uint8_t>((numeric >> 8) & 0xFF);
    const auto profileIdc = static_cast<std::uint8_t>((numeric >> 16) & 0xFF);
    if (std::find(std::begin(H264LevelValues), std::end(H264LevelValues), levelIdc) == std::end(H264LevelValues))
        return std::nullopt;
    for (const H264ProfilePattern& pattern : H264ProfilePatterns)
        if (profileIdc == pattern.profileIdc && (profileIop & pattern.mask) == pattern.maskedValue)
            return pattern.profile;
    return std::nullopt;
}

bool IsSameCodecSpecific(ByteView name, const std::vector<WebRtcStringMapNode*>& left, const std::vector<WebRtcStringMapNode*>& right) {
    switch (CodecKind(name)) {
    case VideoCodecKind::H264: {
        const std::optional<int> profile = ParseH264Profile(left);
        const std::optional<int> otherProfile = ParseH264Profile(right);
        return profile && otherProfile && *profile == *otherProfile &&
            Equals(ParameterOrDefault(left, "packetization-mode", "0"), ParameterOrDefault(right, "packetization-mode", "0"));
    }
    case VideoCodecKind::Vp9:
        return SameProfile(left, right, "profile-id", 3);
    case VideoCodecKind::Av1:
        return SameProfile(left, right, "profile", 2) &&
            Equals(ParameterOrDefault(left, "tier", "0"), ParameterOrDefault(right, "tier", "0")) &&
            Equals(ParameterOrDefault(left, "level-idx", "5"), ParameterOrDefault(right, "level-idx", "5"));
    case VideoCodecKind::H265:
        NotImplemented_nid_no_patch("SdpVideoFormat::IsCodecInList for H265: whether the module is built with RTC_ENABLE_H265 is unknown");
    default:
        return true;
    }
}

std::size_t FeedbackCount(const WebRtcCodec* codec, const char* message) {
    const auto first = reinterpret_cast<std::uintptr_t>(codec->feedbackBegin);
    const auto last = reinterpret_cast<std::uintptr_t>(codec->feedbackEnd);
    const auto limit = reinterpret_cast<std::uintptr_t>(codec->feedbackCapacity);
    if (last < first || limit < last || (!first && limit) || first % alignof(WebRtcFeedbackParam) ||
        (last - first) % sizeof(WebRtcFeedbackParam) || (limit - first) % sizeof(WebRtcFeedbackParam))
        throw std::invalid_argument(message);
    return (last - first) / sizeof(WebRtcFeedbackParam);
}

void ValidateOptionalString(const WebRtcOptionalGuestString& optional, const char* message) {
    if (optional.engaged)
        ValidateString(optional.value, message);
}

void ReleaseOptionalString(WebRtcOptionalGuestString& optional) {
    if (optional.engaged)
        ReleaseString(optional.value);
    optional.engaged = false;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(WebRtcSdpAudioFormat* format, const WebRtcSdpAudioFormat* other) {
    constexpr const char* message = "SdpAudioFormat copy constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpAudioFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);
    ValidateString(other->name, message);

    StringGuard nameGuard;
    CopyString(other->name, nameGuard.value);
    nameGuard.active = true;

    WebRtcStringMap clonedMap{};
    CloneMap(other->parameters, clonedMap, message);

    format->name = nameGuard.value;
    nameGuard.active = false;
    format->clockrateHz = other->clockrateHz;
    format->numChannels = other->numChannels;
    format->parameters = clonedMap;
    if (format->parameters.size == 0)
        format->parameters.beginNode = EndNode(format->parameters);
    else
        format->parameters.root->links.parent = EndNode(format->parameters);
}

void APS5_VABI _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(WebRtcSdpAudioFormat* format, WebRtcSdpAudioFormat* other) {
    constexpr const char* message = "SdpAudioFormat move constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    RequireWritable(other, sizeof(*other), message);
    if (format == other)
        throw std::invalid_argument(message);

    std::vector<WebRtcStringMapNode*> validated;
    ValidateFormat(other, message, validated);

    MoveString(format->name, other->name);
    format->clockrateHz = other->clockrateHz;
    format->numChannels = other->numChannels;
    if (other->parameters.size == 0) {
        format->parameters.beginNode = EndNode(format->parameters);
        format->parameters.root = nullptr;
        format->parameters.size = 0;
    } else {
        format->parameters.beginNode = other->parameters.beginNode;
        format->parameters.root = other->parameters.root;
        format->parameters.size = other->parameters.size;
        format->parameters.root->links.parent = EndNode(format->parameters);
        other->parameters.beginNode = EndNode(other->parameters);
        other->parameters.root = nullptr;
        other->parameters.size = 0;
    }
}

WebRtcSdpAudioFormat* APS5_VABI _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(WebRtcSdpAudioFormat* format, const WebRtcSdpAudioFormat* other) {
    constexpr const char* message = "SdpAudioFormat copy assignment requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpAudioFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);

    std::vector<WebRtcStringMapNode*> existingNodes;
    ValidateFormat(format, message, existingNodes);
    if (format == other)
        return format;

    ValidateString(other->name, message);

    StringGuard nameGuard;
    CopyString(other->name, nameGuard.value);
    nameGuard.active = true;

    WebRtcStringMap clonedMap{};
    CloneMap(other->parameters, clonedMap, message);

    DestroyMapNodes(existingNodes, format->parameters);
    ReleaseString(format->name);

    format->name = nameGuard.value;
    nameGuard.active = false;
    format->clockrateHz = other->clockrateHz;
    format->numChannels = other->numChannels;
    format->parameters = clonedMap;
    if (format->parameters.size == 0)
        format->parameters.beginNode = EndNode(format->parameters);
    else
        format->parameters.root->links.parent = EndNode(format->parameters);
    return format;
}

void APS5_VABI _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(WebRtcSdpAudioFormat* format) {
    constexpr const char* message = "SdpAudioFormat destructor requires a valid object";
    RequireWritable(format, sizeof(*format), message);
    std::vector<WebRtcStringMapNode*> nodes;
    ValidateFormat(format, message, nodes);
    DestroyMapNodes(nodes, format->parameters);
    ReleaseString(format->name);
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat copy constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);
    std::vector<WebRtcStringMapNode*> validated;
    ValidateVideoFormat(other, message, validated);

    const WebRtcInlinedVector<WebRtcScalabilityModeCount>& sourceModes = other->scalabilityModes;
    const std::size_t modeCount = static_cast<std::size_t>(sourceModes.metadata >> 1);
    WebRtcInlinedVector<WebRtcScalabilityModeCount> modes{};
    if (modeCount != 0 && (sourceModes.metadata & 1)) {
        const std::size_t capacity = std::max<std::size_t>(2 * WebRtcScalabilityModeCount, modeCount);
        modes.allocated.data = _Znwm_nid_postfix(capacity);
        modes.allocated.capacity = capacity;
        std::memcpy(modes.allocated.data, sourceModes.allocated.data, modeCount);
        modes.metadata = sourceModes.metadata;
    } else if (modeCount != 0) {
        modes = sourceModes;
    }

    try {
        StringGuard nameGuard;
        CopyString(other->name, nameGuard.value);
        nameGuard.active = true;
        WebRtcStringMap clonedMap{};
        CloneMap(other->parameters, clonedMap, message);
        format->name = nameGuard.value;
        nameGuard.active = false;
        AttachMap(format->parameters, clonedMap);
    } catch (...) {
        ReleaseModes(modes);
        throw;
    }
    format->scalabilityModes = modes;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(WebRtcSdpVideoFormat* format, WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat move constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    RequireWritable(other, sizeof(*other), message);
    if (format == other)
        throw std::invalid_argument(message);
    std::vector<WebRtcStringMapNode*> validated;
    ValidateVideoFormat(other, message, validated);

    MoveString(format->name, other->name);
    if (other->parameters.size == 0) {
        format->parameters.beginNode = EndNode(format->parameters);
        format->parameters.root = nullptr;
        format->parameters.size = 0;
    } else {
        format->parameters = other->parameters;
        format->parameters.root->links.parent = EndNode(format->parameters);
        other->parameters.beginNode = EndNode(other->parameters);
        other->parameters.root = nullptr;
        other->parameters.size = 0;
    }
    format->scalabilityModes = other->scalabilityModes;
    other->scalabilityModes.metadata = 0;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(WebRtcSdpVideoFormat* format) {
    constexpr const char* message = "SdpVideoFormat destructor requires a valid object";
    RequireWritable(format, sizeof(*format), message);
    std::vector<WebRtcStringMapNode*> nodes;
    ValidateVideoFormat(format, message, nodes);
    ReleaseModes(format->scalabilityModes);
    DestroyMapNodes(nodes, format->parameters);
    ReleaseString(format->name);
}

bool APS5_VABI _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(const WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* formats, std::size_t count) {
    constexpr const char* message = "SdpVideoFormat::IsCodecInList requires valid formats";
    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, format, sizeof(snapshot), message);
    if (count != 0 && !formats)
        throw std::invalid_argument(message);
    ValidateString(format->name, message);
    std::vector<WebRtcStringMapNode*> nodes;
    ValidateMapNodes(format->parameters, message, nodes);
    const ByteView name = View(format->name);
    for (std::size_t index = 0; index < count; ++index) {
        const WebRtcSdpVideoFormat* other = formats + index;
        CopyInChecked(&snapshot, other, sizeof(snapshot), message);
        ValidateString(other->name, message);
        if (!EqualsIgnoreCase(name, View(other->name)))
            continue;
        std::vector<WebRtcStringMapNode*> otherNodes;
        ValidateMapNodes(other->parameters, message, otherNodes);
        if (IsSameCodecSpecific(name, nodes, otherNodes))
            return true;
    }
    return false;
}

void APS5_VABI _ZN7cricket5CodecD1Ev_nid_postfix(WebRtcCodec* codec) {
    constexpr const char* message = "cricket::Codec destructor requires a valid codec";
    RequireWritable(codec, sizeof(*codec), message);
    ValidateString(codec->name, message);
    ValidateOptionalString(codec->packetization, message);
    ValidateModes(codec->scalabilityModes, message);
    ValidateOptionalString(codec->txMode, message);
    std::vector<WebRtcStringMapNode*> nodes;
    ValidateMapNodes(codec->params, message, nodes);
    const std::size_t feedbackCount = FeedbackCount(codec, message);
    for (std::size_t index = 0; index < feedbackCount; ++index) {
        ValidateString(codec->feedbackBegin[index].id, message);
        ValidateString(codec->feedbackBegin[index].param, message);
    }
    while (codec->feedbackEnd != codec->feedbackBegin) {
        WebRtcFeedbackParam& feedback = *--codec->feedbackEnd;
        ReleaseString(feedback.param);
        ReleaseString(feedback.id);
    }
    if (codec->feedbackBegin)
        _ZdlPv_nid_postfix(codec->feedbackBegin);
    codec->feedbackBegin = nullptr;
    codec->feedbackEnd = nullptr;
    codec->feedbackCapacity = nullptr;
    DestroyMapNodes(nodes, codec->params);
    ReleaseOptionalString(codec->txMode);
    ReleaseModes(codec->scalabilityModes);
    ReleaseOptionalString(codec->packetization);
    ReleaseString(codec->name);
}

}

/* Derived from api/video/i420_buffer.cc, rtc_base/memory/aligned_malloc.cc, rtc_base/ref_counted_object.h and api/video/video_frame_buffer.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

using PlaneGetter = const std::uint8_t* (APS5_VABI*)(const WebRtcI420Buffer*);

constexpr std::size_t DataYSlot = 17;
constexpr std::size_t DataUSlot = 18;
constexpr std::size_t DataVSlot = 19;

std::uint8_t* Plane(WebRtcI420Buffer* buffer, std::size_t slot, const char* function) {
    if (!buffer || !buffer->vtable)
        throw std::invalid_argument(function);
    const auto getter = reinterpret_cast<PlaneGetter>(const_cast<void*>(static_cast<const void* const*>(buffer->vtable)[slot]));
    return const_cast<std::uint8_t*>(getter(buffer));
}

constexpr std::int32_t VideoFrameBufferTypeI420 = 1;
constexpr std::int32_t ReleaseDroppedLastRef = 0;
constexpr std::int32_t ReleaseOtherRefsRemained = 1;
constexpr std::size_t BufferAlignment = 64;

const WebRtcRefCountedI420Buffer* Object(const void* buffer) {
    return static_cast<const WebRtcRefCountedI420Buffer*>(buffer);
}

void AlignedFree(void* block) {
    if (!block)
        return;
    std::uintptr_t start = 0;
    std::memcpy(&start, static_cast<const std::uint8_t*>(block) - sizeof(std::uintptr_t), sizeof(start));
    free_nid_postfix(reinterpret_cast<void*>(start));
}

void* AlignedMalloc(std::size_t size) {
    void* memory = malloc_nid_postfix(size + sizeof(std::uintptr_t) + BufferAlignment - 1);
    if (!memory)
        throw std::runtime_error("I420Buffer::Create: RTC_CHECK(memory_pointer) failed in AlignedMalloc");
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(memory);
    const std::uintptr_t aligned = (start + sizeof(std::uintptr_t) + BufferAlignment - 1) & ~(static_cast<std::uintptr_t>(BufferAlignment) - 1);
    std::memcpy(reinterpret_cast<void*>(aligned - sizeof(std::uintptr_t)), &start, sizeof(start));
    return reinterpret_cast<void*>(aligned);
}

void APS5_VABI DestroyBuffer_nid_no_patch(WebRtcRefCountedI420Buffer* buffer) {
    AlignedFree(buffer->buffer.data);
}

void APS5_VABI DeleteBuffer_nid_no_patch(WebRtcRefCountedI420Buffer* buffer) {
    DestroyBuffer_nid_no_patch(buffer);
    _ZdlPv_nid_postfix(buffer);
}

void APS5_VABI AddRefBuffer_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    const_cast<WebRtcRefCountedI420Buffer*>(buffer)->refCount.fetch_add(1, std::memory_order_relaxed);
}

std::int32_t APS5_VABI ReleaseBuffer_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    auto* mutableBuffer = const_cast<WebRtcRefCountedI420Buffer*>(buffer);
    if (mutableBuffer->refCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return ReleaseOtherRefsRemained;
    DeleteBuffer_nid_no_patch(mutableBuffer);
    return ReleaseDroppedLastRef;
}

bool APS5_VABI HasOneRef_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->refCount.load(std::memory_order_acquire) == 1;
}

std::int32_t APS5_VABI BufferType_nid_no_patch(const void*) {
    return VideoFrameBufferTypeI420;
}

int APS5_VABI BufferWidth_nid_no_patch(const void* buffer) {
    return Object(buffer)->buffer.width;
}

int APS5_VABI BufferHeight_nid_no_patch(const void* buffer) {
    return Object(buffer)->buffer.height;
}

int APS5_VABI BufferStrideY_nid_no_patch(const void* buffer) {
    return Object(buffer)->buffer.strideY;
}

int APS5_VABI BufferStrideU_nid_no_patch(const void* buffer) {
    return Object(buffer)->buffer.strideU;
}

int APS5_VABI BufferStrideV_nid_no_patch(const void* buffer) {
    return Object(buffer)->buffer.strideV;
}

const std::uint8_t* APS5_VABI BufferDataY_nid_no_patch(const void* buffer) {
    return Object(buffer)->buffer.data;
}

const std::uint8_t* APS5_VABI BufferDataU_nid_no_patch(const void* buffer) {
    const WebRtcI420Buffer& planes = Object(buffer)->buffer;
    return planes.data + static_cast<std::ptrdiff_t>(planes.strideY) * planes.height;
}

const std::uint8_t* APS5_VABI BufferDataV_nid_no_patch(const void* buffer) {
    const WebRtcI420Buffer& planes = Object(buffer)->buffer;
    return planes.data + static_cast<std::ptrdiff_t>(planes.strideY) * planes.height + static_cast<std::ptrdiff_t>(planes.strideU) * ((planes.height + 1) / 2);
}

WebRtcGuestString* APS5_VABI StorageRepresentation_nid_no_patch(WebRtcGuestString* result, const void*) {
    result->reserved = 0;
    result->buffer[0] = '?';
    result->buffer[1] = '\0';
    result->size = 1;
    result->capacity = WebRtcGuestStringInlineCapacity;
    return result;
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&AddRefBuffer_nid_no_patch),
    reinterpret_cast<void*>(&ReleaseBuffer_nid_no_patch),
    reinterpret_cast<void*>(&DestroyBuffer_nid_no_patch),
    reinterpret_cast<void*>(&DeleteBuffer_nid_no_patch),
    reinterpret_cast<void*>(&BufferType_nid_no_patch),
    reinterpret_cast<void*>(&BufferWidth_nid_no_patch),
    reinterpret_cast<void*>(&BufferHeight_nid_no_patch),
    reinterpret_cast<void*>(&_ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix),
    reinterpret_cast<void*>(&_ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix),
    reinterpret_cast<void*>(&_ZN6webrtc16VideoFrameBuffer12CropAndScaleEiiiiii_nid_postfix),
    reinterpret_cast<void*>(&_ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix),
    reinterpret_cast<void*>(&StorageRepresentation_nid_no_patch),
    reinterpret_cast<void*>(&_ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix),
    reinterpret_cast<void*>(&_ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix),
    reinterpret_cast<void*>(&BufferStrideY_nid_no_patch),
    reinterpret_cast<void*>(&BufferStrideU_nid_no_patch),
    reinterpret_cast<void*>(&BufferStrideV_nid_no_patch),
    reinterpret_cast<void*>(&BufferDataY_nid_no_patch),
    reinterpret_cast<void*>(&BufferDataU_nid_no_patch),
    reinterpret_cast<void*>(&BufferDataV_nid_no_patch),
    reinterpret_cast<void*>(&HasOneRef_nid_no_patch),
};

static_assert(sizeof(Vtable) / sizeof(Vtable[0]) == 23);

}

extern "C" {

std::uint8_t* APS5_VABI _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(WebRtcI420Buffer* buffer) {
    return Plane(buffer, DataYSlot, "I420Buffer::MutableDataY requires a buffer with a vtable");
}

std::uint8_t* APS5_VABI _ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(WebRtcI420Buffer* buffer) {
    return Plane(buffer, DataUSlot, "I420Buffer::MutableDataU requires a buffer with a vtable");
}

std::uint8_t* APS5_VABI _ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(WebRtcI420Buffer* buffer) {
    return Plane(buffer, DataVSlot, "I420Buffer::MutableDataV requires a buffer with a vtable");
}

WebRtcRefCountedI420Buffer** APS5_VABI _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(WebRtcRefCountedI420Buffer** result, int width, int height, int strideY, int strideU, int strideV) {
    if (!result)
        throw std::invalid_argument("I420Buffer::Create requires a result reference");
    if (width <= 0 || height <= 0 || strideY < width || strideU <= 0 || strideV <= 0)
        NotImplemented_nid_no_patch("I420Buffer::Create with invalid dimensions: M134 RTC_CHECKs them, M127 does not, and the module's revision is unknown");
    const std::int64_t size = static_cast<std::int64_t>(strideY) * height + (static_cast<std::int64_t>(strideU) + strideV) * ((static_cast<std::int64_t>(height) + 1) / 2);
    if (size > std::numeric_limits<int>::max())
        NotImplemented_nid_no_patch("I420Buffer::Create with a size above INT_MAX: M134 RTC_CHECKs it, M127 overflows, and the module's revision is unknown");
    void* storage = _Znwm_nid_postfix(sizeof(WebRtcRefCountedI420Buffer));
    void* data = nullptr;
    try {
        data = AlignedMalloc(static_cast<std::size_t>(size));
    } catch (...) {
        _ZdlPv_nid_postfix(storage);
        throw;
    }
    auto* buffer = new (storage) WebRtcRefCountedI420Buffer{};
    buffer->buffer.vtable = Vtable + 2;
    buffer->buffer.width = width;
    buffer->buffer.height = height;
    buffer->buffer.strideY = strideY;
    buffer->buffer.strideU = strideU;
    buffer->buffer.strideV = strideV;
    buffer->buffer.data = static_cast<std::uint8_t*>(data);
    buffer->refCount.store(1, std::memory_order_relaxed);
    *result = buffer;
    return result;
}

}

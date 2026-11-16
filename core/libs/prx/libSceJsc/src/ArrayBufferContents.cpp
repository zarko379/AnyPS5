// Derived from Source/JavaScriptCore/runtime/ArrayBuffer.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "prx/libSceJsc/include/ArrayBufferContentsExports.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

using Jsc::JscAbi::ArrayBufferContents;
using Jsc::JscAbi::PackedPointerSize;

constexpr unsigned MaxArrayBufferSize = 0x7fffffffu;
constexpr std::size_t TaskRefCountOffset = 8;
constexpr unsigned AddressWidth = 48;

using TaskDeletingDestructor = void (APS5_VABI*)(void*);
using TaskRun = void (APS5_VABI*)(void*, void*);

void* LoadPacked(const std::uint8_t (&storage)[PackedPointerSize]) {
    std::uint64_t value = 0;
    std::memcpy(&value, storage, PackedPointerSize);
    constexpr unsigned shift = 64 - AddressWidth;
    value = static_cast<std::uint64_t>(static_cast<std::int64_t>(value << shift) >> shift);
    return reinterpret_cast<void*>(value);
}

void StorePacked(std::uint8_t (&storage)[PackedPointerSize], void* pointer) {
    const auto value = reinterpret_cast<std::uint64_t>(pointer);
    std::memcpy(storage, &value, PackedPointerSize);
}

std::atomic_ref<unsigned> TaskRefCount(void* task) {
    return std::atomic_ref<unsigned>(*reinterpret_cast<unsigned*>(static_cast<std::uint8_t*>(task) + TaskRefCountOffset));
}

std::atomic_ref<unsigned> SharedRefCount(void* shared) {
    return std::atomic_ref<unsigned>(*static_cast<unsigned*>(shared));
}

void* const* Vtable(void* object) {
    return *static_cast<void* const* const*>(object);
}

void DerefTask(void* task) {
    if (!task) return;
    auto count = TaskRefCount(task);
    if (--count) return;
    count = 1;
    reinterpret_cast<TaskDeletingDestructor>(Vtable(task)[1])(task);
}

void RefShared(void* shared) {
    if (shared) ++SharedRefCount(shared);
}

void DerefShared(void* shared) {
    if (!shared) return;
    --SharedRefCount(shared);
}

void RequireSharedSurvives(void* shared, const char* function) {
    if (shared && SharedRefCount(shared).load() == 1) NotImplemented_nid_no_patch(function);
}

void Destroy(ArrayBufferContents& contents) {
    if (void* task = LoadPacked(contents.destructor))
        reinterpret_cast<TaskRun>(Vtable(task)[2])(task, contents.data);
}

void Reset(ArrayBufferContents& contents) {
    contents.data = nullptr;
    void* task = LoadPacked(contents.destructor);
    StorePacked(contents.destructor, nullptr);
    DerefTask(task);
    void* shared = LoadPacked(contents.shared);
    StorePacked(contents.shared, nullptr);
    DerefShared(shared);
    contents.sizeInBytes = 0;
}

void Clear(ArrayBufferContents& contents) {
    Destroy(contents);
    Reset(contents);
}

void TransferTo(ArrayBufferContents& source, ArrayBufferContents& target) {
    if (source.sizeInBytes > MaxArrayBufferSize) throw std::length_error("JSC::ArrayBufferContents: size above MAX_ARRAY_BUFFER_SIZE");
    Clear(target);
    target.data = source.data;
    target.sizeInBytes = source.sizeInBytes;
    void* task = LoadPacked(source.destructor);
    StorePacked(source.destructor, nullptr);
    StorePacked(target.destructor, task);
    void* shared = LoadPacked(source.shared);
    RefShared(shared);
    StorePacked(target.shared, shared);
    Reset(source);
}

ArrayBufferContents& Checked(ArrayBufferContents* contents) {
    if (!contents) throw std::invalid_argument("JSC::ArrayBufferContents requires an object");
    return *contents;
}

void Construct(ArrayBufferContents* self) {
    auto& contents = Checked(self);
    contents.data = nullptr;
    StorePacked(contents.destructor, nullptr);
    StorePacked(contents.shared, nullptr);
    contents.sizeInBytes = 0;
}

void MoveConstruct(ArrayBufferContents* self, ArrayBufferContents* other) {
    auto& source = Checked(other);
    Construct(self);
    TransferTo(source, *self);
}

void DestroyMembers(ArrayBufferContents* self, const char* function) {
    auto& contents = Checked(self);
    RequireSharedSurvives(LoadPacked(contents.shared), function);
    Destroy(contents);
    DerefShared(LoadPacked(contents.shared));
    DerefTask(LoadPacked(contents.destructor));
}

}

extern "C" {

void APS5_VABI _ZN3JSC19ArrayBufferContentsC1Ev(ArrayBufferContents* self) {
    Construct(self);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsC2Ev(ArrayBufferContents* self) {
    Construct(self);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsC1EOS0_(ArrayBufferContents* self, ArrayBufferContents* other) {
    MoveConstruct(self, other);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsC2EOS0_(ArrayBufferContents* self, ArrayBufferContents* other) {
    MoveConstruct(self, other);
}

ArrayBufferContents* APS5_VABI _ZN3JSC19ArrayBufferContentsaSEOS0_(ArrayBufferContents* self, ArrayBufferContents* other) {
    auto& target = Checked(self);
    auto& source = Checked(other);
    RequireSharedSurvives(LoadPacked(target.shared), __func__);
    TransferTo(source, target);
    return self;
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsD1Ev(ArrayBufferContents* self) {
    DestroyMembers(self, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsD2Ev(ArrayBufferContents* self) {
    DestroyMembers(self, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContents5clearEv(ArrayBufferContents* self) {
    auto& contents = Checked(self);
    RequireSharedSurvives(LoadPacked(contents.shared), __func__);
    Clear(contents);
}

}

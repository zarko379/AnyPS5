// Derived from Source/JavaScriptCore/heap/Strong.h@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/JavaScriptCore/heap/Strong.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libSceJsc/src/JscHandleSet.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Jsc::JscAbi::HandleNode;
using Jsc::JscAbi::HandleSet;

std::uint64_t* Slot(const void* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires a ScriptValue");
    std::uint64_t* slot = nullptr;
    std::memcpy(&slot, value, sizeof(slot));
    return slot;
}

void SetSlot(void* value, std::uint64_t* slot) {
    std::memcpy(value, &slot, sizeof(slot));
}

void Clear(void* self, const char* function) {
    std::uint64_t* slot = Slot(self, function);
    if (!slot) return;
    HandleNode* node = Jsc::JscHandles::NodeFor(slot, function);
    Jsc::JscHandles::Deallocate(*Jsc::JscHandles::HeapFor(node, function), node, function);
    SetSlot(self, nullptr);
}

void Store(std::uint64_t* slot, std::uint64_t value, const char* function) {
    HandleNode* node = Jsc::JscHandles::NodeFor(slot, function);
    Jsc::JscHandles::WriteBarrier(*Jsc::JscHandles::HeapFor(node, function), node, value, function);
    node->value = value;
}

std::uint64_t* Allocate(HandleSet& set, const char* function) {
    if (!set.freeHead) NotImplemented_nid_no_patch(function);
    Jsc::JscHandles::RequireList(set.immediateHead, function);
    HandleNode* node = set.freeHead;
    set.freeHead = node->next;
    node->value = 0;
    node->prev = nullptr;
    node->next = nullptr;
    Jsc::JscHandles::Push(set.immediateHead, node);
    return &node->value;
}

void Copy(void* self, const void* other, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires a ScriptValue");
    std::uint64_t* source = Slot(other, function);
    if (!source) {
        SetSlot(self, nullptr);
        return;
    }
    HandleNode* sourceNode = Jsc::JscHandles::NodeFor(source, function);
    HandleSet* set = Jsc::JscHandles::HeapFor(sourceNode, function);
    const std::uint64_t value = sourceNode->value;
    std::uint64_t* slot = Allocate(*set, function);
    SetSlot(self, slot);
    Store(slot, value, function);
}

void* Assign(void* self, const void* other, const char* function) {
    std::uint64_t* source = Slot(other, function);
    std::uint64_t* target = Slot(self, function);
    if (!source) {
        Clear(self, function);
        return self;
    }
    HandleNode* sourceNode = Jsc::JscHandles::NodeFor(source, function);
    HandleSet* set = Jsc::JscHandles::HeapFor(sourceNode, function);
    const std::uint64_t value = sourceNode->value;
    if (!target) {
        target = Allocate(*set, function);
        SetSlot(self, target);
    }
    Store(target, value, function);
    return self;
}

} // namespace

extern "C" {

void APS5_VABI _ZN10Deprecated11ScriptValueC1ERKS0_(void* self, const void* other) {
    Copy(self, other, __func__);
}

void APS5_VABI _ZN10Deprecated11ScriptValueC2ERKS0_(void* self, const void* other) {
    Copy(self, other, __func__);
}

void APS5_VABI _ZN10Deprecated11ScriptValueD1Ev(void* self) {
    Clear(self, __func__);
}

void APS5_VABI _ZN10Deprecated11ScriptValueD2Ev(void* self) {
    Clear(self, __func__);
}

void* APS5_VABI _ZN10Deprecated11ScriptValueaSERKS0_(void* self, const void* other) {
    return Assign(self, other, __func__);
}

}

// Derived from Source/JavaScriptCore/heap/HandleSet.h@c55364d53f971f63a709976f168d932d558b8909
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
 */

#pragma once

#include "prx/libSceJsc/include/JscHandleExports.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace Jsc::JscHandles {

using Jsc::JscAbi::HandleNode;
using Jsc::JscAbi::HandleSet;

inline constexpr std::uint64_t NotCellMask = 0xfffe000000000002ull;

inline bool IsCell(std::uint64_t value) {
    return !(value & NotCellMask);
}

inline HandleNode* NodeFor(std::uint64_t* slot, const char* function) {
    if (!slot) throw std::invalid_argument(std::string(function) + " requires a handle slot");
    return reinterpret_cast<HandleNode*>(slot);
}

inline HandleSet* HeapFor(HandleNode* node, const char* function) {
    const auto block = reinterpret_cast<std::uintptr_t>(node) & ~static_cast<std::uintptr_t>(Jsc::JscAbi::HandleBlockSize - 1);
    HandleSet* set = nullptr;
    std::memcpy(&set, reinterpret_cast<const unsigned char*>(block) + offsetof(Jsc::JscAbi::HandleBlock, handleSet), sizeof(set));
    if (!set) throw std::invalid_argument(std::string(function) + " requires a handle block with a HandleSet");
    return set;
}

inline void RequireLinked(const HandleNode* node, const char* function) {
    if (!node->prev || !node->next) throw std::invalid_argument(std::string(function) + " requires a handle on a list");
}

inline void RequireList(const HandleNode& head, const char* function) {
    if (!head.next) throw std::invalid_argument(std::string(function) + " requires an initialized handle list");
}

inline void Remove(HandleNode* node) {
    HandleNode* prev = node->prev;
    HandleNode* next = node->next;
    prev->next = next;
    next->prev = prev;
    node->prev = nullptr;
    node->next = nullptr;
}

inline void Push(HandleNode& head, HandleNode* node) {
    HandleNode* prev = &head;
    HandleNode* next = head.next;
    node->prev = prev;
    node->next = next;
    prev->next = node;
    next->prev = node;
}

inline void WriteBarrier(HandleSet& set, HandleNode* node, std::uint64_t value, const char* function) {
    const std::uint64_t current = node->value;
    if (!value == !current && IsCell(current) == IsCell(value)) return;
    HandleNode& head = !value || !IsCell(value) ? set.immediateHead : set.strongHead;
    RequireLinked(node, function);
    RequireList(head, function);
    Remove(node);
    Push(head, node);
}

inline void Deallocate(HandleSet& set, HandleNode* node, const char* function) {
    RequireLinked(node, function);
    Remove(node);
    node->next = set.freeHead;
    set.freeHead = node;
}

} // namespace Jsc::JscHandles

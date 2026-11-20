// Derived from Source/JavaScriptCore/inspector/ScriptCallStack.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (c) 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceJsc/include/InspectorExports.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Jsc::InspectorAbi::FrameVector;
using Jsc::InspectorAbi::ScriptCallStack;

void Construct(ScriptCallStack* self, FrameVector* frames, const char* function) {
    if (!self || !frames) throw std::invalid_argument(std::string(function) + " requires a ScriptCallStack and a Vector");
    self->refCount = 1;
    self->frames = FrameVector{nullptr, 0, 0};
    std::swap(self->frames.buffer, frames->buffer);
    std::swap(self->frames.capacity, frames->capacity);
    std::swap(self->frames.size, frames->size);
}

} // namespace

extern "C" {

void APS5_VABI _ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    ScriptCallStack* self, FrameVector* frames) {
    Construct(self, frames, __func__);
}

void APS5_VABI _ZN9Inspector15ScriptCallStackC2ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    ScriptCallStack* self, FrameVector* frames) {
    Construct(self, frames, __func__);
}

}

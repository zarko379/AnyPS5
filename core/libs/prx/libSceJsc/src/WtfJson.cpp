// Derived from Source/WTF/wtf/JSONValues.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "prx/libSceJsc/include/WtfJsonExports.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>

namespace {

using namespace Jsc::WtfAbi;

constexpr unsigned StringRefCountIncrement = 2;

template<typename T>
T& Checked(T* pointer, const char* message) {
    if (!pointer) throw std::invalid_argument(message);
    return *pointer;
}

void DerefValue(JsonValue* value) {
    if (!value) return;
    const unsigned next = value->refCount - 1;
    if (!next) {
        value->vptr->deletingDestructor(value);
        return;
    }
    value->refCount = next;
}

}

extern "C" {

bool APS5_VABI _ZNK3WTF8JSONImpl5Value9asBooleanERb(const JsonValue* self, bool* output) {
    const JsonValue& value = Checked(self, "WTF::JSONImpl::Value::asBoolean requires an object");
    if (value.type != JsonType::Boolean) return false;
    Checked(output, "WTF::JSONImpl::Value::asBoolean requires an output") = value.data.boolean;
    return true;
}

bool APS5_VABI _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(const JsonValue* self, String* output) {
    const JsonValue& value = Checked(self, "WTF::JSONImpl::Value::asString requires an object");
    if (value.type != JsonType::String) return false;
    String& target = Checked(output, "WTF::JSONImpl::Value::asString requires an output");
    auto* incoming = const_cast<StringImpl*>(value.data.string);
    auto* outgoing = const_cast<StringImpl*>(target.impl);
    if (outgoing && outgoing != incoming && outgoing->refCount == StringRefCountIncrement) NotImplemented_nid_no_patch(__func__);
    if (incoming) incoming->refCount += StringRefCountIncrement;
    target.impl = incoming;
    if (outgoing) outgoing->refCount -= StringRefCountIncrement;
    return true;
}

bool APS5_VABI _ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(JsonValue* self, JsonRefPtr* output) {
    JsonValue& value = Checked(self, "WTF::JSONImpl::Value::asValue requires an object");
    JsonRefPtr& target = Checked(output, "WTF::JSONImpl::Value::asValue requires an output");
    ++value.refCount;
    JsonValue* outgoing = target.ptr;
    target.ptr = &value;
    DerefValue(outgoing);
    return true;
}

unsigned APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase6lengthEv(const JsonArrayBase* self) {
    return Checked(self, "WTF::JSONImpl::ArrayBase::length requires an object").size;
}

const JsonRefPtr* APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3endEv(const JsonArrayBase* self) {
    const JsonArrayBase& array = Checked(self, "WTF::JSONImpl::ArrayBase::end requires an object");
    return array.buffer + array.size;
}

}

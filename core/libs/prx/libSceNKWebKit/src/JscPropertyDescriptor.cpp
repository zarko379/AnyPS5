// Derived from Source/JavaScriptCore/runtime/PropertyDescriptor.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/PropertyDescriptorExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::JscAbi::PropertyDescriptor;

constexpr unsigned ReadOnly = 1u << 1;
constexpr unsigned DontEnum = 1u << 2;
constexpr unsigned DontDelete = 1u << 3;
constexpr unsigned Accessor = 1u << 4;
constexpr unsigned WritablePresent = 1;
constexpr unsigned EnumerablePresent = 2;
constexpr unsigned ConfigurablePresent = 4;
constexpr std::uint64_t ValueUndefined = 0x2 | 0x8;

template<class Descriptor>
Descriptor& Checked(Descriptor* self) {
    if (!self) throw std::invalid_argument("null JSC::PropertyDescriptor");
    return *self;
}

void SetFlag(PropertyDescriptor& descriptor, unsigned attribute, bool clear, unsigned present) {
    if (clear)
        descriptor.attributes &= ~attribute;
    else
        descriptor.attributes |= attribute;
    descriptor.seenAttributes |= present;
}

}

extern "C" {

bool APS5_VABI _ZNK3JSC18PropertyDescriptor8writableEv(const PropertyDescriptor* self) {
    return !(Checked(self).attributes & ReadOnly);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor10enumerableEv(const PropertyDescriptor* self) {
    return !(Checked(self).attributes & DontEnum);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor12configurableEv(const PropertyDescriptor* self) {
    return !(Checked(self).attributes & DontDelete);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(const PropertyDescriptor* self) {
    const auto& descriptor = Checked(self);
    return descriptor.value || (descriptor.seenAttributes & WritablePresent);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(const PropertyDescriptor* self) {
    const auto& descriptor = Checked(self);
    return descriptor.getter || descriptor.setter;
}

void APS5_VABI _ZN3JSC18PropertyDescriptor12setUndefinedEv(PropertyDescriptor* self) {
    auto& descriptor = Checked(self);
    descriptor.value = ValueUndefined;
    descriptor.attributes = ReadOnly | DontDelete | DontEnum;
}

void APS5_VABI _ZN3JSC18PropertyDescriptor11setWritableEb(PropertyDescriptor* self, bool writable) {
    SetFlag(Checked(self), ReadOnly, writable, WritablePresent);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor13setEnumerableEb(PropertyDescriptor* self, bool enumerable) {
    SetFlag(Checked(self), DontEnum, enumerable, EnumerablePresent);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor15setConfigurableEb(PropertyDescriptor* self, bool configurable) {
    SetFlag(Checked(self), DontDelete, configurable, ConfigurablePresent);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(PropertyDescriptor* self, std::uint64_t setter) {
    auto& descriptor = Checked(self);
    descriptor.setter = setter;
    descriptor.attributes |= Accessor;
    descriptor.attributes &= ~ReadOnly;
}

void APS5_VABI _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(PropertyDescriptor* self, std::uint64_t getter) {
    auto& descriptor = Checked(self);
    descriptor.getter = getter;
    descriptor.attributes |= Accessor;
    descriptor.attributes &= ~ReadOnly;
}

}

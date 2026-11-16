#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PropertyDescriptorExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::JscAbi::PropertyDescriptor;

constexpr unsigned ReadOnly = 1u << 1;
constexpr unsigned DontEnum = 1u << 2;
constexpr unsigned DontDelete = 1u << 3;
constexpr unsigned Accessor = 1u << 4;
constexpr unsigned CustomAccessor = 1u << 5;
constexpr std::uint64_t Undefined = 0xa;
constexpr std::uint64_t Null = 0x2;
constexpr std::uint64_t Int32One = 0xfffe000000000001ull;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PropertyDescriptor check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

PropertyDescriptor Default() {
    return PropertyDescriptor{0, 0, 0, DontDelete | DontEnum | ReadOnly, 0};
}

void TestQueries() {
    auto descriptor = Default();
    Require(!_ZNK3JSC18PropertyDescriptor8writableEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor10enumerableEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor12configurableEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(&descriptor));

    descriptor.attributes = 0;
    Require(_ZNK3JSC18PropertyDescriptor8writableEv(&descriptor));
    Require(_ZNK3JSC18PropertyDescriptor10enumerableEv(&descriptor));
    Require(_ZNK3JSC18PropertyDescriptor12configurableEv(&descriptor));

    descriptor.value = Int32One;
    Require(_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    descriptor.value = 0;
    descriptor.seenAttributes = 1;
    Require(_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    descriptor.seenAttributes = 6;
    Require(!_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));

    descriptor.setter = Undefined;
    Require(_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(&descriptor));
    descriptor.setter = 0;
    descriptor.getter = Null;
    Require(_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(&descriptor));
}

void TestSetters() {
    auto descriptor = Default();
    _ZN3JSC18PropertyDescriptor11setWritableEb(&descriptor, true);
    Require(descriptor.attributes == (DontDelete | DontEnum) && descriptor.seenAttributes == 1);
    _ZN3JSC18PropertyDescriptor11setWritableEb(&descriptor, false);
    Require(descriptor.attributes == (DontDelete | DontEnum | ReadOnly) && descriptor.seenAttributes == 1);
    _ZN3JSC18PropertyDescriptor13setEnumerableEb(&descriptor, true);
    Require(descriptor.attributes == (DontDelete | ReadOnly) && descriptor.seenAttributes == 3);
    _ZN3JSC18PropertyDescriptor13setEnumerableEb(&descriptor, false);
    Require(descriptor.attributes == (DontDelete | DontEnum | ReadOnly));
    _ZN3JSC18PropertyDescriptor15setConfigurableEb(&descriptor, true);
    Require(descriptor.attributes == (DontEnum | ReadOnly) && descriptor.seenAttributes == 7);
    _ZN3JSC18PropertyDescriptor15setConfigurableEb(&descriptor, false);
    Require(descriptor.attributes == (DontDelete | DontEnum | ReadOnly) && descriptor.seenAttributes == 7);

    auto accessor = PropertyDescriptor{Int32One, 0, 0, ReadOnly | CustomAccessor, 2};
    _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(&accessor, Null);
    Require(accessor.getter == Null && accessor.setter == 0);
    Require(accessor.attributes == (Accessor | CustomAccessor));
    Require(accessor.value == Int32One && accessor.seenAttributes == 2);
    accessor.attributes |= ReadOnly;
    _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(&accessor, Undefined);
    Require(accessor.setter == Undefined && accessor.getter == Null);
    Require(accessor.attributes == (Accessor | CustomAccessor));

    auto undefinedValue = PropertyDescriptor{Int32One, Null, Null, Accessor, 5};
    _ZN3JSC18PropertyDescriptor12setUndefinedEv(&undefinedValue);
    Require(undefinedValue.value == Undefined);
    Require(undefinedValue.attributes == (ReadOnly | DontDelete | DontEnum));
    Require(undefinedValue.getter == Null && undefinedValue.setter == Null && undefinedValue.seenAttributes == 5);
}

void TestNullDescriptor() {
    RequireInvalidArgument([] { _ZNK3JSC18PropertyDescriptor8writableEv(nullptr); });
    RequireInvalidArgument([] { _ZNK3JSC18PropertyDescriptor10enumerableEv(nullptr); });
    RequireInvalidArgument([] { _ZNK3JSC18PropertyDescriptor12configurableEv(nullptr); });
    RequireInvalidArgument([] { _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(nullptr); });
    RequireInvalidArgument([] { _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(nullptr); });
    RequireInvalidArgument([] { _ZN3JSC18PropertyDescriptor12setUndefinedEv(nullptr); });
    RequireInvalidArgument([] { _ZN3JSC18PropertyDescriptor11setWritableEb(nullptr, true); });
    RequireInvalidArgument([] { _ZN3JSC18PropertyDescriptor13setEnumerableEb(nullptr, true); });
    RequireInvalidArgument([] { _ZN3JSC18PropertyDescriptor15setConfigurableEb(nullptr, true); });
    RequireInvalidArgument([] { _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(nullptr, Undefined); });
    RequireInvalidArgument([] { _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(nullptr, Undefined); });
}

}

int main() {
    TestQueries();
    TestSetters();
    TestNullDescriptor();
    return 0;
}

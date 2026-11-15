#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::WtfAbi::JsonArrayBase;
using Jsc::WtfAbi::JsonRefPtr;
using Jsc::WtfAbi::JsonType;
using Jsc::WtfAbi::JsonValue;
using Jsc::WtfAbi::JsonValueVtable;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;

using AsBooleanFunction = bool (APS5_VABI*)(const JsonValue*, bool*);
using AsStringFunction = bool (APS5_VABI*)(const JsonValue*, String*);
using AsValueFunction = bool (APS5_VABI*)(JsonValue*, JsonRefPtr*);
using LengthFunction = unsigned (APS5_VABI*)(const JsonArrayBase*);
using EndFunction = const JsonRefPtr* (APS5_VABI*)(const JsonArrayBase*);

static_assert(std::is_same_v<decltype(&_ZNK3WTF8JSONImpl5Value9asBooleanERb), AsBooleanFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE), AsStringFunction>);
static_assert(std::is_same_v<decltype(&_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE), AsValueFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3WTF8JSONImpl9ArrayBase6lengthEv), LengthFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3WTF8JSONImpl9ArrayBase3endEv), EndFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF JSON check failed at line %d\n", line);
        std::abort();
    }
}

template <typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

JsonValue* deleted = nullptr;
int deleteCount = 0;
int completeCount = 0;

void APS5_VABI CompleteDestructor(JsonValue*) {
    ++completeCount;
}

void APS5_VABI DeletingDestructor(JsonValue* value) {
    deleted = value;
    ++deleteCount;
}

const JsonValueVtable vtable{CompleteDestructor, DeletingDestructor};

JsonValue Make(JsonType type, unsigned refCount = 1) {
    JsonValue value{};
    value.vptr = &vtable;
    value.refCount = refCount;
    value.type = type;
    value.data.bits = 0xA5A5A5A5A5A5A5A5ull;
    return value;
}

void TestAsBoolean() {
    for (bool expected : {false, true}) {
        JsonValue value = Make(JsonType::Boolean);
        value.data.bits = 0;
        value.data.boolean = expected;
        bool output = !expected;
        Check(_ZNK3WTF8JSONImpl5Value9asBooleanERb(&value, &output), __LINE__);
        Check(output == expected, __LINE__);
        Check(value.refCount == 1, __LINE__);
    }
    for (JsonType type : {JsonType::Null, JsonType::Double, JsonType::Integer, JsonType::String, JsonType::Object, JsonType::Array}) {
        JsonValue value = Make(type);
        bool output = true;
        Check(!_ZNK3WTF8JSONImpl5Value9asBooleanERb(&value, &output), __LINE__);
        Check(output, __LINE__);
        Check(!_ZNK3WTF8JSONImpl5Value9asBooleanERb(&value, nullptr), __LINE__);
    }
    JsonValue boolean = Make(JsonType::Boolean);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF8JSONImpl5Value9asBooleanERb(&boolean, nullptr); }), __LINE__);
    bool output = false;
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF8JSONImpl5Value9asBooleanERb(nullptr, &output); }), __LINE__);
}

void TestAsString() {
    StringImpl text{4, 3, "abc", Jsc::WtfAbi::StringImplFlag8Bit};
    StringImpl old{6, 3, "old", Jsc::WtfAbi::StringImplFlag8Bit};
    StringImpl staticText{3, 1, "s", Jsc::WtfAbi::StringImplFlag8Bit};
    StringImpl last{2, 4, "last", Jsc::WtfAbi::StringImplFlag8Bit};

    JsonValue value = Make(JsonType::String);
    value.data.string = &text;

    String output{nullptr};
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == &text && text.refCount == 6, __LINE__);
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == &text && text.refCount == 6, __LINE__);

    output.impl = &old;
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == &text && text.refCount == 8 && old.refCount == 4, __LINE__);

    output.impl = &staticText;
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == &text && text.refCount == 10 && staticText.refCount == 1, __LINE__);

    JsonValue nullString = Make(JsonType::String);
    nullString.data.string = nullptr;
    output.impl = &old;
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&nullString, &output), __LINE__);
    Check(output.impl == nullptr && old.refCount == 2, __LINE__);

    last.refCount = 2;
    output.impl = &last;
    value.data.string = &last;
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == &last && last.refCount == 2, __LINE__);

    value.data.string = &text;
    const unsigned textBefore = text.refCount;
    Check(Throws<std::runtime_error>([&] { _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output); }), __LINE__);
    Check(output.impl == &last && last.refCount == 2 && text.refCount == textBefore, __LINE__);

    for (JsonType type : {JsonType::Null, JsonType::Boolean, JsonType::Double, JsonType::Integer, JsonType::Object, JsonType::Array}) {
        JsonValue other = Make(type);
        String untouched{&old};
        Check(!_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&other, &untouched), __LINE__);
        Check(untouched.impl == &old && old.refCount == 2, __LINE__);
        Check(!_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&other, nullptr), __LINE__);
    }

    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(nullptr, &output); }), __LINE__);
}

void TestAsValue() {
    JsonValue value = Make(JsonType::Integer, 1);
    JsonRefPtr output{nullptr};
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&value, &output), __LINE__);
    Check(output.ptr == &value && value.refCount == 2, __LINE__);
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&value, &output), __LINE__);
    Check(output.ptr == &value && value.refCount == 2 && deleteCount == 0, __LINE__);

    JsonValue shared = Make(JsonType::Null, 3);
    output.ptr = &shared;
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&value, &output), __LINE__);
    Check(output.ptr == &value && value.refCount == 3 && shared.refCount == 2 && deleteCount == 0, __LINE__);

    JsonValue lastRef = Make(JsonType::Array, 1);
    output.ptr = &lastRef;
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&value, &output), __LINE__);
    Check(output.ptr == &value && value.refCount == 4, __LINE__);
    Check(deleteCount == 1 && deleted == &lastRef && completeCount == 0 && lastRef.refCount == 1, __LINE__);

    JsonValue alone = Make(JsonType::Boolean, 1);
    JsonRefPtr selfOutput{&alone};
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&alone, &selfOutput), __LINE__);
    Check(selfOutput.ptr == &alone && alone.refCount == 1 && deleteCount == 1, __LINE__);

    Check(Throws<std::invalid_argument>([&] { _ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&value, nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(nullptr, &output); }), __LINE__);
    Check(value.refCount == 4, __LINE__);
}

void TestArrayBase() {
    JsonValue first = Make(JsonType::Null);
    JsonValue second = Make(JsonType::Null);
    JsonRefPtr elements[4]{{&first}, {&second}, {nullptr}, {nullptr}};
    JsonArrayBase array{};
    array.base.vptr = &vtable;
    array.base.refCount = 1;
    array.base.type = JsonType::Array;
    array.buffer = elements;
    array.capacity = 4;
    array.size = 2;
    Check(_ZNK3WTF8JSONImpl9ArrayBase6lengthEv(&array) == 2, __LINE__);
    Check(_ZNK3WTF8JSONImpl9ArrayBase3endEv(&array) == elements + 2, __LINE__);
    array.size = 4;
    Check(_ZNK3WTF8JSONImpl9ArrayBase6lengthEv(&array) == 4, __LINE__);
    Check(_ZNK3WTF8JSONImpl9ArrayBase3endEv(&array) == elements + 4, __LINE__);
    array.buffer = nullptr;
    array.capacity = 0;
    array.size = 0;
    Check(_ZNK3WTF8JSONImpl9ArrayBase6lengthEv(&array) == 0, __LINE__);
    Check(_ZNK3WTF8JSONImpl9ArrayBase3endEv(&array) == nullptr, __LINE__);
    Check(array.base.refCount == 1 && deleteCount == 1, __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF8JSONImpl9ArrayBase6lengthEv(nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF8JSONImpl9ArrayBase3endEv(nullptr); }), __LINE__);
}

}

int main() {
    TestAsBoolean();
    TestAsString();
    TestAsValue();
    TestArrayBase();
    std::puts("WTF JSON tests passed");
    return 0;
}

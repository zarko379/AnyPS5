#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfStringConversionExports.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

struct JsonValue;

using JsonDestructor = void (APS5_VABI*)(JsonValue*);

struct JsonValueVtable {
    JsonDestructor completeDestructor;
    JsonDestructor deletingDestructor;
};

enum class JsonType : int {
    Null = 0,
    Boolean,
    Double,
    Integer,
    String,
    Object,
    Array,
};

union JsonData {
    bool boolean;
    double number;
    const StringImpl* string;
    std::uint64_t bits;
};

struct JsonValue {
    const JsonValueVtable* vptr;
    unsigned refCount;
    JsonType type;
    JsonData data;
};

struct JsonRefPtr {
    JsonValue* ptr;
};

struct JsonArrayBase {
    JsonValue base;
    JsonRefPtr* buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(JsonValueVtable) == 16);
static_assert(sizeof(JsonType) == 4);
static_assert(sizeof(JsonValue) == 24);
static_assert(offsetof(JsonValue, vptr) == 0);
static_assert(offsetof(JsonValue, refCount) == 8);
static_assert(offsetof(JsonValue, type) == 12);
static_assert(offsetof(JsonValue, data) == 16);
static_assert(sizeof(JsonRefPtr) == 8);
static_assert(sizeof(JsonArrayBase) == 40);
static_assert(offsetof(JsonArrayBase, base) == 0);
static_assert(offsetof(JsonArrayBase, buffer) == 24);
static_assert(offsetof(JsonArrayBase, capacity) == 32);
static_assert(offsetof(JsonArrayBase, size) == 36);

} // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZNK3WTF8JSONImpl5Value9asBooleanERb(const Jsc::WtfAbi::JsonValue* self, bool* output);
bool APS5_VABI _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(const Jsc::WtfAbi::JsonValue* self, Jsc::WtfAbi::String* output);
bool APS5_VABI _ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(Jsc::WtfAbi::JsonValue* self, Jsc::WtfAbi::JsonRefPtr* output);
unsigned APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase6lengthEv(const Jsc::WtfAbi::JsonArrayBase* self);
const Jsc::WtfAbi::JsonRefPtr* APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3endEv(const Jsc::WtfAbi::JsonArrayBase* self);

}

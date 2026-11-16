#include "prx/libSceIcu/ClassIds.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/CoreClasses.hpp"
#include <unicode/udata.h>
#include "util.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU CLASS IDS: %s\n", message);
    std::abort();
}

template<typename TAction>
bool Rejects(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using ClassId = const void* (APS5_VABI*)();

void CheckClassIds() {
    const std::vector<ClassId> ids{
        _ZN6icu_6810Normalizer16getStaticClassIDEv_nid_postfix,         _ZN6icu_6810UnicodeSet16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix,      _ZN6icu_6813ICUServiceKey16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix,      _ZN6icu_6813SimpleFactory16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6813UnicodeFilter16getStaticClassIDEv_nid_postfix,      _ZN6icu_6813UnicodeString16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6814ResourceBundle16getStaticClassIDEv_nid_postfix,     _ZN6icu_6814UnicodeFunctor16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix,    _ZN6icu_6816LocaleKeyFactory16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6817CanonicalIterator16getStaticClassIDEv_nid_postfix,  _ZN6icu_6818UStringEnumeration16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6818UnicodeSetIterator16getStaticClassIDEv_nid_postfix, _ZN6icu_6822RuleBasedBreakIterator16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6822SimpleLocaleKeyFactory16getStaticClassIDEv_nid_postfix, _ZN6icu_6822UCharCharacterIterator16getStaticClassIDEv_nid_postfix,
        _ZN6icu_6823StringCharacterIterator16getStaticClassIDEv_nid_postfix, _ZN6icu_6824ICUResourceBundleFactory16getStaticClassIDEv_nid_postfix,
        _ZN6icu_686Locale16getStaticClassIDEv_nid_postfix,              _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix,
        _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix,             _ZN6icu_689LocaleKey16getStaticClassIDEv_nid_postfix,
        _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix};
    std::set<const void*> seen;
    for (ClassId id : ids) {
        const void* value = id();
        Require(value != nullptr, "a class ID is not null");
        Require(id() == value, "a class ID is stable");
        Require(seen.insert(value).second, "every class has its own ID");
    }
    Require(seen.size() == 25, "25 class IDs");
    const char object[16]{};
    Require(_ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix(),
            "ParsePosition's dynamic ID is its static ID");
    Require(_ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix(),
            "UStack's dynamic ID is its static ID");
    Require(_ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix(object) != _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix(),
            "UStack is told apart from its base");
    Require(Rejects([] { _ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(nullptr); }), "getDynamicClassID rejects a null object");
}

struct FakeErrorCode {
    const void* vtable;
    std::int32_t code;
};

void CheckErrorCode() {
    FakeErrorCode error{reinterpret_cast<const void*>(0x1234), 7};
    Require(std::strcmp(_ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(&error), "U_MEMORY_ALLOCATION_ERROR") == 0, "errorName names the code");
    error.code = -128;
    Require(std::strcmp(_ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(&error), "U_USING_FALLBACK_WARNING") == 0, "errorName names a warning");
    error.code = 15;
    Require(_ZN6icu_689ErrorCode5resetEv_nid_postfix(&error) == 15 && error.code == 0 && error.vtable == reinterpret_cast<const void*>(0x1234),
            "reset returns the old code and clears it");
    Require(_ZN6icu_689ErrorCode5resetEv_nid_postfix(&error) == 0, "a second reset returns zero");
    error.code = 3;
    _ZNK6icu_689ErrorCode13handleFailureEv_nid_postfix(&error);
    _ZN6icu_689ErrorCodeD1Ev_nid_postfix(&error);
    _ZN6icu_689ErrorCodeD2Ev_nid_postfix(&error);
    Require(error.code == 3 && error.vtable == reinterpret_cast<const void*>(0x1234), "handleFailure and the destructors change nothing");
    auto* heapError = static_cast<FakeErrorCode*>(_ZN6icu_687UMemorynwEm_nid_postfix(sizeof(FakeErrorCode)));
    heapError->code = 0;
    _ZN6icu_689ErrorCodeD0Ev_nid_postfix(heapError);
    Require(Rejects([] { _ZN6icu_689ErrorCode5resetEv_nid_postfix(nullptr); }), "reset rejects a null object");
    Require(Rejects([] { _ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(nullptr); }), "errorName rejects a null object");
    Require(Rejects([] { _ZN6icu_689ErrorCodeD0Ev_nid_postfix(nullptr); }), "the deleting destructor rejects a null object");
}

std::set<void*> live;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    void* memory = std::malloc(size ? size : 1);
    live.insert(memory);
    return memory;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void* memory, std::size_t size) {
    live.erase(memory);
    void* result = std::realloc(memory, size ? size : 1);
    live.insert(result);
    return result;
}

void APS5_VABI Release_nid_no_patch(const void*, void* memory) {
    Require(live.erase(memory) == 1, "the library releases only memory it allocated");
    std::free(memory);
}

void CheckUnprintable() {
    for (std::int32_t c = -5; c < 0x11000; ++c)
        Require(_ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(c) == icu::ICU_Utility::isUnprintable(c), "isUnprintable matches ICU");
    Require(_ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(u'~') == 0 && _ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(0x7f) == 1,
            "only printable ASCII is printable");
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckClassIds();
    CheckErrorCode();
    CheckUnprintable();
    Require(live.empty(), "the deleting destructor frees the object");
    return 0;
}

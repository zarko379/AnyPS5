#include "prx/libSceIcu/BaseClasses.hpp"
#include "prx/libSceIcu/ClassIds.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/CoreClasses.hpp"
#include <unicode/appendable.h>
#include <unicode/bytestream.h>
#include <unicode/locid.h>
#include <unicode/uchriter.h>
#include <unicode/udata.h>
#include "bytesinkutil.h"
#include "charstr.h"
#include "dictionarydata.h"
#include "locbased.h"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU BASE CLASSES: %s\n", message);
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

template<typename TAction>
bool RejectsStatus(TAction action) {
    errno = 71;
    bool threw = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    return threw && errno == 71;
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

const void* const Sentinel = reinterpret_cast<const void*>(std::uintptr_t{0x5a5a});

using Destructor = void (APS5_VABI*)(void*);

struct Trio {
    Destructor deleting;
    Destructor complete;
    Destructor base;
    const char* name;
};

void CheckDestructors() {
    const std::vector<Trio> trios{
        {_ZN6icu_6810AppendableD0Ev_nid_postfix, _ZN6icu_6810AppendableD1Ev_nid_postfix, _ZN6icu_6810AppendableD2Ev_nid_postfix, "Appendable"},
        {_ZN6icu_6811ReplaceableD0Ev_nid_postfix, _ZN6icu_6811ReplaceableD1Ev_nid_postfix, _ZN6icu_6811ReplaceableD2Ev_nid_postfix, "Replaceable"},
        {_ZN6icu_6812ResourceSinkD0Ev_nid_postfix, _ZN6icu_6812ResourceSinkD1Ev_nid_postfix, _ZN6icu_6812ResourceSinkD2Ev_nid_postfix, "ResourceSink"},
        {_ZN6icu_6813EventListenerD0Ev_nid_postfix, _ZN6icu_6813EventListenerD1Ev_nid_postfix, _ZN6icu_6813EventListenerD2Ev_nid_postfix, "EventListener"},
        {_ZN6icu_6813ParsePositionD0Ev_nid_postfix, _ZN6icu_6813ParsePositionD1Ev_nid_postfix, _ZN6icu_6813ParsePositionD2Ev_nid_postfix, "ParsePosition"},
        {_ZN6icu_6813ResourceValueD0Ev_nid_postfix, _ZN6icu_6813ResourceValueD1Ev_nid_postfix, _ZN6icu_6813ResourceValueD2Ev_nid_postfix, "ResourceValue"},
        {_ZN6icu_6813UnicodeFilterD0Ev_nid_postfix, _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix, _ZN6icu_6813UnicodeFilterD2Ev_nid_postfix, "UnicodeFilter"},
        {_ZN6icu_6814UnicodeFunctorD0Ev_nid_postfix, _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix, _ZN6icu_6814UnicodeFunctorD2Ev_nid_postfix, "UnicodeFunctor"},
        {_ZN6icu_6815ServiceListenerD0Ev_nid_postfix, _ZN6icu_6815ServiceListenerD1Ev_nid_postfix, _ZN6icu_6815ServiceListenerD2Ev_nid_postfix, "ServiceListener"},
        {_ZN6icu_6817CharacterIteratorD0Ev_nid_postfix, _ZN6icu_6817CharacterIteratorD1Ev_nid_postfix, _ZN6icu_6817CharacterIteratorD2Ev_nid_postfix,
         "CharacterIterator"},
        {_ZN6icu_6817DictionaryMatcherD0Ev_nid_postfix, _ZN6icu_6817DictionaryMatcherD1Ev_nid_postfix, _ZN6icu_6817DictionaryMatcherD2Ev_nid_postfix,
         "DictionaryMatcher"},
        {_ZN6icu_6818CharStringByteSinkD0Ev_nid_postfix, _ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix, _ZN6icu_6818CharStringByteSinkD2Ev_nid_postfix,
         "CharStringByteSink"},
        {_ZN6icu_6820CheckedArrayByteSinkD0Ev_nid_postfix, _ZN6icu_6820CheckedArrayByteSinkD1Ev_nid_postfix, _ZN6icu_6820CheckedArrayByteSinkD2Ev_nid_postfix,
         "CheckedArrayByteSink"},
        {_ZN6icu_6823UnicodeStringAppendableD0Ev_nid_postfix, _ZN6icu_6823UnicodeStringAppendableD1Ev_nid_postfix,
         _ZN6icu_6823UnicodeStringAppendableD2Ev_nid_postfix, "UnicodeStringAppendable"},
        {_ZN6icu_6824ForwardCharacterIteratorD0Ev_nid_postfix, _ZN6icu_6824ForwardCharacterIteratorD1Ev_nid_postfix,
         _ZN6icu_6824ForwardCharacterIteratorD2Ev_nid_postfix, "ForwardCharacterIterator"},
        {_ZN6icu_6828FilteredBreakIteratorBuilderD0Ev_nid_postfix, _ZN6icu_6828FilteredBreakIteratorBuilderD1Ev_nid_postfix,
         _ZN6icu_6828FilteredBreakIteratorBuilderD2Ev_nid_postfix, "FilteredBreakIteratorBuilder"},
        {_ZN6icu_688ByteSinkD0Ev_nid_postfix, _ZN6icu_688ByteSinkD1Ev_nid_postfix, _ZN6icu_688ByteSinkD2Ev_nid_postfix, "ByteSink"}};
    for (const Trio& trio : trios) {
        unsigned char object[32];
        std::memset(object, 0x33, sizeof(object));
        unsigned char before[32];
        std::memcpy(before, object, sizeof(object));
        trio.complete(object);
        trio.base(object);
        Require(std::memcmp(object, before, sizeof(object)) == 0, trio.name);
        void* heap = _ZN6icu_687UMemorynwEm_nid_postfix(32);
        Require(heap != nullptr && live.count(heap) == 1, "the object comes from the library heap");
        trio.deleting(heap);
        Require(live.count(heap) == 0, trio.name);
        Require(Rejects([&] { trio.deleting(nullptr); }), "a deleting destructor rejects a null object");
        Require(Rejects([&] { trio.complete(nullptr); }), "a complete destructor rejects a null object");
        Require(Rejects([&] { trio.base(nullptr); }), "a base destructor rejects a null object");
    }
    unsigned char mixin[16];
    std::memset(mixin, 0x44, sizeof(mixin));
    _ZN6icu_6811SymbolTableD1Ev_nid_postfix(mixin);
    _ZN6icu_6811SymbolTableD2Ev_nid_postfix(mixin);
    _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(mixin);
    _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(mixin);
    _ZThn8_N6icu_6813UnicodeFilterD1Ev_nid_postfix(mixin + 8);
    for (unsigned char byte : mixin) Require(byte == 0x44, "the mixin destructors change nothing");
    Require(Rejects([] { _ZN6icu_6811SymbolTableD1Ev_nid_postfix(nullptr); }), "SymbolTable rejects a null object");
    Require(Rejects([] { _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(nullptr); }), "UnicodeMatcher rejects a null object");
    auto* filter = static_cast<unsigned char*>(_ZN6icu_687UMemorynwEm_nid_postfix(16));
    Require(_ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(filter) == filter + 8, "toMatcher points at the UnicodeMatcher base at offset 8");
    _ZThn8_N6icu_6813UnicodeFilterD0Ev_nid_postfix(filter + 8);
    Require(live.count(filter) == 0, "the thunk deletes the whole UnicodeFilter");
    Require(Rejects([] { _ZThn8_N6icu_6813UnicodeFilterD0Ev_nid_postfix(nullptr); }), "the thunk rejects a null object");
}

void CheckSimpleBodies() {
    unsigned char object[32] = {};
    Require(_ZNK6icu_6811Replaceable11hasMetaDataEv_nid_postfix(object) == 1, "a Replaceable has metadata by default");
    Require(_ZNK6icu_6811Replaceable5cloneEv_nid_postfix(object) == nullptr, "a Replaceable does not clone by default");
    Require(_ZNK6icu_6814UnicodeFunctor9toMatcherEv_nid_postfix(object) == nullptr, "a functor is no matcher by default");
    Require(_ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix(object) == nullptr, "a functor is no replacer by default");
    _ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix(object, Sentinel);
    _ZN6icu_688ByteSink5FlushEv_nid_postfix(object);
    _ZN6icu_6824ForwardCharacterIteratorC2Ev_nid_postfix(object);
    _ZN6icu_6824ForwardCharacterIteratorC2ERKS0__nid_postfix(object, object + 8);
    _ZN6icu_6828FilteredBreakIteratorBuilderC2Ev_nid_postfix(object);
    for (unsigned char byte : object) Require(byte == 0, "the empty bodies change nothing");
    Require(_ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix(object, 99) == 1, "reserveAppendCapacity accepts any capacity");
    Require(Rejects([] { _ZNK6icu_6811Replaceable5cloneEv_nid_postfix(nullptr); }), "clone rejects a null object");
    Require(Rejects([] { _ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(nullptr); }), "toMatcher rejects a null object");
    Require(Rejects([&] { _ZN6icu_6824ForwardCharacterIteratorC2ERKS0__nid_postfix(object, nullptr); }), "the copy constructor rejects a null source");

    Require(_ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix(), "EventListener ID");
    Require(_ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix(),
            "ServiceListener ID");
    Require(_ZNK6icu_6813SimpleFactory17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_6813SimpleFactory16getStaticClassIDEv_nid_postfix(), "SimpleFactory ID");
    Require(_ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix(), "UVector ID");
    Require(_ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix(), "UVector32 ID");
    Require(_ZNK6icu_6818UStringEnumeration17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_6818UStringEnumeration16getStaticClassIDEv_nid_postfix(),
            "UStringEnumeration ID");
    Require(_ZNK6icu_6818UnicodeSetIterator17getDynamicClassIDEv_nid_postfix(object) == _ZN6icu_6818UnicodeSetIterator16getStaticClassIDEv_nid_postfix(),
            "UnicodeSetIterator ID");
    Require(_ZNK6icu_6824ICUResourceBundleFactory17getDynamicClassIDEv_nid_postfix(object) ==
                _ZN6icu_6824ICUResourceBundleFactory16getStaticClassIDEv_nid_postfix(),
            "ICUResourceBundleFactory ID");
    Require(Rejects([] { _ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(nullptr); }), "getDynamicClassID rejects a null object");

    Require(_ZNK6icu_6822BytesDictionaryMatcher7getTypeEv_nid_postfix(object) == icu::DictionaryData::TRIE_TYPE_BYTES, "a bytes matcher has the bytes type");
    Require(_ZNK6icu_6823UCharsDictionaryMatcher7getTypeEv_nid_postfix(object) == icu::DictionaryData::TRIE_TYPE_UCHARS, "a UChars matcher has the UChars type");
    Icu::BytesDictionaryMatcher matcher{Sentinel, nullptr, icu::DictionaryData::TRANSFORM_TYPE_OFFSET | 0x0e00, nullptr};
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x0e01) == 1, "an offset transform subtracts the offset");
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x0e00 + 0xfd) == 0xfd, "the last offset byte is 0xfd");
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x0e00 + 0xfe) == -1, "past 0xfd is no byte");
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x0dff) == -1, "below the offset is no byte");
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x200d) == 0xff, "ZWJ maps to 0xff");
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x200c) == 0xfe, "ZWNJ maps to 0xfe");
    matcher.transformConstant = icu::DictionaryData::TRANSFORM_NONE;
    Require(_ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(&matcher, 0x200d) == 0x200d, "no transform keeps the code point");
    Require(Rejects([] { _ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(nullptr, 1); }), "transform rejects a null matcher");
}

struct HostSink : icu::ByteSink {
    void Append(const char*, int32_t) override {}
};

struct HostAppendable : icu::Appendable {
    UBool appendCodeUnit(char16_t) override { return 1; }
};

void CheckDefaultBuffers() {
    HostSink hostSink;
    HostAppendable hostAppendable;
    unsigned char object[16] = {};
    char scratch[8];
    char16_t scratch16[8];
    for (std::int32_t minimum = -1; minimum <= 9; ++minimum) {
        for (std::int32_t capacity = -1; capacity <= 9; ++capacity) {
            std::int32_t hostCapacity = -5;
            std::int32_t guestCapacity = -5;
            char* host = hostSink.GetAppendBuffer(minimum, 100, scratch, capacity, &hostCapacity);
            char* guest = _ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix(object, minimum, 100, scratch, capacity, &guestCapacity);
            Require(host == guest && hostCapacity == guestCapacity, "ByteSink::GetAppendBuffer matches ICU");
            hostCapacity = guestCapacity = -5;
            char16_t* host16 = hostAppendable.getAppendBuffer(minimum, 100, scratch16, capacity, &hostCapacity);
            char16_t* guest16 = _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(object, minimum, 100, scratch16, capacity, &guestCapacity);
            Require(host16 == guest16 && hostCapacity == guestCapacity, "Appendable::getAppendBuffer matches ICU");
        }
    }
    Require(Rejects([&] { _ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix(object, 1, 1, scratch, 8, nullptr); }), "a null result capacity throws");
    Require(Rejects([&] { std::int32_t c = 0; _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(nullptr, 1, 1, scratch16, 8, &c); }),
            "a null appendable throws");
}

static_assert(sizeof(icu::CheckedArrayByteSink) == sizeof(Icu::CheckedArrayByteSink));

void CheckCheckedArrayByteSink() {
    std::mt19937 random(68);
    for (std::int32_t capacity : {-3, 0, 1, 7, 32}) {
        for (int round = 0; round < 40; ++round) {
            char hostBuffer[40];
            char guestBuffer[40];
            std::memset(hostBuffer, '.', sizeof(hostBuffer));
            std::memset(guestBuffer, '.', sizeof(guestBuffer));
            icu::CheckedArrayByteSink host(hostBuffer, capacity);
            Icu::CheckedArrayByteSink guest{Sentinel, guestBuffer, capacity < 0 ? 0 : capacity, 0, 0, 0};
            for (int step = 0; step < 12; ++step) {
                const int action = static_cast<int>(random() % 5);
                if (action == 0) {
                    host.Reset();
                    Require(_ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix(&guest) == &guest, "Reset returns the sink");
                } else if (action == 1) {
                    const std::int32_t minimum = static_cast<std::int32_t>(random() % 6) - 1;
                    const std::int32_t scratchCapacity = static_cast<std::int32_t>(random() % 6);
                    char hostScratch[8];
                    char guestScratch[8];
                    std::int32_t hostCapacity = -5;
                    std::int32_t guestCapacity = -5;
                    char* hostResult = host.GetAppendBuffer(minimum, 4, hostScratch, scratchCapacity, &hostCapacity);
                    char* guestResult =
                        _ZN6icu_6820CheckedArrayByteSink15GetAppendBufferEiiPciPi_nid_postfix(&guest, minimum, 4, guestScratch, scratchCapacity, &guestCapacity);
                    Require(hostCapacity == guestCapacity, "GetAppendBuffer capacity matches ICU");
                    const bool hostInBuffer = hostResult >= hostBuffer && hostResult < hostBuffer + sizeof(hostBuffer);
                    const bool guestInBuffer = guestResult >= guestBuffer && guestResult < guestBuffer + sizeof(guestBuffer);
                    Require(hostInBuffer == guestInBuffer, "GetAppendBuffer picks the same buffer");
                    if (hostInBuffer) Require(hostResult - hostBuffer == guestResult - guestBuffer, "GetAppendBuffer points at the same offset");
                    else if (hostResult) Require(hostResult == hostScratch && guestResult == guestScratch, "GetAppendBuffer falls back to scratch");
                    else Require(guestResult == nullptr, "GetAppendBuffer refuses the same requests");
                    if (hostInBuffer && hostCapacity > 0) {
                        const std::int32_t count = static_cast<std::int32_t>(random() % (hostCapacity + 1));
                        std::memset(hostResult, 'w', static_cast<std::size_t>(count));
                        std::memset(guestResult, 'w', static_cast<std::size_t>(count));
                        host.Append(hostResult, count);
                        _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(&guest, guestResult, count);
                    }
                } else {
                    const std::int32_t count = static_cast<std::int32_t>(random() % 14) - 2;
                    char bytes[16];
                    for (char& byte : bytes) byte = static_cast<char>('a' + random() % 26);
                    host.Append(bytes, count);
                    _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(&guest, bytes, count);
                }
                Require(host.NumberOfBytesWritten() == guest.size && host.NumberOfBytesAppended() == guest.appended, "the counts match ICU");
                Require((host.Overflowed() != 0) == (guest.overflowed != 0), "the overflow flag matches ICU");
                Require(std::memcmp(hostBuffer, guestBuffer, sizeof(hostBuffer)) == 0, "the bytes match ICU");
                Require(guest.vtable == Sentinel && guest.outbuf == guestBuffer, "the sink keeps its vtable and buffer");
            }
        }
    }
    char buffer[4];
    Icu::CheckedArrayByteSink guest{Sentinel, buffer, 4, 0, INT32_MAX - 2, 0};
    _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(&guest, "abc", 3);
    Require(guest.appended == INT32_MAX && guest.overflowed == 1 && guest.size == 0, "an appended count past INT32_MAX saturates");
    guest = Icu::CheckedArrayByteSink{Sentinel, buffer, 4, 0, 0, 0};
    Require(Rejects([&] { _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(&guest, nullptr, 2); }), "Append rejects NULL bytes it would copy");
    Require(Rejects([] { _ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix(nullptr); }), "Reset rejects a null sink");
    _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(&guest, nullptr, 0);
    Require(guest.appended == 0, "an empty append reads nothing");
}

static_assert(sizeof(icu::CharString) == sizeof(Icu::CharString));

void Init(Icu::CharString& string) {
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(&string.buffer);
    string.len = 0;
    string.buffer.ptr[0] = 0;
}

void CheckCharStringByteSink() {
    std::mt19937 random(71);
    for (int round = 0; round < 30; ++round) {
        icu::CharString hostString;
        icu::CharStringByteSink host(&hostString);
        Icu::CharString guestString;
        Init(guestString);
        Icu::CharStringByteSink guest{Sentinel, &guestString};
        for (int step = 0; step < 10; ++step) {
            if (random() % 3 == 0) {
                const std::int32_t minimum = static_cast<std::int32_t>(random() % 70) - 1;
                const std::int32_t scratchCapacity = static_cast<std::int32_t>(random() % 70);
                std::vector<char> hostScratch(80);
                std::vector<char> guestScratch(80);
                std::int32_t hostCapacity = -5;
                std::int32_t guestCapacity = -5;
                char* hostResult = host.GetAppendBuffer(minimum, 90, hostScratch.data(), scratchCapacity, &hostCapacity);
                char* guestResult =
                    _ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix(&guest, minimum, 90, guestScratch.data(), scratchCapacity, &guestCapacity);
                Require(hostCapacity == guestCapacity, "CharStringByteSink::GetAppendBuffer capacity matches ICU");
                Require((hostResult == nullptr) == (guestResult == nullptr), "CharStringByteSink::GetAppendBuffer refuses the same requests");
                if (hostResult) {
                    Require(hostResult - hostString.data() == guestResult - guestString.buffer.ptr, "the append buffer starts after the string");
                    const std::int32_t count = static_cast<std::int32_t>(random() % (minimum + 1));
                    std::memset(hostResult, 'q', static_cast<std::size_t>(count));
                    std::memset(guestResult, 'q', static_cast<std::size_t>(count));
                    host.Append(hostResult, count);
                    _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(&guest, guestResult, count);
                }
            } else {
                std::string bytes(static_cast<std::size_t>(random() % 30), 'x');
                for (char& byte : bytes) byte = static_cast<char>('a' + random() % 26);
                const std::int32_t length = random() % 4 == 0 ? -1 : static_cast<std::int32_t>(bytes.size());
                host.Append(bytes.c_str(), length);
                _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(&guest, bytes.c_str(), length);
            }
            Require(guestString.len == hostString.length() && std::memcmp(guestString.buffer.ptr, hostString.data(), static_cast<std::size_t>(guestString.len) + 1) == 0,
                    "the appended string matches ICU");
            Require(guest.vtable == Sentinel && guest.dest == &guestString, "the sink keeps its vtable and string");
        }
        _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&guestString.buffer);
    }
    Icu::CharStringByteSink empty{Sentinel, nullptr};
    Require(Rejects([&] { _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(&empty, "a", 1); }), "Append rejects a sink without a string");
    Require(Rejects([] { _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(nullptr, "a", 1); }), "Append rejects a null sink");
    char scratch[4];
    Require(Rejects([&] { _ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix(&empty, 1, 1, scratch, 4, nullptr); }),
            "GetAppendBuffer rejects a null result capacity");
}

static_assert(sizeof(icu::UCharCharacterIterator) >= sizeof(Icu::CharacterIterator));

bool SameIterator(const Icu::CharacterIterator& guest, const icu::CharacterIterator& host) {
    return guest.textLength == host.getLength() && guest.pos == host.getIndex() && guest.begin == host.startIndex() && guest.end == host.endIndex();
}

void CheckCharacterIterators() {
    const std::u16string text(16, u'a');
    for (std::int32_t length = -2; length <= 12; ++length) {
        const std::int32_t hostLength = length < 0 ? 0 : length;
        Icu::CharacterIterator guest{Sentinel, 9, 9, 9, 9};
        _ZN6icu_6817CharacterIteratorC2Ei_nid_postfix(&guest, length);
        Require(SameIterator(guest, icu::UCharCharacterIterator(text.c_str(), hostLength)), "CharacterIterator(length) matches ICU");
        Require(guest.vtable == Sentinel, "a base constructor leaves the vtable to the derived constructor");
        for (std::int32_t position = -3; position <= 15; ++position) {
            _ZN6icu_6817CharacterIteratorC2Eii_nid_postfix(&guest, length, position);
            Require(SameIterator(guest, icu::UCharCharacterIterator(text.c_str(), hostLength, position)), "CharacterIterator(length, position) matches ICU");
            for (std::int32_t begin = -2; begin <= 14; begin += 2) {
                for (std::int32_t end = -2; end <= 14; end += 3) {
                    _ZN6icu_6817CharacterIteratorC2Eiiii_nid_postfix(&guest, length, begin, end, position);
                    Require(SameIterator(guest, icu::UCharCharacterIterator(text.c_str(), hostLength, begin, end, position)),
                            "CharacterIterator(length, begin, end, position) matches ICU");
                }
            }
        }
    }
    Icu::CharacterIterator source{Sentinel, 10, 4, 2, 8};
    Icu::CharacterIterator copy{nullptr, 0, 0, 0, 0};
    _ZN6icu_6817CharacterIteratorC2ERKS0__nid_postfix(&copy, &source);
    Require(copy.vtable == nullptr && copy.textLength == 10 && copy.pos == 4 && copy.begin == 2 && copy.end == 8, "the copy constructor copies the fields");
    Icu::CharacterIterator assigned{nullptr, 1, 1, 1, 1};
    Require(_ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(&assigned, &source) == &assigned, "assignment returns the target");
    Require(assigned.vtable == nullptr && assigned.textLength == 10 && assigned.pos == 4 && assigned.begin == 2 && assigned.end == 8, "assignment copies the fields");
    _ZN6icu_6817CharacterIteratorC2Ev_nid_postfix(&assigned);
    Require(assigned.textLength == 0 && assigned.pos == 0 && assigned.begin == 0 && assigned.end == 0, "the default constructor clears the fields");
    Require(Rejects([&] { _ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(&assigned, nullptr); }), "assignment rejects a null source");
    Require(Rejects([] { _ZN6icu_6817CharacterIteratorC2Ev_nid_postfix(nullptr); }), "a constructor rejects a null object");
}

static_assert(sizeof(icu::Locale) == sizeof(Icu::LocaleFields));

void CheckLocaleBased() {
    char hostValid[160];
    char hostActual[160];
    char guestValid[160];
    char guestActual[160];
    std::memset(hostValid, '#', sizeof(hostValid));
    std::memset(hostActual, '#', sizeof(hostActual));
    std::memset(guestValid, '#', sizeof(guestValid));
    std::memset(guestActual, '#', sizeof(guestActual));
    icu::LocaleBased host(hostValid, hostActual);
    Icu::LocaleBased guest{guestValid, guestActual};
    const std::string longId(200, 'z');
    const char* ids[] = {"en_US", nullptr, "", "de@collation=phonebook", longId.c_str()};
    for (const char* valid : ids) {
        for (const char* actual : ids) {
            host.setLocaleIDs(valid, actual);
            _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(&guest, valid, actual);
            Require(std::memcmp(hostValid, guestValid, sizeof(hostValid)) == 0 && std::memcmp(hostActual, guestActual, sizeof(hostActual)) == 0,
                    "setLocaleIDs(const char*, const char*) writes what ICU writes");
        }
    }
    const icu::Locale french("fr_CA");
    const icu::Locale german("de_DE@calendar=gregorian");
    Require(reinterpret_cast<const Icu::LocaleFields*>(&french)->fullName == french.getName(), "the Locale layout puts fullName at offset 40");
    host.setLocaleIDs(french, german);
    _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(&guest, reinterpret_cast<const Icu::LocaleFields*>(&french),
                                                                      reinterpret_cast<const Icu::LocaleFields*>(&german));
    Require(std::strcmp(guestValid, "fr_CA") == 0 && std::strcmp(guestActual, "de_DE@calendar=gregorian") == 0, "setLocaleIDs(const Locale&) copies the names");
    Require(std::memcmp(hostValid, guestValid, sizeof(hostValid)) == 0 && std::memcmp(hostActual, guestActual, sizeof(hostActual)) == 0,
            "setLocaleIDs(const Locale&) writes what ICU writes");

    const std::int32_t types[] = {ULOC_ACTUAL_LOCALE, ULOC_VALID_LOCALE, 2, 3};
    for (std::int32_t type : types) {
        for (std::int32_t incoming : {0, -128, 1}) {
            UErrorCode hostStatus = static_cast<UErrorCode>(incoming);
            std::int32_t guestStatus = incoming;
            const char* hostId = host.getLocaleID(static_cast<ULocDataLocaleType>(type), hostStatus);
            const char* guestId = _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(&guest, type, &guestStatus);
            Require(guestStatus == hostStatus, "getLocaleID sets the status ICU sets");
            Require((hostId == nullptr && guestId == nullptr) || (hostId == hostValid && guestId == guestValid) || (hostId == hostActual && guestId == guestActual),
                    "getLocaleID returns the buffer ICU returns");
        }
    }
    Require(RejectsStatus([&] {
                _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(&guest, ULOC_VALID_LOCALE,
                                                                                                   reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}));
            }),
            "getLocaleID rejects an unreadable status");
    Require(Rejects([&] { _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(&guest, ULOC_VALID_LOCALE, nullptr); }),
            "getLocaleID rejects a null status");
    Require(Rejects([&] { _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(&guest, nullptr, nullptr); }), "setLocaleIDs rejects a null Locale");
    Icu::LocaleBased unset{nullptr, nullptr};
    Require(Rejects([&] { _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(&unset, "en", nullptr); }), "setLocaleIDs rejects a NULL buffer it would write");
    _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(&unset, nullptr, nullptr);
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckDestructors();
    CheckSimpleBodies();
    CheckDefaultBuffers();
    CheckCheckedArrayByteSink();
    CheckCharStringByteSink();
    CheckCharacterIterators();
    CheckLocaleBased();
    Require(live.empty(), "no library memory leaks");
    std::puts("guest_icu_base_classes passed");
    return 0;
}

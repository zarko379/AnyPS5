#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Jsc::JscAbi::CallData;
using Jsc::JscAbi::CallDataType;
using Jsc::JscAbi::ClassInfo;
using Jsc::JscAbi::FreeCell;
using Jsc::JscAbi::FreeList;
using Jsc::JscAbi::HashTable;
using Jsc::JscAbi::InternalFunction;
using Jsc::JscAbi::ScriptCallArgumentHandler;

using InitializeBumpFunction = void (APS5_VABI*)(FreeList*, char*, unsigned);
using InitializeListFunction = void (APS5_VABI*)(FreeList*, FreeCell*, std::uintptr_t, unsigned);
using HasStaticSetterFunction = bool (APS5_VABI*)(const ClassInfo*);
using GetCallDataFunction = CallData (APS5_VABI*)(void*);
using InternalCallDataFunction = CallData (APS5_VABI*)(InternalFunction*);
using SpeculationFunction = std::uint64_t (APS5_VABI*)(std::uint64_t);
using DestroyFunction = void (APS5_VABI*)(void*);
using ToThisFunction = std::uint64_t (APS5_VABI*)(void*, void*, std::uint8_t);
using ToThisSlowCaseFunction = std::uint64_t (APS5_VABI*)(const std::uint64_t*, void*, std::uint8_t);

static_assert(std::is_same_v<decltype(&_ZN3JSC8FreeList14initializeBumpEPcj), InitializeBumpFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj), InitializeListFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv), HasStaticSetterFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell11getCallDataEPS0_), GetCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell16getConstructDataEPS0_), GetCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE), InternalCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE), InternalCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC20speculationFromValueENS_7JSValueE), SpeculationFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell7destroyEPS0_), DestroyFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE), ToThisFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE), ToThisFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE), ToThisSlowCaseFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC runtime check failed at line %d\n", line);
        std::abort();
    }
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

FreeList Poisoned() {
    FreeList list;
    std::memset(&list, 0xA5, sizeof(list));
    return list;
}

void TestInitializeList() {
    FreeCell cells[2]{};
    FreeList list = Poisoned();
    const std::uintptr_t secret = 0x1234'5678'9ABC'DEF0ull;
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(&list, &cells[1], secret, 4096);
    Check(list.scrambledHead == (reinterpret_cast<std::uintptr_t>(&cells[1]) ^ secret), __LINE__);
    Check(list.secret == secret, __LINE__);
    Check(list.payloadEnd == nullptr, __LINE__);
    Check(list.remaining == 0, __LINE__);
    Check(list.originalSize == 4096, __LINE__);
    Check(list.cellSize == 0xA5A5A5A5u, __LINE__);

    list = Poisoned();
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(&list, nullptr, 0, 0);
    Check(list.scrambledHead == 0 && list.secret == 0 && list.payloadEnd == nullptr, __LINE__);
    Check(list.remaining == 0 && list.originalSize == 0, __LINE__);
    Check(list.cellSize == 0xA5A5A5A5u, __LINE__);

    list = Poisoned();
    _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(&list, nullptr, secret, 0xFFFFFFFFu);
    Check(list.scrambledHead == secret, __LINE__);
    Check(list.originalSize == 0xFFFFFFFFu, __LINE__);

    Check(Throws([&] { _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(nullptr, cells, secret, 16); }), __LINE__);
}

void TestInitializeBump() {
    char payload[64];
    FreeList list = Poisoned();
    _ZN3JSC8FreeList14initializeBumpEPcj(&list, payload + sizeof(payload), 48);
    Check(list.scrambledHead == 0, __LINE__);
    Check(list.secret == 0, __LINE__);
    Check(list.payloadEnd == payload + sizeof(payload), __LINE__);
    Check(list.remaining == 48, __LINE__);
    Check(list.originalSize == 48, __LINE__);
    Check(list.cellSize == 0xA5A5A5A5u, __LINE__);

    list = Poisoned();
    _ZN3JSC8FreeList14initializeBumpEPcj(&list, nullptr, 0);
    Check(list.payloadEnd == nullptr && list.remaining == 0 && list.originalSize == 0, __LINE__);
    Check(list.scrambledHead == 0 && list.secret == 0, __LINE__);

    Check(Throws([&] { _ZN3JSC8FreeList14initializeBumpEPcj(nullptr, payload, 1); }), __LINE__);
}

void TestHasStaticSetterOrReadonlyProperties() {
    HashTable plain{3, 3, false, nullptr, nullptr, nullptr};
    HashTable setter{1, 1, true, nullptr, nullptr, nullptr};
    ClassInfo root{"Root", nullptr, nullptr};
    ClassInfo rootWithPlain{"RootPlain", nullptr, &plain};
    ClassInfo rootWithSetter{"RootSetter", nullptr, &setter};
    ClassInfo childOfSetter{"Child", &rootWithSetter, nullptr};
    ClassInfo grandchildOfSetter{"Grandchild", &childOfSetter, &plain};
    ClassInfo childOfPlain{"ChildPlain", &rootWithPlain, &plain};
    ClassInfo setterOverPlain{"SetterOverPlain", &rootWithPlain, &setter};

    Check(!_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&root), __LINE__);
    Check(!_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&rootWithPlain), __LINE__);
    Check(_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&rootWithSetter), __LINE__);
    Check(_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&childOfSetter), __LINE__);
    Check(_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&grandchildOfSetter), __LINE__);
    Check(!_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&childOfPlain), __LINE__);
    Check(_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(&setterOverPlain), __LINE__);
    Check(Throws([] { _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(nullptr); }), __LINE__);
}

void TestCallData() {
    unsigned char cell[16]{};
    const CallData call = _ZN3JSC6JSCell11getCallDataEPS0_(cell);
    Check(call.type == CallDataType::None, __LINE__);
    Check(call.first == 0, __LINE__);
    Check(call.second == 0, __LINE__);
    const CallData construct = _ZN3JSC6JSCell16getConstructDataEPS0_(cell);
    Check(construct.type == CallDataType::None, __LINE__);
    Check(construct.first == 0, __LINE__);
    const CallData nullCall = _ZN3JSC6JSCell11getCallDataEPS0_(nullptr);
    Check(nullCall.type == CallDataType::None && nullCall.first == 0, __LINE__);
    const CallData nullConstruct = _ZN3JSC6JSCell16getConstructDataEPS0_(nullptr);
    Check(nullConstruct.type == CallDataType::None && nullConstruct.first == 0, __LINE__);
    Check(static_cast<unsigned>(CallDataType::Native) == 1 && static_cast<unsigned>(CallDataType::JS) == 2, __LINE__);
}

template <typename Function>
bool ThrowsRuntime(Function function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

std::uintptr_t ExportedAddress(const char* name) {
#ifdef _WIN32
    const auto address = reinterpret_cast<std::uintptr_t>(GetProcAddress(GetModuleHandleA("libSceJsc.prx"), name));
#else
    const auto address = reinterpret_cast<std::uintptr_t>(dlsym(RTLD_DEFAULT, name));
#endif
    Check(address != 0, __LINE__);
    return address;
}

void TestInternalFunction() {
    InternalFunction function{};
    std::memset(&function, 0xA5, sizeof(function));
    function.functionForCall = 0x1111;
    function.functionForConstruct = 0x2222;
    CallData call = _ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE(&function);
    Check(call.type == CallDataType::Native && call.first == 0x1111 && call.second == 0, __LINE__);
    CallData construct = _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE(&function);
    Check(construct.type == CallDataType::Native && construct.first == 0x2222 && construct.second == 0, __LINE__);
    function.functionForConstruct = 0;
    construct = _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE(&function);
    Check(construct.type == CallDataType::Native && construct.first == 0, __LINE__);
    function.functionForConstruct = ExportedAddress("_ZN3JSC29callHostFunctionAsConstructorEPNS_14JSGlobalObjectEPNS_9CallFrameE");
    construct = _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE(&function);
    Check(construct.type == CallDataType::None && construct.first == 0 && construct.second == 0, __LINE__);
    call = _ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE(&function);
    Check(call.type == CallDataType::Native && call.first == 0x1111, __LINE__);
    Check(Throws([] { _ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE(nullptr); }), __LINE__);
    Check(Throws([] { _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE(nullptr); }), __LINE__);
}

std::uint64_t EncodeDouble(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits + (1ull << 49);
}

void TestSpeculationFromValue() {
    constexpr std::uint64_t NumberTag = 0xfffe000000000000ull;
    constexpr std::uint64_t SpecBoolInt32 = 1ull << 30;
    constexpr std::uint64_t SpecNonBoolInt32 = 1ull << 31;
    constexpr std::uint64_t SpecAnyIntAsDouble = 1ull << 34;
    constexpr std::uint64_t SpecNonIntAsDouble = 1ull << 35;
    constexpr std::uint64_t SpecDoublePureNaN = 1ull << 36;
    constexpr std::uint64_t SpecBoolean = 1ull << 38;
    constexpr std::uint64_t SpecOther = 1ull << 39;
    constexpr std::uint64_t SpecEmpty = 1ull << 40;
    const auto speculate = _ZN3JSC20speculationFromValueENS_7JSValueE;

    Check(speculate(0) == SpecEmpty, __LINE__);
    Check(speculate(NumberTag | 0) == SpecBoolInt32, __LINE__);
    Check(speculate(NumberTag | 1) == SpecBoolInt32, __LINE__);
    Check(speculate(NumberTag | 2) == SpecNonBoolInt32, __LINE__);
    Check(speculate(NumberTag | 0xFFFFFFFFu) == SpecNonBoolInt32, __LINE__);
    Check(speculate(NumberTag | 0x80000000u) == SpecNonBoolInt32, __LINE__);
    Check(speculate(0xffff000000000001ull) == SpecBoolInt32, __LINE__);

    Check(speculate(EncodeDouble(0.0)) == SpecAnyIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(-0.0)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(42.0)) == SpecAnyIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(-7.0)) == SpecAnyIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(0.5)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(2251799813685247.0)) == SpecAnyIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(2251799813685248.0)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(-2251799813685248.0)) == SpecAnyIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(-2251799813685249.0)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(1e300)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(-1.0 / 0.0)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(1.0 / 0.0)) == SpecNonIntAsDouble, __LINE__);
    Check(speculate(EncodeDouble(std::numeric_limits<double>::quiet_NaN())) == SpecDoublePureNaN, __LINE__);

    Check(speculate(0x6) == SpecBoolean, __LINE__);
    Check(speculate(0x7) == SpecBoolean, __LINE__);
    Check(speculate(0x2) == SpecOther, __LINE__);
    Check(speculate(0xA) == SpecOther, __LINE__);

    Check(ThrowsRuntime([&] { speculate(0x10000); }), __LINE__);
}

void TestDestroy() {
    unsigned char cell[16];
    std::memset(cell, 0x5A, sizeof(cell));
    _ZN3JSC6JSCell7destroyEPS0_(cell);
    for (unsigned char byte : cell) Check(byte == 0x5A, __LINE__);
}

void TestToThis() {
    unsigned char cell[16]{};
    unsigned char global[16]{};
    const auto cellBits = reinterpret_cast<std::uint64_t>(cell);
    Check(_ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(cell, global, 0) == cellBits, __LINE__);
    Check(_ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(cell, nullptr, 1) == cellBits, __LINE__);
    Check(_ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(nullptr, global, 1) == 0, __LINE__);

    Check(_ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(cell, global, 0) == 0xA, __LINE__);
    Check(_ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(nullptr, nullptr, 0) == 0xA, __LINE__);
    Check(ThrowsRuntime([&] { _ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(cell, global, 1); }), __LINE__);

    for (std::uint64_t value : {0xfffe000000000005ull, 0x0002000000000000ull, 0x6ull, 0x7ull, 0x2ull, 0xAull}) {
        Check(_ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE(&value, global, 0) == value, __LINE__);
        Check(ThrowsRuntime([&] { _ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE(&value, global, 1); }), __LINE__);
    }
    Check(Throws([&] { _ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE(nullptr, global, 0); }), __LINE__);
}

void TestScriptCallArgumentHandler() {
    using Constructor = void (APS5_VABI*)(ScriptCallArgumentHandler*, void*);
    using Destructor = void (APS5_VABI*)(ScriptCallArgumentHandler*);
    unsigned char global[8]{};
    for (Constructor construct : {_ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE, _ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE}) {
        ScriptCallArgumentHandler handler;
        std::memset(&handler, 0xA5, sizeof(handler));
        construct(&handler, global);
        Check(handler.arguments.overflowed == 0 && handler.arguments.size == 0 && handler.arguments.capacity == 8, __LINE__);
        Check(handler.arguments.buffer == handler.arguments.inlineBuffer && handler.arguments.markSet == nullptr, __LINE__);
        Check(handler.globalObject == global, __LINE__);
        Check(handler.arguments.inlineBuffer[0] == 0xA5A5A5A5A5A5A5A5ull, __LINE__);
    }

    ScriptCallArgumentHandler handler;
    _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(&handler, nullptr);
    Check(handler.globalObject == nullptr, __LINE__);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, true);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, false);
    _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE(&handler, 0xfffe00000000002Aull);
    Check(handler.arguments.size == 3, __LINE__);
    Check(handler.arguments.inlineBuffer[0] == 0x7 && handler.arguments.inlineBuffer[1] == 0x6, __LINE__);
    Check(handler.arguments.inlineBuffer[2] == 0xfffe00000000002Aull, __LINE__);
    for (std::uint64_t value = 3; value < 8; ++value)
        _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE(&handler, value);
    Check(handler.arguments.size == 8 && handler.arguments.inlineBuffer[7] == 7, __LINE__);
    Check(ThrowsRuntime([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&handler, true); }), __LINE__);
    Check(ThrowsRuntime([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE(&handler, 0xA); }), __LINE__);
    Check(handler.arguments.size == 8 && handler.arguments.buffer == handler.arguments.inlineBuffer, __LINE__);

    for (Destructor destroy : {_ZN10Deprecated25ScriptCallArgumentHandlerD1Ev, _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev}) {
        ScriptCallArgumentHandler before = handler;
        destroy(&handler);
        Check(std::memcmp(&before, &handler, sizeof(handler)) == 0, __LINE__);
        std::uint64_t heap[16]{};
        handler.arguments.buffer = heap;
        Check(ThrowsRuntime([&] { destroy(&handler); }), __LINE__);
        handler.arguments.buffer = handler.arguments.inlineBuffer;
        handler.arguments.markSet = global;
        Check(ThrowsRuntime([&] { destroy(&handler); }), __LINE__);
        handler.arguments.markSet = nullptr;
    }

    std::uint64_t heap[16]{};
    ScriptCallArgumentHandler grown;
    _ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE(&grown, global);
    grown.arguments.buffer = heap;
    grown.arguments.capacity = 16;
    Check(ThrowsRuntime([&] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(&grown, true); }), __LINE__);
    Check(grown.arguments.size == 0 && heap[0] == 0, __LINE__);

    Check(Throws([] { _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(nullptr, nullptr); }), __LINE__);
    Check(Throws([] { _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev(nullptr); }), __LINE__);
    Check(Throws([] { _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(nullptr, true); }), __LINE__);
}

}

int main() {
    TestInitializeList();
    TestInitializeBump();
    TestHasStaticSetterOrReadonlyProperties();
    TestCallData();
    TestInternalFunction();
    TestSpeculationFromValue();
    TestDestroy();
    TestToThis();
    TestScriptCallArgumentHandler();
    std::puts("JSC runtime tests passed");
    return 0;
}

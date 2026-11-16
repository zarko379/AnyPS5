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

using InitializeBumpFunction = void (APS5_VABI*)(FreeList*, char*, unsigned);
using InitializeListFunction = void (APS5_VABI*)(FreeList*, FreeCell*, std::uintptr_t, unsigned);
using HasStaticSetterFunction = bool (APS5_VABI*)(const ClassInfo*);
using GetCallDataFunction = CallData (APS5_VABI*)(void*);
using InternalCallDataFunction = CallData (APS5_VABI*)(InternalFunction*);
using SpeculationFunction = std::uint64_t (APS5_VABI*)(std::uint64_t);
using DestroyFunction = void (APS5_VABI*)(void*);

static_assert(std::is_same_v<decltype(&_ZN3JSC8FreeList14initializeBumpEPcj), InitializeBumpFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj), InitializeListFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv), HasStaticSetterFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell11getCallDataEPS0_), GetCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell16getConstructDataEPS0_), GetCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE), InternalCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE), InternalCallDataFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC20speculationFromValueENS_7JSValueE), SpeculationFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell7destroyEPS0_), DestroyFunction>);

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

}

int main() {
    TestInitializeList();
    TestInitializeBump();
    TestHasStaticSetterOrReadonlyProperties();
    TestCallData();
    TestInternalFunction();
    TestSpeculationFromValue();
    TestDestroy();
    std::puts("JSC runtime tests passed");
    return 0;
}

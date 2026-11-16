#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
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

using Jsc::JscAbi::HandleBlock;
using Jsc::JscAbi::HandleNode;
using Jsc::JscAbi::HandleSet;
using Jsc::JscAbi::WeakImpl;
using WeakClearFunction = void (APS5_VABI*)(WeakImpl**);
using WriteBarrierFunction = void (APS5_VABI*)(HandleSet*, std::uint64_t*, const std::uint64_t*);

static_assert(std::is_same_v<decltype(&_ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE), WeakClearFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_), WriteBarrierFunction>);

constexpr std::uint64_t kCell = 0x0000000000010000ull;
constexpr std::uint64_t kOtherCell = 0x0000000000020000ull;
constexpr std::uint64_t kInt = 0xFFFE000000000001ull;
constexpr std::uint64_t kUndefined = 0xAull;

struct HandleArena {
    std::vector<unsigned char> storage = std::vector<unsigned char>(3 * Jsc::JscAbi::HandleBlockSize);
    HandleSet set{};
    HandleNode* nodes = nullptr;

    HandleArena() {
        const std::uintptr_t mask = Jsc::JscAbi::HandleBlockSize - 1;
        const std::uintptr_t base = (reinterpret_cast<std::uintptr_t>(storage.data()) + mask) & ~mask;
        auto* block = reinterpret_cast<HandleBlock*>(base);
        block->prev = nullptr;
        block->next = nullptr;
        block->handleSet = &set;
        nodes = reinterpret_cast<HandleNode*>(base + sizeof(HandleBlock));
        set.strongHead.next = &set.strongTail;
        set.strongTail.prev = &set.strongHead;
        set.immediateHead.next = &set.immediateTail;
        set.immediateTail.prev = &set.immediateHead;
    }

    HandleArena(const HandleArena&) = delete;
    HandleArena& operator=(const HandleArena&) = delete;

    HandleNode* Linked(unsigned index, std::uint64_t value, bool strong) {
        HandleNode* node = &nodes[index];
        HandleNode& head = strong ? set.strongHead : set.immediateHead;
        node->value = value;
        node->prev = &head;
        node->next = head.next;
        head.next->prev = node;
        head.next = node;
        return node;
    }
};

bool OnlyNode(const HandleNode& head, const HandleNode& tail, const HandleNode* node) {
    return head.next == node && node->prev == &head && node->next == &tail && tail.prev == node;
}

bool Empty(const HandleNode& head, const HandleNode& tail) {
    return head.next == &tail && tail.prev == &head;
}

void TestWeakClearSlowCase() {
    alignas(8) unsigned char owner[8]{};
    WeakImpl impl{kCell, reinterpret_cast<std::uintptr_t>(owner) | 1, owner};
    WeakImpl* reference = &impl;
    _ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE(&reference);
    Check(reference == nullptr, __LINE__);
    Check(impl.weakHandleOwner == (reinterpret_cast<std::uintptr_t>(owner) | 3), __LINE__);
    Check(impl.jsValue == kCell && impl.context == owner, __LINE__);

    WeakImpl live{kCell, reinterpret_cast<std::uintptr_t>(owner), nullptr};
    reference = &live;
    _ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE(&reference);
    Check(reference == nullptr && live.weakHandleOwner == (reinterpret_cast<std::uintptr_t>(owner) | 3), __LINE__);

    WeakImpl* null = nullptr;
    Check(Throws([&] { _ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE(&null); }), __LINE__);
    Check(Throws([] { _ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE(nullptr); }), __LINE__);
}

void TestHandleSetWriteBarrier() {
    HandleArena arena;
    HandleSet& set = arena.set;
    HandleNode* node = arena.Linked(0, 0, false);

    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node->value, &kCell);
    Check(node->value == 0, __LINE__);
    Check(OnlyNode(set.strongHead, set.strongTail, node), __LINE__);
    Check(Empty(set.immediateHead, set.immediateTail), __LINE__);

    node->value = kCell;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node->value, &kOtherCell);
    Check(OnlyNode(set.strongHead, set.strongTail, node), __LINE__);

    HandleNode* other = arena.Linked(1, kUndefined, false);
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node->value, &kInt);
    Check(node->value == kCell, __LINE__);
    Check(Empty(set.strongHead, set.strongTail), __LINE__);
    Check(set.immediateHead.next == node && node->prev == &set.immediateHead, __LINE__);
    Check(node->next == other && other->prev == node && other->next == &set.immediateTail, __LINE__);
    Check(set.immediateTail.prev == other, __LINE__);

    node->value = kInt;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &other->value, &kInt);
    Check(set.immediateHead.next == node && node->next == other, __LINE__);

    const std::uint64_t empty = 0;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &other->value, &empty);
    Check(set.immediateHead.next == other && other->next == node && node->next == &set.immediateTail, __LINE__);
    Check(other->prev == &set.immediateHead && node->prev == other && set.immediateTail.prev == node, __LINE__);

    other->value = 0;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &other->value, &empty);
    Check(set.immediateHead.next == other && other->next == node, __LINE__);

    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node->value, &kOtherCell);
    Check(OnlyNode(set.strongHead, set.strongTail, node), __LINE__);
    Check(OnlyNode(set.immediateHead, set.immediateTail, other), __LINE__);

    node->value = kOtherCell;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node->value, &empty);
    Check(Empty(set.strongHead, set.strongTail), __LINE__);
    Check(set.immediateHead.next == node && node->next == other && other->next == &set.immediateTail, __LINE__);

    HandleNode* loose = &arena.nodes[2];
    loose->value = kInt;
    loose->prev = nullptr;
    loose->next = nullptr;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &loose->value, &kUndefined);
    Check(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &loose->value, &kCell); }), __LINE__);
    Check(loose->prev == nullptr && loose->next == nullptr && Empty(set.strongHead, set.strongTail), __LINE__);

    Check(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(nullptr, &node->value, &kCell); }), __LINE__);
    Check(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, nullptr, &kCell); }), __LINE__);
    Check(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node->value, nullptr); }), __LINE__);
}


using Jsc::InspectorAbi::AsyncCallIdentifier;
using Jsc::InspectorAbi::FrameVector;
using Jsc::InspectorAbi::RemoteTarget;
using Jsc::InspectorAbi::ScriptCallStack;
using TargetFlagFunction = bool (APS5_VABI*)(const RemoteTarget*);
using ScriptCallStackFunction = void (APS5_VABI*)(ScriptCallStack*, FrameVector*);
using AsyncCallIdentifierFunction = AsyncCallIdentifier (APS5_VABI*)(int, int);

static_assert(std::is_same_v<decltype(&_ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv), TargetFlagFunction>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv), TargetFlagFunction>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE), ScriptCallStackFunction>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStackC2ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE), ScriptCallStackFunction>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi), AsyncCallIdentifierFunction>);

RemoteTarget Target(bool flag) {
    RemoteTarget target;
    std::memset(&target, 0xA5, sizeof(target));
    target.vtable = &target;
    target.targetIdentifier = 7;
    target.flag = flag;
    return target;
}

void TestRemoteTargets() {
    const RemoteTarget unpaired = Target(false);
    const RemoteTarget paired = Target(true);
    Check(_ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(&unpaired), __LINE__);
    Check(!_ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(&paired), __LINE__);
    Check(!_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(&unpaired), __LINE__);
    Check(_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(&paired), __LINE__);
    Check(paired.targetIdentifier == 7 && paired.vtable == &paired, __LINE__);

    alignas(8) unsigned char raw[16];
    std::memset(raw, 0, sizeof(raw));
    raw[12] = 1;
    raw[8] = 0xFF;
    raw[13] = 0xFF;
    Check(_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(reinterpret_cast<const RemoteTarget*>(raw)), __LINE__);
    raw[12] = 0;
    Check(_ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(reinterpret_cast<const RemoteTarget*>(raw)), __LINE__);

    Check(Throws([] { _ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(nullptr); }), __LINE__);
    Check(Throws([] { _ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(nullptr); }), __LINE__);
}

void TestScriptCallStackConstructors() {
    for (const ScriptCallStackFunction construct : {&_ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE,
                                                    &_ZN9Inspector15ScriptCallStackC2ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE}) {
        unsigned char frames[64];
        FrameVector vector{frames, 2, 1};
        ScriptCallStack stack;
        std::memset(&stack, 0x5A, sizeof(stack));
        construct(&stack, &vector);
        Check(stack.refCount == 1, __LINE__);
        Check(stack.frames.buffer == frames && stack.frames.capacity == 2 && stack.frames.size == 1, __LINE__);
        Check(vector.buffer == nullptr && vector.capacity == 0 && vector.size == 0, __LINE__);

        FrameVector empty{nullptr, 0, 0};
        std::memset(&stack, 0x5A, sizeof(stack));
        construct(&stack, &empty);
        Check(stack.refCount == 1 && stack.frames.buffer == nullptr && stack.frames.capacity == 0 && stack.frames.size == 0, __LINE__);
        Check(empty.buffer == nullptr && empty.capacity == 0 && empty.size == 0, __LINE__);

        Check(Throws([&] { construct(nullptr, &vector); }), __LINE__);
        Check(Throws([&] { construct(&stack, nullptr); }), __LINE__);
    }
}

void TestAsyncCallIdentifier() {
    const AsyncCallIdentifier timer = _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(0, 42);
    Check(timer.type == 0 && timer.callbackId == 42, __LINE__);
    const AsyncCallIdentifier listener = _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(1, -5);
    Check(listener.type == 1 && listener.callbackId == -5, __LINE__);
    const AsyncCallIdentifier wide = _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(-1, (std::numeric_limits<int>::min)());
    Check(wide.type == (std::numeric_limits<unsigned>::max)() && wide.callbackId == (std::numeric_limits<int>::min)(), __LINE__);
}


using Jsc::JscAbi::YarrRegularExpression;
using Jsc::JscAbi::YarrRegularExpressionPrivate;
using IsValidFunction = bool (APS5_VABI*)(const YarrRegularExpression*);
using AnalyzeHeapFunction = void (APS5_VABI*)(void*, void*);
using StructurePropertyNamesFunction = void (APS5_VABI*)(void*, void*, void*, std::uint64_t);

static_assert(std::is_same_v<decltype(&_ZNK3JSC4Yarr17RegularExpression7isValidEv), IsValidFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC6JSCell11analyzeHeapEPS0_RNS_12HeapAnalyzerE), AnalyzeHeapFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC7JSProxy25getStructurePropertyNamesEPNS_8JSObjectEPNS_14JSGlobalObjectERNS_17PropertyNameArrayENS_15EnumerationModeE), StructurePropertyNamesFunction>);

void TestRegularExpressionIsValid() {
    int bytecode = 0;
    YarrRegularExpressionPrivate compiled{1, 0, nullptr, -1, 2, &bytecode};
    YarrRegularExpressionPrivate failed{1, 3, nullptr, -1, 0, nullptr};
    const YarrRegularExpression valid{&compiled};
    const YarrRegularExpression invalid{&failed};
    Check(_ZNK3JSC4Yarr17RegularExpression7isValidEv(&valid), __LINE__);
    Check(!_ZNK3JSC4Yarr17RegularExpression7isValidEv(&invalid), __LINE__);
    Check(compiled.refCount == 1 && compiled.lastMatchLength == -1 && compiled.byteCode == &bytecode, __LINE__);

    alignas(8) unsigned char raw[32];
    std::memset(raw, 0xFF, sizeof(raw));
    std::memset(raw + 24, 0, 8);
    const YarrRegularExpression rawExpression{reinterpret_cast<YarrRegularExpressionPrivate*>(raw)};
    Check(!_ZNK3JSC4Yarr17RegularExpression7isValidEv(&rawExpression), __LINE__);
    raw[31] = 1;
    Check(_ZNK3JSC4Yarr17RegularExpression7isValidEv(&rawExpression), __LINE__);

    const YarrRegularExpression empty{nullptr};
    Check(Throws([&] { _ZNK3JSC4Yarr17RegularExpression7isValidEv(&empty); }), __LINE__);
    Check(Throws([] { _ZNK3JSC4Yarr17RegularExpression7isValidEv(nullptr); }), __LINE__);
}

void TestEmptyBodies() {
    unsigned char cell[32];
    unsigned char other[32];
    std::memset(cell, 0x5A, sizeof(cell));
    std::memset(other, 0xA5, sizeof(other));
    _ZN3JSC6JSCell11analyzeHeapEPS0_RNS_12HeapAnalyzerE(cell, other);
    _ZN3JSC6JSCell11analyzeHeapEPS0_RNS_12HeapAnalyzerE(nullptr, nullptr);
    _ZN3JSC7JSProxy25getStructurePropertyNamesEPNS_8JSObjectEPNS_14JSGlobalObjectERNS_17PropertyNameArrayENS_15EnumerationModeE(cell, other, other, 0x100000001ull);
    _ZN3JSC7JSProxy25getStructurePropertyNamesEPNS_8JSObjectEPNS_14JSGlobalObjectERNS_17PropertyNameArrayENS_15EnumerationModeE(nullptr, nullptr, nullptr, 0);
    for (unsigned char byte : cell) Check(byte == 0x5A, __LINE__);
    for (unsigned char byte : other) Check(byte == 0xA5, __LINE__);
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
    TestWeakClearSlowCase();
    TestHandleSetWriteBarrier();
    TestRemoteTargets();
    TestScriptCallStackConstructors();
    TestAsyncCallIdentifier();
    TestRegularExpressionIsValid();
    TestEmptyBodies();
    std::puts("JSC runtime tests passed");
    return 0;
}

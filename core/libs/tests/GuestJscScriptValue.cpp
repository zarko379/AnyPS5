#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libSceJsc/include/JscHandleExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

std::array<unsigned char, 16> ValueHolding(const std::uint64_t* slot) {
    std::array<unsigned char, 16> object{};
    std::memset(object.data(), 0x5A, object.size());
    std::memcpy(object.data(), &slot, sizeof(slot));
    return object;
}

using Object = std::array<unsigned char, 16>;
using Destroy = void (APS5_VABI*)(void*);
using Copy = void (APS5_VABI*)(void*, const void*);
using Assign = void* (APS5_VABI*)(void*, const void*);

static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueD1Ev), Destroy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueD2Ev), Destroy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueC1ERKS0_), Copy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueC2ERKS0_), Copy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueaSERKS0_), Assign>);

Object EmptyValue(unsigned char tail) {
    Object object{};
    object.fill(tail);
    _ZN10Deprecated11ScriptValueC1Ev(object.data());
    return object;
}

bool IsEmptyValue(const Object& object, unsigned char tail) {
    for (std::size_t i = 0; i < 8; ++i)
        if (object[i] != 0) return false;
    for (std::size_t i = 8; i < object.size(); ++i)
        if (object[i] != tail) return false;
    return _ZNK10Deprecated11ScriptValue10hasNoValueEv(object.data()) && _ZNK10Deprecated11ScriptValue7jsValueEv(object.data()) == 0;
}

void TestDestroyEmpty() {
    for (const Destroy destroy : {&_ZN10Deprecated11ScriptValueD1Ev, &_ZN10Deprecated11ScriptValueD2Ev}) {
        Object value = EmptyValue(0x5A);
        destroy(value.data());
        Require(IsEmptyValue(value, 0x5A));
    }
}

void TestCopyEmpty() {
    for (const Copy copy : {&_ZN10Deprecated11ScriptValueC1ERKS0_, &_ZN10Deprecated11ScriptValueC2ERKS0_}) {
        const Object source = EmptyValue(0x5A);
        Object target{};
        target.fill(0xA5);
        copy(target.data(), source.data());
        Require(IsEmptyValue(target, 0xA5));
        Require(IsEmptyValue(source, 0x5A));
    }
}

void TestAssignEmpty() {
    Object target = EmptyValue(0x5A);
    const Object source = EmptyValue(0xA5);
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), source.data()) == target.data());
    Require(IsEmptyValue(target, 0x5A));
    Require(IsEmptyValue(source, 0xA5));
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), target.data()) == target.data());
    Require(IsEmptyValue(target, 0x5A));
}

struct ScriptObject {
    unsigned char value[8];
    void* globalObject;
};

static_assert(sizeof(ScriptObject) == 16);

void TestScriptObjectLifetime() {
    int firstGlobal = 0;
    int secondGlobal = 0;
    ScriptObject first{};
    std::memset(first.value, 0x5A, sizeof(first.value));
    _ZN10Deprecated11ScriptValueC2Ev(&first);
    first.globalObject = &firstGlobal;
    ScriptObject copy{};
    std::memset(copy.value, 0x5A, sizeof(copy.value));
    _ZN10Deprecated11ScriptValueC2ERKS0_(&copy, &first);
    copy.globalObject = first.globalObject;
    ScriptObject second{};
    _ZN10Deprecated11ScriptValueC2Ev(&second);
    second.globalObject = &secondGlobal;
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(&second, &copy) == &second);
    second.globalObject = copy.globalObject;
    for (ScriptObject* object : {&first, &copy, &second}) {
        Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(object));
        Require(object->globalObject == &firstGlobal);
        _ZN10Deprecated11ScriptValueD2Ev(object);
        Require(object->globalObject == &firstGlobal);
        for (const unsigned char byte : object->value) Require(byte == 0);
    }
}

using Jsc::JscAbi::HandleBlock;
using Jsc::JscAbi::HandleNode;
using Jsc::JscAbi::HandleSet;

constexpr std::uint64_t kCell = 0x0000000000040000ull;
constexpr std::uint64_t kInt = 0xFFFE000000000007ull;

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

    void Free(unsigned index) {
        HandleNode* node = &nodes[index];
        node->value = 0x5A5A;
        node->prev = nullptr;
        node->next = set.freeHead;
        set.freeHead = node;
    }
};

Object Holding(HandleNode* node, unsigned char tail) {
    Object object{};
    object.fill(tail);
    std::uint64_t* slot = &node->value;
    std::memcpy(object.data(), &slot, sizeof(slot));
    return object;
}

std::uint64_t* SlotOf(const Object& object) {
    std::uint64_t* slot = nullptr;
    std::memcpy(&slot, object.data(), sizeof(slot));
    return slot;
}

bool EmptyList(const HandleNode& head, const HandleNode& tail) {
    return head.next == &tail && tail.prev == &head;
}

template <typename Function>
bool ThrowsNotImplemented(Function function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template <typename Function>
bool ThrowsInvalid(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestDestroyHeld() {
    for (const Destroy destroy : {&_ZN10Deprecated11ScriptValueD1Ev, &_ZN10Deprecated11ScriptValueD2Ev}) {
        HandleArena arena;
        arena.Free(2);
        HandleNode* node = arena.Linked(0, kCell, true);
        Object value = Holding(node, 0x5A);
        destroy(value.data());
        Require(IsEmptyValue(value, 0x5A));
        Require(EmptyList(arena.set.strongHead, arena.set.strongTail));
        Require(arena.set.freeHead == node && node->next == &arena.nodes[2] && node->prev == nullptr);
        Require(node->value == kCell);
    }
}

void TestCopyHeld() {
    for (const Copy copy : {&_ZN10Deprecated11ScriptValueC1ERKS0_, &_ZN10Deprecated11ScriptValueC2ERKS0_}) {
        HandleArena arena;
        arena.Free(3);
        arena.Free(1);
        HandleNode* sourceNode = arena.Linked(0, kCell, true);
        const Object source = Holding(sourceNode, 0x5A);
        Object target{};
        target.fill(0xA5);
        copy(target.data(), source.data());
        HandleNode* copied = &arena.nodes[1];
        Require(SlotOf(target) == &copied->value);
        for (std::size_t i = 8; i < target.size(); ++i) Require(target[i] == 0xA5);
        Require(copied->value == kCell);
        Require(arena.set.freeHead == &arena.nodes[3]);
        Require(arena.set.strongHead.next == copied && copied->prev == &arena.set.strongHead);
        Require(copied->next == sourceNode && sourceNode->prev == copied);
        Require(EmptyList(arena.set.immediateHead, arena.set.immediateTail));
        Require(SlotOf(source) == &sourceNode->value && sourceNode->value == kCell);

        HandleNode* intNode = arena.Linked(2, kInt, false);
        const Object intSource = Holding(intNode, 0x5A);
        Object intTarget{};
        copy(intTarget.data(), intSource.data());
        Require(SlotOf(intTarget) == &arena.nodes[3].value && arena.nodes[3].value == kInt);
        Require(arena.set.immediateHead.next == &arena.nodes[3] && arena.nodes[3].next == intNode);
        Require(arena.set.freeHead == nullptr);

        Object exhausted{};
        exhausted.fill(0x11);
        Require(ThrowsNotImplemented([&] { copy(exhausted.data(), source.data()); }));
        for (const unsigned char byte : exhausted) Require(byte == 0x11);
    }
}

void TestAssignHeld() {
    HandleArena arena;
    arena.Free(2);
    HandleNode* sourceNode = arena.Linked(0, kCell, true);
    HandleNode* targetNode = arena.Linked(1, kInt, false);
    const Object source = Holding(sourceNode, 0x5A);
    Object target = Holding(targetNode, 0xA5);
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), source.data()) == target.data());
    Require(SlotOf(target) == &targetNode->value && targetNode->value == kCell);
    Require(arena.set.strongHead.next == targetNode && targetNode->next == sourceNode);
    Require(EmptyList(arena.set.immediateHead, arena.set.immediateTail));
    Require(arena.set.freeHead == &arena.nodes[2]);

    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), target.data()) == target.data());
    Require(SlotOf(target) == &targetNode->value && targetNode->value == kCell);

    const Object empty = EmptyValue(0x33);
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), empty.data()) == target.data());
    Require(IsEmptyValue(target, 0xA5));
    Require(arena.set.freeHead == targetNode && targetNode->next == &arena.nodes[2]);
    Require(arena.set.strongHead.next == sourceNode && sourceNode->next == &arena.set.strongTail);

    Object fresh = EmptyValue(0x44);
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(fresh.data(), source.data()) == fresh.data());
    Require(SlotOf(fresh) == &targetNode->value && targetNode->value == kCell);
    Require(arena.set.freeHead == &arena.nodes[2]);
    Require(arena.set.strongHead.next == targetNode && targetNode->next == sourceNode);
}

void TestHeldNullAndUnlinked() {
    HandleArena arena;
    HandleNode* loose = &arena.nodes[0];
    loose->value = kCell;
    loose->prev = nullptr;
    loose->next = nullptr;
    Object value = Holding(loose, 0x5A);
    Require(ThrowsInvalid([&] { _ZN10Deprecated11ScriptValueD1Ev(value.data()); }));
    Require(SlotOf(value) == &loose->value && arena.set.freeHead == nullptr);
    Require(ThrowsInvalid([] { _ZN10Deprecated11ScriptValueD2Ev(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN10Deprecated11ScriptValueC1ERKS0_(nullptr, value.data()); }));
    Require(ThrowsInvalid([&] { _ZN10Deprecated11ScriptValueaSERKS0_(value.data(), nullptr); }));
}

} // namespace

int main() {
    std::array<unsigned char, 16> empty{};
    std::memset(empty.data(), 0x5A, empty.size());
    _ZN10Deprecated11ScriptValueC1Ev(empty.data());
    Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(empty.data()));
    Require(_ZNK10Deprecated11ScriptValue7jsValueEv(empty.data()) == 0);

    for (const std::uint64_t encoded : {std::uint64_t{0x0002000000000007}, std::uint64_t{0xFFFE000000000001}, std::uint64_t{1}}) {
        std::uint64_t slot = encoded;
        const auto object = ValueHolding(&slot);
        Require(!_ZNK10Deprecated11ScriptValue10hasNoValueEv(object.data()));
        Require(_ZNK10Deprecated11ScriptValue7jsValueEv(object.data()) == encoded);
        Require(slot == encoded);
        for (std::size_t i = sizeof(&slot); i < object.size(); ++i) Require(object[i] == 0x5A);
    }

    std::uint64_t emptySlot = 0;
    const auto holdingEmpty = ValueHolding(&emptySlot);
    Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(holdingEmpty.data()));
    Require(_ZNK10Deprecated11ScriptValue7jsValueEv(holdingEmpty.data()) == 0);

    TestDestroyEmpty();
    TestCopyEmpty();
    TestAssignEmpty();
    TestScriptObjectLifetime();
    TestDestroyHeld();
    TestCopyHeld();
    TestAssignHeld();
    TestHeldNullAndUnlinked();
}

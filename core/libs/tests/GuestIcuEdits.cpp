#include "prx/libSceIcu/Edits.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include <unicode/edits.h>
#include <unicode/udata.h>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU EDITS: %s\n", message);
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

struct Heap {
    std::set<void*> live;
    int failAt = -1;
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    if (heap.failAt == 0) {
        heap.failAt = -1;
        return nullptr;
    }
    if (heap.failAt > 0) --heap.failAt;
    void* memory = std::malloc(size ? size : 1);
    heap.live.insert(memory);
    return memory;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void* memory, std::size_t size) {
    heap.live.erase(memory);
    void* result = std::realloc(memory, size ? size : 1);
    heap.live.insert(result);
    return result;
}

void APS5_VABI Release_nid_no_patch(const void*, void* memory) {
    Require(heap.live.erase(memory) == 1, "the library releases only memory it allocated");
    std::free(memory);
}

static_assert(sizeof(icu::Edits) == sizeof(Icu::Edits));
static_assert(sizeof(icu::Edits::Iterator) == sizeof(Icu::EditsIterator));

void Init(Icu::Edits& edits) {
    edits.array = edits.stackArray;
    edits.capacity = 100;
    edits.length = edits.delta = edits.numChanges = 0;
    edits.errorCode = 0;
}

const Icu::Edits& View(const icu::Edits& host) {
    return reinterpret_cast<const Icu::Edits&>(host);
}

void Same(const Icu::Edits& guest, const icu::Edits& host, const char* message) {
    const Icu::Edits& view = View(host);
    Require(guest.length == view.length && guest.delta == view.delta && guest.numChanges == view.numChanges, message);
    Require(guest.errorCode == view.errorCode && guest.capacity == view.capacity, message);
    Require((guest.array == guest.stackArray) == (view.array == view.stackArray), message);
    Require(guest.length == 0 || std::memcmp(guest.array, view.array, static_cast<std::size_t>(guest.length) * 2) == 0, message);
}

void SameIterator(const Icu::EditsIterator& guest, const icu::Edits::Iterator& host, const char* message) {
    Require((guest.changed != 0) == (host.hasChange() != 0), message);
    Require(guest.oldLength == host.oldLength() && guest.newLength == host.newLength(), message);
    Require(guest.srcIndex == host.sourceIndex() && guest.replIndex == host.replacementIndex() && guest.destIndex == host.destinationIndex(), message);
}

std::int32_t RandomLength(std::mt19937& random) {
    static const std::int32_t lengths[] = {0, 1, 2, 3, 5, 6, 7, 8, 60, 61, 62, 100, 0x7fff, 0x8000, 0x12345, 4095, 4096, 4097, 9000, 1 << 30};
    return lengths[random() % (sizeof(lengths) / sizeof(lengths[0]))];
}

void ApplyRandom(Icu::Edits& guest, icu::Edits& host, std::mt19937& random, int steps, bool allowInvalid) {
    for (int step = 0; step < steps; ++step) {
        const unsigned operation = random() % 10;
        if (operation < 3) {
            std::int32_t length = static_cast<std::int32_t>(random() % 12);
            if (random() % 15 == 0) length = RandomLength(random);
            if (allowInvalid && random() % 200 == 0) length = -1;
            _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&guest, length);
            host.addUnchanged(length);
        } else {
            std::int32_t oldLength = static_cast<std::int32_t>(random() % 8);
            std::int32_t newLength = static_cast<std::int32_t>(random() % 9);
            if (random() % 8 == 0) oldLength = RandomLength(random);
            if (random() % 8 == 0) newLength = RandomLength(random);
            if (allowInvalid && random() % 300 == 0) newLength = -2;
            _ZN6icu_685Edits10addReplaceEii_nid_postfix(&guest, oldLength, newLength);
            host.addReplace(oldLength, newLength);
        }
    }
}

icu::Edits::Iterator HostIterator(const icu::Edits& host, bool onlyChanges, bool coarse) {
    if (onlyChanges) return coarse ? host.getCoarseChangesIterator() : host.getFineChangesIterator();
    return coarse ? host.getCoarseIterator() : host.getFineIterator();
}

void CheckIterators(const Icu::Edits& guest, const icu::Edits& host, std::mt19937& random) {
    for (int kind = 0; kind < 4; ++kind) {
        const bool onlyChanges = (kind & 1) != 0;
        const bool coarse = (kind & 2) != 0;
        Icu::EditsIterator guestIterator;
        _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(&guestIterator, guest.array, guest.length, onlyChanges, coarse);
        icu::Edits::Iterator hostIterator = HostIterator(host, onlyChanges, coarse);
        const auto& fresh = reinterpret_cast<const Icu::EditsIterator&>(hostIterator);
        Require(guestIterator.index == fresh.index && guestIterator.length == fresh.length && guestIterator.remaining == fresh.remaining &&
                    guestIterator.onlyChanges == fresh.onlyChanges && guestIterator.coarse == fresh.coarse && guestIterator.dir == fresh.dir,
                "a fresh iterator has ICU's state");
        SameIterator(guestIterator, hostIterator, "a fresh iterator starts at zero");
        std::int32_t guestStatus = 0;
        UErrorCode hostStatus = U_ZERO_ERROR;
        int count = 0;
        for (;;) {
            const bool guestNext = _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&guestIterator, onlyChanges, &guestStatus) != 0;
            const bool hostNext = hostIterator.next(hostStatus) != 0;
            Require(guestNext == hostNext, "next agrees on more edits");
            SameIterator(guestIterator, hostIterator, "next moves to the same span");
            if (!guestNext || ++count > 100000) break;
        }
        Icu::EditsIterator guestFind;
        _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&guestFind, guest.array, guest.length, onlyChanges, coarse);
        icu::Edits::Iterator hostFind = HostIterator(host, onlyChanges, coarse);
        const std::int32_t limit = 60;
        for (int query = 0; query < 60; ++query) {
            const auto index = static_cast<std::int32_t>(random() % (limit + 2)) - 1;
            const unsigned which = random() % 4;
            std::int32_t status = 0;
            UErrorCode hostCode = U_ZERO_ERROR;
            if (which == 0) {
                const std::int8_t guestFound = _ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&guestFind, index, 1, &status) == 0;
                const UBool hostFound = hostFind.findSourceIndex(index, hostCode);
                Require(guestFound == hostFound, "findIndex for a source index agrees with findSourceIndex");
            } else if (which == 1) {
                const std::int8_t guestFound = _ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&guestFind, index, 0, &status) == 0;
                const UBool hostFound = hostFind.findDestinationIndex(index, hostCode);
                Require(guestFound == hostFound, "findIndex for a destination index agrees with findDestinationIndex");
            } else if (which == 2) {
                Require(_ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(&guestFind, index, &status) ==
                            hostFind.destinationIndexFromSourceIndex(index, hostCode),
                        "destinationIndexFromSourceIndex matches ICU");
            } else {
                Require(_ZN6icu_685Edits8Iterator31sourceIndexFromDestinationIndexEiR10UErrorCode_nid_postfix(&guestFind, index, &status) ==
                            hostFind.sourceIndexFromDestinationIndex(index, hostCode),
                        "sourceIndexFromDestinationIndex matches ICU");
            }
            Require(status == hostCode, "index queries report the same status");
            SameIterator(guestFind, hostFind, "index queries leave the same iterator state");
            Require(guestFind.index == reinterpret_cast<const Icu::EditsIterator&>(hostFind).index &&
                        guestFind.remaining == reinterpret_cast<const Icu::EditsIterator&>(hostFind).remaining &&
                        guestFind.dir == reinterpret_cast<const Icu::EditsIterator&>(hostFind).dir,
                    "index queries leave the same array position");
        }
    }
}

void CheckRandomEdits() {
    std::mt19937 random(17);
    for (int round = 0; round < 300; ++round) {
        Icu::Edits guest;
        Init(guest);
        icu::Edits host;
        ApplyRandom(guest, host, random, static_cast<int>(random() % 400), true);
        Same(guest, host, "random edits encode the same array");
        if (guest.errorCode == 0) CheckIterators(guest, host, random);
        _ZN6icu_685Edits5resetEv_nid_postfix(&guest);
        host.reset();
        Same(guest, host, "reset clears like ICU but keeps the array");
        _ZN6icu_685EditsD1Ev_nid_postfix(&guest);
    }
    Require(heap.live.empty(), "destroying edits frees their grown arrays");
}

std::int32_t OldTotal(const icu::Edits& host) {
    UErrorCode status = U_ZERO_ERROR;
    icu::Edits::Iterator iterator = host.getFineIterator();
    std::int32_t total = 0;
    while (iterator.next(status)) total += iterator.oldLength();
    return total;
}

void BuildWithOldTotal(Icu::Edits& guest, icu::Edits& host, std::int32_t total, std::mt19937& random) {
    std::int32_t remaining = total;
    while (remaining > 0 || random() % 3 == 0) {
        const unsigned operation = random() % 3;
        if (operation == 0 && remaining > 0) {
            const auto length = static_cast<std::int32_t>(1 + random() % static_cast<unsigned>(remaining < 6 ? remaining : 6));
            _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&guest, length);
            host.addUnchanged(length);
            remaining -= length;
        } else {
            const auto oldLength = remaining > 0 ? static_cast<std::int32_t>(random() % static_cast<unsigned>((remaining < 4 ? remaining : 4) + 1)) : 0;
            const auto newLength = static_cast<std::int32_t>(random() % 4);
            _ZN6icu_685Edits10addReplaceEii_nid_postfix(&guest, oldLength, newLength);
            host.addReplace(oldLength, newLength);
            remaining -= oldLength;
        }
    }
}

void CheckMerge() {
    std::mt19937 random(23);
    for (int round = 0; round < 3000; ++round) {
        Icu::Edits guestAb;
        Icu::Edits guestBc;
        Icu::Edits guestAc;
        Init(guestAb);
        Init(guestBc);
        Init(guestAc);
        icu::Edits hostAb;
        icu::Edits hostBc;
        icu::Edits hostAc;
        BuildWithOldTotal(guestAb, hostAb, static_cast<std::int32_t>(random() % 20), random);
        const std::int32_t middle = OldTotal(hostAb) + hostAb.lengthDelta();
        const bool mismatch = random() % 10 == 0;
        BuildWithOldTotal(guestBc, hostBc, middle + (mismatch ? 1 + static_cast<std::int32_t>(random() % 2) * -2 : 0), random);
        if (random() % 2) {
            _ZN6icu_685Edits10addReplaceEii_nid_postfix(&guestAc, 2, 1);
            hostAc.addReplace(2, 1);
        }
        std::int32_t guestStatus = 0;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(_ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&guestAc, &guestAb, &guestBc, &guestStatus) == &guestAc,
                "mergeAndAppend returns the edits");
        hostAc.mergeAndAppend(hostAb, hostBc, hostStatus);
        Require(guestStatus == hostStatus, "mergeAndAppend reports the same status");
        Same(guestAc, hostAc, "mergeAndAppend appends the same edits");
        _ZN6icu_685EditsD1Ev_nid_postfix(&guestAb);
        _ZN6icu_685EditsD2Ev_nid_postfix(&guestBc);
        _ZN6icu_685EditsD1Ev_nid_postfix(&guestAc);
    }
    Icu::Edits a;
    Icu::Edits b;
    Icu::Edits c;
    Init(a);
    Init(b);
    Init(c);
    std::int32_t status = 3;
    _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&c, &a, &b, &status);
    Require(status == 3 && c.length == 0, "mergeAndAppend keeps a failed input status");
    c.errorCode = 7;
    status = 0;
    _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&c, &a, &b, &status);
    Require(status == 7, "mergeAndAppend copies the edits' own error first");
    Require(Rejects([&] { _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&c, &a, &b, nullptr); }), "mergeAndAppend rejects a null status");
    Require(Rejects([&] { _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&c, nullptr, &b, &status); }), "mergeAndAppend rejects a null ab");
    Require(Rejects([&] { _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&c, &a, nullptr, &status); }), "mergeAndAppend rejects a null bc");
    Require(Rejects([&] { _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(nullptr, &a, &b, &status); }), "mergeAndAppend rejects a null this");
}

void Grow(Icu::Edits& guest, icu::Edits& host, int count) {
    for (int index = 0; index < count; ++index) {
        _ZN6icu_685Edits10addReplaceEii_nid_postfix(&guest, 1 + index % 5, index % 3);
        host.addReplace(1 + index % 5, index % 3);
    }
}

void CheckCopyAndMove() {
    for (int count : {0, 3, 99, 150, 2500}) {
        Icu::Edits guest;
        Init(guest);
        icu::Edits host;
        Grow(guest, host, count);
        Same(guest, host, "grown edits match");

        Icu::Edits guestCopy;
        Init(guestCopy);
        guestCopy.length = guest.length;
        guestCopy.delta = guest.delta;
        guestCopy.numChanges = guest.numChanges;
        guestCopy.errorCode = guest.errorCode;
        Require(_ZN6icu_685Edits9copyArrayERKS0__nid_postfix(&guestCopy, &guest) == &guestCopy, "copyArray returns the edits");
        icu::Edits hostCopy(host);
        Same(guestCopy, hostCopy, "the copy constructor path copies like ICU");

        Icu::Edits guestAssigned;
        Init(guestAssigned);
        icu::Edits hostAssigned;
        Grow(guestAssigned, hostAssigned, 120);
        Require(_ZN6icu_685EditsaSERKS0__nid_postfix(&guestAssigned, &guest) == &guestAssigned, "copy assignment returns the edits");
        hostAssigned = host;
        Same(guestAssigned, hostAssigned, "copy assignment matches ICU");

        Icu::Edits guestMoved;
        Init(guestMoved);
        guestMoved.length = guestCopy.length;
        guestMoved.delta = guestCopy.delta;
        guestMoved.numChanges = guestCopy.numChanges;
        guestMoved.errorCode = guestCopy.errorCode;
        Require(_ZN6icu_685Edits9moveArrayERS0__nid_postfix(&guestMoved, &guestCopy) == &guestMoved, "moveArray returns the edits");
        icu::Edits hostMoved(std::move(hostCopy));
        Same(guestMoved, hostMoved, "the move constructor path moves like ICU");
        Same(guestCopy, hostCopy, "the move source is left like ICU's");

        Icu::Edits guestMoveAssigned;
        Init(guestMoveAssigned);
        icu::Edits hostMoveAssigned;
        Grow(guestMoveAssigned, hostMoveAssigned, 300);
        Require(_ZN6icu_685EditsaSEOS0__nid_postfix(&guestMoveAssigned, &guestMoved) == &guestMoveAssigned, "move assignment returns the edits");
        hostMoveAssigned = std::move(hostMoved);
        Same(guestMoveAssigned, hostMoveAssigned, "move assignment matches ICU");
        Same(guestMoved, hostMoved, "the move-assignment source is left like ICU's");

        _ZN6icu_685EditsD1Ev_nid_postfix(&guest);
        _ZN6icu_685EditsD1Ev_nid_postfix(&guestCopy);
        _ZN6icu_685EditsD1Ev_nid_postfix(&guestAssigned);
        _ZN6icu_685EditsD2Ev_nid_postfix(&guestMoved);
        _ZN6icu_685EditsD1Ev_nid_postfix(&guestMoveAssigned);
    }
    Icu::Edits failed;
    Init(failed);
    Icu::Edits source;
    Init(source);
    icu::Edits unused;
    Grow(source, unused, 10);
    failed.errorCode = 1;
    failed.length = 10;
    failed.delta = 3;
    _ZN6icu_685Edits9copyArrayERKS0__nid_postfix(&failed, &source);
    Require(failed.length == 0 && failed.delta == 0 && failed.numChanges == 0, "copyArray of a failed edits empties it");
    failed.length = 10;
    _ZN6icu_685Edits9moveArrayERS0__nid_postfix(&failed, &source);
    Require(failed.length == 0 && source.length > 0, "moveArray of a failed edits empties it and keeps the source");
    Require(heap.live.empty(), "copies and moves leak nothing");
}

void CheckDirectHelpers() {
    Icu::Edits edits;
    Init(edits);
    for (int index = 0; index < 100; ++index) _ZN6icu_685Edits6appendEi_nid_postfix(&edits, index);
    Require(edits.length == 100 && edits.array == edits.stackArray, "append fills the stack array");
    _ZN6icu_685Edits6appendEi_nid_postfix(&edits, 0x1234);
    Require(edits.length == 101 && edits.capacity == 2000 && edits.array != edits.stackArray && edits.array[100] == 0x1234 && edits.array[99] == 99,
            "append grows the stack array to 2000 units");
    Require(_ZN6icu_685Edits9growArrayEv_nid_postfix(&edits) == 1 && edits.capacity == 4000 && edits.array[100] == 0x1234, "growArray doubles");
    void* grown = edits.array;
    _ZN6icu_685Edits12releaseArrayEv_nid_postfix(&edits);
    Require(!heap.live.count(grown), "releaseArray frees a heap array");
    edits.array = edits.stackArray;
    edits.capacity = 100;
    _ZN6icu_685Edits12releaseArrayEv_nid_postfix(&edits);
    Require(edits.array == edits.stackArray, "releaseArray leaves the stack array alone");
    edits.length = 0;
    heap.failAt = 0;
    Require(_ZN6icu_685Edits9growArrayEv_nid_postfix(&edits) == 0 && edits.errorCode == 7 && edits.array == edits.stackArray,
            "a failed growth reports a memory error and keeps the array");
    edits.errorCode = 0;
    edits.capacity = INT32_MAX;
    std::uint16_t fake[1];
    edits.array = fake;
    Require(_ZN6icu_685Edits9growArrayEv_nid_postfix(&edits) == 0 && edits.errorCode == 8, "growth past INT32_MAX reports an index error");
    edits.errorCode = 0;
    edits.capacity = INT32_MAX - 3;
    Require(_ZN6icu_685Edits9growArrayEv_nid_postfix(&edits) == 0 && edits.errorCode == 8, "growth by fewer than five units reports an index error");
    edits.errorCode = 0;
    edits.capacity = INT32_MAX - 4;
    Require(_ZN6icu_685Edits9growArrayEv_nid_postfix(&edits) == 0 && edits.errorCode == 8, "growth by exactly four units reports an index error");
    Init(edits);
    icu::Edits sequence;
    for (int index = 0; index < 1100; ++index) {
        _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 1, 1);
        sequence.addReplace(1, 1);
    }
    Same(edits, sequence, "512 equal short changes fill one unit before the next starts");
    _ZN6icu_685EditsD1Ev_nid_postfix(&edits);
    Init(edits);
    Icu::Edits heapSource;
    Init(heapSource);
    for (int index = 0; index < 101; ++index) _ZN6icu_685Edits6appendEi_nid_postfix(&heapSource, index);
    heapSource.length = 100;
    std::uint16_t* heapArray = heapSource.array;
    edits.length = 100;
    _ZN6icu_685Edits9moveArrayERS0__nid_postfix(&edits, &heapSource);
    Require(edits.array == edits.stackArray && heapSource.array == heapArray && edits.stackArray[99] == 99, "moving exactly 100 units copies them to the stack");
    _ZN6icu_685EditsD1Ev_nid_postfix(&heapSource);
    Init(edits);
    for (int index = 0; index < 100; ++index) _ZN6icu_685Edits6appendEi_nid_postfix(&edits, 0);
    Icu::Edits ab;
    Icu::Edits bc;
    Init(ab);
    Init(bc);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&ab, 1, 2);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&bc, 3);
    std::int32_t mergeStatus = 0;
    heap.failAt = 0;
    _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(&edits, &ab, &bc, &mergeStatus);
    Require(edits.errorCode == 7 && mergeStatus == 7, "a mismatch after a failed growth reports the edits' own error");
    Init(edits);

    std::int32_t out = 0;
    Require(_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &out) == 0 && out == 0, "copyErrorTo of a clean edits is false");
    edits.errorCode = 8;
    Require(_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &out) == 1 && out == 8, "copyErrorTo copies the error");
    out = 2;
    Require(_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &out) == 1 && out == 2, "copyErrorTo keeps an existing failure");
    out = -128;
    edits.errorCode = -127;
    Require(_ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, &out) == 0 && out == -128, "warnings are not failures");
    Init(edits);
    icu::Edits host;
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 0, INT32_MAX);
    host.addReplace(0, INT32_MAX);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 0, 1);
    host.addReplace(0, 1);
    Same(edits, host, "a delta overflow reports an index error like ICU");
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 1, 1);
    _ZN6icu_685Edits12addUnchangedEi_nid_postfix(&edits, 5);
    host.addReplace(1, 1);
    host.addUnchanged(5);
    Same(edits, host, "a failed edits ignores later additions");
    Init(edits);
    icu::Edits underflow;
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, INT32_MAX, 0);
    underflow.addReplace(INT32_MAX, 0);
    _ZN6icu_685Edits10addReplaceEii_nid_postfix(&edits, 2, 0);
    underflow.addReplace(2, 0);
    Same(edits, underflow, "a delta underflow reports an index error like ICU");
    _ZN6icu_685EditsD1Ev_nid_postfix(&edits);

    Icu::EditsIterator iterator;
    _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(&iterator, nullptr, 0, 0, 0);
    std::int32_t status = 0;
    Require(_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status) == 0, "an empty iterator has no next edit");
    Require(_ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(&iterator, &status) == 0, "an empty iterator has no previous edit");
    status = 1;
    Require(_ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, &status) == 0 && status == 1, "next stops on a failed status");
    Require(_ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&iterator, 0, 1, &status) == -1, "findIndex stops on a failed status");
    Require(_ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(&iterator, 3, &status) == 0,
            "a failed index query maps to 0");
    status = 0;
    Require(_ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(&iterator, -1, 1, &status) == -1, "findIndex rejects a negative index");
    const std::uint16_t units[] = {0x7000 | (62 << 6) | 61, 0x8001, 0x8002, 0x8005, 0x2400};
    _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(&iterator, units, 5, 0, 0);
    iterator.index = 1;
    Require(_ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 62) == ((1 << 15) | 2) && iterator.index == 3,
            "readLength reads two trail units");
    Require(_ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 61) == 5 && iterator.index == 4, "readLength reads one trail unit");
    iterator.index = 1;
    Require(_ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 63) == ((1 << 30) | (1 << 15) | 2) && iterator.index == 3,
            "readLength takes bit 30 from the head");
    Require(_ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(&iterator, 60) == 60 && iterator.index == 3, "a short head is the length");
    iterator.oldLength = 4;
    iterator.newLength = 6;
    iterator.changed = 1;
    iterator.srcIndex = iterator.replIndex = iterator.destIndex = 10;
    _ZN6icu_685Edits8Iterator17updateNextIndexesEv_nid_postfix(&iterator);
    Require(iterator.srcIndex == 14 && iterator.replIndex == 16 && iterator.destIndex == 16, "updateNextIndexes advances by the span");
    iterator.changed = 0;
    _ZN6icu_685Edits8Iterator21updatePreviousIndexesEv_nid_postfix(&iterator);
    Require(iterator.srcIndex == 10 && iterator.replIndex == 16 && iterator.destIndex == 10, "updatePreviousIndexes keeps replIndex for unchanged text");
    iterator.dir = 1;
    Require(_ZN6icu_685Edits8Iterator6noNextEv_nid_postfix(&iterator) == 0 && iterator.dir == 0 && iterator.changed == 0 && iterator.oldLength == 0 &&
                iterator.newLength == 0,
            "noNext clears the span");

    Require(Rejects([] { _ZN6icu_685Edits5resetEv_nid_postfix(nullptr); }), "reset rejects a null edits");
    Require(Rejects([] { _ZN6icu_685Edits12addUnchangedEi_nid_postfix(nullptr, 1); }), "addUnchanged rejects a null edits");
    Require(Rejects([] { _ZN6icu_685Edits10addReplaceEii_nid_postfix(nullptr, 1, 1); }), "addReplace rejects a null edits");
    Require(Rejects([] { _ZN6icu_685EditsD1Ev_nid_postfix(nullptr); }), "the destructor rejects a null edits");
    Require(Rejects([&] { _ZN6icu_685EditsaSERKS0__nid_postfix(&edits, nullptr); }), "copy assignment rejects a null source");
    Require(Rejects([&] { _ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(&edits, nullptr); }), "copyErrorTo rejects a null status");
    Require(Rejects([&] { _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&iterator, 0, nullptr); }), "next rejects a null status");
    Require(Rejects([&] { _ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(&iterator, nullptr); }), "previous rejects a null status");
    Require(Rejects([] { _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(nullptr, nullptr, 0, 0, 0); }), "the iterator constructor rejects a null object");
    Icu::EditsIterator broken;
    _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(&broken, nullptr, 3, 0, 0);
    Require(Rejects([&] {
        std::int32_t code = 0;
        _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(&broken, 0, &code);
    }), "next rejects a null array it reads");
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckDirectHelpers();
    CheckCopyAndMove();
    CheckRandomEdits();
    CheckMerge();
    Require(heap.live.empty(), "every allocation is released");
    return 0;
}

#include "prx/libSceIcu/CharString.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include <unicode/udata.h>
#include "charstr.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU CHAR STRING: %s\n", message);
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
    int failNext = 0;
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    if (heap.failNext > 0) {
        --heap.failNext;
        return nullptr;
    }
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

static_assert(sizeof(icu::CharString) == sizeof(Icu::CharString));

const Icu::CharString& View(const icu::CharString& host) {
    return reinterpret_cast<const Icu::CharString&>(host);
}

void Init(Icu::CharString& string) {
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(&string.buffer);
    string.len = 0;
    string.buffer.ptr[0] = 0;
}

void Destroy(Icu::CharString& string) {
    _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&string.buffer);
}

void Same(const Icu::CharString& guest, const icu::CharString& host, const char* message) {
    const Icu::CharString& view = View(host);
    Require(guest.len == view.len && guest.buffer.capacity == view.buffer.capacity && guest.buffer.needToRelease == view.buffer.needToRelease, message);
    Require((guest.buffer.ptr == guest.buffer.stackArray) == (view.buffer.ptr == view.buffer.stackArray), message);
    Require(std::memcmp(guest.buffer.ptr, view.buffer.ptr, static_cast<std::size_t>(guest.len) + 1) == 0, message);
}

void CheckArray() {
    Icu::CharArray array;
    std::memset(&array, 0x55, sizeof(array));
    _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ev_nid_postfix(&array);
    Require(array.ptr == array.stackArray && array.capacity == 40 && array.needToRelease == 0, "the default constructor uses the stack array");
    Require(_ZNK6icu_6815MaybeStackArrayIcLi40EE11getCapacityEv_nid_postfix(&array) == 40, "getCapacity");
    Require(_ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(&array) == array.stackArray, "getAlias");
    Require(_ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(&array) == array.stackArray + 40, "getArrayLimit");
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(&array, 3) == array.stackArray + 3, "operator[]");
    Require(_ZNK6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(&array, -1) == array.stackArray - 1, "const operator[] does no bounds check");
    std::memcpy(array.stackArray, "abcdefghij", 10);
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&array, 0, 5) == nullptr && array.ptr == array.stackArray, "resize to 0 fails");
    char* grown = _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&array, 100, 6);
    Require(grown && array.ptr == grown && array.capacity == 100 && array.needToRelease == 1 && std::memcmp(grown, "abcdef", 6) == 0,
            "resize copies the requested prefix");
    char* shrunk = _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&array, 4, 50);
    Require(shrunk && !heap.live.count(grown) && array.capacity == 4 && std::memcmp(shrunk, "abcd", 4) == 0, "resize clamps the copy to the new capacity");
    heap.failAt = 0;
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&array, 64, 4) == nullptr && array.ptr == shrunk, "a failed resize keeps the array");
    std::int32_t capacity = -1;
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 99, &capacity) == shrunk && capacity == 99 &&
                array.ptr == array.stackArray && array.needToRelease == 0,
            "orphanOrClone gives up an owned array and reports the given length");
    _ZN6icu_6815MaybeStackArrayIcLi40EE12releaseArrayEv_nid_postfix(&array);
    IcuReleaseMemory_nid_no_patch(shrunk);
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 0, &capacity) == nullptr, "orphanOrClone of nothing returns null");
    std::memcpy(array.stackArray, "xyz", 3);
    char* clone = _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 90, &capacity);
    Require(clone && capacity == 40 && std::memcmp(clone, "xyz", 3) == 0 && heap.live.count(clone), "orphanOrClone clones the stack array up to its capacity");
    IcuReleaseMemory_nid_no_patch(clone);
    heap.failAt = 0;
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 3, &capacity) == nullptr, "a failed clone returns null");
    char other[8];
    _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(&array, other, 0);
    Require(array.ptr == array.stackArray, "aliasInstead ignores a zero capacity");
    _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(&array, nullptr, 5);
    Require(array.ptr == array.stackArray, "aliasInstead ignores a null array");
    _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&array, 50, 0);
    char* owned = array.ptr;
    _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(&array, other, 8);
    Require(array.ptr == other && array.capacity == 8 && array.needToRelease == 0 && !heap.live.count(owned), "aliasInstead frees an owned array");
    _ZN6icu_6815MaybeStackArrayIcLi40EE17resetToStackArrayEv_nid_postfix(&array);
    Require(array.ptr == array.stackArray && array.capacity == 40, "resetToStackArray");

    Icu::CharArray sized;
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ei10UErrorCode_nid_postfix(&sized, 30, 0);
    Require(sized.ptr == sized.stackArray, "a small capacity stays on the stack");
    _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ei10UErrorCode_nid_postfix(&sized, 300, 0);
    Require(sized.capacity == 300 && sized.needToRelease == 1, "a large capacity allocates");
    _ZN6icu_6815MaybeStackArrayIcLi40EED2Ev_nid_postfix(&sized);
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ei10UErrorCode_nid_postfix(&sized, 300, 1);
    Require(sized.ptr == sized.stackArray && sized.capacity == 40, "a failed status keeps the stack array");

    Icu::CharArray source;
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(&source);
    std::memcpy(source.stackArray, "stack-content", 13);
    Icu::CharArray moved;
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(&moved, &source);
    Require(moved.ptr == moved.stackArray && std::memcmp(moved.stackArray, "stack-content", 13) == 0, "moving a stack array copies it");
    _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&source, 70, 0);
    char* heapArray = source.ptr;
    Icu::CharArray taken;
    _ZN6icu_6815MaybeStackArrayIcLi40EEC2EOS1__nid_postfix(&taken, &source);
    Require(taken.ptr == heapArray && taken.capacity == 70 && taken.needToRelease == 1 && source.ptr == source.stackArray && source.needToRelease == 0,
            "moving a heap array transfers it");
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(&moved, &taken) == &moved && moved.ptr == heapArray && taken.ptr == taken.stackArray,
            "move assignment transfers a heap array");
    std::memcpy(taken.stackArray, "again", 5);
    _ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(&moved, &taken);
    Require(!heap.live.count(heapArray) && moved.ptr == moved.stackArray && std::memcmp(moved.stackArray, "again", 5) == 0,
            "move assignment releases the old array and copies a stack source");
    std::int32_t status = 0;
    _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(&taken, 60, 0);
    std::memset(taken.ptr, 'q', 60);
    _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(&moved, &taken, &status);
    Require(status == 0 && moved.capacity == 60 && moved.needToRelease == 1 && moved.ptr[59] == 'q', "copyFrom resizes to the source capacity");
    heap.failAt = 0;
    _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(&moved, &taken, &status);
    Require(status == 7, "a failed copyFrom reports a memory error");
    _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(&moved, &taken, &status);
    Require(status == 7, "copyFrom keeps a failed status");
    _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&moved);
    _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&taken);
    Require(Rejects([] { _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(nullptr); }), "the constructor rejects a null array");
    Require(Rejects([&] { _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 3, nullptr); }), "orphanOrClone rejects a null capacity");
    Require(_ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 0, nullptr) == nullptr, "orphanOrClone of nothing writes no capacity");
    Require(heap.live.empty(), "a rejected orphanOrClone allocates nothing");
    Require(Rejects([&] { _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(&array, &array, nullptr); }),
            "copyFrom rejects a null status");
}

void CheckRandomStrings() {
    std::mt19937 random(31);
    const char pool[] = "The quick brown fox jumps over the lazy dog 0123456789 / path\\part";
    for (int round = 0; round < 400; ++round) {
        Icu::CharString guest;
        Init(guest);
        icu::CharString host;
        for (int step = 0; step < 60; ++step) {
            std::int32_t guestStatus = random() % 40 == 0 ? 1 : 0;
            UErrorCode hostStatus = static_cast<UErrorCode>(guestStatus);
            const unsigned operation = random() % 9;
            if (operation == 0) {
                const char c = pool[random() % (sizeof(pool) - 1)];
                _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(&guest, c, &guestStatus);
                host.append(c, hostStatus);
            } else if (operation <= 2) {
                const auto start = random() % (sizeof(pool) - 1);
                auto length = static_cast<std::int32_t>(random() % (sizeof(pool) - start));
                if (random() % 5 == 0) length = -1;
                _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&guest, pool + start, length, &guestStatus);
                host.append(pool + start, length, hostStatus);
            } else if (operation == 3 && guest.len > 0) {
                const auto offset = static_cast<std::int32_t>(random() % static_cast<unsigned>(guest.len + 1));
                const auto length = static_cast<std::int32_t>(random() % static_cast<unsigned>(guest.len - offset + 1));
                _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&guest, guest.buffer.ptr + offset, length, &guestStatus);
                host.append(host.data() + offset, length, hostStatus);
            } else if (operation == 4) {
                const auto length = static_cast<std::int32_t>(random() % 80) - 5;
                _ZN6icu_6810CharString8truncateEi_nid_postfix(&guest, length);
                host.truncate(length);
            } else if (operation == 5) {
                const auto minimum = static_cast<std::int32_t>(random() % 70);
                const auto hint = static_cast<std::int32_t>(random() % 120);
                std::int32_t guestCapacity = -5;
                std::int32_t hostCapacity = -5;
                char* guestBuffer = _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(&guest, minimum, hint, &guestCapacity, &guestStatus);
                char* hostBuffer = host.getAppendBuffer(minimum, hint, hostCapacity, hostStatus);
                Require(guestCapacity == hostCapacity && (guestBuffer == nullptr) == (hostBuffer == nullptr), "getAppendBuffer matches ICU");
                Require(!guestBuffer || guestBuffer == guest.buffer.ptr + guest.len, "getAppendBuffer points after the text");
            } else if (operation == 7) {
                char16_t units[12];
                const auto length = static_cast<std::int32_t>(random() % 12);
                for (std::int32_t index = 0; index < length; ++index) units[index] = random() % 13 == 0 ? u'~' : static_cast<char16_t>(u'a' + random() % 26);
                _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(&guest, units, length, &guestStatus);
                host.appendInvariantChars(units, length, hostStatus);
            } else {
                Icu::CharString copy;
                Init(copy);
                icu::CharString hostCopy;
                std::int32_t status = 0;
                UErrorCode hostCode = U_ZERO_ERROR;
                _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&copy, pool, static_cast<std::int32_t>(random() % 60), &status);
                hostCopy.append(pool, copy.len, hostCode);
                _ZN6icu_6810CharString8copyFromERKS0_R10UErrorCode_nid_postfix(&guest, &copy, &guestStatus);
                host.copyFrom(hostCopy, hostStatus);
                Destroy(copy);
            }
            Require(guestStatus == hostStatus, "statuses match");
            Same(guest, host, "strings match ICU");
            const char probe = pool[random() % (sizeof(pool) - 1)];
            Require(_ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(&guest, probe) == host.lastIndexOf(probe), "lastIndexOf matches ICU");
            const auto start = random() % 50;
            const auto length = static_cast<std::int32_t>(random() % 4);
            Require(_ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(&guest, Icu::StringPiece{pool + start, length}) ==
                        host.contains(icu::StringPiece(pool + start, length)),
                    "contains matches ICU");
        }
        char guestOut[100];
        char hostOut[100];
        for (std::int32_t capacity : {0, guest.len - 1, guest.len, guest.len + 1, 99}) {
            if (capacity < 0) continue;
            std::int32_t guestStatus = random() % 3 == 0 ? -124 : 0;
            UErrorCode hostStatus = static_cast<UErrorCode>(guestStatus);
            std::memset(guestOut, '#', sizeof(guestOut));
            std::memset(hostOut, '#', sizeof(hostOut));
            Require(_ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&guest, guestOut, capacity, &guestStatus) ==
                        host.extract(hostOut, capacity, hostStatus),
                    "extract returns the length");
            Require(guestStatus == hostStatus && std::memcmp(guestOut, hostOut, sizeof(guestOut)) == 0, "extract writes like ICU");
        }
        std::int32_t status = 0;
        char* clone = _ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(&guest, &status);
        Require(clone && std::strcmp(clone, guest.buffer.ptr) == 0 && heap.live.count(clone), "cloneData copies the text and terminator");
        IcuReleaseMemory_nid_no_patch(clone);
        Destroy(guest);
    }
    Require(heap.live.empty(), "strings release their buffers");
}

void CheckMovesAndPaths() {
    std::int32_t status = 0;
    Icu::CharString source;
    Init(source);
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&source, "a fairly long text that does not fit on the stack", -1, &status);
    char* heapText = source.buffer.ptr;
    Icu::CharString moved;
    _ZN6icu_6810CharStringC1EOS0__nid_postfix(&moved, &source);
    Require(moved.buffer.ptr == heapText && moved.len == 49 && source.len == 0 && source.buffer.ptr == source.buffer.stackArray, "the move constructor takes the heap buffer");
    Icu::CharString target;
    Init(target);
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&target, "short", -1, &status);
    Require(_ZN6icu_6810CharStringaSEOS0__nid_postfix(&target, &moved) == &target && target.buffer.ptr == heapText && target.len == 49 && moved.len == 0,
            "move assignment takes the heap buffer");
    Icu::CharString small;
    Init(small);
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&small, "tiny", -1, &status);
    Icu::CharString smallMoved;
    _ZN6icu_6810CharStringC2EOS0__nid_postfix(&smallMoved, &small);
    Require(smallMoved.buffer.ptr == smallMoved.buffer.stackArray && std::strcmp(smallMoved.buffer.ptr, "tiny") == 0 && smallMoved.len == 4,
            "moving a short string copies its stack buffer");
    Destroy(target);
    Destroy(moved);
    Destroy(source);

    Icu::CharString path;
    Init(path);
    Require(_ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(&path) == '/', "the POSIX build separates with a slash");
    _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(&path, &status);
    Require(path.len == 0, "an empty path gets no separator");
    _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(&path, Icu::StringPiece{"data", 4}, &status);
    Require(std::strcmp(path.buffer.ptr, "data") == 0, "the first path part is appended as is");
    _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(&path, Icu::StringPiece{"icu", 3}, &status);
    Require(std::strcmp(path.buffer.ptr, "data/icu") == 0, "later parts are joined with a slash");
    _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(&path, Icu::StringPiece{nullptr, 0}, &status);
    Require(std::strcmp(path.buffer.ptr, "data/icu") == 0, "an empty part is ignored");
    _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(&path, &status);
    _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(&path, &status);
    Require(std::strcmp(path.buffer.ptr, "data/icu/") == 0, "a separator is added once");
    _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(&path, Icu::StringPiece{"x", 1}, &status);
    Require(std::strcmp(path.buffer.ptr, "data/icu/x") == 0, "no separator is doubled");
    _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(&path, '\\', &status);
    _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(&path, &status);
    Require(std::strcmp(path.buffer.ptr, "data/icu/x\\/") == 0, "a backslash is not a separator on a POSIX build");
    status = 1;
    _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(&path, Icu::StringPiece{"y", 1}, &status);
    _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(&path, &status);
    Require(path.len == 12, "a failed status changes nothing");
    status = 0;
    Require(_ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&path, nullptr, 2, &status) == &path && status == 1, "a null text with a length is an error");
    status = 0;
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&path, "z", -2, &status);
    Require(status == 1, "a length below -1 is an error");
    status = 0;
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&path, nullptr, 0, &status);
    Require(status == 0, "a null empty text is fine");
    std::int32_t capacity = 0;
    char* end = _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(&path, 1, 0, &capacity, &status);
    Require(end == path.buffer.ptr + path.len && capacity == 40 - 12 - 1, "getAppendBuffer offers the rest of the stack buffer");
    std::memcpy(end, "ABC", 3);
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&path, end, 3, &status);
    Require(std::strcmp(path.buffer.ptr, "data/icu/x\\/ABC") == 0 && status == 0, "appending the append buffer only moves the end");
    char* full = _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(&path, 1, 0, &capacity, &status);
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(&path, full, capacity + 1, &status);
    Require(status == 5, "appending more than the append buffer holds is an internal error");
    status = 0;
    Icu::CharString sizing;
    Init(sizing);
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&sizing, 40, 0, &status) == 1 && sizing.buffer.ptr == sizing.buffer.stackArray,
            "a capacity the stack holds allocates nothing");
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&sizing, 100, 0, &status) == 1 && sizing.buffer.capacity == 140,
            "without a hint the capacity grows by the old capacity");
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&sizing, 200, 300, &status) == 1 && sizing.buffer.capacity == 300,
            "a larger hint is used");
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&sizing, 400, 350, &status) == 1 && sizing.buffer.capacity == 400,
            "a hint below the capacity is ignored");
    std::int32_t failedSizing = 1;
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&sizing, 900, 0, &failedSizing) == 0 && sizing.buffer.capacity == 400,
            "a failed status grows nothing");
    Destroy(sizing);
    Icu::CharString failing;
    Init(failing);
    heap.failAt = 0;
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&failing, 100, 500, &status) == 1 && failing.buffer.capacity == 100,
            "ensureCapacity falls back to the minimum when the hint fails");
    heap.failNext = 2;
    std::int32_t fail = 0;
    Require(_ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(&failing, 300, 0, &fail) == 0 && fail == 7,
            "ensureCapacity reports a memory error");
    heap.failAt = 0;
    std::int32_t cloneStatus = 0;
    Require(_ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(&failing, &cloneStatus) == nullptr && cloneStatus == 7, "cloneData reports a memory error");
    Require(_ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(&failing, &cloneStatus) == nullptr, "cloneData keeps a failed status");
    std::int32_t extractStatus = 0;
    Require(_ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&path, nullptr, 3, &extractStatus) == path.len && extractStatus == 1,
            "extract rejects a null destination with a capacity");
    Require(_ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(&path, Icu::StringPiece{nullptr, 0}) == 0, "an empty piece is never contained");
    Require(Rejects([&] { _ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(&path, Icu::StringPiece{nullptr, 2}); }),
            "contains rejects a null text it reads");
    Require(Rejects([&] { _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(&path, 'a', nullptr); }), "append rejects a null status");
    Require(Rejects([&] { _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(nullptr, 'a', &status); }), "append rejects a null string");
    Require(Rejects([&] { _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(&path, 1, 1, nullptr, &status); }),
            "getAppendBuffer rejects a null capacity");
    std::int32_t invariant = 0;
    const char16_t bad[] = {u'a', 0x00E9};
    _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(&failing, bad, 2, &invariant);
    Require(invariant == 26, "a non-invariant character is a conversion error");
    Destroy(failing);
    Destroy(path);
    Destroy(smallMoved);
    Destroy(small);
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckArray();
    CheckRandomStrings();
    CheckMovesAndPaths();
    Require(heap.live.empty(), "every allocation is released");
    return 0;
}

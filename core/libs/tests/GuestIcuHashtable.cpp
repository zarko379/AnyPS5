#include "prx/libSceIcu/Hashtable.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

using HostHashFunction = std::int32_t (*)(UHashTok);
using HostComparator = std::int8_t (*)(UHashTok, UHashTok);
using HostDeleter = void (*)(void*);

extern "C" {
Icu::Hashtable* uhash_open_68(HostHashFunction, HostComparator, HostComparator, std::int32_t*);
Icu::Hashtable* uhash_openSize_68(HostHashFunction, HostComparator, HostComparator, std::int32_t, std::int32_t*);
Icu::Hashtable* uhash_initSize_68(Icu::Hashtable*, HostHashFunction, HostComparator, HostComparator, std::int32_t, std::int32_t*);
void uhash_close_68(Icu::Hashtable*);
HostDeleter uhash_setKeyDeleter_68(Icu::Hashtable*, HostDeleter);
HostDeleter uhash_setValueDeleter_68(Icu::Hashtable*, HostDeleter);
void uhash_setResizePolicy_68(Icu::Hashtable*, std::int32_t);
std::int32_t uhash_count_68(const Icu::Hashtable*);
void* uhash_get_68(const Icu::Hashtable*, const void*);
void* uhash_iget_68(const Icu::Hashtable*, std::int32_t);
std::int32_t uhash_geti_68(const Icu::Hashtable*, const void*);
std::int32_t uhash_igeti_68(const Icu::Hashtable*, std::int32_t);
void* uhash_put_68(Icu::Hashtable*, void*, void*, std::int32_t*);
void* uhash_iput_68(Icu::Hashtable*, std::int32_t, void*, std::int32_t*);
std::int32_t uhash_puti_68(Icu::Hashtable*, void*, std::int32_t, std::int32_t*);
std::int32_t uhash_iputi_68(Icu::Hashtable*, std::int32_t, std::int32_t, std::int32_t*);
void* uhash_remove_68(Icu::Hashtable*, const void*);
void* uhash_iremove_68(Icu::Hashtable*, std::int32_t);
std::int32_t uhash_removei_68(Icu::Hashtable*, const void*);
std::int32_t uhash_iremovei_68(Icu::Hashtable*, std::int32_t);
void uhash_removeAll_68(Icu::Hashtable*);
const Icu::HashElement* uhash_find_68(const Icu::Hashtable*, const void*);
const Icu::HashElement* uhash_nextElement_68(const Icu::Hashtable*, std::int32_t*);
void* uhash_removeElement_68(Icu::Hashtable*, const Icu::HashElement*);
std::int32_t uhash_hashChars_68(UHashTok);
std::int8_t uhash_compareChars_68(UHashTok, UHashTok);
std::int8_t uhash_equals_68(const Icu::Hashtable*, const Icu::Hashtable*);
}

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocationError = 7;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU HASHTABLE: %s\n", message);
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

std::int32_t Mix(std::int32_t value) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) * 2654435761u);
}

std::int32_t APS5_VABI GuestHashInteger(UHashTok key) {
    return Mix(key.integer);
}

std::int8_t APS5_VABI GuestCompareInteger(UHashTok left, UHashTok right) {
    return left.integer == right.integer;
}

std::int8_t APS5_VABI GuestOtherCompare(UHashTok left, UHashTok right) {
    return left.integer == right.integer;
}

std::int32_t HostHashInteger(UHashTok key) {
    return Mix(key.integer);
}

std::int8_t HostCompareInteger(UHashTok left, UHashTok right) {
    return left.integer == right.integer;
}

std::int32_t APS5_VABI GuestConstantHash(UHashTok) {
    return 77;
}

std::int32_t HostConstantHash(UHashTok) {
    return 77;
}

std::int32_t APS5_VABI GuestNegativeHash(UHashTok key) {
    return static_cast<std::int32_t>(0x80000000u | static_cast<std::uint32_t>(key.integer));
}

std::int32_t HostNegativeHash(UHashTok key) {
    return static_cast<std::int32_t>(0x80000000u | static_cast<std::uint32_t>(key.integer));
}

struct Heap {
    std::set<void*> live;
    int failAt = -1;
    int allocations = 0;
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    if (heap.failAt == 0) {
        heap.failAt = -1;
        return nullptr;
    }
    if (heap.failAt > 0) --heap.failAt;
    ++heap.allocations;
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

std::vector<void*> guestKeyDeletes;
std::vector<void*> guestValueDeletes;
std::vector<void*> hostKeyDeletes;
std::vector<void*> hostValueDeletes;

void APS5_VABI GuestDeleteKey(void* pointer) {
    guestKeyDeletes.push_back(pointer);
}

void APS5_VABI GuestDeleteValue(void* pointer) {
    guestValueDeletes.push_back(pointer);
}

void HostDeleteKey(void* pointer) {
    hostKeyDeletes.push_back(pointer);
}

void HostDeleteValue(void* pointer) {
    hostValueDeletes.push_back(pointer);
}

void ClearDeletes() {
    guestKeyDeletes.clear();
    guestValueDeletes.clear();
    hostKeyDeletes.clear();
    hostValueDeletes.clear();
}

void SameDeletes(const char* message) {
    Require(guestKeyDeletes == hostKeyDeletes, message);
    Require(guestValueDeletes == hostValueDeletes, message);
}

struct HashElementsGuard {
    Icu::HashElement* elements;
};

std::array<char, 64> keyStorage[200];
std::array<int, 400> valueStorage{};

void* KeyText(std::size_t index) {
    return keyStorage[index].data();
}

void* Value(std::size_t index) {
    return &valueStorage[index];
}

std::int32_t Low(const void* pointer) {
    return static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

void SameTables(const Icu::Hashtable* guest, const Icu::Hashtable* host, const char* message, bool integers = false) {
    Require(uhash_count_nid_postfix(guest) == uhash_count_68(host), message);
    Require(guest->length == host->length && guest->primeIndex == host->primeIndex, message);
    Require(guest->highWaterMark == host->highWaterMark && guest->lowWaterMark == host->lowWaterMark, message);
    Require(guest->highWaterRatio == host->highWaterRatio && guest->lowWaterRatio == host->lowWaterRatio, message);
    std::int32_t guestPosition = -1;
    std::int32_t hostPosition = -1;
    for (;;) {
        const Icu::HashElement* left = uhash_nextElement_nid_postfix(guest, &guestPosition);
        const Icu::HashElement* right = uhash_nextElement_68(host, &hostPosition);
        Require((left == nullptr) == (right == nullptr), message);
        if (!left) break;
        Require(guestPosition == hostPosition, message);
        Require(left->hashcode == right->hashcode, message);
        if (integers) Require(left->key.integer == right->key.integer && left->value.integer == right->value.integer, message);
        else Require(left->key.pointer == right->key.pointer && left->value.pointer == right->value.pointer, message);
    }
    Require(guestPosition == hostPosition, message);
}

void CheckIntegerSweep(std::uint32_t seed, std::int32_t size, bool sized) {
    std::int32_t guestStatus = Ok;
    std::int32_t hostStatus = Ok;
    Icu::Hashtable* guest = sized ? uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, size, &guestStatus)
                                  : uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, &guestStatus);
    Icu::Hashtable* host = sized ? uhash_openSize_68(HostHashInteger, HostCompareInteger, nullptr, size, &hostStatus)
                                 : uhash_open_68(HostHashInteger, HostCompareInteger, nullptr, &hostStatus);
    Require(guest && host && guestStatus == Ok && hostStatus == Ok && guest->allocated == 1, "integer tables open");
    SameTables(guest, host, "fresh integer tables match", true);
    std::mt19937 random(seed);
    for (int step = 0; step < 6000; ++step) {
        const auto key = static_cast<std::int32_t>(random() % 700) - 50;
        const auto value = static_cast<std::int32_t>(random() % 1000) - 500;
        const unsigned operation = random() % 100;
        if (operation < 30) {
            Require(uhash_iputi_nid_postfix(guest, key, value, &guestStatus) == uhash_iputi_68(host, key, value, &hostStatus), "iputi results match");
        } else if (operation < 45) {
            void* pointer = value % 5 == 0 ? nullptr : Value(static_cast<std::size_t>(value + 500) % 400);
            Require(Low(uhash_iput_nid_postfix(guest, key, pointer, &guestStatus)) == Low(uhash_iput_68(host, key, pointer, &hostStatus)), "iput results match");
        } else if (operation < 60) {
            Require(uhash_iremovei_nid_postfix(guest, key) == uhash_iremovei_68(host, key), "iremovei results match");
        } else if (operation < 70) {
            Require(Low(uhash_iremove_nid_postfix(guest, key)) == Low(uhash_iremove_68(host, key)), "iremove results match");
        } else if (operation < 80) {
            Require(uhash_igeti_nid_postfix(guest, key) == uhash_igeti_68(host, key), "igeti results match");
        } else if (operation < 90) {
            Require(Low(uhash_iget_nid_postfix(guest, key)) == Low(uhash_iget_68(host, key)), "iget results match");
        } else if (operation < 97) {
            const auto policy = static_cast<std::int32_t>(random() % 3);
            uhash_setResizePolicy_nid_postfix(guest, policy);
            uhash_setResizePolicy_68(host, policy);
        } else if (operation < 98) {
            uhash_removeAll_nid_postfix(guest);
            uhash_removeAll_68(host);
            Require(uhash_count_nid_postfix(guest) == 0, "removeAll empties the table");
        } else {
            std::int32_t guestPosition = -1;
            std::int32_t hostPosition = -1;
            const Icu::HashElement* element = uhash_nextElement_nid_postfix(guest, &guestPosition);
            const Icu::HashElement* hostElement = uhash_nextElement_68(host, &hostPosition);
            Require((element == nullptr) == (hostElement == nullptr), "both tables agree on a first element");
            if (element) {
                Require(Low(uhash_removeElement_nid_postfix(guest, element)) == Low(uhash_removeElement_68(host, hostElement)), "removeElement results match");
                Require(element->hashcode == static_cast<std::int32_t>(0x80000000u), "removeElement leaves the deleted marker");
                Require(uhash_removeElement_nid_postfix(guest, element) == nullptr, "removing a deleted slot returns null");
            }
        }
        Require(guestStatus == hostStatus, "statuses match");
        if (step % 37 == 0) SameTables(guest, host, "integer tables match during the sweep", true);
    }
    SameTables(guest, host, "integer tables match after the sweep", true);
    uhash_close_nid_postfix(guest);
    uhash_close_68(host);
}

void CheckPointerSweep(std::uint32_t seed) {
    std::int32_t guestStatus = Ok;
    std::int32_t hostStatus = Ok;
    Icu::Hashtable* guest = uhash_open_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr, &guestStatus);
    Icu::Hashtable* host = uhash_open_68(uhash_hashChars_68, uhash_compareChars_68, nullptr, &hostStatus);
    Require(guest && host, "string tables open");
    Require(uhash_setKeyDeleter_nid_postfix(guest, GuestDeleteKey) == nullptr, "the first key deleter replaces null");
    Require(uhash_setValueDeleter_nid_postfix(guest, GuestDeleteValue) == nullptr, "the first value deleter replaces null");
    uhash_setKeyDeleter_68(host, HostDeleteKey);
    uhash_setValueDeleter_68(host, HostDeleteValue);
    ClearDeletes();
    std::mt19937 random(seed);
    for (int step = 0; step < 4000; ++step) {
        const std::size_t keyIndex = random() % 200;
        const std::size_t valueIndex = random() % 400;
        void* key = KeyText(keyIndex);
        const unsigned operation = random() % 100;
        if (operation < 35) {
            void* value = valueIndex % 9 == 0 ? nullptr : Value(valueIndex);
            Require(uhash_put_nid_postfix(guest, key, value, &guestStatus) == uhash_put_68(host, key, value, &hostStatus), "put results match");
        } else if (operation < 60) {
            Require(uhash_remove_nid_postfix(guest, key) == uhash_remove_68(host, key), "remove results match");
        } else if (operation < 80) {
            Require(uhash_get_nid_postfix(guest, key) == uhash_get_68(host, key), "get results match");
        } else if (operation < 95) {
            const Icu::HashElement* left = uhash_find_nid_postfix(guest, key);
            const Icu::HashElement* right = uhash_find_68(host, key);
            Require((left == nullptr) == (right == nullptr), "find agrees on presence");
            if (left) Require(left->key.pointer == right->key.pointer && left->value.pointer == right->value.pointer, "find returns the same entry");
        } else if (operation < 99) {
            const auto policy = static_cast<std::int32_t>(random() % 3);
            uhash_setResizePolicy_nid_postfix(guest, policy);
            uhash_setResizePolicy_68(host, policy);
        } else {
            uhash_removeAll_nid_postfix(guest);
            uhash_removeAll_68(host);
        }
        Require(guestStatus == hostStatus, "string statuses match");
        SameDeletes("deleters run on the same pointers in the same order");
        if (step % 29 == 0) SameTables(guest, host, "string tables match during the sweep");
    }
    SameTables(guest, host, "string tables match after the sweep");
    uhash_close_nid_postfix(guest);
    uhash_close_68(host);
    SameDeletes("close deletes every remaining key and value in table order");
    ClearDeletes();
}

void CheckSizes() {
    for (std::int32_t size : {-5, 0, 1, 7, 8, 13, 14, 100, 1000, 65521, 65522, 2147483647}) {
        std::int32_t guestStatus = Ok;
        std::int32_t hostStatus = Ok;
        if (size > 70000) {
            Icu::Hashtable guestFill{};
            Icu::Hashtable hostFill{};
            guestStatus = IllegalArgument;
            hostStatus = IllegalArgument;
            Require(uhash_initSize_nid_postfix(&guestFill, GuestHashInteger, GuestCompareInteger, nullptr, size, &guestStatus) == nullptr,
                    "a failed status stops initSize");
            Require(uhash_initSize_68(&hostFill, HostHashInteger, HostCompareInteger, nullptr, size, &hostStatus) == nullptr, "host initSize stops");
            continue;
        }
        Icu::Hashtable* guest = uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, size, &guestStatus);
        Icu::Hashtable* host = uhash_openSize_68(HostHashInteger, HostCompareInteger, nullptr, size, &hostStatus);
        Require(guest && host && guestStatus == Ok && hostStatus == Ok, "sized tables open");
        SameTables(guest, host, "openSize chooses the same prime", true);
        uhash_close_nid_postfix(guest);
        uhash_close_68(host);
        Icu::Hashtable guestFill;
        Icu::Hashtable hostFill;
        std::memset(&guestFill, 0xA5, sizeof(guestFill));
        std::memset(&hostFill, 0xA5, sizeof(hostFill));
        Require(uhash_initSize_nid_postfix(&guestFill, GuestHashInteger, GuestCompareInteger, nullptr, size, &guestStatus) == &guestFill,
                "initSize returns the fill-in");
        Require(uhash_initSize_68(&hostFill, HostHashInteger, HostCompareInteger, nullptr, size, &hostStatus) == &hostFill, "host initSize");
        Require(guestFill.allocated == 0 && guestFill.keyDeleter == nullptr && guestFill.valueDeleter == nullptr, "initSize clears the deleters");
        SameTables(&guestFill, &hostFill, "initSize chooses the same prime", true);
        uhash_close_nid_postfix(&guestFill);
        uhash_close_68(&hostFill);
        Require(guestFill.elements == nullptr, "close of a fill-in clears the elements");
    }
    std::int32_t status = Ok;
    Icu::Hashtable fill;
    Require(uhash_init_nid_postfix(&fill, GuestHashInteger, GuestCompareInteger, GuestCompareInteger, &status) == &fill, "init fills in");
    Require(fill.length == 127 && fill.primeIndex == 4 && fill.valueComparator == GuestCompareInteger, "init uses the default prime 127");
    uhash_close_nid_postfix(&fill);
}

void CheckFixedPolicy() {
    std::int32_t guestStatus = Ok;
    std::int32_t hostStatus = Ok;
    Icu::Hashtable* guest = uhash_openSize_nid_postfix(GuestConstantHash, GuestCompareInteger, nullptr, 7, &guestStatus);
    Icu::Hashtable* host = uhash_openSize_68(HostConstantHash, HostCompareInteger, nullptr, 7, &hostStatus);
    uhash_setResizePolicy_nid_postfix(guest, 2);
    uhash_setResizePolicy_68(host, 2);
    uhash_setValueDeleter_nid_postfix(guest, GuestDeleteValue);
    uhash_setValueDeleter_68(host, HostDeleteValue);
    ClearDeletes();
    for (std::int32_t key = 0; key < 9; ++key) {
        guestStatus = Ok;
        hostStatus = Ok;
        Require(uhash_iput_nid_postfix(guest, key, Value(static_cast<std::size_t>(key)), &guestStatus) ==
                    uhash_iput_68(host, key, Value(static_cast<std::size_t>(key)), &hostStatus),
                "fixed iput results match");
        Require(guestStatus == hostStatus, "fixed statuses match");
        Require(guestStatus == (key >= 6 ? MemoryAllocationError : Ok), "a fixed table refuses the slot that would fill it");
        SameTables(guest, host, "fixed tables match", true);
    }
    SameDeletes("a refused put deletes the adopted value");
    Require(guestValueDeletes.size() == 3, "three refused values are deleted");
    ClearDeletes();
    uhash_close_nid_postfix(guest);
    uhash_close_68(host);
    SameDeletes("closing a fixed table deletes the stored values");
    ClearDeletes();
}

void CheckErrors() {
    std::int32_t status = Ok;
    Require(Rejects([] { uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, nullptr); }), "open rejects a null status");
    Require(Rejects([] { uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, 5, nullptr); }), "openSize rejects a null status");
    Require(Rejects([] { uhash_init_nid_postfix(nullptr, GuestHashInteger, GuestCompareInteger, nullptr, nullptr); }), "init rejects a null status first");
    Require(Rejects([] { uhash_initSize_nid_postfix(nullptr, GuestHashInteger, GuestCompareInteger, nullptr, 5, nullptr); }),
            "initSize rejects a null status first");
    status = IllegalArgument;
    Require(uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, &status) == nullptr && status == IllegalArgument,
            "open keeps a failed status");
    Require(uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, 5, &status) == nullptr && status == IllegalArgument,
            "openSize keeps a failed status");
    Require(uhash_init_nid_postfix(nullptr, GuestHashInteger, GuestCompareInteger, nullptr, &status) == nullptr,
            "init with a failed status returns before the fill-in");
    Icu::Hashtable untouched;
    std::memset(&untouched, 0x5A, sizeof(untouched));
    Require(uhash_init_nid_postfix(&untouched, GuestHashInteger, GuestCompareInteger, nullptr, &status) == nullptr, "init keeps a failed status");
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&untouched);
    for (std::size_t index = 0; index < sizeof(untouched); ++index) Require(bytes[index] == 0x5A, "a failed init leaves the fill-in untouched");
    status = Ok;
    Require(Rejects([&] { uhash_init_nid_postfix(nullptr, GuestHashInteger, GuestCompareInteger, nullptr, &status); }), "init rejects a null fill-in");
    Require(Rejects([&] { uhash_initSize_nid_postfix(nullptr, GuestHashInteger, GuestCompareInteger, nullptr, 3, &status); }),
            "initSize rejects a null fill-in");

    uhash_close_nid_postfix(nullptr);
    Require(Rejects([] { uhash_count_nid_postfix(nullptr); }), "count rejects a null table");
    Require(Rejects([] { uhash_get_nid_postfix(nullptr, nullptr); }), "get rejects a null table");
    Require(Rejects([] { uhash_iget_nid_postfix(nullptr, 1); }), "iget rejects a null table");
    Require(Rejects([] { uhash_geti_nid_postfix(nullptr, nullptr); }), "geti rejects a null table");
    Require(Rejects([] { uhash_igeti_nid_postfix(nullptr, 1); }), "igeti rejects a null table");
    Require(Rejects([&] { uhash_put_nid_postfix(nullptr, nullptr, nullptr, &status); }), "put rejects a null table");
    Require(Rejects([&] { uhash_iput_nid_postfix(nullptr, 1, nullptr, &status); }), "iput rejects a null table");
    Require(Rejects([&] { uhash_puti_nid_postfix(nullptr, nullptr, 1, &status); }), "puti rejects a null table");
    Require(Rejects([&] { uhash_iputi_nid_postfix(nullptr, 1, 1, &status); }), "iputi rejects a null table");
    Require(Rejects([] { uhash_remove_nid_postfix(nullptr, nullptr); }), "remove rejects a null table");
    Require(Rejects([] { uhash_iremove_nid_postfix(nullptr, 1); }), "iremove rejects a null table");
    Require(Rejects([] { uhash_removei_nid_postfix(nullptr, nullptr); }), "removei rejects a null table");
    Require(Rejects([] { uhash_iremovei_nid_postfix(nullptr, 1); }), "iremovei rejects a null table");
    Require(Rejects([] { uhash_removeAll_nid_postfix(nullptr); }), "removeAll rejects a null table");
    Require(Rejects([] { uhash_find_nid_postfix(nullptr, nullptr); }), "find rejects a null table");
    Require(Rejects([] {
        std::int32_t position = -1;
        uhash_nextElement_nid_postfix(nullptr, &position);
    }), "nextElement rejects a null table");
    Require(Rejects([] { uhash_removeElement_nid_postfix(nullptr, nullptr); }), "removeElement rejects a null table");
    Require(Rejects([] { uhash_setKeyHasher_nid_postfix(nullptr, nullptr); }), "setKeyHasher rejects a null table");
    Require(Rejects([] { uhash_setKeyComparator_nid_postfix(nullptr, nullptr); }), "setKeyComparator rejects a null table");
    Require(Rejects([] { uhash_setValueComparator_nid_postfix(nullptr, nullptr); }), "setValueComparator rejects a null table");
    Require(Rejects([] { uhash_setKeyDeleter_nid_postfix(nullptr, nullptr); }), "setKeyDeleter rejects a null table");
    Require(Rejects([] { uhash_setValueDeleter_nid_postfix(nullptr, nullptr); }), "setValueDeleter rejects a null table");
    Require(Rejects([] { uhash_setResizePolicy_nid_postfix(nullptr, 0); }), "setResizePolicy rejects a null table");

    Icu::Hashtable* table = uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, &status);
    Require(table && status == Ok, "a table opens");
    Require(Rejects([&] { uhash_iputi_nid_postfix(table, 1, 1, nullptr); }), "iputi rejects a null status");
    Require(Rejects([&] { uhash_nextElement_nid_postfix(table, nullptr); }), "nextElement rejects a null position");
    Require(Rejects([&] { uhash_removeElement_nid_postfix(table, nullptr); }), "removeElement rejects a null element");
    Require(Rejects([&] { uhash_setResizePolicy_nid_postfix(table, 3); }), "setResizePolicy rejects a policy past U_FIXED");
    Require(Rejects([&] { uhash_setResizePolicy_nid_postfix(table, -1); }), "setResizePolicy rejects a negative policy");
    Require(uhash_setKeyHasher_nid_postfix(table, GuestConstantHash) == GuestHashInteger, "setKeyHasher returns the old hasher");
    Require(uhash_setKeyHasher_nid_postfix(table, GuestHashInteger) == GuestConstantHash, "setKeyHasher stores the new hasher");
    Require(uhash_setKeyComparator_nid_postfix(table, GuestOtherCompare) == GuestCompareInteger, "setKeyComparator returns the old comparator");
    Require(uhash_setKeyComparator_nid_postfix(table, GuestCompareInteger) == GuestOtherCompare, "setKeyComparator stores the new comparator");
    Require(uhash_setValueComparator_nid_postfix(table, GuestOtherCompare) == nullptr, "setValueComparator returns the old comparator");
    Require(uhash_setValueComparator_nid_postfix(table, nullptr) == GuestOtherCompare, "setValueComparator stores the new comparator");
    Require(uhash_setKeyDeleter_nid_postfix(table, GuestDeleteKey) == nullptr && uhash_setKeyDeleter_nid_postfix(table, nullptr) == GuestDeleteKey,
            "setKeyDeleter swaps the deleter");
    Require(uhash_setValueDeleter_nid_postfix(table, GuestDeleteValue) == nullptr &&
                uhash_setValueDeleter_nid_postfix(table, nullptr) == GuestDeleteValue,
            "setValueDeleter swaps the deleter");

    uhash_setKeyDeleter_nid_postfix(table, GuestDeleteKey);
    uhash_setValueDeleter_nid_postfix(table, GuestDeleteValue);
    ClearDeletes();
    status = IllegalArgument;
    Require(uhash_put_nid_postfix(table, KeyText(1), Value(2), &status) == nullptr && status == IllegalArgument, "a failed put returns null");
    Require(guestKeyDeletes.size() == 1 && guestKeyDeletes[0] == KeyText(1), "a failed put deletes the adopted key");
    Require(guestValueDeletes.size() == 1 && guestValueDeletes[0] == Value(2), "a failed put deletes the adopted value");
    Require(uhash_count_nid_postfix(table) == 0, "a failed put stores nothing");
    ClearDeletes();
    status = Ok;
    uhash_iput_nid_postfix(table, 5, Value(5), &status);
    Require(uhash_iput_nid_postfix(table, 5, Value(5), &status) == nullptr && guestValueDeletes.empty(), "storing the same value again deletes nothing");
    Require(uhash_iput_nid_postfix(table, 5, Value(6), &status) == nullptr && guestValueDeletes.size() == 1 && guestValueDeletes[0] == Value(5),
            "replacing a value deletes the old one and returns null with a value deleter");
    Require(uhash_iput_nid_postfix(table, 5, nullptr, &status) == nullptr && uhash_count_nid_postfix(table) == 0, "a null value removes the key");
    Require(guestValueDeletes.size() == 2 && guestValueDeletes[1] == Value(6), "removal through a null value deletes the stored value");
    uhash_setKeyDeleter_nid_postfix(table, nullptr);
    uhash_setValueDeleter_nid_postfix(table, nullptr);
    Require(uhash_iputi_nid_postfix(table, 9, 0, &status) == 0 && uhash_count_nid_postfix(table) == 1, "iputi stores a zero value");
    Require(uhash_iputi_nid_postfix(table, 9, 4, &status) == 0 && uhash_igeti_nid_postfix(table, 9) == 4, "iputi returns the old integer");
    Require(uhash_iputi_nid_postfix(table, 9, 8, &status) == 4, "iputi returns the replaced integer");
    Require(uhash_iremovei_nid_postfix(table, 9) == 8 && uhash_iremovei_nid_postfix(table, 9) == 0, "iremovei returns the removed integer once");
    Require(uhash_find_nid_postfix(table, nullptr) == nullptr, "find misses an absent key");
    uhash_close_nid_postfix(table);
    Icu::Hashtable* strings = uhash_open_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr, &status);
    Icu::Hashtable* hostStrings = uhash_open_68(uhash_hashChars_68, uhash_compareChars_68, nullptr, &status);
    for (std::size_t index = 0; index < 150; ++index) {
        const auto value = static_cast<std::int32_t>(index * 3 + 1);
        Require(uhash_puti_nid_postfix(strings, KeyText(index), value, &status) == uhash_puti_68(hostStrings, KeyText(index), value, &status),
                "puti results match");
    }
    Require(uhash_puti_nid_postfix(strings, KeyText(4), 99, &status) == 13, "puti returns the replaced integer");
    uhash_puti_68(hostStrings, KeyText(4), 99, &status);
    for (std::size_t index = 0; index < 150; index += 2) {
        Require(uhash_geti_nid_postfix(strings, KeyText(index)) == uhash_geti_68(hostStrings, KeyText(index)), "geti results match");
        Require(uhash_removei_nid_postfix(strings, KeyText(index)) == uhash_removei_68(hostStrings, KeyText(index)), "removei results match");
        Require(uhash_geti_nid_postfix(strings, KeyText(index)) == 0, "geti misses a removed key");
    }
    SameTables(strings, hostStrings, "string integer tables match", true);
    uhash_close_nid_postfix(strings);
    uhash_close_68(hostStrings);
    ClearDeletes();

    Icu::Hashtable* noHasher = uhash_open_nid_postfix(nullptr, nullptr, nullptr, &status);
    Require(noHasher && status == Ok, "open accepts null callbacks as ICU only asserts");
    Require(Rejects([&] { uhash_iputi_nid_postfix(noHasher, 1, 1, &status); }), "a put calls the null hasher and throws");
    uhash_setKeyHasher_nid_postfix(noHasher, GuestConstantHash);
    Require(uhash_iputi_nid_postfix(noHasher, 1, 1, &status) == 0, "the first put of a key compares nothing");
    Require(Rejects([&] { uhash_igeti_nid_postfix(noHasher, 2); }), "a lookup that meets an equal hash calls the null comparator and throws");
    uhash_close_nid_postfix(noHasher);
}

void CheckNegativeHashes() {
    std::int32_t guestStatus = Ok;
    std::int32_t hostStatus = Ok;
    Icu::Hashtable* guest = uhash_open_nid_postfix(GuestNegativeHash, GuestCompareInteger, nullptr, &guestStatus);
    Icu::Hashtable* host = uhash_open_68(HostNegativeHash, HostCompareInteger, nullptr, &hostStatus);
    for (std::int32_t key = 0; key < 300; key += 3) {
        uhash_iputi_nid_postfix(guest, key, key + 1, &guestStatus);
        uhash_iputi_68(host, key, key + 1, &hostStatus);
    }
    for (std::int32_t key = 0; key < 300; key += 7) Require(uhash_iremovei_nid_postfix(guest, key) == uhash_iremovei_68(host, key), "negative removes");
    SameTables(guest, host, "negative hash codes are stored masked to 31 bits", true);
    uhash_close_nid_postfix(guest);
    uhash_close_68(host);
}

void CheckEquals() {
    std::int32_t status = Ok;
    Icu::Hashtable* left = uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, GuestCompareInteger, &status);
    Icu::Hashtable* right = uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, GuestCompareInteger, 500, &status);
    Icu::Hashtable* plain = uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, &status);
    Icu::Hashtable* other = uhash_open_nid_postfix(GuestHashInteger, GuestOtherCompare, GuestCompareInteger, &status);
    Require(uhash_equals_nid_postfix(left, left) == 1, "a table equals itself");
    Require(uhash_equals_nid_postfix(nullptr, nullptr) == 1, "two null tables are the same pointer");
    Require(uhash_equals_nid_postfix(left, nullptr) == 0 && uhash_equals_nid_postfix(nullptr, left) == 0, "a null table equals nothing else");
    Require(uhash_equals_nid_postfix(plain, plain) == 1, "the same table needs no value comparator");
    Require(uhash_equals_nid_postfix(left, plain) == 0, "different value comparators are unequal");
    Require(uhash_equals_nid_postfix(left, other) == 0, "different key comparators are unequal");
    Require(uhash_equals_nid_postfix(left, right) == 1, "empty compatible tables are equal");
    for (std::int32_t key = 0; key < 40; ++key) {
        uhash_iputi_nid_postfix(left, key, key * 3, &status);
        uhash_iputi_nid_postfix(right, 39 - key, (39 - key) * 3, &status);
    }
    Require(uhash_equals_nid_postfix(left, right) == 1, "tables with equal pairs are equal across sizes");
    uhash_iputi_nid_postfix(right, 7, 5, &status);
    Require(uhash_equals_nid_postfix(left, right) == 0, "a differing value makes tables unequal");
    uhash_iremovei_nid_postfix(right, 7);
    Require(uhash_equals_nid_postfix(left, right) == 0, "a differing count makes tables unequal");
    Icu::Hashtable* plainCopy = uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, &status);
    Require(uhash_equals_nid_postfix(plain, plainCopy) == 0, "tables without a value comparator are unequal");
    uhash_close_nid_postfix(left);
    uhash_close_nid_postfix(right);
    uhash_close_nid_postfix(plain);
    uhash_close_nid_postfix(other);
    uhash_close_nid_postfix(plainCopy);
}

void CheckAllocationFailures() {
    std::int32_t status = Ok;
    heap.failAt = 0;
    Require(uhash_open_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, &status) == nullptr && status == MemoryAllocationError,
            "open reports a failed table allocation");
    Require(heap.live.empty(), "a failed open leaks nothing");
    status = Ok;
    heap.failAt = 1;
    Require(uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, 20, &status) == nullptr && status == MemoryAllocationError,
            "openSize reports a failed element allocation");
    Require(heap.live.empty(), "a failed element allocation frees the table");
    status = Ok;
    Icu::Hashtable fill{};
    heap.failAt = 0;
    Require(uhash_init_nid_postfix(&fill, GuestHashInteger, GuestCompareInteger, nullptr, &status) == nullptr && status == MemoryAllocationError,
            "init reports a failed element allocation");
    Require(fill.elements == nullptr && fill.keyHasher == GuestHashInteger && fill.length == 127, "a failed init keeps the fields it set");
    status = Ok;
    Icu::Hashtable* table = uhash_openSize_nid_postfix(GuestHashInteger, GuestCompareInteger, nullptr, 7, &status);
    uhash_setValueDeleter_nid_postfix(table, GuestDeleteValue);
    for (std::int32_t key = 0; key < 4; ++key) uhash_iput_nid_postfix(table, key, Value(static_cast<std::size_t>(key)), &status);
    Require(status == Ok && table->count == 4 && table->highWaterMark == 3 && table->primeIndex == 0, "a small table reaches its high water mark");
    HashElementsGuard guard{table->elements};
    ClearDeletes();
    heap.failAt = 0;
    Require(uhash_iput_nid_postfix(table, 9, Value(9), &status) == nullptr && status == MemoryAllocationError, "a failed rehash fails the put");
    Require(guestValueDeletes.size() == 1 && guestValueDeletes[0] == Value(9), "a failed rehash deletes the adopted value");
    Require(table->elements == guard.elements && table->length == 7 && table->count == 4, "a failed rehash keeps the old elements");
    Require(table->primeIndex == 1, "a failed rehash leaves the next prime index as ICU does");
    status = Ok;
    Require(uhash_iget_nid_postfix(table, 2) == Value(2), "the table still answers after a failed rehash");
    uhash_close_nid_postfix(table);
    ClearDeletes();
    Require(heap.live.empty(), "closing frees the table and its elements");
}

void CheckDeleteHashtable() {
    uhash_deleteHashtable_nid_postfix(nullptr);
    struct HashtableObject {
        Icu::Hashtable* hash;
        Icu::Hashtable hashObject;
    };
    auto* object = static_cast<HashtableObject*>(uprv_malloc_nid_postfix(sizeof(HashtableObject)));
    std::int32_t status = Ok;
    object->hash = uhash_init_nid_postfix(&object->hashObject, GuestHashInteger, GuestCompareInteger, nullptr, &status);
    Require(object->hash == &object->hashObject, "the embedded table is initialized");
    uhash_setValueDeleter_nid_postfix(object->hash, GuestDeleteValue);
    ClearDeletes();
    for (std::int32_t key = 0; key < 5; ++key) uhash_iput_nid_postfix(object->hash, key, Value(static_cast<std::size_t>(key)), &status);
    uhash_deleteHashtable_nid_postfix(object);
    Require(guestValueDeletes.size() == 5, "deleteHashtable closes the table and deletes its values");
    ClearDeletes();
    auto* empty = static_cast<HashtableObject*>(uprv_malloc_nid_postfix(sizeof(HashtableObject)));
    empty->hash = nullptr;
    uhash_deleteHashtable_nid_postfix(empty);
    Require(guestValueDeletes.empty(), "a Hashtable without a table only frees the object");
}

} // namespace

int main() {
    std::int32_t memoryStatus = Ok;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &memoryStatus);
    Require(memoryStatus == Ok, "the tracking allocator is installed");
    for (std::size_t index = 0; index < 200; ++index) std::snprintf(keyStorage[index].data(), keyStorage[index].size(), "key-%zu-%zu", index, index * 7919);
    CheckErrors();
    CheckSizes();
    CheckFixedPolicy();
    CheckNegativeHashes();
    CheckEquals();
    CheckDeleteHashtable();
    CheckAllocationFailures();
    for (std::uint32_t seed = 1; seed <= 4; ++seed) {
        CheckIntegerSweep(seed, 0, false);
        CheckIntegerSweep(seed * 31, static_cast<std::int32_t>(seed * 250), true);
        CheckPointerSweep(seed);
    }
    Require(heap.live.empty(), "every table allocation is released");
    Require(heap.allocations > 100, "tables allocate through the guest memory functions");
    return 0;
}

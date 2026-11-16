#include "prx/libSceIcu/Vectors.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/CoreClasses.hpp"
#include <unicode/udata.h>
#include "uelement.h"
#include "uvector.h"
#include "uvectr32.h"
#include <cerrno>
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
    std::fprintf(stderr, "ICU VECTORS: %s\n", message);
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

std::vector<void*> hostDeleted;
std::vector<void*> guestDeleted;

void U_CALLCONV HostDelete(void* object) {
    hostDeleted.push_back(object);
}

void APS5_VABI GuestDelete_nid_no_patch(void* object) {
    guestDeleted.push_back(object);
}

std::int32_t Low(std::int32_t value) {
    return value % 7;
}

UBool U_CALLCONV HostEqual(const UElement left, const UElement right) {
    return Low(left.integer) == Low(right.integer);
}

std::int8_t APS5_VABI GuestEqual_nid_no_patch(UHashTok left, UHashTok right) {
    return Low(left.integer) == Low(right.integer) ? 1 : 0;
}

int8_t U_CALLCONV HostOrder(UElement left, UElement right) {
    return left.integer < right.integer ? -1 : left.integer == right.integer ? 0 : 1;
}

std::int8_t APS5_VABI GuestKeyEqual_nid_no_patch(UHashTok key, UHashTok element) {
    return static_cast<const UHashTok*>(key.pointer)->integer == element.integer ? 1 : 0;
}

std::int8_t APS5_VABI GuestOrder_nid_no_patch(UHashTok left, UHashTok right) {
    return left.integer < right.integer ? -1 : left.integer == right.integer ? 0 : 1;
}

int32_t U_CALLCONV HostCompare(const void* context, const void* left, const void* right) {
    const std::int32_t step = *static_cast<const std::int32_t*>(context);
    const std::int32_t a = static_cast<const UElement*>(left)->integer % step;
    const std::int32_t b = static_cast<const UElement*>(right)->integer % step;
    return a < b ? -1 : a == b ? 0 : 1;
}

std::int32_t APS5_VABI GuestCompare_nid_no_patch(const void* context, const void* left, const void* right) {
    const std::int32_t step = *static_cast<const std::int32_t*>(context);
    const std::int32_t a = static_cast<const UHashTok*>(left)->integer % step;
    const std::int32_t b = static_cast<const UHashTok*>(right)->integer % step;
    return a < b ? -1 : a == b ? 0 : 1;
}

void U_CALLCONV HostAssign(UElement* destination, UElement* source) {
    destination->pointer = nullptr;
    destination->integer = source->integer + 1;
}

void APS5_VABI GuestAssign_nid_no_patch(UHashTok* destination, UHashTok* source) {
    destination->pointer = nullptr;
    destination->integer = source->integer + 1;
}

static_assert(sizeof(icu::UStack) == sizeof(Icu::Vector));
static_assert(sizeof(icu::UVector32) == sizeof(Icu::Vector32));

const Icu::Vector& View(const icu::UVector& host) {
    return reinterpret_cast<const Icu::Vector&>(host);
}

const Icu::Vector32& View(const icu::UVector32& host) {
    return reinterpret_cast<const Icu::Vector32&>(host);
}

void Same(const Icu::Vector& guest, const icu::UVector& host, const char* message) {
    const Icu::Vector& view = View(host);
    Require(guest.count == view.count && guest.capacity == view.capacity, message);
    Require(std::memcmp(guest.elements, view.elements, sizeof(UHashTok) * static_cast<std::size_t>(guest.count)) == 0, message);
    Require(hostDeleted == guestDeleted, message);
    Require(guest.vtable == Sentinel, "the vector keeps its vtable");
}

void Same(const Icu::Vector32& guest, const icu::UVector32& host, const char* message) {
    const Icu::Vector32& view = View(host);
    Require(guest.count == view.count && guest.capacity == view.capacity && guest.maxCapacity == view.maxCapacity, message);
    Require(std::memcmp(guest.elements, view.elements, sizeof(std::int32_t) * static_cast<std::size_t>(guest.count)) == 0, message);
    Require(guest.vtable == Sentinel, "the vector keeps its vtable");
}

Icu::Vector MakeGuest(std::int32_t capacity, bool owning, bool comparing) {
    Icu::Vector guest{Sentinel, 0, 0, nullptr, owning ? GuestDelete_nid_no_patch : nullptr, comparing ? GuestEqual_nid_no_patch : nullptr};
    std::int32_t status = 0;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&guest, capacity, &status);
    Require(status == 0 && guest.elements != nullptr, "a guest vector initializes");
    return guest;
}

std::int32_t objects[16];

void* Pointer(std::mt19937& random) {
    return &objects[random() % 16];
}

void CheckVectors() {
    std::mt19937 random(68);
    for (int round = 0; round < 48; ++round) {
        const bool owning = round % 2 == 0;
        const bool comparing = round % 4 >= 2;
        const std::int32_t capacity = static_cast<std::int32_t>(round % 5) - 1;
        UErrorCode hostStatus = U_ZERO_ERROR;
        icu::UStack host(owning ? HostDelete : nullptr, comparing ? HostEqual : nullptr, capacity, hostStatus);
        Require(U_SUCCESS(hostStatus), "a host stack opens");
        Icu::Vector guest = MakeGuest(capacity, owning, comparing);
        Same(guest, host, "a new vector");
        UErrorCode otherStatus = U_ZERO_ERROR;
        icu::UVector hostOther(otherStatus);
        Icu::Vector guestOther = MakeGuest(8, false, false);
        for (int i = 0; i < 6; ++i) {
            const std::int32_t value = static_cast<std::int32_t>(random() % 20);
            hostOther.addElement(value, otherStatus);
            std::int32_t status = 0;
            _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&guestOther, value, &status);
        }
        for (int step = 0; step < 60; ++step) {
            hostStatus = U_ZERO_ERROR;
            std::int32_t status = 0;
            const std::int32_t value = static_cast<std::int32_t>(random() % 20);
            const std::int32_t index = static_cast<std::int32_t>(random() % (guest.count + 3)) - 1;
            void* pointer = Pointer(random);
            switch (random() % 22) {
            case 0:
                host.addElement(value, hostStatus);
                _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&guest, value, &status);
                break;
            case 1:
                host.addElement(pointer, hostStatus);
                _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&guest, pointer, &status);
                break;
            case 2:
                host.insertElementAt(value, index, hostStatus);
                _ZN6icu_687UVector15insertElementAtEiiR10UErrorCode_nid_postfix(&guest, value, index, &status);
                break;
            case 3:
                host.insertElementAt(pointer, index, hostStatus);
                _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(&guest, pointer, index, &status);
                break;
            case 4:
                host.setElementAt(value, index);
                _ZN6icu_687UVector12setElementAtEii_nid_postfix(&guest, value, index);
                break;
            case 5:
                host.setElementAt(pointer, index);
                _ZN6icu_687UVector12setElementAtEPvi_nid_postfix(&guest, pointer, index);
                break;
            case 6:
                host.removeElementAt(index);
                _ZN6icu_687UVector15removeElementAtEi_nid_postfix(&guest, index);
                break;
            case 7:
                Require(host.orphanElementAt(index) == _ZN6icu_687UVector15orphanElementAtEi_nid_postfix(&guest, index), "orphanElementAt matches ICU");
                break;
            case 8:
                Require((host.removeElement(pointer) != 0) == (_ZN6icu_687UVector13removeElementEPv_nid_postfix(&guest, pointer) != 0), "removeElement matches ICU");
                break;
            case 9: {
                const std::int32_t size = static_cast<std::int32_t>(random() % 14) - 1;
                host.setSize(size, hostStatus);
                _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(&guest, size, &status);
                break;
            }
            case 10: {
                const std::int32_t minimum = static_cast<std::int32_t>(random() % 40) - 1;
                Require((host.ensureCapacity(minimum, hostStatus) != 0) == (_ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(&guest, minimum, &status) != 0),
                        "ensureCapacity matches ICU");
                break;
            }
            case 11: {
                UElement hostKey;
                hostKey.pointer = nullptr;
                hostKey.integer = value;
                UHashTok guestKey;
                guestKey.pointer = nullptr;
                guestKey.integer = value;
                host.sortedInsert(hostKey, HostOrder, hostStatus);
                _ZN6icu_687UVector12sortedInsertE8UElementPFaS1_S1_ER10UErrorCode_nid_postfix(&guest, guestKey, GuestOrder_nid_no_patch, &status);
                break;
            }
            case 12:
                host.sortedInsert(pointer, HostOrder, hostStatus);
                _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(&guest, pointer, GuestOrder_nid_no_patch, &status);
                break;
            case 13:
                host.sorti(hostStatus);
                _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(&guest, &status);
                break;
            case 14:
                host.sort(HostOrder, hostStatus);
                _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(&guest, GuestOrder_nid_no_patch, &status);
                break;
            case 15: {
                const std::int32_t modulus = 3;
                host.sortWithUComparator(HostCompare, &modulus, hostStatus);
                _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(&guest, GuestCompare_nid_no_patch, &modulus, &status);
                break;
            }
            case 16:
                host.assign(hostOther, HostAssign, hostStatus);
                _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(&guest, &guestOther, GuestAssign_nid_no_patch, &status);
                break;
            case 17:
                Require((host.removeAll(hostOther) != 0) == (_ZN6icu_687UVector9removeAllERKS0__nid_postfix(&guest, &guestOther) != 0), "removeAll matches ICU");
                break;
            case 18:
                Require((host.retainAll(hostOther) != 0) == (_ZN6icu_687UVector9retainAllERKS0__nid_postfix(&guest, &guestOther) != 0), "retainAll matches ICU");
                break;
            case 19:
                Require(host.pop() == _ZN6icu_686UStack3popEv_nid_postfix(&guest), "pop matches ICU");
                break;
            case 20:
                Require(host.popi() == _ZN6icu_686UStack4popiEv_nid_postfix(&guest), "popi matches ICU");
                break;
            default:
                host.removeAllElements();
                _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&guest);
                break;
            }
            Require(status == hostStatus, "the status matches ICU");
            Same(guest, host, "a vector operation matches ICU");
            for (std::int32_t probe = -1; probe <= guest.count; ++probe) {
                Require(host.elementAt(probe) == _ZNK6icu_687UVector9elementAtEi_nid_postfix(&guest, probe), "elementAt matches ICU");
                Require(host.elementAti(probe) == _ZNK6icu_687UVector10elementAtiEi_nid_postfix(&guest, probe), "elementAti matches ICU");
            }
            const std::int32_t start = static_cast<std::int32_t>(random() % (guest.count + 1));
            Require(host.indexOf(value, start) == _ZNK6icu_687UVector7indexOfEii_nid_postfix(&guest, value, start), "indexOf(int) matches ICU");
            Require(host.indexOf(pointer, start) == _ZNK6icu_687UVector7indexOfEPvi_nid_postfix(&guest, pointer, start), "indexOf(void*) matches ICU");
            UElement hostKey;
            hostKey.pointer = pointer;
            UHashTok guestKey;
            guestKey.pointer = pointer;
            for (std::int8_t hint : {0, 1}) {
                Require(host.indexOf(hostKey, start, hint) == _ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(&guest, guestKey, start, hint), "indexOf(UElement) matches ICU");
            }
            Require(host.search(pointer) == _ZNK6icu_686UStack6searchEPv_nid_postfix(&guest, pointer), "search matches ICU");
            Require((host.containsAll(hostOther) != 0) == (_ZNK6icu_687UVector11containsAllERKS0__nid_postfix(&guest, &guestOther) != 0), "containsAll matches ICU");
            Require((host.containsNone(hostOther) != 0) == (_ZNK6icu_687UVector12containsNoneERKS0__nid_postfix(&guest, &guestOther) != 0), "containsNone matches ICU");
            Require((host == hostOther) == (_ZN6icu_687UVectoreqERKS0__nid_postfix(&guest, &guestOther) != 0), "operator== matches ICU");
            if (!comparing) Require((host.equals(hostOther) != 0) == (_ZNK6icu_687UVector6equalsERKS0__nid_postfix(&guest, &guestOther) != 0), "equals matches ICU");
            std::vector<void*> hostArray(static_cast<std::size_t>(guest.count) + 1, nullptr);
            std::vector<void*> guestArray(static_cast<std::size_t>(guest.count) + 1, nullptr);
            host.toArray(hostArray.data());
            Require(_ZNK6icu_687UVector7toArrayEPPv_nid_postfix(&guest, guestArray.data()) == guestArray.data() && hostArray == guestArray, "toArray matches ICU");
        }
        _ZN6icu_687UVectorD1Ev_nid_postfix(&guestOther);
        hostDeleted.clear();
        guestDeleted.clear();
        _ZN6icu_686UStackD1Ev_nid_postfix(&guest);
        host.removeAllElements();
        Require(hostDeleted == guestDeleted, "the destructor deletes what ICU deletes");
        Require(guest.elements == nullptr, "the destructor frees the elements");
        hostDeleted.clear();
        guestDeleted.clear();
    }

    Icu::Vector guest = MakeGuest(4, false, false);
    Require(_ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(&guest, GuestDelete_nid_no_patch) == nullptr && guest.deleter == GuestDelete_nid_no_patch,
            "setDeleter returns the old deleter");
    Require(_ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(&guest, GuestEqual_nid_no_patch) == nullptr && guest.comparer == GuestEqual_nid_no_patch,
            "setComparer returns the old comparer");
    Require(_ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(&guest, nullptr) == GuestDelete_nid_no_patch, "setDeleter returns the previous deleter");
    std::int32_t status = 0;
    _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(&guest, 5, GuestOrder_nid_no_patch, &status);
    _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(&guest, 2, GuestOrder_nid_no_patch, &status);
    _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(&guest, 9, GuestOrder_nid_no_patch, &status);
    Require(status == 0 && guest.count == 3 && guest.elements[0].integer == 2 && guest.elements[1].integer == 5 && guest.elements[2].integer == 9,
            "sortedInsert(int) keeps the order");
    Require(Rejects([&] { _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(&guest, 1, nullptr, &status); }),
            "sortedInsert rejects a null comparator it would call");
    Require(Rejects([&] { _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(&guest, nullptr, &status); }), "sort rejects a null comparator it would call");
    Require(Rejects([&] { _ZNK6icu_687UVector7indexOfEii_nid_postfix(&guest, 1, -1); }), "indexOf rejects a negative start ICU would read before");
    Require(Rejects([&] { _ZNK6icu_687UVector7toArrayEPPv_nid_postfix(&guest, nullptr); }), "toArray rejects a NULL result it would write");
    Require(Rejects([] { _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(nullptr); }), "a null vector throws");
    Require(RejectsStatus([&] { _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&guest, 1, reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10})); }),
            "addElement rejects an unreadable status");
    Require(Rejects([&] { _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&guest, 1, nullptr); }), "addElement rejects a null status");
    status = 1;
    UHashTok* elements = guest.elements;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&guest, 4, &status);
    Require(status == 1 && guest.elements == elements, "_init returns first on a failed status");
    _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(&guest, &status);
    Require(status == 1 && guest.count == 3, "sorti returns first on a failed status");
    _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&guest, 4, &status);
    Require(guest.count == 4, "addElement grows even on a failed status, as ICU's ensureCapacity ignores it");
    status = 0;
    _ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(&guest, -1, &status);
    Require(status == 1, "a negative capacity is illegal");
    auto* heap = static_cast<Icu::Vector*>(_ZN6icu_687UMemorynwEm_nid_postfix(sizeof(Icu::Vector)));
    *heap = MakeGuest(2, false, false);
    _ZN6icu_687UVectorD0Ev_nid_postfix(heap);
    auto* stack = static_cast<Icu::Vector*>(_ZN6icu_687UMemorynwEm_nid_postfix(sizeof(Icu::Vector)));
    *stack = MakeGuest(2, false, false);
    _ZN6icu_686UStackD0Ev_nid_postfix(stack);
    auto* set = static_cast<Icu::Vector*>(_ZN6icu_687UMemorynwEm_nid_postfix(sizeof(Icu::Vector)));
    *set = MakeGuest(2, false, false);
    _ZN6icu_6810UStringSetD0Ev_nid_postfix(set);
    Icu::Vector strings = MakeGuest(2, true, false);
    status = 0;
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&strings, &objects[3], &status);
    Icu::Vector left = MakeGuest(4, false, false);
    Icu::Vector right = MakeGuest(4, false, false);
    for (std::int32_t value : {4, 8}) {
        _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&left, value, &status);
        _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&right, value, &status);
    }
    left.comparer = GuestKeyEqual_nid_no_patch;
    Require(_ZNK6icu_687UVector6equalsERKS0__nid_postfix(&left, &right) == 1, "equals passes the comparer a pointer to the other element, as ICU does");
    right.elements[1].integer = 9;
    Require(_ZNK6icu_687UVector6equalsERKS0__nid_postfix(&left, &right) == 0, "equals compares through the comparer");
    _ZN6icu_687UVectorD1Ev_nid_postfix(&left);
    _ZN6icu_687UVectorD1Ev_nid_postfix(&right);
    guestDeleted.clear();
    _ZN6icu_6810UStringSetD2Ev_nid_postfix(&strings);
    Require(guestDeleted.size() == 1 && guestDeleted[0] == &objects[3], "the UStringSet destructor deletes its elements");
    guestDeleted.clear();
    _ZN6icu_687UVectorD2Ev_nid_postfix(&guest);
}

Icu::Vector32 MakeGuest32(std::int32_t maxCapacity, std::int32_t capacity) {
    Icu::Vector32 guest{Sentinel, 0, 0, maxCapacity, nullptr};
    std::int32_t status = 0;
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&guest, capacity, &status);
    Require(status == 0 && guest.elements != nullptr, "a guest UVector32 initializes");
    return guest;
}

void CheckVector32() {
    std::mt19937 random(32);
    for (int round = 0; round < 40; ++round) {
        const std::int32_t capacity = static_cast<std::int32_t>(round % 6) - 1;
        UErrorCode hostStatus = U_ZERO_ERROR;
        icu::UVector32 host(capacity, hostStatus);
        Icu::Vector32 guest = MakeGuest32(0, capacity);
        Same(guest, host, "a new UVector32");
        icu::UVector32 hostOther(hostStatus);
        Icu::Vector32 guestOther = MakeGuest32(0, 8);
        for (int i = 0; i < 5; ++i) {
            const std::int32_t value = static_cast<std::int32_t>(random() % 12);
            hostOther.addElement(value, hostStatus);
            std::int32_t status = 0;
            _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(&guestOther, value, guestOther.count, &status);
        }
        Same(guestOther, hostOther, "the second UVector32");
        for (int step = 0; step < 60; ++step) {
            hostStatus = U_ZERO_ERROR;
            std::int32_t status = 0;
            const std::int32_t value = static_cast<std::int32_t>(random() % 12);
            const std::int32_t index = static_cast<std::int32_t>(random() % (guest.count + 3)) - 1;
            switch (random() % 12) {
            case 0:
                host.setElementAt(value, index);
                _ZN6icu_689UVector3212setElementAtEii_nid_postfix(&guest, value, index);
                break;
            case 1:
                host.sortedInsert(value, hostStatus);
                _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(&guest, value, &status);
                break;
            case 2: {
                const std::int32_t minimum = static_cast<std::int32_t>(random() % 40) - 1;
                hostStatus = random() % 5 == 0 ? U_ILLEGAL_ARGUMENT_ERROR : U_ZERO_ERROR;
                status = hostStatus;
                Require((host.expandCapacity(minimum, hostStatus) != 0) == (_ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&guest, minimum, &status) != 0),
                        "expandCapacity matches ICU");
                break;
            }
            case 3: {
                const std::int32_t limit = static_cast<std::int32_t>(random() % 20);
                host.setMaxCapacity(limit);
                _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(&guest, limit);
                break;
            }
            case 4:
                host.insertElementAt(value, index, hostStatus);
                _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(&guest, value, index, &status);
                break;
            case 5:
                if (index < guest.count) {
                    host.removeElementAt(index);
                    _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(&guest, index);
                }
                break;
            case 6:
                host.removeAllElements();
                _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(&guest);
                break;
            case 7:
                host.assign(hostOther, hostStatus);
                _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(&guest, &guestOther, &status);
                break;
            case 8: {
                const std::int32_t size = static_cast<std::int32_t>(random() % 14) - 1;
                host.setSize(size);
                _ZN6icu_689UVector327setSizeEi_nid_postfix(&guest, size);
                break;
            }
            case 9:
                Require((host.removeAll(hostOther) != 0) == (_ZN6icu_689UVector329removeAllERKS0__nid_postfix(&guest, &guestOther) != 0), "removeAll matches ICU");
                break;
            case 10:
                Require((host.retainAll(hostOther) != 0) == (_ZN6icu_689UVector329retainAllERKS0__nid_postfix(&guest, &guestOther) != 0), "retainAll matches ICU");
                break;
            default:
                host.addElement(value, hostStatus);
                _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(&guest, value, guest.count, &status);
                break;
            }
            Require(status == hostStatus, "the UVector32 status matches ICU");
            Same(guest, host, "a UVector32 operation matches ICU");
            const std::int32_t start = static_cast<std::int32_t>(random() % (guest.count + 1));
            Require(host.indexOf(value, start) == _ZNK6icu_689UVector327indexOfEii_nid_postfix(&guest, value, start), "UVector32 indexOf matches ICU");
            Require((host.containsAll(hostOther) != 0) == (_ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(&guest, &guestOther) != 0), "containsAll matches ICU");
            Require((host.containsNone(hostOther) != 0) == (_ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(&guest, &guestOther) != 0), "containsNone matches ICU");
            Require((host == hostOther) == (_ZN6icu_689UVector32eqERKS0__nid_postfix(&guest, &guestOther) != 0), "operator== matches ICU");
            Require((host.equals(hostOther) != 0) == (_ZNK6icu_689UVector326equalsERKS0__nid_postfix(&guest, &guestOther) != 0), "equals matches ICU");
        }
        _ZN6icu_689UVector32D1Ev_nid_postfix(&guest);
        _ZN6icu_689UVector32D2Ev_nid_postfix(&guestOther);
        Require(guest.elements == nullptr && guestOther.elements == nullptr, "the UVector32 destructors free the elements");
    }
    for (std::int32_t maxCapacity : {0, 3, 20}) {
        for (std::int32_t capacity : {-1, 0, 1, 5, 30}) {
            Icu::Vector32 guest = MakeGuest32(maxCapacity, capacity);
            std::int32_t expected = capacity < 1 ? 8 : capacity;
            if (maxCapacity > 0 && maxCapacity < expected) expected = maxCapacity;
            Require(guest.capacity == expected, "_init clamps the capacity as ICU does");
            std::int32_t status = 0;
            _ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&guest, 25, &status);
            Require(status == (maxCapacity > 0 && maxCapacity < 25 ? 15 : 0), "expandCapacity past the maximum overflows");
            _ZN6icu_689UVector32D1Ev_nid_postfix(&guest);
        }
    }
    auto* heap = static_cast<Icu::Vector32*>(_ZN6icu_687UMemorynwEm_nid_postfix(sizeof(Icu::Vector32)));
    *heap = MakeGuest32(0, 4);
    _ZN6icu_689UVector32D0Ev_nid_postfix(heap);
    Require(Rejects([] { _ZN6icu_689UVector327setSizeEi_nid_postfix(nullptr, 1); }), "a null UVector32 throws");
    Icu::Vector32 guest = MakeGuest32(0, 4);
    Require(Rejects([&] { _ZNK6icu_689UVector327indexOfEii_nid_postfix(&guest, 1, -1); }), "UVector32 indexOf rejects a negative start ICU would read before");
    Require(RejectsStatus([&] { _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(&guest, 1, reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10})); }),
            "sortedInsert rejects an unreadable status");
    _ZN6icu_689UVector32D1Ev_nid_postfix(&guest);
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckVectors();
    CheckVector32();
    Require(live.empty(), "no library memory leaks");
    std::puts("guest_icu_vectors passed");
    return 0;
}

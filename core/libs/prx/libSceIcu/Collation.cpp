#include "prx/libSceIcu/Collation.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ucol.h>
#include <unicode/udata.h>
#include <unicode/uenum.h>
#include <unicode/uiter.h>
#include <unicode/ures.h>
#include <unicode/uset.h>
#include <cstddef>

namespace {

constexpr const char* CollationTree = U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll";

struct GuestIterator {
    using GetIndexFn = std::int32_t (APS5_VABI*)(GuestIterator* iterator, std::int32_t origin);
    using MoveFn = std::int32_t (APS5_VABI*)(GuestIterator* iterator, std::int32_t delta, std::int32_t origin);
    using HasFn = std::int8_t (APS5_VABI*)(GuestIterator* iterator);
    using UnitFn = std::int32_t (APS5_VABI*)(GuestIterator* iterator);
    using ReservedFn = std::int32_t (APS5_VABI*)(GuestIterator* iterator, std::int32_t something);
    using GetStateFn = std::uint32_t (APS5_VABI*)(const GuestIterator* iterator);
    using SetStateFn = void (APS5_VABI*)(GuestIterator* iterator, std::uint32_t state, std::int32_t* status);

    const void* context;
    std::int32_t length;
    std::int32_t start;
    std::int32_t index;
    std::int32_t limit;
    std::int32_t reservedField;
    GetIndexFn getIndex;
    MoveFn move;
    HasFn hasNext;
    HasFn hasPrevious;
    UnitFn current;
    UnitFn next;
    UnitFn previous;
    ReservedFn reservedFn;
    GetStateFn getState;
    SetStateFn setState;
};

static_assert(offsetof(GuestIterator, getIndex) == 32);
static_assert(offsetof(GuestIterator, setState) == 104);
static_assert(sizeof(GuestIterator) == 112);

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

const UCollator* Collator(const UCollator* collator) {
    IcuBackend::RequireObject(collator);
    return collator;
}

template<typename TCallback>
TCallback Callback(TCallback callback) {
    if (!callback) APS5_INVALID_ARG_EX;
    return callback;
}

GuestIterator* Guest(UCharIterator* iterator) {
    return static_cast<GuestIterator*>(const_cast<void*>(iterator->context));
}

int32_t U_CALLCONV BridgeGetIndex(UCharIterator* iterator, UCharIteratorOrigin origin) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->getIndex)(guest, origin);
}

int32_t U_CALLCONV BridgeMove(UCharIterator* iterator, int32_t delta, UCharIteratorOrigin origin) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->move)(guest, delta, origin);
}

UBool U_CALLCONV BridgeHasNext(UCharIterator* iterator) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->hasNext)(guest);
}

UBool U_CALLCONV BridgeHasPrevious(UCharIterator* iterator) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->hasPrevious)(guest);
}

UChar32 U_CALLCONV BridgeCurrent(UCharIterator* iterator) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->current)(guest);
}

UChar32 U_CALLCONV BridgeNext(UCharIterator* iterator) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->next)(guest);
}

UChar32 U_CALLCONV BridgePrevious(UCharIterator* iterator) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->previous)(guest);
}

int32_t U_CALLCONV BridgeReserved(UCharIterator* iterator, int32_t something) {
    GuestIterator* guest = Guest(iterator);
    return Callback(guest->reservedFn)(guest, something);
}

uint32_t U_CALLCONV BridgeGetState(const UCharIterator* iterator) {
    const auto* guest = static_cast<const GuestIterator*>(iterator->context);
    return Callback(guest->getState)(guest);
}

void U_CALLCONV BridgeSetState(UCharIterator* iterator, uint32_t state, UErrorCode* status) {
    GuestIterator* guest = Guest(iterator);
    Callback(guest->setState)(guest, state, reinterpret_cast<std::int32_t*>(status));
}

UCharIterator* Bridge(UCharIterator& host, UCharIterator* guest) {
    if (!guest) return nullptr;
    host = UCharIterator{};
    host.context = guest;
    host.getIndex = BridgeGetIndex;
    host.move = BridgeMove;
    host.hasNext = BridgeHasNext;
    host.hasPrevious = BridgeHasPrevious;
    host.current = BridgeCurrent;
    host.next = BridgeNext;
    host.previous = BridgePrevious;
    host.reservedFn = BridgeReserved;
    host.getState = BridgeGetState;
    host.setState = BridgeSetState;
    return &host;
}

}

extern "C" {

UCollator* APS5_VABI ucol_open_nid_postfix(const char* locale, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return ucol_open(IcuBackend::LocaleObject(locale), error);
}

void APS5_VABI ucol_close_nid_postfix(UCollator* collator) {
    IcuBackend::Ensure();
    ucol_close(collator);
}

std::int32_t APS5_VABI ucol_strcoll_nid_postfix(const UCollator* collator, const char16_t* source, std::int32_t sourceLength, const char16_t* target, std::int32_t targetLength) {
    IcuBackend::Ensure();
    return ucol_strcoll(Collator(collator), source, sourceLength, target, targetLength);
}

std::int32_t APS5_VABI ucol_strcollUTF8_nid_postfix(const UCollator* collator, const char* source, std::int32_t sourceLength, const char* target, std::int32_t targetLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return UCOL_EQUAL;
    return ucol_strcollUTF8(Collator(collator), source, sourceLength, target, targetLength, error);
}

std::int32_t APS5_VABI ucol_strcollIter_nid_postfix(const UCollator* collator, UCharIterator* source, UCharIterator* target, std::int32_t* status) {
    IcuBackend::Ensure();
    UCharIterator left;
    UCharIterator right;
    UCharIterator* hostSource = Bridge(left, source);
    UCharIterator* hostTarget = target == source ? hostSource : Bridge(right, target);
    return ucol_strcollIter(collator, hostSource, hostTarget, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI ucol_getStrength_nid_postfix(const UCollator* collator) {
    IcuBackend::Ensure();
    return ucol_getStrength(collator);
}

void APS5_VABI ucol_setStrength_nid_postfix(UCollator* collator, std::int32_t strength) {
    IcuBackend::Ensure();
    ucol_setStrength(collator, static_cast<UCollationStrength>(strength));
}

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* collator, std::int32_t attribute, std::int32_t* status) {
    return ucol_getAttribute(collator, static_cast<UColAttribute>(attribute), Status(status));
}

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* collator, std::int32_t attribute, std::int32_t value, std::int32_t* status) {
    ucol_setAttribute(collator, static_cast<UColAttribute>(attribute), static_cast<UColAttributeValue>(value), Status(status));
}

const char16_t* APS5_VABI ucol_getRules_nid_postfix(const UCollator* collator, std::int32_t* length) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(length);
    return ucol_getRules(Collator(collator), length);
}

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator* collator, std::int32_t* dest, std::int32_t destCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return 0;
    return ucol_getReorderCodes(Collator(collator), dest, destCapacity, error);
}

std::int32_t APS5_VABI ucol_countAvailable_nid_postfix() {
    IcuBackend::Ensure();
    return ucol_countAvailable();
}

const char* APS5_VABI ucol_getAvailable_nid_postfix(std::int32_t index) {
    IcuBackend::Ensure();
    if (index < 0) APS5_INVALID_ARG_EX;
    return ucol_getAvailable(index);
}

USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator* collator, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    icu::LocalUSetPointer host(ucol_getTailoredSet(Collator(collator), error));
    if (!host.isValid()) return nullptr;
    USet* guest = IcuBackend::GuestSet(host.getAlias());
    if (!guest) *error = U_MEMORY_ALLOCATION_ERROR;
    return guest;
}

UEnumeration* APS5_VABI ucol_getKeywordValues_nid_postfix(const char* keyword, std::int32_t* status) {
    return IcuBackend::GuestEnumeration(ucol_getKeywordValues(keyword, Status(status)), status);
}

UEnumeration* APS5_VABI ucol_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale, std::int8_t commonlyUsed, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::CanonicalLocale(locale);
    UErrorCode probe = U_ZERO_ERROR;
    ures_close(ures_open(CollationTree, locale, &probe));
    if (probe == U_USING_DEFAULT_WARNING) IcuBackend::RequireGuestDefault();
    icu::LocalUEnumerationPointer host(ucol_getKeywordValuesForLocale(key, locale, commonlyUsed, error));
    return IcuBackend::GuestEnumeration(host.orphan(), status);
}

}

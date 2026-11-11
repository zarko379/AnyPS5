#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Set.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uenum.h>
#include <unicode/uniset.h>
#include <unicode/uset.h>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <stdexcept>

namespace IcuBackend {

namespace {

std::atomic<bool> guestDefaultLocale{false};
std::atomic<bool> guestDefaultConverter{false};

struct GuestEnumerationLayout {
    using CloseFn = void (APS5_VABI*)(GuestEnumerationLayout* enumeration);
    using CountFn = std::int32_t (APS5_VABI*)(GuestEnumerationLayout* enumeration, std::int32_t* status);
    using UnicodeNextFn = const char16_t* (APS5_VABI*)(GuestEnumerationLayout* enumeration, std::int32_t* resultLength, std::int32_t* status);
    using NextFn = const char* (APS5_VABI*)(GuestEnumerationLayout* enumeration, std::int32_t* resultLength, std::int32_t* status);
    using ResetFn = void (APS5_VABI*)(GuestEnumerationLayout* enumeration, std::int32_t* status);

    void* baseContext;
    void* context;
    CloseFn close;
    CountFn count;
    UnicodeNextFn uNext;
    NextFn next;
    ResetFn reset;
};

static_assert(offsetof(GuestEnumerationLayout, context) == 8);
static_assert(offsetof(GuestEnumerationLayout, close) == 16);
static_assert(offsetof(GuestEnumerationLayout, reset) == 48);
static_assert(sizeof(GuestEnumerationLayout) == 56);

UEnumeration* HostEnumeration(GuestEnumerationLayout* enumeration) {
    IcuBackend::RequireObject(enumeration);
    return static_cast<UEnumeration*>(enumeration->context);
}

UErrorCode* EnumerationStatus(std::int32_t* status) {
    IcuBackend::RequireObject(status);
    return reinterpret_cast<UErrorCode*>(status);
}

void APS5_VABI CloseHostEnumeration_nid_no_patch(GuestEnumerationLayout* enumeration) {
    uenum_close(HostEnumeration(enumeration));
    free_nid_postfix(enumeration);
}

std::int32_t APS5_VABI CountHostEnumeration_nid_no_patch(GuestEnumerationLayout* enumeration, std::int32_t* status) {
    return uenum_count(HostEnumeration(enumeration), EnumerationStatus(status));
}

const char16_t* APS5_VABI UnicodeNextHostEnumeration_nid_no_patch(GuestEnumerationLayout* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    return uenum_unext(HostEnumeration(enumeration), resultLength, EnumerationStatus(status));
}

const char* APS5_VABI NextHostEnumeration_nid_no_patch(GuestEnumerationLayout* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    return uenum_next(HostEnumeration(enumeration), resultLength, EnumerationStatus(status));
}

void APS5_VABI ResetHostEnumeration_nid_no_patch(GuestEnumerationLayout* enumeration, std::int32_t* status) {
    uenum_reset(HostEnumeration(enumeration), EnumerationStatus(status));
}

void ApplyFileAccess() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    if (U_FAILURE(status)) throw std::runtime_error("libSceIcu backend: udata_setFileAccess failed");
}

}

void Ensure() {
    static std::once_flag once;
    std::call_once(once, ApplyFileAccess);
}

std::int32_t* RequireStatus(std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    Ensure();
    return status;
}

const void* RequireObject(const void* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

void SetGuestDefaultLocale(bool set) {
    guestDefaultLocale.store(set);
}

void SetGuestDefaultConverter(bool set) {
    guestDefaultConverter.store(set);
}

const char* Locale(const char* locale) {
    Ensure();
    if (!locale && !guestDefaultLocale.load()) NotImplemented_nid_no_patch("libSceIcu: the console's default locale");
    return locale;
}

const char* Converter(const char* name) {
    Ensure();
    if (!name && !guestDefaultConverter.load()) NotImplemented_nid_no_patch("libSceIcu: the console's default converter");
    return name;
}

const char16_t* Zone(const char16_t* zone, std::int32_t) {
    Ensure();
    if (!zone) NotImplemented_nid_no_patch("libSceIcu: the console's default time zone");
    return zone;
}

void CheckFallback(const std::int32_t* status) {
    if (status && *status == U_USING_DEFAULT_WARNING && !guestDefaultLocale.load())
        NotImplemented_nid_no_patch("libSceIcu: a fallback to the console's default locale");
}

void Cleanup() {
    static std::mutex cleanupMutex;
    const std::lock_guard lock(cleanupMutex);
    Ensure();
    u_cleanup();
    guestDefaultLocale.store(false);
    guestDefaultConverter.store(false);
    ApplyFileAccess();
}

USet* GuestSet(const USet* host) {
    const icu::UnicodeSet* set = icu::UnicodeSet::fromUSet(host);
    const std::int32_t count = set->getRangeCount();
    if (uset_getItemCount(host) != count) NotImplemented_nid_no_patch("libSceIcu: a set with strings (the guest USet holds code point ranges only)");
    USet* guest = uset_openEmpty_nid_postfix();
    if (!guest) return nullptr;
    for (std::int32_t index = 0; index < count; ++index) uset_addRange_nid_postfix(guest, set->getRangeStart(index), set->getRangeEnd(index));
    return guest;
}

UEnumeration* GuestEnumeration(UEnumeration* host, std::int32_t* status) {
    if (!host) return nullptr;
    auto* guest = static_cast<GuestEnumerationLayout*>(malloc_nid_postfix(sizeof(GuestEnumerationLayout)));
    if (!guest) {
        uenum_close(host);
        if (!status) APS5_INVALID_ARG_EX;
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    *guest = GuestEnumerationLayout{nullptr, host, CloseHostEnumeration_nid_no_patch, CountHostEnumeration_nid_no_patch, UnicodeNextHostEnumeration_nid_no_patch, NextHostEnumeration_nid_no_patch, ResetHostEnumeration_nid_no_patch};
    return reinterpret_cast<UEnumeration*>(guest);
}

}

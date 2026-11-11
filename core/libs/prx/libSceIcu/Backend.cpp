#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Set.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/putil.h>
#include <unicode/uloc.h>
#include <unicode/uenum.h>
#include <unicode/locid.h>
#include <unicode/uniset.h>
#include <unicode/ures.h>
#include <unicode/uset.h>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <unordered_set>

namespace IcuBackend {

namespace {

std::atomic<bool> guestDefaultLocale{false};
std::atomic<bool> guestDefaultConverter{false};
std::atomic<bool> patternGeneratorUsed{false};
std::atomic<bool> patternGeneratorFreed{false};

std::mutex& TrackedMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_set<const void*>& TrackedHandles() {
    static std::unordered_set<const void*> handles;
    return handles;
}

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
    u_setTimeZoneFilesDirectory("", &status);
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

bool GuestDefault() {
    return guestDefaultLocale.load();
}

void RequireGuestDefault() {
    Ensure();
    if (!guestDefaultLocale.load()) NotImplemented_nid_no_patch("libSceIcu: a result decided by the console's default locale");
}

const char* CanonicalLocale(const char* locale) {
    Locale(locale);
    if (!locale || std::strncmp(locale, "i-default", 9) != 0) return locale;
    const char next = locale[9];
    if (next != '\0' && next != '@' && next != '.' && next != '_' && next != '-') return locale;
    if (!std::strchr(locale, '@')) {
        char converted[ULOC_FULLNAME_CAPACITY];
        UErrorCode status = U_ZERO_ERROR;
        const std::int32_t length = uloc_forLanguageTag(locale, converted, sizeof(converted), nullptr, &status);
        if (length > 0 || U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) return locale;
    }
    RequireGuestDefault();
    return locale;
}

const char* Package(const char* path) {
    Ensure();
    if (!path) return path;
    const bool tree = std::strcmp(path, "ICUDATA") == 0 || std::strncmp(path, "ICUDATA-", 8) == 0 || std::strncmp(path, U_ICUDATA_NAME "-", sizeof(U_ICUDATA_NAME)) == 0;
    if (!tree || std::strchr(path, '\\')) NotImplemented_nid_no_patch("libSceIcu: a resource package outside ICU's own data (ICU would read the package from the file system)");
    return path;
}

bool LocaleData(const char* tree, const char* locale) {
    Ensure();
    char compat[8] = "";
    UErrorCode compatStatus = U_ZERO_ERROR;
    const std::int32_t compatLength = icu::Locale(locale).getKeywordValue("compat", compat, sizeof(compat), compatStatus);
    if (U_SUCCESS(compatStatus) && compatLength > 0 && std::strcmp(compat, "host") == 0) NotImplemented_nid_no_patch("libSceIcu: the host's own formats (@compat=host)");
    UErrorCode status = U_ZERO_ERROR;
    ures_close(ures_open(tree, locale, &status));
    const bool needsDefault = status == U_USING_DEFAULT_WARNING;
    if (needsDefault && !guestDefaultLocale.load()) NotImplemented_nid_no_patch("libSceIcu: a fallback to the console's default locale");
    return needsDefault;
}

void Track(const void* handle, bool needsDefault) {
    if (!handle) return;
    const std::lock_guard lock(TrackedMutex());
    if (needsDefault) TrackedHandles().insert(handle);
    else TrackedHandles().erase(handle);
}

void CheckTracked(const void* handle) {
    if (guestDefaultLocale.load()) return;
    const std::lock_guard lock(TrackedMutex());
    if (TrackedHandles().count(handle)) NotImplemented_nid_no_patch("libSceIcu: an object whose locale falls back to the console's default locale");
}

bool Tracked(const void* handle) {
    const std::lock_guard lock(TrackedMutex());
    return TrackedHandles().count(handle) != 0;
}

void Untrack(const void* handle) {
    const std::lock_guard lock(TrackedMutex());
    TrackedHandles().erase(handle);
}

void PatternGenerator() {
    Ensure();
    if (patternGeneratorFreed.load()) NotImplemented_nid_no_patch("libSceIcu: a pattern generator after u_cleanup (ICU 68.2 frees its allowed hour formats table without resetting its init-once)");
    patternGeneratorUsed.store(true);
}

void Cleanup() {
    static std::mutex cleanupMutex;
    const std::lock_guard lock(cleanupMutex);
    Ensure();
    u_cleanup();
    if (patternGeneratorUsed.load()) patternGeneratorFreed.store(true);
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

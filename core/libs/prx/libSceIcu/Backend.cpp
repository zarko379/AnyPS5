#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Set.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uniset.h>
#include <unicode/uset.h>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace IcuBackend {

namespace {

std::atomic<bool> guestDefaultLocale{false};
std::atomic<bool> guestDefaultConverter{false};

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

}

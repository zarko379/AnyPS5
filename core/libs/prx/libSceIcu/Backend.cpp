#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/uclean.h>
#include <unicode/udata.h>
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

}

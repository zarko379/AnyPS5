#include "prx/libSceIcu/CharacterProperties.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Script.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/uset.h>
#include "uprops.h"
#include "uset_imp.h"

namespace {

UErrorCode* Status(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

UErrorCode* RequiredStatus(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

struct CharNamesForward {
    GuestCharNamesCallback fn;
    void* context;
};

UBool U_CALLCONV ForwardCharName(void* context, UChar32 code, UCharNameChoice nameChoice, const char* name, int32_t length) {
    const auto* forward = static_cast<const CharNamesForward*>(context);
    return forward->fn(forward->context, code, nameChoice, name, length);
}

static_assert(offsetof(GuestSetAdder, add) == offsetof(USetAdder, add) && offsetof(GuestSetAdder, removeRange) == offsetof(USetAdder, removeRange) && sizeof(GuestSetAdder) == sizeof(USetAdder));

thread_local const GuestSetAdder* activeAdder = nullptr;

void U_CALLCONV ForwardAdd(USet*, UChar32 c) {
    const GuestSetAdder* adder = activeAdder;
    if (!adder->add) APS5_INVALID_ARG_EX;
    adder->add(adder->set, c);
}

}

extern "C" {

std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t code, std::int32_t nameChoice, char* buffer, std::int32_t bufferLength, std::int32_t* status) {
    return u_charName(code, static_cast<UCharNameChoice>(nameChoice), buffer, bufferLength, Status(status));
}

std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t nameChoice, const char* name, std::int32_t* status) {
    return u_charFromName(static_cast<UCharNameChoice>(nameChoice), name, Status(status));
}

void APS5_VABI u_enumCharNames_nid_postfix(std::int32_t start, std::int32_t limit, GuestCharNamesCallback fn, void* context, std::int32_t nameChoice, std::int32_t* status) {
    CharNamesForward forward{fn, context};
    u_enumCharNames(start, limit, fn ? ForwardCharName : nullptr, &forward, static_cast<UCharNameChoice>(nameChoice), Status(status));
}

std::int32_t APS5_VABI uscript_getCode_nid_postfix(const char* nameOrAbbrOrLocale, std::int32_t* fillIn, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    if (U_SUCCESS(*error) && nameOrAbbrOrLocale && (fillIn == nullptr ? capacity == 0 : capacity >= 0)) IcuBackend::CanonicalLocale(nameOrAbbrOrLocale);
    return uscript_getCode(nameOrAbbrOrLocale, reinterpret_cast<UScriptCode*>(fillIn), capacity, error);
}

std::int32_t APS5_VABI uprv_getMaxCharNameLength_nid_postfix() {
    IcuBackend::Ensure();
    return uprv_getMaxCharNameLength();
}

void APS5_VABI uprv_getCharNameCharacters_nid_postfix(const GuestSetAdder* sa) {
    IcuBackend::Ensure();
    if (uprv_getMaxCharNameLength() == 0) return;
    if (!sa) APS5_INVALID_ARG_EX;
    const GuestSetAdder* saved = activeAdder;
    activeAdder = sa;
    const USetAdder host{nullptr, ForwardAdd, nullptr, nullptr, nullptr, nullptr};
    try {
        uprv_getCharNameCharacters(&host);
    } catch (...) {
        activeAdder = saved;
        throw;
    }
    activeAdder = saved;
}

}

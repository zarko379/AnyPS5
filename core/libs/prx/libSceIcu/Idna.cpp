#include "prx/libSceIcu/Idna.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/uidna.h>
#include <cstddef>

namespace {

static_assert(sizeof(UIDNAInfo) == 16);
static_assert(offsetof(UIDNAInfo, isTransitionalDifferent) == 2);
static_assert(offsetof(UIDNAInfo, errors) == 4);
static_assert(offsetof(UIDNAInfo, reservedI3) == 12);

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

bool Processes(const char16_t* name, std::int32_t length, const char16_t* dest, std::int32_t capacity, const UIDNAInfo* info, const UErrorCode* status) {
    if (U_FAILURE(*status) || !info || info->size < 16) return false;
    if (name ? length < -1 : length != 0) return false;
    if (dest ? capacity < 0 : capacity != 0) return false;
    return !(dest == name && name);
}

const UIDNA* Idna(const UIDNA* idna, const char16_t* name, std::int32_t length, const char16_t* dest, std::int32_t capacity, const UIDNAInfo* info, const UErrorCode* status) {
    if (!idna && Processes(name, length, dest, capacity, info, status)) APS5_INVALID_ARG_EX;
    return idna;
}

}

extern "C" {

UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t options, std::int32_t* status) {
    return uidna_openUTS46(options, Status(status));
}

void APS5_VABI uidna_close_nid_postfix(UIDNA* idna) {
    IcuBackend::Ensure();
    uidna_close(idna);
}

std::int32_t APS5_VABI uidna_nameToASCII_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest, std::int32_t capacity, UIDNAInfo* info, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return uidna_nameToASCII(Idna(idna, name, length, dest, capacity, info, error), name, length, dest, capacity, info, error);
}

std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest, std::int32_t capacity, UIDNAInfo* info, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return uidna_nameToUnicode(Idna(idna, name, length, dest, capacity, info, error), name, length, dest, capacity, info, error);
}

}

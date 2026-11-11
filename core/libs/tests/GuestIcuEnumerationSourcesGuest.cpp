#include "prx/libSceIcu/Enumeration.hpp"
#include <cstdint>

bool GuestEnumerationLayout(void* enumeration) {
    const auto* guest = static_cast<const UEnumeration*>(enumeration);
    return !guest->baseContext && guest->context && guest->close && guest->count && guest->uNext && guest->next && guest->reset;
}

std::int32_t GuestEnumerationCount(void* enumeration, std::int32_t* status) {
    return uenum_count_nid_postfix(static_cast<UEnumeration*>(enumeration), status);
}

const char* GuestEnumerationNext(void* enumeration, std::int32_t* length, std::int32_t* status) {
    return uenum_next_nid_postfix(static_cast<UEnumeration*>(enumeration), length, status);
}

const char16_t* GuestEnumerationUnicodeNext(void* enumeration, std::int32_t* length, std::int32_t* status) {
    return uenum_unext_nid_postfix(static_cast<UEnumeration*>(enumeration), length, status);
}

void GuestEnumerationReset(void* enumeration, std::int32_t* status) {
    uenum_reset_nid_postfix(static_cast<UEnumeration*>(enumeration), status);
}

void GuestEnumerationClose(void* enumeration) {
    uenum_close_nid_postfix(static_cast<UEnumeration*>(enumeration));
}

std::int32_t GuestEnumerationCallbackCount(void* enumeration, std::int32_t* status) {
    auto* guest = static_cast<UEnumeration*>(enumeration);
    return guest->count(guest, status);
}

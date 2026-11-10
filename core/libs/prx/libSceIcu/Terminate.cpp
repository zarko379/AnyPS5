#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>

namespace {

std::int32_t TerminateUnits(char16_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    if (!status) return length;
    if (!destination && *status <= 0 && length >= 0 && length < capacity)
        NotImplemented_nid_no_patch("libSceIcu: terminating a null destination");
    Icu::Terminate(destination, capacity, length, status);
    return length;
}

}

extern "C" {

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t value) {
    if (value >= u'a' && value <= u'z') return static_cast<char16_t>(value - u'a' + u'A');
    return value;
}

std::int32_t APS5_VABI u_terminateUChars_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return TerminateUnits(destination, capacity, length, status);
}

std::int32_t APS5_VABI u_terminateWChars_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return TerminateUnits(destination, capacity, length, status);
}

}

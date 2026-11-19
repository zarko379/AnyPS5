#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>

extern "C" {

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t value) {
    if (value >= u'a' && value <= u'z') return static_cast<char16_t>(value - u'a' + u'A');
    return value;
}

}

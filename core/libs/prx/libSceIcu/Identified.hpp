#pragma once
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
const char* APS5_VABI locale_getKeywordsStart_nid_postfix(const char* localeID);
std::int32_t APS5_VABI res_getPublicType_nid_postfix(std::uint32_t resource);
int APS5_VABI u_charDigitValue_nid_postfix(std::int32_t character);
int APS5_VABI u_digit_nid_postfix(std::int32_t character, std::int8_t radix);
int APS5_VABI u_flushDefaultConverter_nid_postfix();
int APS5_VABI u_getDefaultConverter_nid_postfix();
int APS5_VABI u_releaseDefaultConverter_nid_postfix();
}

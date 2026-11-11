#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int8_t APS5_VABI u_isalnum_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isalpha_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isbase_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_iscntrl_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isdefined_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isdigit_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isgraph_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_islower_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isprint_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_ispunct_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_istitle_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isupper_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isxdigit_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isIDIgnorable_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isIDPart_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isIDStart_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isJavaIDPart_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isJavaIDStart_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isMirrored_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isUAlphabetic_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isULowercase_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_isUUppercase_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI u_charType_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_charDirection_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_charMirror_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_getBidiPairedBracket_nid_postfix(std::int32_t c);
void APS5_VABI u_charAge_nid_postfix(std::int32_t c, std::uint8_t* versionArray);
std::uint8_t APS5_VABI u_getCombiningClass_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_getIntPropertyValue_nid_postfix(std::int32_t c, std::int32_t which);
std::int32_t APS5_VABI u_getIntPropertyMinValue_nid_postfix(std::int32_t which);
std::int32_t APS5_VABI u_getIntPropertyMaxValue_nid_postfix(std::int32_t which);
std::int8_t APS5_VABI u_hasBinaryProperty_nid_postfix(std::int32_t c, std::int32_t which);
std::int32_t APS5_VABI u_getPropertyEnum_nid_postfix(const char* alias);
const char* APS5_VABI u_getPropertyName_nid_postfix(std::int32_t property, std::int32_t nameChoice);
std::int32_t APS5_VABI u_getPropertyValueEnum_nid_postfix(std::int32_t property, const char* alias);
const char* APS5_VABI u_getPropertyValueName_nid_postfix(std::int32_t property, std::int32_t value, std::int32_t nameChoice);
double APS5_VABI u_getNumericValue_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_foldCase_nid_postfix(std::int32_t c, std::uint32_t options);
std::int32_t APS5_VABI u_tolower_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_toupper_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_totitle_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t code, std::int32_t nameChoice, char* buffer, std::int32_t bufferLength, std::int32_t* status);
std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t nameChoice, const char* name, std::int32_t* status);
std::int32_t APS5_VABI u_getFC_NFKC_Closure_nid_postfix(std::int32_t c, char16_t* dest, std::int32_t destCapacity, std::int32_t* status);
std::int32_t APS5_VABI ublock_getCode_nid_postfix(std::int32_t c);
}

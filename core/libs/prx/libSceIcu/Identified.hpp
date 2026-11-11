#pragma once
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
int APS5_VABI UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix();
int APS5_VABI UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix();
int APS5_VABI UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix();
int APS5_VABI UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix();
const char* APS5_VABI locale_getKeywordsStart_nid_postfix(const char* localeID);
int APS5_VABI res_countArrayItems_nid_postfix();
int APS5_VABI res_getAlias_nid_postfix();
int APS5_VABI res_getArrayItem_nid_postfix();
int APS5_VABI res_getBinaryNoTrace_nid_postfix();
int APS5_VABI res_getIntVectorNoTrace_nid_postfix();
std::int32_t APS5_VABI res_getPublicType_nid_postfix(std::uint32_t resource);
int APS5_VABI res_getResource_nid_postfix();
int APS5_VABI res_getStringNoTrace_nid_postfix();
int APS5_VABI res_getTableItemByIndex_nid_postfix();
int APS5_VABI res_getTableItemByKey_nid_postfix();
int APS5_VABI res_read_nid_postfix();
int APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix();
int APS5_VABI u_charDigitValue_nid_postfix(std::int32_t character);
int APS5_VABI u_digit_nid_postfix(std::int32_t character, std::int8_t radix);
int APS5_VABI u_flushDefaultConverter_nid_postfix();
int APS5_VABI u_getDefaultConverter_nid_postfix();
int APS5_VABI u_memcasecmp_nid_postfix();
int APS5_VABI u_releaseDefaultConverter_nid_postfix();
int APS5_VABI u_strCaseCompare_nid_postfix();
int APS5_VABI u_strFoldCase_nid_postfix();
int APS5_VABI u_strToLower_nid_postfix();
int APS5_VABI u_strToUpper_nid_postfix();
int APS5_VABI u_strcasecmp_nid_postfix();
int APS5_VABI u_strncasecmp_nid_postfix();
}

#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCollator;
struct UCharIterator;
struct USet;
struct UEnumeration;

extern "C" {
UCollator* APS5_VABI ucol_open_nid_postfix(const char* locale, std::int32_t* status);
void APS5_VABI ucol_close_nid_postfix(UCollator* collator);
std::int32_t APS5_VABI ucol_strcoll_nid_postfix(const UCollator* collator, const char16_t* source, std::int32_t sourceLength, const char16_t* target, std::int32_t targetLength);
std::int32_t APS5_VABI ucol_strcollUTF8_nid_postfix(const UCollator* collator, const char* source, std::int32_t sourceLength, const char* target, std::int32_t targetLength, std::int32_t* status);
std::int32_t APS5_VABI ucol_strcollIter_nid_postfix(const UCollator* collator, UCharIterator* source, UCharIterator* target, std::int32_t* status);
std::int32_t APS5_VABI ucol_getStrength_nid_postfix(const UCollator* collator);
void APS5_VABI ucol_setStrength_nid_postfix(UCollator* collator, std::int32_t strength);
std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* collator, std::int32_t attribute, std::int32_t* status);
void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* collator, std::int32_t attribute, std::int32_t value, std::int32_t* status);
const char16_t* APS5_VABI ucol_getRules_nid_postfix(const UCollator* collator, std::int32_t* length);
std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator* collator, std::int32_t* dest, std::int32_t destCapacity, std::int32_t* status);
std::int32_t APS5_VABI ucol_countAvailable_nid_postfix();
const char* APS5_VABI ucol_getAvailable_nid_postfix(std::int32_t index);
USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator* collator, std::int32_t* status);
UEnumeration* APS5_VABI ucol_getKeywordValues_nid_postfix(const char* keyword, std::int32_t* status);
UEnumeration* APS5_VABI ucol_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale, std::int8_t commonlyUsed, std::int32_t* status);
}

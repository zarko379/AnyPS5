#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI ucurr_forLocale_nid_postfix(const char* locale, char16_t* buff, std::int32_t buffCapacity, std::int32_t* ec);
const void* APS5_VABI ucurr_register_nid_postfix(const char16_t* isoCode, const char* locale, std::int32_t* status);
std::int8_t APS5_VABI ucurr_unregister_nid_postfix(const void* key, std::int32_t* status);
const char16_t* APS5_VABI ucurr_getName_nid_postfix(const char16_t* currency, const char* locale, std::int32_t nameStyle, std::int8_t* isChoiceFormat, std::int32_t* len, std::int32_t* ec);
const char16_t* APS5_VABI ucurr_getPluralName_nid_postfix(const char16_t* currency, const char* locale, std::int8_t* isChoiceFormat, const char* pluralCount, std::int32_t* len, std::int32_t* ec);
std::int32_t APS5_VABI ucurr_getDefaultFractionDigits_nid_postfix(const char16_t* currency, std::int32_t* ec);
std::int32_t APS5_VABI ucurr_getDefaultFractionDigitsForUsage_nid_postfix(const char16_t* currency, std::int32_t usage, std::int32_t* ec);
double APS5_VABI ucurr_getRoundingIncrement_nid_postfix(const char16_t* currency, std::int32_t* ec);
double APS5_VABI ucurr_getRoundingIncrementForUsage_nid_postfix(const char16_t* currency, std::int32_t usage, std::int32_t* ec);
std::int32_t APS5_VABI ucurr_getNumericCode_nid_postfix(const char16_t* currency);
std::int8_t APS5_VABI ucurr_isAvailable_nid_postfix(const char16_t* isoCode, double from, double to, std::int32_t* errorCode);
std::int32_t APS5_VABI ucurr_forLocaleAndDate_nid_postfix(const char* locale, double date, std::int32_t index, char16_t* buff, std::int32_t buffCapacity, std::int32_t* ec);
std::int32_t APS5_VABI ucurr_countCurrencies_nid_postfix(const char* locale, double date, std::int32_t* ec);
}

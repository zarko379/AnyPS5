#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI uloc_getName_nid_postfix(const char* localeID, char* name, std::int32_t nameCapacity, std::int32_t* err);
std::int32_t APS5_VABI uloc_getParent_nid_postfix(const char* localeID, char* parent, std::int32_t parentCapacity, std::int32_t* err);
std::int32_t APS5_VABI uloc_getVariant_nid_postfix(const char* localeID, char* variant, std::int32_t variantCapacity, std::int32_t* err);
const char* APS5_VABI uloc_getISO3Language_nid_postfix(const char* localeID);
const char* APS5_VABI uloc_getISO3Country_nid_postfix(const char* localeID);
const char* const* APS5_VABI uloc_getISOLanguages_nid_postfix();
const char* const* APS5_VABI uloc_getISOCountries_nid_postfix();
std::uint32_t APS5_VABI uloc_getLCID_nid_postfix(const char* localeID);
std::int32_t APS5_VABI uloc_getLocaleForLCID_nid_postfix(std::uint32_t hostID, char* locale, std::int32_t localeCapacity, std::int32_t* status);
const char* APS5_VABI uloc_toUnicodeLocaleKey_nid_postfix(const char* keyword);
const char* APS5_VABI uloc_toUnicodeLocaleType_nid_postfix(const char* keyword, const char* value);
const char* APS5_VABI uloc_toLegacyKey_nid_postfix(const char* keyword);
const char* APS5_VABI uloc_toLegacyType_nid_postfix(const char* keyword, const char* value);
std::int32_t APS5_VABI uloc_setKeywordValue_nid_postfix(const char* keywordName, const char* keywordValue, char* buffer, std::int32_t bufferCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_countAvailable_nid_postfix();
const char* APS5_VABI uloc_getAvailable_nid_postfix(std::int32_t n);
std::int32_t APS5_VABI uloc_addLikelySubtags_nid_postfix(const char* localeID, char* maximizedLocaleID, std::int32_t maximizedLocaleIDCapacity, std::int32_t* err);
std::int32_t APS5_VABI uloc_minimizeSubtags_nid_postfix(const char* localeID, char* minimizedLocaleID, std::int32_t minimizedLocaleIDCapacity, std::int32_t* err);
std::int32_t APS5_VABI uloc_toLanguageTag_nid_postfix(const char* localeID, char* langtag, std::int32_t langtagCapacity, std::int8_t strict, std::int32_t* err);
std::int32_t APS5_VABI uloc_getCharacterOrientation_nid_postfix(const char* localeId, std::int32_t* status);
std::int32_t APS5_VABI uloc_getLineOrientation_nid_postfix(const char* localeId, std::int32_t* status);
std::int8_t APS5_VABI uloc_isRightToLeft_nid_postfix(const char* locale);
const char16_t* APS5_VABI uloc_getTableStringWithFallback_nid_postfix(const char* path, const char* locale, const char* tableKey, const char* subTableKey, const char* itemKey, std::int32_t* pLength, std::int32_t* pErrorCode);
std::int32_t APS5_VABI uloc_getDisplayLanguage_nid_postfix(const char* locale, const char* displayLocale, char16_t* language, std::int32_t languageCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayScript_nid_postfix(const char* locale, const char* displayLocale, char16_t* script, std::int32_t scriptCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayCountry_nid_postfix(const char* locale, const char* displayLocale, char16_t* country, std::int32_t countryCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayVariant_nid_postfix(const char* locale, const char* displayLocale, char16_t* variant, std::int32_t variantCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayName_nid_postfix(const char* localeID, const char* inLocaleID, char16_t* result, std::int32_t maxResultSize, std::int32_t* err);
std::int32_t APS5_VABI uloc_getDisplayKeyword_nid_postfix(const char* keyword, const char* displayLocale, char16_t* dest, std::int32_t destCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayKeywordValue_nid_postfix(const char* locale, const char* keyword, const char* displayLocale, char16_t* dest, std::int32_t destCapacity, std::int32_t* status);
std::int32_t APS5_VABI ulocimp_getRegionForSupplementalData_nid_postfix(const char* localeID, std::int8_t inferRegion, char* region, std::int32_t regionCapacity, std::int32_t* status);
const char* const* APS5_VABI ulocimp_getKnownCanonicalizedLocaleForTest_nid_postfix(std::int32_t* length);
bool APS5_VABI ulocimp_isCanonicalizedLocaleForTest_nid_postfix(const char* localeName);
}

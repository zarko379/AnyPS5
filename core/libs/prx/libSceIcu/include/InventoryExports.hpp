#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI udata_setCommonData();
int APS5_VABI udata_setFileAccess();
std::int32_t APS5_VABI uloc_canonicalize(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_forLanguageTag(const char* langtag, char* localeID, std::int32_t capacity, std::int32_t* parsedLength, std::int32_t* status);
std::int32_t APS5_VABI uloc_getBaseName(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getCountry(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
const char* APS5_VABI uloc_getDefault();
const char* APS5_VABI uloc_getISO3Country(const char* locale);
const char* APS5_VABI uloc_getISO3Language(const char* locale);
const char* const* APS5_VABI uloc_getISOCountries();
const char* const* APS5_VABI uloc_getISOLanguages();
std::int32_t APS5_VABI uloc_getKeywordValue(const char* locale, const char* keyword, char* result,
                                          std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getLanguage(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getParent(const char* locale, char* result, std::int32_t capacity,
                                                  std::int32_t* status);
std::int32_t APS5_VABI uloc_getScript(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int8_t APS5_VABI uloc_isRightToLeft_nid_postfix(const char* locale);
int APS5_VABI unorm2_getNFKCInstance();
int APS5_VABI unorm2_getNFKDInstance();
const char* APS5_VABI uprv_getDefaultLocaleID_nid_postfix();
int APS5_VABI utext_openConstUnicodeString();

}

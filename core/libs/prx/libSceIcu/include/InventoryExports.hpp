#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI ubrk_setUText();
int APS5_VABI udata_setCommonData();
int APS5_VABI udata_setFileAccess();
std::int32_t APS5_VABI uloc_canonicalize(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_forLanguageTag(const char* langtag, char* localeID, std::int32_t capacity, std::int32_t* parsedLength, std::int32_t* status);
std::int32_t APS5_VABI uloc_getBaseName(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getCountry(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getKeywordValue(const char* locale, const char* keyword, char* result,
                                          std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getLanguage(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getScript(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
int APS5_VABI unorm2_getNFKCInstance();
int APS5_VABI unorm2_getNFKDInstance();
int APS5_VABI utext_close();
int APS5_VABI utext_openConstUnicodeString();
int APS5_VABI utext_openUChars();
int APS5_VABI utext_setup();

}

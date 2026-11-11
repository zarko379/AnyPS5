#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UResourceBundle;
struct UEnumeration;

extern "C" {
UResourceBundle* APS5_VABI ures_open_nid_postfix(const char* packageName, const char* locale, std::int32_t* status);
UResourceBundle* APS5_VABI ures_openNoDefault_nid_postfix(const char* path, const char* localeID, std::int32_t* status);
UResourceBundle* APS5_VABI ures_openDirect_nid_postfix(const char* packageName, const char* locale, std::int32_t* status);
void APS5_VABI ures_openFillIn_nid_postfix(UResourceBundle* r, const char* packageName, const char* localeID, std::int32_t* status);
void APS5_VABI ures_openDirectFillIn_nid_postfix(UResourceBundle* r, const char* packageName, const char* locale, std::int32_t* status);
void APS5_VABI ures_close_nid_postfix(UResourceBundle* resourceBundle);
UEnumeration* APS5_VABI ures_openAvailableLocales_nid_postfix(const char* packageName, std::int32_t* status);
UEnumeration* APS5_VABI ures_getKeywordValues_nid_postfix(const char* path, const char* keyword, std::int32_t* status);
std::int32_t APS5_VABI ures_getFunctionalEquivalent_nid_postfix(char* result, std::int32_t resultCapacity, const char* path, const char* resName, const char* keyword, const char* locid, std::int8_t* isAvailable, std::int8_t omitDefault, std::int32_t* status);
const char* APS5_VABI ures_getVersionNumber_nid_postfix(const UResourceBundle* resourceBundle);
const char* APS5_VABI ures_getVersionNumberInternal_nid_postfix(const UResourceBundle* resourceBundle);
void APS5_VABI ures_getVersion_nid_postfix(const UResourceBundle* resB, std::uint8_t* versionInfo);
void APS5_VABI ures_getVersionByKey_nid_postfix(const UResourceBundle* res, const char* key, std::uint8_t* ver, std::int32_t* status);
const char* APS5_VABI ures_getLocale_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status);
const char* APS5_VABI ures_getLocaleInternal_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status);
const char* APS5_VABI ures_getLocaleByType_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t type, std::int32_t* status);
const char16_t* APS5_VABI ures_getString_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* len, std::int32_t* status);
const char* APS5_VABI ures_getUTF8String_nid_postfix(const UResourceBundle* resB, char* dest, std::int32_t* length, std::int8_t forceCopy, std::int32_t* status);
const std::uint8_t* APS5_VABI ures_getBinary_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* len, std::int32_t* status);
const std::int32_t* APS5_VABI ures_getIntVector_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* len, std::int32_t* status);
std::uint32_t APS5_VABI ures_getUInt_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status);
std::int32_t APS5_VABI ures_getInt_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status);
std::int32_t APS5_VABI ures_getSize_nid_postfix(const UResourceBundle* resourceBundle);
std::int32_t APS5_VABI ures_getType_nid_postfix(const UResourceBundle* resourceBundle);
const char* APS5_VABI ures_getKey_nid_postfix(const UResourceBundle* resourceBundle);
void APS5_VABI ures_resetIterator_nid_postfix(UResourceBundle* resourceBundle);
std::int8_t APS5_VABI ures_hasNext_nid_postfix(const UResourceBundle* resourceBundle);
UResourceBundle* APS5_VABI ures_getNextResource_nid_postfix(UResourceBundle* resourceBundle, UResourceBundle* fillIn, std::int32_t* status);
const char16_t* APS5_VABI ures_getNextString_nid_postfix(UResourceBundle* resourceBundle, std::int32_t* len, const char** key, std::int32_t* status);
UResourceBundle* APS5_VABI ures_getByIndex_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t indexR, UResourceBundle* fillIn, std::int32_t* status);
const char16_t* APS5_VABI ures_getStringByIndex_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t indexS, std::int32_t* len, std::int32_t* status);
const char* APS5_VABI ures_getUTF8StringByIndex_nid_postfix(const UResourceBundle* resB, std::int32_t stringIndex, char* dest, std::int32_t* pLength, std::int8_t forceCopy, std::int32_t* status);
UResourceBundle* APS5_VABI ures_getByKey_nid_postfix(const UResourceBundle* resourceBundle, const char* key, UResourceBundle* fillIn, std::int32_t* status);
const char16_t* APS5_VABI ures_getStringByKey_nid_postfix(const UResourceBundle* resB, const char* key, std::int32_t* len, std::int32_t* status);
const char* APS5_VABI ures_getUTF8StringByKey_nid_postfix(const UResourceBundle* resB, const char* key, char* dest, std::int32_t* pLength, std::int8_t forceCopy, std::int32_t* status);
std::int32_t APS5_VABI ures_countArrayItems_nid_postfix(const UResourceBundle* resourceBundle, const char* resourceKey, std::int32_t* err);
UResourceBundle* APS5_VABI ures_findResource_nid_postfix(const char* pathToResource, UResourceBundle* fillIn, std::int32_t* status);
UResourceBundle* APS5_VABI ures_findSubResource_nid_postfix(const UResourceBundle* resB, char* pathToResource, UResourceBundle* fillIn, std::int32_t* status);
UResourceBundle* APS5_VABI ures_getByKeyWithFallback_nid_postfix(const UResourceBundle* resB, const char* inKey, UResourceBundle* fillIn, std::int32_t* status);
const char16_t* APS5_VABI ures_getStringByKeyWithFallback_nid_postfix(const UResourceBundle* resB, const char* inKey, std::int32_t* len, std::int32_t* status);
}

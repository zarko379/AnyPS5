#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UEnumeration;

extern "C" {
UEnumeration* APS5_VABI uloc_openKeywords_nid_postfix(const char* localeID, std::int32_t* status);
UEnumeration* APS5_VABI uloc_openKeywordList_nid_postfix(const char* keywordList, std::int32_t keywordListSize, std::int32_t* status);
UEnumeration* APS5_VABI uloc_openAvailableByType_nid_postfix(std::int32_t type, std::int32_t* status);
UEnumeration* APS5_VABI ucurr_openISOCurrencies_nid_postfix(std::uint32_t currType, std::int32_t* status);
UEnumeration* APS5_VABI ucurr_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale, std::int8_t commonlyUsed, std::int32_t* status);
UEnumeration* APS5_VABI ucnv_openAllNames_nid_postfix(std::int32_t* status);
UEnumeration* APS5_VABI ucnv_openStandardNames_nid_postfix(const char* convName, const char* standard, std::int32_t* status);
UEnumeration* APS5_VABI ucal_openTimeZones_nid_postfix(std::int32_t* status);
UEnumeration* APS5_VABI ucal_openTimeZoneIDEnumeration_nid_postfix(std::int32_t zoneType, const char* region, const std::int32_t* rawOffset, std::int32_t* status);
UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale, std::int8_t commonlyUsed, std::int32_t* status);
UEnumeration* APS5_VABI unumsys_openAvailableNames_nid_postfix(std::int32_t* status);
UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const strings[], std::int32_t count, std::int32_t* status);
UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const strings[], std::int32_t count, std::int32_t* status);
}

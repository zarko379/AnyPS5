#include "prx/libSceIcu/EnumerationSources.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ucal.h>
#include <unicode/ucnv.h>
#include <unicode/ucurr.h>
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/unumsys.h>
#include "ulocimp.h"
#include <cstring>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

void RequireTerminatedKeywords(const char* keywordList, std::int32_t keywordListSize) {
    if (keywordListSize < 0 || (keywordListSize > 0 && !keywordList)) APS5_INVALID_ARG_EX;
    std::int32_t position = 0;
    while (position < keywordListSize && keywordList[position] != '\0') {
        const void* end = std::memchr(keywordList + position, 0, static_cast<std::size_t>(keywordListSize - position));
        if (!end) APS5_INVALID_ARG_EX;
        position = static_cast<std::int32_t>(static_cast<const char*>(end) - keywordList) + 1;
    }
}

UEnumeration* Guest(UEnumeration* host, std::int32_t* status) {
    return IcuBackend::GuestEnumeration(host, status);
}

}

extern "C" {

UEnumeration* APS5_VABI uloc_openKeywords_nid_postfix(const char* localeID, std::int32_t* status) {
    IcuBackend::Ensure();
    auto* error = reinterpret_cast<UErrorCode*>(status);
    if (!error || U_FAILURE(*error)) return nullptr;
    return Guest(uloc_openKeywords(IcuBackend::Locale(localeID), error), status);
}

UEnumeration* APS5_VABI uloc_openKeywordList_nid_postfix(const char* keywordList, std::int32_t keywordListSize, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    RequireTerminatedKeywords(keywordList, keywordListSize);
    return Guest(uloc_openKeywordList(keywordList, keywordListSize, error), status);
}

UEnumeration* APS5_VABI uloc_openAvailableByType_nid_postfix(std::int32_t type, std::int32_t* status) {
    return Guest(uloc_openAvailableByType(static_cast<ULocAvailableType>(type), Status(status)), status);
}

UEnumeration* APS5_VABI ucurr_openISOCurrencies_nid_postfix(std::uint32_t currType, std::int32_t* status) {
    IcuBackend::Ensure();
    return Guest(ucurr_openISOCurrencies(currType, reinterpret_cast<UErrorCode*>(status)), status);
}

UEnumeration* APS5_VABI ucurr_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale, std::int8_t commonlyUsed, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return Guest(ucurr_getKeywordValuesForLocale(key, IcuBackend::Locale(locale), commonlyUsed, error), status);
}

UEnumeration* APS5_VABI ucnv_openAllNames_nid_postfix(std::int32_t* status) {
    return Guest(ucnv_openAllNames(Status(status)), status);
}

UEnumeration* APS5_VABI ucnv_openStandardNames_nid_postfix(const char* convName, const char* standard, std::int32_t* status) {
    return Guest(ucnv_openStandardNames(convName, standard, Status(status)), status);
}

UEnumeration* APS5_VABI ucal_openTimeZones_nid_postfix(std::int32_t* status) {
    return Guest(ucal_openTimeZones(Status(status)), status);
}

UEnumeration* APS5_VABI ucal_openTimeZoneIDEnumeration_nid_postfix(std::int32_t zoneType, const char* region, const std::int32_t* rawOffset, std::int32_t* status) {
    return Guest(ucal_openTimeZoneIDEnumeration(static_cast<USystemTimeZoneType>(zoneType), region, rawOffset, Status(status)), status);
}

UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char* key, const char* locale, std::int8_t commonlyUsed, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return Guest(ucal_getKeywordValuesForLocale(key, IcuBackend::Locale(locale), commonlyUsed, error), status);
}

UEnumeration* APS5_VABI unumsys_openAvailableNames_nid_postfix(std::int32_t* status) {
    return Guest(unumsys_openAvailableNames(Status(status)), status);
}

UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const strings[], std::int32_t count, std::int32_t* status) {
    return Guest(uenum_openCharStringsEnumeration(strings, count, Status(status)), status);
}

UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const strings[], std::int32_t count, std::int32_t* status) {
    return Guest(uenum_openUCharStringsEnumeration(strings, count, Status(status)), status);
}

}

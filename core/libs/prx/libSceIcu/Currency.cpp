#include "prx/libSceIcu/Currency.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ucurr.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include <unicode/ustring.h>
#include "uresimp.h"
#include "ulocimp.h"
#include "ureslocs.h"
#include <cstring>
#include <string>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

const std::int32_t* Code(const UErrorCode* status) {
    return reinterpret_cast<const std::int32_t*>(status);
}

bool LocaleName(const char* locale, char (&name)[ULOC_FULLNAME_CAPACITY]) {
    IcuBackend::CanonicalLocale(locale);
    UErrorCode status = U_ZERO_ERROR;
    uloc_getName(locale, name, ULOC_FULLNAME_CAPACITY, &status);
    return U_SUCCESS(status) && status != U_STRING_NOT_TERMINATED_WARNING;
}

std::string IsoCode(const char16_t* currency, bool upper) {
    char code[4];
    u_UCharsToChars(currency, code, 3);
    code[3] = 0;
    if (upper) {
        for (char* letter = code; *letter; ++letter) {
            if (*letter >= 'a' && *letter <= 'z') *letter = static_cast<char>(*letter - 'a' + 'A');
        }
    }
    return code;
}

void CheckName(const char* loc, const std::string& code, std::int32_t style) {
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle* bundle = ures_open(U_ICUDATA_CURR, loc, &status);
    IcuBackend::CheckFallback(Code(&status));
    const char16_t* name = nullptr;
    if (style == UCURR_NARROW_SYMBOL_NAME || style == UCURR_FORMAL_SYMBOL_NAME || style == UCURR_VARIANT_SYMBOL_NAME) {
        const std::string table = style == UCURR_NARROW_SYMBOL_NAME ? "Currencies%narrow" : style == UCURR_FORMAL_SYMBOL_NAME ? "Currencies%formal" : "Currencies%variant";
        const std::string key = table + "/" + code;
        IcuBackend::CheckWithFallback(bundle, key.c_str(), Code(&status));
        std::int32_t length = 0;
        name = ures_getStringByKeyWithFallback(bundle, key.c_str(), &length, &status);
        if (status == U_MISSING_RESOURCE_ERROR) status = U_ZERO_ERROR;
    }
    if (!name) {
        IcuBackend::CheckKey(bundle, "Currencies", Code(&status));
        bundle = ures_getByKey(bundle, "Currencies", bundle, &status);
        IcuBackend::CheckWithFallback(bundle, code.c_str(), Code(&status));
    }
    ures_close(bundle);
}

void CheckPluralName(const char* locale, const char* loc, const char16_t* currency, const char* pluralCount) {
    const std::string code = IsoCode(currency, false);
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle* bundle = ures_open(U_ICUDATA_CURR, loc, &status);
    IcuBackend::CheckFallback(Code(&status));
    IcuBackend::CheckKey(bundle, "CurrencyPlurals", Code(&status));
    bundle = ures_getByKey(bundle, "CurrencyPlurals", bundle, &status);
    IcuBackend::CheckWithFallback(bundle, code.c_str(), Code(&status));
    bundle = ures_getByKeyWithFallback(bundle, code.c_str(), bundle, &status);
    IcuBackend::CheckWithFallback(bundle, pluralCount, Code(&status));
    std::int32_t length = 0;
    ures_getStringByKeyWithFallback(bundle, pluralCount, &length, &status);
    if (U_FAILURE(status)) {
        status = U_ZERO_ERROR;
        IcuBackend::CheckWithFallback(bundle, "other", Code(&status));
        ures_getStringByKeyWithFallback(bundle, "other", &length, &status);
        if (U_FAILURE(status)) {
            ures_close(bundle);
            char name[ULOC_FULLNAME_CAPACITY];
            LocaleName(locale, name);
            CheckName(name, IsoCode(currency, true), UCURR_LONG_NAME);
            return;
        }
    }
    ures_close(bundle);
}

const char16_t* Name(const char16_t* result, const char16_t* currency, std::int32_t length, std::int32_t* len) {
    if (len) *len = length;
    else if (result == currency) APS5_INVALID_ARG_EX;
    return result;
}


double Date(const UResourceBundle* entry, const char* key, bool& present) {
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer value(ures_getByKey(entry, key, nullptr, &status));
    std::int32_t length = 0;
    const std::int32_t* parts = ures_getIntVector(value.getAlias(), &length, &status);
    present = U_SUCCESS(status) && parts && length >= 2;
    if (!present) return 0;
    return static_cast<double>((static_cast<std::int64_t>(parts[0]) << 32) | (static_cast<std::int64_t>(parts[1]) & INT64_C(0x00000000FFFFFFFF)));
}

void CheckCurrencyMap(const char* locale, double date, std::int32_t index, bool all) {
    char id[ULOC_FULLNAME_CAPACITY];
    UErrorCode status = U_ZERO_ERROR;
    ulocimp_getRegionForSupplementalData(locale, false, id, sizeof(id), &status);
    if (U_FAILURE(status)) return;
    if (char* variant = std::strchr(id, '_')) *variant = 0;
    icu::LocalUResourceBundlePointer data(ures_openDirect(U_ICUDATA_CURR, "supplementalData", &status));
    icu::LocalUResourceBundlePointer map(ures_getByKey(data.getAlias(), "CurrencyMap", nullptr, &status));
    icu::LocalUResourceBundlePointer region(ures_getByKey(map.getAlias(), id, nullptr, &status));
    if (U_FAILURE(status)) return;
    const std::int32_t size = ures_getSize(region.getAlias());
    if (!all && (index <= 0 || index > size)) return;
    std::int32_t matched = 0;
    for (std::int32_t item = 0; item < size; ++item) {
        UErrorCode entryStatus = U_ZERO_ERROR;
        icu::LocalUResourceBundlePointer entry(ures_getByIndex(region.getAlias(), item, nullptr, &entryStatus));
        bool present = false;
        const double from = Date(entry.getAlias(), "from", present);
        if (!present) APS5_INVALID_ARG_EX;
        bool valid = from <= date;
        if (ures_getSize(entry.getAlias()) > 2) {
            const double to = Date(entry.getAlias(), "to", present);
            if (!present) APS5_INVALID_ARG_EX;
            valid = valid && date < to;
        }
        if (!all && valid && ++matched == index) return;
    }
}

}

extern "C" {

std::int32_t APS5_VABI ucurr_forLocale_nid_postfix(const char* locale, char16_t* buff, std::int32_t buffCapacity, std::int32_t* ec) {
    UErrorCode* error = Status(ec);
    if (U_SUCCESS(*error) && buffCapacity >= 0 && (buff || buffCapacity == 0)) IcuBackend::Locale(locale);
    return ucurr_forLocale(locale, buff, buffCapacity, error);
}

const void* APS5_VABI ucurr_register_nid_postfix(const char16_t* isoCode, const char* locale, std::int32_t* status) {
    IcuBackend::Ensure();
    if (status && *status <= U_ZERO_ERROR) IcuBackend::Locale(locale);
    return ucurr_register(isoCode, locale, reinterpret_cast<UErrorCode*>(status));
}

std::int8_t APS5_VABI ucurr_unregister_nid_postfix(const void* key, std::int32_t* status) {
    IcuBackend::Ensure();
    return ucurr_unregister(key, reinterpret_cast<UErrorCode*>(status));
}

const char16_t* APS5_VABI ucurr_getName_nid_postfix(const char16_t* currency, const char* locale, std::int32_t nameStyle, std::int8_t* isChoiceFormat, std::int32_t* len, std::int32_t* ec) {
    UErrorCode* error = Status(ec);
    if (U_FAILURE(*error)) return nullptr;
    if (nameStyle >= 0 && nameStyle <= UCURR_VARIANT_SYMBOL_NAME) {
        char loc[ULOC_FULLNAME_CAPACITY];
        if (LocaleName(locale, loc)) {
            IcuBackend::RequireObject(currency);
            CheckName(loc, IsoCode(currency, true), nameStyle);
        }
    }
    std::int32_t length = len ? *len : 0;
    const char16_t* result = ucurr_getName(currency, locale, static_cast<UCurrNameStyle>(nameStyle), reinterpret_cast<UBool*>(isChoiceFormat), &length, error);
    return Name(result, currency, length, len);
}

const char16_t* APS5_VABI ucurr_getPluralName_nid_postfix(const char16_t* currency, const char* locale, std::int8_t* isChoiceFormat, const char* pluralCount, std::int32_t* len, std::int32_t* ec) {
    UErrorCode* error = Status(ec);
    if (U_FAILURE(*error)) return nullptr;
    char loc[ULOC_FULLNAME_CAPACITY];
    if (LocaleName(locale, loc)) {
        IcuBackend::RequireObject(currency);
        CheckPluralName(locale, loc, currency, pluralCount);
    }
    std::int32_t length = len ? *len : 0;
    const char16_t* result = ucurr_getPluralName(currency, locale, reinterpret_cast<UBool*>(isChoiceFormat), pluralCount, &length, error);
    return Name(result, currency, length, len);
}

std::int32_t APS5_VABI ucurr_getDefaultFractionDigits_nid_postfix(const char16_t* currency, std::int32_t* ec) {
    return ucurr_getDefaultFractionDigits(currency, Status(ec));
}

std::int32_t APS5_VABI ucurr_getDefaultFractionDigitsForUsage_nid_postfix(const char16_t* currency, std::int32_t usage, std::int32_t* ec) {
    return ucurr_getDefaultFractionDigitsForUsage(currency, static_cast<UCurrencyUsage>(usage), Status(ec));
}

double APS5_VABI ucurr_getRoundingIncrement_nid_postfix(const char16_t* currency, std::int32_t* ec) {
    return ucurr_getRoundingIncrement(currency, Status(ec));
}

double APS5_VABI ucurr_getRoundingIncrementForUsage_nid_postfix(const char16_t* currency, std::int32_t usage, std::int32_t* ec) {
    return ucurr_getRoundingIncrementForUsage(currency, static_cast<UCurrencyUsage>(usage), Status(ec));
}

std::int32_t APS5_VABI ucurr_getNumericCode_nid_postfix(const char16_t* currency) {
    IcuBackend::Ensure();
    return ucurr_getNumericCode(currency);
}

std::int8_t APS5_VABI ucurr_isAvailable_nid_postfix(const char16_t* isoCode, double from, double to, std::int32_t* errorCode) {
    return ucurr_isAvailable(isoCode, from, to, Status(errorCode));
}

std::int32_t APS5_VABI ucurr_forLocaleAndDate_nid_postfix(const char* locale, double date, std::int32_t index, char16_t* buff, std::int32_t buffCapacity, std::int32_t* ec) {
    IcuBackend::Ensure();
    if (ec && *ec <= U_ZERO_ERROR && ((buff && buffCapacity) || !buffCapacity)) {
        IcuBackend::Locale(locale);
        CheckCurrencyMap(locale, date, index, false);
    }
    return ucurr_forLocaleAndDate(locale, date, index, buff, buffCapacity, reinterpret_cast<UErrorCode*>(ec));
}

std::int32_t APS5_VABI ucurr_countCurrencies_nid_postfix(const char* locale, double date, std::int32_t* ec) {
    IcuBackend::Ensure();
    if (ec && *ec <= U_ZERO_ERROR) {
        IcuBackend::Locale(locale);
        CheckCurrencyMap(locale, date, 0, true);
    }
    return ucurr_countCurrencies(locale, date, reinterpret_cast<UErrorCode*>(ec));
}

}

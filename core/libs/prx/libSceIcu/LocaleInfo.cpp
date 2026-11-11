#include "prx/libSceIcu/LocaleInfo.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include "cstring.h"
#include "ulocimp.h"
#include "ureslocs.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr char Untouched = 0x7f;
constexpr std::int32_t ScratchLimit = 4096;

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

template<typename TCall>
std::int32_t Chars(char* dest, std::int32_t capacity, TCall call) {
    if (dest || capacity <= 0) return call(dest, capacity);
    const std::int32_t size = std::min(capacity, ScratchLimit);
    std::vector<char> scratch(static_cast<std::size_t>(size), Untouched);
    const std::int32_t length = call(scratch.data(), size);
    if (std::any_of(scratch.begin(), scratch.end(), [](char value) { return value != Untouched; })) APS5_INVALID_ARG_EX;
    return length;
}

void Probe(const char* tree, const char* locale) {
    UErrorCode probe = U_ZERO_ERROR;
    ures_close(ures_open(tree, locale, &probe));
    IcuBackend::CheckFallback(reinterpret_cast<const std::int32_t*>(&probe));
}

bool Replaced(const char* table, const char* item) {
    if (!table || !item) return false;
    if (std::strcmp(table, "Countries") == 0) return uloc_getCurrentCountryID(item) != item;
    if (std::strcmp(table, "Languages") == 0) return uloc_getCurrentLanguageID(item) != item;
    return false;
}

void CheckReplacement(const char* table, const char* item) {
    if (Replaced(table, item)) IcuBackend::RequireGuestDefault();
}

bool DisplayProceeds(const std::int32_t* status, const char16_t* dest, std::int32_t capacity) {
    return status && *status <= U_ZERO_ERROR && capacity >= 0 && (capacity == 0 || dest);
}

enum class Component { Language, Script, Country, Variant };

void CheckComponent(const char* locale, const char* displayLocale, Component component) {
    IcuBackend::Locale(locale);
    char item[ULOC_FULLNAME_CAPACITY * 4];
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t length = 0;
    switch (component) {
    case Component::Language: length = uloc_getLanguage(locale, item, sizeof(item), &status); break;
    case Component::Script: length = uloc_getScript(locale, item, sizeof(item), &status); break;
    case Component::Country: length = uloc_getCountry(locale, item, sizeof(item), &status); break;
    case Component::Variant: length = uloc_getVariant(locale, item, sizeof(item), &status); break;
    }
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) return;
    if (length == 0) {
        if (component != Component::Language) return;
        std::strcpy(item, "und");
    }
    if (component == Component::Language && std::strtol(item, nullptr, 10)) return;
    IcuBackend::CanonicalLocale(displayLocale);
    Probe(component == Component::Country ? U_ICUDATA_REGION : U_ICUDATA_LANG, displayLocale);
    CheckReplacement(component == Component::Country ? "Countries" : component == Component::Language ? "Languages" : nullptr, item);
}

std::string KeywordValue(const char* locale, const char* keyword, bool& ok) {
    std::vector<char> buffer(64);
    for (;;) {
        UErrorCode status = U_ZERO_ERROR;
        const std::int32_t length = uloc_getKeywordValue(locale, keyword, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        if (status == U_BUFFER_OVERFLOW_ERROR || status == U_STRING_NOT_TERMINATED_WARNING) {
            buffer.resize(static_cast<std::size_t>(length) + 1);
            continue;
        }
        ok = U_SUCCESS(status);
        return ok ? std::string(buffer.data(), static_cast<std::size_t>(length)) : std::string();
    }
}

void CheckCurrencyName(const char* displayLocale, const std::string& currency) {
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer bundle(ures_open(U_ICUDATA_CURR, displayLocale, &status));
    IcuBackend::CheckFallback(reinterpret_cast<const std::int32_t*>(&status));
    if (U_FAILURE(status)) return;
    const std::int32_t proceed = U_ZERO_ERROR;
    IcuBackend::CheckKey(bundle.getAlias(), "Currencies", &proceed);
    UErrorCode tableStatus = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer currencies(ures_getByKey(bundle.getAlias(), "Currencies", nullptr, &tableStatus));
    if (U_FAILURE(tableStatus)) return;
    IcuBackend::CheckWithFallback(currencies.getAlias(), currency.c_str(), &proceed);
}

void CheckKeywordValue(const char* locale, const char* keyword, const char* displayLocale) {
    bool ok = false;
    const std::string value = KeywordValue(locale, keyword, ok);
    if (keyword && uprv_stricmp(keyword, "currency") == 0 && !ok) return;
    IcuBackend::CanonicalLocale(displayLocale);
    if (keyword && uprv_stricmp(keyword, "currency") == 0) CheckCurrencyName(displayLocale, value);
    else Probe(U_ICUDATA_LANG, displayLocale);
}

void CheckDisplayName(const char* locale, const char* displayLocale) {
    IcuBackend::Locale(locale);
    IcuBackend::CanonicalLocale(displayLocale);
    Probe(U_ICUDATA_LANG, displayLocale);
    CheckComponent(locale, displayLocale, Component::Language);
    CheckComponent(locale, displayLocale, Component::Script);
    CheckComponent(locale, displayLocale, Component::Country);
    CheckComponent(locale, displayLocale, Component::Variant);
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUEnumerationPointer keywords(uloc_openKeywords(locale, &status));
    while (const char* keyword = uenum_next(keywords.getAlias(), nullptr, &status)) {
        Probe(U_ICUDATA_LANG, displayLocale);
        CheckKeywordValue(locale, keyword, displayLocale);
    }
}

void CheckOrientation(const char* localeId) {
    IcuBackend::CanonicalLocale(localeId);
    char canonical[ULOC_FULLNAME_CAPACITY];
    UErrorCode status = U_ZERO_ERROR;
    uloc_canonicalize(localeId, canonical, sizeof(canonical), &status);
    if (U_SUCCESS(status)) Probe(nullptr, canonical);
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getName_nid_postfix(const char* localeID, char* name, std::int32_t nameCapacity, std::int32_t* err) {
    UErrorCode* error = Status(err);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CanonicalLocale(localeID);
    return Chars(name, nameCapacity, [&](char* dest, std::int32_t capacity) { return uloc_getName(localeID, dest, capacity, error); });
}

std::int32_t APS5_VABI uloc_getParent_nid_postfix(const char* localeID, char* parent, std::int32_t parentCapacity, std::int32_t* err) {
    UErrorCode* error = Status(err);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::Locale(localeID);
    const char* id = localeID ? localeID : uloc_getDefault();
    const char* lastUnderscore = std::strrchr(id, '_');
    if (lastUnderscore && lastUnderscore != id && parentCapacity < 0 && (uprv_strnicmp(id, "und_", 4) == 0 || parent != id)) APS5_INVALID_ARG_EX;
    if (!parent && parentCapacity > 0) APS5_INVALID_ARG_EX;
    return uloc_getParent(localeID, parent, parentCapacity, error);
}

std::int32_t APS5_VABI uloc_getVariant_nid_postfix(const char* localeID, char* variant, std::int32_t variantCapacity, std::int32_t* err) {
    IcuBackend::Ensure();
    auto* error = reinterpret_cast<UErrorCode*>(err);
    if (!error || U_FAILURE(*error)) return 0;
    IcuBackend::Locale(localeID);
    return Chars(variant, variantCapacity, [&](char* dest, std::int32_t capacity) { return uloc_getVariant(localeID, dest, capacity, error); });
}

const char* APS5_VABI uloc_getISO3Language_nid_postfix(const char* localeID) {
    return uloc_getISO3Language(IcuBackend::Locale(localeID));
}

const char* APS5_VABI uloc_getISO3Country_nid_postfix(const char* localeID) {
    return uloc_getISO3Country(IcuBackend::Locale(localeID));
}

const char* const* APS5_VABI uloc_getISOLanguages_nid_postfix() {
    IcuBackend::Ensure();
    return uloc_getISOLanguages();
}

const char* const* APS5_VABI uloc_getISOCountries_nid_postfix() {
    IcuBackend::Ensure();
    return uloc_getISOCountries();
}

std::uint32_t APS5_VABI uloc_getLCID_nid_postfix(const char* localeID) {
    IcuBackend::Ensure();
    if (localeID && std::strchr(localeID, '@')) {
        bool ok = false;
        if (!KeywordValue(localeID, "collation", ok).empty() && ok) IcuBackend::CanonicalLocale(localeID);
    }
    return uloc_getLCID(localeID);
}

std::int32_t APS5_VABI uloc_getLocaleForLCID_nid_postfix(std::uint32_t hostID, char* locale, std::int32_t localeCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (localeCapacity < 0 || (!locale && localeCapacity > 0)) {
        char probe[ULOC_FULLNAME_CAPACITY];
        UErrorCode probeStatus = U_ZERO_ERROR;
        if (uloc_getLocaleForLCID(hostID, probe, sizeof(probe), &probeStatus) >= 0) APS5_INVALID_ARG_EX;
    }
    return uloc_getLocaleForLCID(hostID, locale, localeCapacity, error);
}

const char* APS5_VABI uloc_toUnicodeLocaleKey_nid_postfix(const char* keyword) {
    IcuBackend::Ensure();
    return uloc_toUnicodeLocaleKey(static_cast<const char*>(IcuBackend::RequireObject(keyword)));
}

const char* APS5_VABI uloc_toUnicodeLocaleType_nid_postfix(const char* keyword, const char* value) {
    IcuBackend::Ensure();
    return uloc_toUnicodeLocaleType(keyword, static_cast<const char*>(IcuBackend::RequireObject(value)));
}

const char* APS5_VABI uloc_toLegacyKey_nid_postfix(const char* keyword) {
    IcuBackend::Ensure();
    return uloc_toLegacyKey(static_cast<const char*>(IcuBackend::RequireObject(keyword)));
}

const char* APS5_VABI uloc_toLegacyType_nid_postfix(const char* keyword, const char* value) {
    IcuBackend::Ensure();
    return uloc_toLegacyType(keyword, static_cast<const char*>(IcuBackend::RequireObject(value)));
}

std::int32_t APS5_VABI uloc_setKeywordValue_nid_postfix(const char* keywordName, const char* keywordValue, char* buffer, std::int32_t bufferCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && keywordName && keywordName[0] && bufferCapacity > 1) IcuBackend::RequireObject(buffer);
    return uloc_setKeywordValue(keywordName, keywordValue, buffer, bufferCapacity, error);
}

std::int32_t APS5_VABI uloc_countAvailable_nid_postfix() {
    IcuBackend::Ensure();
    return uloc_countAvailable();
}

const char* APS5_VABI uloc_getAvailable_nid_postfix(std::int32_t n) {
    IcuBackend::Ensure();
    const std::int32_t count = uloc_countAvailable();
    if (count > 0 && (n < 0 || n == count)) APS5_INVALID_ARG_EX;
    return uloc_getAvailable(n);
}

std::int32_t APS5_VABI uloc_addLikelySubtags_nid_postfix(const char* localeID, char* maximizedLocaleID, std::int32_t maximizedLocaleIDCapacity, std::int32_t* err) {
    UErrorCode* error = Status(err);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CanonicalLocale(localeID);
    return Chars(maximizedLocaleID, maximizedLocaleIDCapacity, [&](char* dest, std::int32_t capacity) { return uloc_addLikelySubtags(localeID, dest, capacity, error); });
}

std::int32_t APS5_VABI uloc_minimizeSubtags_nid_postfix(const char* localeID, char* minimizedLocaleID, std::int32_t minimizedLocaleIDCapacity, std::int32_t* err) {
    UErrorCode* error = Status(err);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CanonicalLocale(localeID);
    return Chars(minimizedLocaleID, minimizedLocaleIDCapacity, [&](char* dest, std::int32_t capacity) { return uloc_minimizeSubtags(localeID, dest, capacity, error); });
}

std::int32_t APS5_VABI uloc_toLanguageTag_nid_postfix(const char* localeID, char* langtag, std::int32_t langtagCapacity, std::int8_t strict, std::int32_t* err) {
    UErrorCode* error = Status(err);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CanonicalLocale(localeID);
    return Chars(langtag, langtagCapacity, [&](char* dest, std::int32_t capacity) { return uloc_toLanguageTag(localeID, dest, capacity, strict, error); });
}

std::int32_t APS5_VABI uloc_getCharacterOrientation_nid_postfix(const char* localeId, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) CheckOrientation(localeId);
    return uloc_getCharacterOrientation(localeId, error);
}

std::int32_t APS5_VABI uloc_getLineOrientation_nid_postfix(const char* localeId, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) CheckOrientation(localeId);
    return uloc_getLineOrientation(localeId, error);
}

std::int8_t APS5_VABI uloc_isRightToLeft_nid_postfix(const char* locale) {
    IcuBackend::Locale(locale);
    char script[8];
    UErrorCode status = U_ZERO_ERROR;
    const std::int32_t length = uloc_getScript(locale, script, sizeof(script), &status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING || length == 0) IcuBackend::CanonicalLocale(locale);
    return uloc_isRightToLeft(locale);
}

const char16_t* APS5_VABI uloc_getTableStringWithFallback_nid_postfix(const char* path, const char* locale, const char* tableKey, const char* subTableKey, const char* itemKey, std::int32_t* pLength, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    IcuBackend::Package(path);
    IcuBackend::CanonicalLocale(locale);
    const bool replaceable = tableKey && (std::strcmp(tableKey, "Countries") == 0 || std::strcmp(tableKey, "Languages") == 0);
    if (!tableKey || (replaceable && !itemKey)) APS5_INVALID_ARG_EX;
    Probe(path, locale);
    CheckReplacement(tableKey, itemKey);
    return uloc_getTableStringWithFallback(path, locale, tableKey, subTableKey, itemKey, pLength, error);
}

std::int32_t APS5_VABI uloc_getDisplayLanguage_nid_postfix(const char* locale, const char* displayLocale, char16_t* language, std::int32_t languageCapacity, std::int32_t* status) {
    IcuBackend::Ensure();
    if (DisplayProceeds(status, language, languageCapacity)) CheckComponent(locale, displayLocale, Component::Language);
    return uloc_getDisplayLanguage(locale, displayLocale, language, languageCapacity, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI uloc_getDisplayScript_nid_postfix(const char* locale, const char* displayLocale, char16_t* script, std::int32_t scriptCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (scriptCapacity >= 0 && (scriptCapacity == 0 || script)) CheckComponent(locale, displayLocale, Component::Script);
    return uloc_getDisplayScript(locale, displayLocale, script, scriptCapacity, error);
}

std::int32_t APS5_VABI uloc_getDisplayCountry_nid_postfix(const char* locale, const char* displayLocale, char16_t* country, std::int32_t countryCapacity, std::int32_t* status) {
    IcuBackend::Ensure();
    if (DisplayProceeds(status, country, countryCapacity)) CheckComponent(locale, displayLocale, Component::Country);
    return uloc_getDisplayCountry(locale, displayLocale, country, countryCapacity, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI uloc_getDisplayVariant_nid_postfix(const char* locale, const char* displayLocale, char16_t* variant, std::int32_t variantCapacity, std::int32_t* status) {
    IcuBackend::Ensure();
    if (DisplayProceeds(status, variant, variantCapacity)) CheckComponent(locale, displayLocale, Component::Variant);
    return uloc_getDisplayVariant(locale, displayLocale, variant, variantCapacity, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI uloc_getDisplayName_nid_postfix(const char* localeID, const char* inLocaleID, char16_t* result, std::int32_t maxResultSize, std::int32_t* err) {
    IcuBackend::Ensure();
    if (DisplayProceeds(err, result, maxResultSize)) CheckDisplayName(localeID, inLocaleID);
    return uloc_getDisplayName(localeID, inLocaleID, result, maxResultSize, reinterpret_cast<UErrorCode*>(err));
}

std::int32_t APS5_VABI uloc_getDisplayKeyword_nid_postfix(const char* keyword, const char* displayLocale, char16_t* dest, std::int32_t destCapacity, std::int32_t* status) {
    IcuBackend::Ensure();
    if (DisplayProceeds(status, dest, destCapacity)) {
        IcuBackend::RequireObject(keyword);
        IcuBackend::CanonicalLocale(displayLocale);
        Probe(U_ICUDATA_LANG, displayLocale);
    }
    return uloc_getDisplayKeyword(keyword, displayLocale, dest, destCapacity, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI uloc_getDisplayKeywordValue_nid_postfix(const char* locale, const char* keyword, const char* displayLocale, char16_t* dest, std::int32_t destCapacity, std::int32_t* status) {
    IcuBackend::Ensure();
    if (DisplayProceeds(status, dest, destCapacity)) CheckKeywordValue(locale, keyword, displayLocale);
    return uloc_getDisplayKeywordValue(locale, keyword, displayLocale, dest, destCapacity, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI ulocimp_getRegionForSupplementalData_nid_postfix(const char* localeID, std::int8_t inferRegion, char* region, std::int32_t regionCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return 0;
    if (inferRegion) IcuBackend::CanonicalLocale(localeID);
    else IcuBackend::Locale(localeID);
    if (regionCapacity < 0 || (!region && regionCapacity > 0)) APS5_INVALID_ARG_EX;
    return ulocimp_getRegionForSupplementalData(localeID, inferRegion, region, regionCapacity, error);
}

const char* const* APS5_VABI ulocimp_getKnownCanonicalizedLocaleForTest_nid_postfix(std::int32_t* length) {
    IcuBackend::Ensure();
    return ulocimp_getKnownCanonicalizedLocaleForTest(static_cast<std::int32_t*>(const_cast<void*>(IcuBackend::RequireObject(length))));
}

bool APS5_VABI ulocimp_isCanonicalizedLocaleForTest_nid_postfix(const char* localeName) {
    return ulocimp_isCanonicalizedLocaleForTest(IcuBackend::CanonicalLocale(localeName));
}

}

#include "prx/libSceIcu/LocaleInfo.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"

namespace GuestLocale {

const auto getParent = &uloc_getParent;
const auto getISO3Language = &uloc_getISO3Language;
const auto getISO3Country = &uloc_getISO3Country;
const auto getISOLanguages = &uloc_getISOLanguages;
const auto getISOCountries = &uloc_getISOCountries;

}

#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uloc.h>
#include "ulocimp.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU LOCALE INFO: %s\n", message.c_str());
        std::abort();
    }
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    }
    return false;
}

template<typename TAction>
bool NotImplemented(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void SetDefaults(const char* locale) {
    std::int32_t status = Ok;
    u_cleanup_nid_postfix();
    u_cleanup();
    UErrorCode accessStatus = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &accessStatus);
    if (locale) uloc_setDefault_nid_postfix(locale, &status);
    UErrorCode hostStatus = U_ZERO_ERROR;
    uloc_setDefault(locale ? locale : "en_US_POSIX", &hostStatus);
    Require(status == Ok && U_SUCCESS(hostStatus), std::string("the defaults are set to ") + (locale ? locale : "none"));
}

std::string Name(const char* text) {
    return text ? text : "(null)";
}

const char* const Locales[] = {
    "en", "en_US", "de_AT", "zh_Hant_TW", "sr_Latn_RS", "es_419", "und_US", "ja_JP@calendar=japanese", "en-US-u-ca-gregory", "fr@currency=EUR",
    "de@collation=phonebook;currency=CHF", "", "root", "x-klingon", "iw_IL", "in", "zh_CN_PINYIN", "sh", "en_US_POSIX", "hy_AM_REVISED",
    "art_lojban", "i-klingon", "ar_EG", "he", "fa_IR", "ku", "123", "en__POSIX", "C", "c.utf8", "no_NO_NY", "toolongtoolong_ZZ", "de__PHONEBOOK",
    "und", "und_Arab", "sr@latn", "en_GB@rg=uszzzz", "en@rg=gbzzzz", "pt_BR@currency=usd", "zz_YU", "en_YU", "ms_MY@sd=my10", "ccp", "ks_Arab_IN",
    "i-default", "en-x-i-default", "es-u-rg-mxzzzz", "x-private", "en_US@attribute=x;calendar=buddhist", "@calendar=gregorian", "ja@collation",
};

template<typename TGuest, typename THost>
void CompareChars(const std::string& what, TGuest guest, THost host) {
    for (std::int32_t capacity : {0, 1, 3, 8, 200}) {
        std::vector<char> buffer(static_cast<std::size_t>(capacity) + 2, 'x');
        std::vector<char> hostBuffer(static_cast<std::size_t>(capacity) + 2, 'x');
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t length = guest(capacity ? buffer.data() : nullptr, capacity, &status);
        const std::int32_t hostLength = host(capacity ? hostBuffer.data() : nullptr, capacity, &hostStatus);
        Require(length == hostLength && status == hostStatus && buffer == hostBuffer, what + " at capacity " + std::to_string(capacity) + ": " + std::to_string(length) + "/" + std::to_string(hostLength) + " " + std::to_string(status) + "/" + std::to_string(hostStatus));
    }
    std::vector<char> hostBuffer(8, 'x');
    UErrorCode hostStatus = U_ZERO_ERROR;
    host(hostBuffer.data(), static_cast<std::int32_t>(hostBuffer.size()), &hostStatus);
    const bool writes = std::any_of(hostBuffer.begin(), hostBuffer.end(), [](char value) { return value != 'x'; });
    if (writes) {
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; guest(nullptr, 8, &ok); }), what + ": a NULL buffer with a capacity throws where ICU writes");
    } else {
        std::int32_t status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(guest(nullptr, 8, &status) == host(nullptr, 8, &hostStatus) && status == hostStatus, what + ": a NULL buffer ICU never writes matches");
    }
    std::int32_t failed = U_INVALID_FORMAT_ERROR;
    Require(guest(nullptr, 8, &failed) <= 0 && failed == U_INVALID_FORMAT_ERROR, what + ": a failed status returns");
}

std::u16string Text(const char16_t* text, std::int32_t length, std::int32_t capacity) {
    return std::u16string(text, static_cast<std::size_t>(std::max(0, std::min(length, capacity))));
}

template<typename TGuest, typename THost>
void CompareUChars(const std::string& what, TGuest guest, THost host) {
    for (std::int32_t capacity : {0, 2, 5, 300}) {
        std::vector<char16_t> buffer(static_cast<std::size_t>(capacity) + 2, u'x');
        std::vector<char16_t> hostBuffer(static_cast<std::size_t>(capacity) + 2, u'x');
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t length = guest(capacity ? buffer.data() : nullptr, capacity, &status);
        const std::int32_t hostLength = host(capacity ? hostBuffer.data() : nullptr, capacity, &hostStatus);
        Require(length == hostLength && status == hostStatus && buffer == hostBuffer, what + " at capacity " + std::to_string(capacity) + ": " + std::to_string(length) + "/" + std::to_string(hostLength) + " " + std::to_string(status) + "/" + std::to_string(hostStatus));
    }
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(guest(nullptr, 4, &status) == host(nullptr, 4, &hostStatus) && status == hostStatus, what + ": a NULL buffer with a capacity is ICU's illegal argument");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    char16_t buffer[4];
    char16_t hostBuffer[4];
    Require(guest(buffer, -1, &status) == host(hostBuffer, -1, &hostStatus) && status == hostStatus, what + ": a negative capacity is ICU's illegal argument");
}

void CheckStrings() {
    for (const char* locale : Locales) {
        const std::string what = std::string("locale ") + locale;
        CompareChars(what + " getName", [&](char* d, std::int32_t c, std::int32_t* s) { return uloc_getName_nid_postfix(locale, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return uloc_getName(locale, d, c, s); });
        CompareChars(what + " addLikelySubtags", [&](char* d, std::int32_t c, std::int32_t* s) { return uloc_addLikelySubtags_nid_postfix(locale, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return uloc_addLikelySubtags(locale, d, c, s); });
        CompareChars(what + " minimizeSubtags", [&](char* d, std::int32_t c, std::int32_t* s) { return uloc_minimizeSubtags_nid_postfix(locale, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return uloc_minimizeSubtags(locale, d, c, s); });
        for (std::int8_t strict : {0, 1}) {
            CompareChars(what + " toLanguageTag " + std::to_string(strict), [&](char* d, std::int32_t c, std::int32_t* s) { return uloc_toLanguageTag_nid_postfix(locale, d, c, strict, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return uloc_toLanguageTag(locale, d, c, strict, s); });
            CompareChars(what + " region " + std::to_string(strict), [&](char* d, std::int32_t c, std::int32_t* s) { return ulocimp_getRegionForSupplementalData_nid_postfix(locale, strict, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return ulocimp_getRegionForSupplementalData(locale, strict, d, c, s); });
        }
        for (std::int32_t capacity : {0, 1, 2, 64}) {
            std::vector<char> buffer(static_cast<std::size_t>(capacity) + 2, 'x');
            std::vector<char> hostBuffer(static_cast<std::size_t>(capacity) + 2, 'x');
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            const std::int32_t length = GuestLocale::getParent(locale, capacity ? buffer.data() : nullptr, capacity, &status);
            const std::int32_t hostLength = uloc_getParent(locale, capacity ? hostBuffer.data() : nullptr, capacity, &hostStatus);
            Require(length == hostLength && status == hostStatus && buffer == hostBuffer, what + " getParent " + std::to_string(capacity));
        }
        Require(ulocimp_isCanonicalizedLocaleForTest_nid_postfix(locale) == ulocimp_isCanonicalizedLocaleForTest(locale), what + " canonicalized");
        for (int line = 0; line < 2; ++line) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            const std::int32_t layout = line ? uloc_getLineOrientation_nid_postfix(locale, &status) : uloc_getCharacterOrientation_nid_postfix(locale, &status);
            const std::int32_t hostLayout = line ? uloc_getLineOrientation(locale, &hostStatus) : uloc_getCharacterOrientation(locale, &hostStatus);
            Require(layout == hostLayout && status == hostStatus, what + " orientation " + std::to_string(line));
        }
    }
    const char* const keys[] = {"calendar", "ca", "collation", "co", "currency", "cu", "timezone", "tz", "colAlternate", "ka", "kr", "vt", "rg", "sd", "x", "abc", "a-b", "", "numbers", "nu", "hc", "em", "foo1"};
    const char* const values[] = {"gregorian", "gregory", "buddhist", "phonebook", "phonebk", "usd", "America/Los_Angeles", "uslax", "Etc/GMT+1", "shifted", "latn-digit", "0041", "u0041", "uszzzz", "usca", "", "a_", "x/y", "abc-def", "1234_5678", "-", "foo bar"};
    for (const char* key : keys) {
        Require(Name(uloc_toUnicodeLocaleKey_nid_postfix(key)) == Name(uloc_toUnicodeLocaleKey(key)), std::string("toUnicodeLocaleKey ") + key);
        Require(Name(uloc_toLegacyKey_nid_postfix(key)) == Name(uloc_toLegacyKey(key)), std::string("toLegacyKey ") + key);
        for (const char* value : values) {
            Require(Name(uloc_toUnicodeLocaleType_nid_postfix(key, value)) == Name(uloc_toUnicodeLocaleType(key, value)), std::string("toUnicodeLocaleType ") + key + " " + value);
            Require(Name(uloc_toLegacyType_nid_postfix(key, value)) == Name(uloc_toLegacyType(key, value)), std::string("toLegacyType ") + key + " " + value);
        }
    }
    Require(Name(uloc_toUnicodeLocaleType_nid_postfix(nullptr, "gregory")) == Name(uloc_toUnicodeLocaleType(nullptr, "gregory")), "toUnicodeLocaleType accepts a NULL key");
    Require(Name(uloc_toLegacyType_nid_postfix(nullptr, "gregory")) == Name(uloc_toLegacyType(nullptr, "gregory")), "toLegacyType accepts a NULL key");
    Require(Throws<std::invalid_argument>([] { uloc_toUnicodeLocaleKey_nid_postfix(nullptr); }), "toUnicodeLocaleKey throws for NULL");
    Require(Throws<std::invalid_argument>([] { uloc_toLegacyKey_nid_postfix(nullptr); }), "toLegacyKey throws for NULL");
    Require(Throws<std::invalid_argument>([] { uloc_toUnicodeLocaleType_nid_postfix("ca", nullptr); }), "toUnicodeLocaleType throws for a NULL value");
    Require(Throws<std::invalid_argument>([] { uloc_toLegacyType_nid_postfix("vt", nullptr); }), "toLegacyType throws for a NULL value");
}

void CheckLists() {
    const std::int32_t count = uloc_countAvailable_nid_postfix();
    Require(count == uloc_countAvailable() && count > 500, "countAvailable matches");
    for (std::int32_t index = 0; index < count; ++index) Require(std::string(uloc_getAvailable_nid_postfix(index)) == uloc_getAvailable(index), "getAvailable " + std::to_string(index));
    Require(Throws<std::invalid_argument>([count] { uloc_getAvailable_nid_postfix(count); }), "getAvailable throws one past the end, which ICU 68.2 reads");
    Require(Throws<std::invalid_argument>([] { uloc_getAvailable_nid_postfix(-1); }), "getAvailable throws for a negative index");
    Require(uloc_getAvailable_nid_postfix(count + 1) == nullptr && uloc_getAvailable_nid_postfix(1 << 30) == nullptr, "getAvailable returns NULL beyond the end");
    const char* const* languages = GuestLocale::getISOLanguages();
    const char* const* hostLanguages = uloc_getISOLanguages();
    std::size_t index = 0;
    for (; hostLanguages[index]; ++index) Require(languages[index] && std::strcmp(languages[index], hostLanguages[index]) == 0, "ISO languages");
    Require(!languages[index] && index > 100, "ISO languages end together");
    const char* const* countries = GuestLocale::getISOCountries();
    const char* const* hostCountries = uloc_getISOCountries();
    for (index = 0; hostCountries[index]; ++index) Require(countries[index] && std::strcmp(countries[index], hostCountries[index]) == 0, "ISO countries");
    Require(!countries[index] && index > 100, "ISO countries end together");
    std::int32_t length = -1;
    std::int32_t hostLength = -2;
    const char* const* known = ulocimp_getKnownCanonicalizedLocaleForTest_nid_postfix(&length);
    const char* const* hostKnown = ulocimp_getKnownCanonicalizedLocaleForTest(&hostLength);
    Require(length == hostLength && length > 10, "known canonicalized locales count");
    for (std::int32_t item = 0; item < length; ++item) Require(std::strcmp(known[item], hostKnown[item]) == 0, "known canonicalized locales");
    Require(Throws<std::invalid_argument>([] { ulocimp_getKnownCanonicalizedLocaleForTest_nid_postfix(nullptr); }), "a NULL length throws");
    for (const char* locale : {"en", "en_US", "de_AT", "zh_Hant_TW", "sr_Latn_RS", "es_419", "ar_EG", "fa_IR", "he", "ja_JP@calendar=japanese", "pt_BR@currency=usd"}) {
        Require(std::string(GuestLocale::getISO3Language(locale)) == uloc_getISO3Language(locale), std::string("ISO3 language ") + locale);
        Require(std::string(GuestLocale::getISO3Country(locale)) == uloc_getISO3Country(locale), std::string("ISO3 country ") + locale);
    }
}

const char* const DisplayLocales[] = {"en", "de", "fr_CA", "ja", "zh_Hant", "", "ar", "sr_Latn", "en_GB", "es_MX"};

void CheckDisplayNames() {
    for (const char* locale : Locales) {
        for (const char* display : DisplayLocales) {
            const std::string what = std::string("display ") + locale + " in " + display;
            CompareUChars(what + " language", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayLanguage_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayLanguage(locale, display, d, c, s); });
            CompareUChars(what + " script", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayScript_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayScript(locale, display, d, c, s); });
            CompareUChars(what + " country", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayCountry_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayCountry(locale, display, d, c, s); });
            CompareUChars(what + " variant", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayVariant_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayVariant(locale, display, d, c, s); });
            CompareUChars(what + " name", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayName_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayName(locale, display, d, c, s); });
            for (const char* keyword : {"calendar", "currency", "collation", "numbers", "rg", "unknown", "Currency"}) {
                CompareUChars(what + " keyword value " + keyword, [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayKeywordValue_nid_postfix(locale, keyword, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayKeywordValue(locale, keyword, display, d, c, s); });
            }
        }
    }
    for (const char* keyword : {"calendar", "currency", "collation", "numbers", "unknown", "", "x"}) {
        for (const char* display : DisplayLocales) {
            CompareUChars(std::string("display keyword ") + keyword + " in " + display, [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayKeyword_nid_postfix(keyword, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayKeyword(keyword, display, d, c, s); });
        }
    }
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; char16_t buffer[8]; uloc_getDisplayKeyword_nid_postfix(nullptr, "en", buffer, 8, &ok); }), "a NULL keyword throws where ICU measures it");
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    char16_t buffer[64];
    char16_t hostBuffer[64];
    const std::int32_t length = uloc_getDisplayKeywordValue_nid_postfix("de@calendar=buddhist", nullptr, "en", buffer, 64, &status);
    const std::int32_t hostLength = uloc_getDisplayKeywordValue("de@calendar=buddhist", nullptr, "en", hostBuffer, 64, &hostStatus);
    Require(length == hostLength && status == hostStatus && Text(buffer, length, 64) == Text(hostBuffer, hostLength, 64), "a NULL keyword value matches ICU");
    for (const char* function : {"language", "country", "variant", "name", "keyword", "value"}) {
        const std::string which(function);
        auto call = [&](std::int32_t* code) {
            char16_t text[8];
            if (which == "language") return uloc_getDisplayLanguage_nid_postfix("en", "en", text, 8, code);
            if (which == "country") return uloc_getDisplayCountry_nid_postfix("en_US", "en", text, 8, code);
            if (which == "variant") return uloc_getDisplayVariant_nid_postfix("en_US_POSIX", "en", text, 8, code);
            if (which == "name") return uloc_getDisplayName_nid_postfix("en_US", "en", text, 8, code);
            if (which == "keyword") return uloc_getDisplayKeyword_nid_postfix("calendar", "en", text, 8, code);
            return uloc_getDisplayKeywordValue_nid_postfix("en@calendar=buddhist", "calendar", "en", text, 8, code);
        };
        Require(call(nullptr) == 0, which + ": a NULL status returns as in ICU");
        std::int32_t failed = U_INVALID_FORMAT_ERROR;
        Require(call(&failed) == 0 && failed == U_INVALID_FORMAT_ERROR, which + ": a failed status returns as in ICU");
    }
    Require(Throws<std::invalid_argument>([] { char16_t text[8]; uloc_getDisplayScript_nid_postfix("en_Latn", "en", text, 8, nullptr); }), "getDisplayScript throws for a NULL status ICU writes");
    status = U_INVALID_FORMAT_ERROR;
    hostStatus = U_INVALID_FORMAT_ERROR;
    Require(uloc_getDisplayScript_nid_postfix("en_Latn", "en", buffer, 64, &status) == uloc_getDisplayScript("en_Latn", "en", hostBuffer, 64, &hostStatus) && status == hostStatus, "getDisplayScript ignores a failed status as ICU 68.2 does");
}

void CheckTableStrings() {
    struct Case {
        const char* path;
        const char* table;
        const char* subTable;
        const char* item;
    };
    const Case cases[] = {
        {"ICUDATA-lang", "Languages", nullptr, "de"}, {"ICUDATA-lang", "Languages", nullptr, "xx"}, {"ICUDATA-region", "Countries", nullptr, "DE"}, {"ICUDATA-region", "Countries", nullptr, "QQ"},
        {"ICUDATA-lang", "Scripts", nullptr, "Latn"}, {"ICUDATA-lang", "Keys", nullptr, "calendar"}, {"ICUDATA-lang", "Types", "calendar", "buddhist"}, {"ICUDATA-lang", "Types", "missing", "x"},
        {nullptr, "layout", nullptr, "characters"}, {nullptr, "layout", nullptr, "lines"}, {nullptr, "missing", nullptr, "x"}, {"ICUDATA-lang", "Languages", nullptr, ""},
        {"ICUDATA-lang", "Keys", nullptr, nullptr}, {"ICUDATA-curr", "Currencies", nullptr, "USD"},
    };
    for (const Case& item : cases) {
        for (const char* locale : {"en", "de_AT", "ja", "", "zh_Hant_HK", "sr_Latn_ME", "en_GB"}) {
            for (std::int32_t incoming : {Ok, static_cast<std::int32_t>(U_USING_FALLBACK_WARNING), static_cast<std::int32_t>(U_USING_DEFAULT_WARNING), static_cast<std::int32_t>(U_INVALID_FORMAT_ERROR)}) {
                std::int32_t length = -9;
                std::int32_t hostLength = -9;
                std::int32_t status = incoming;
                UErrorCode hostStatus = static_cast<UErrorCode>(incoming);
                const char16_t* text = uloc_getTableStringWithFallback_nid_postfix(item.path, locale, item.table, item.subTable, item.item, &length, &status);
                const UChar* hostText = uloc_getTableStringWithFallback(item.path, locale, item.table, item.subTable, item.item, &hostLength, &hostStatus);
                Require(status == hostStatus && length == hostLength && (text == nullptr) == (hostText == nullptr), std::string("table string ") + Name(item.table) + " " + Name(item.item) + " " + locale + " " + std::to_string(incoming));
                if (text) Require(std::u16string(text, static_cast<std::size_t>(length)) == std::u16string(hostText, static_cast<std::size_t>(hostLength)), std::string("table string text ") + Name(item.item));
            }
        }
    }
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; uloc_getTableStringWithFallback_nid_postfix("ICUDATA-region", "en", "Countries", nullptr, nullptr, nullptr, &ok); }), "a NULL country item throws where ICU 68.2 compares it");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; uloc_getTableStringWithFallback_nid_postfix("ICUDATA-lang", "en", nullptr, nullptr, "de", nullptr, &ok); }), "a NULL table throws where ICU 68.2 compares it");
    Require(Throws<std::invalid_argument>([] { uloc_getTableStringWithFallback_nid_postfix("ICUDATA-lang", "en", "Languages", nullptr, "de", nullptr, nullptr); }), "a NULL status throws in getTableStringWithFallback");
    Require(NotImplemented([] { std::int32_t ok = Ok; uloc_getTableStringWithFallback_nid_postfix("foo", "en", "Languages", nullptr, "de", nullptr, &ok); }), "a foreign package throws in getTableStringWithFallback");
}

void CheckDefaults() {
    SetDefaults(nullptr);
    std::int32_t ok = Ok;
    char chars[64];
    char16_t text[64];
    Require(NotImplemented([&] { uloc_getName_nid_postfix(nullptr, chars, 64, &ok); }), "getName throws for NULL");
    Require(NotImplemented([&] { uloc_addLikelySubtags_nid_postfix(nullptr, chars, 64, &ok); }), "addLikelySubtags throws for NULL");
    Require(NotImplemented([&] { uloc_minimizeSubtags_nid_postfix(nullptr, chars, 64, &ok); }), "minimizeSubtags throws for NULL");
    Require(NotImplemented([&] { uloc_toLanguageTag_nid_postfix(nullptr, chars, 64, 0, &ok); }), "toLanguageTag throws for NULL");
    Require(NotImplemented([&] { uloc_getCharacterOrientation_nid_postfix(nullptr, &ok); }), "getCharacterOrientation throws for NULL");
    Require(NotImplemented([&] { uloc_getLineOrientation_nid_postfix(nullptr, &ok); }), "getLineOrientation throws for NULL");
    Require(NotImplemented([&] { ulocimp_getRegionForSupplementalData_nid_postfix(nullptr, 0, chars, 64, &ok); }), "getRegionForSupplementalData throws for NULL");
    Require(NotImplemented([&] { ulocimp_isCanonicalizedLocaleForTest_nid_postfix(nullptr); }), "isCanonicalizedLocaleForTest throws for NULL");
    Require(NotImplemented([&] { uloc_getTableStringWithFallback_nid_postfix("ICUDATA-lang", nullptr, "Languages", nullptr, "de", nullptr, &ok); }), "getTableStringWithFallback throws for NULL");
    for (const char* function : {"language", "script", "country", "variant", "name"}) {
        const std::string which(function);
        auto call = [&](const char* locale, const char* display) {
            std::int32_t code = Ok;
            if (which == "language") return uloc_getDisplayLanguage_nid_postfix(locale, display, text, 64, &code);
            if (which == "script") return uloc_getDisplayScript_nid_postfix(locale, display, text, 64, &code);
            if (which == "country") return uloc_getDisplayCountry_nid_postfix(locale, display, text, 64, &code);
            if (which == "variant") return uloc_getDisplayVariant_nid_postfix(locale, display, text, 64, &code);
            return uloc_getDisplayName_nid_postfix(locale, display, text, 64, &code);
        };
        Require(NotImplemented([&] { call(nullptr, "en"); }), which + ": a NULL locale throws");
        Require(NotImplemented([&] { call("sr_Latn_RS_REVISED", nullptr); }), which + ": a NULL display locale throws");
        Require(NotImplemented([&] { call("sr_Latn_RS_REVISED", "xx"); }), which + ": a display locale without data throws");
        Require(NotImplemented([&] { call("sr_Latn_RS_REVISED", "i-default@calendar=gregorian"); }), which + ": an i-default display locale throws");
    }
    Require(NotImplemented([&] { uloc_getDisplayLanguage_nid_postfix("iw", "en", text, 64, &ok); }), "a deprecated language code throws");
    Require(NotImplemented([&] { uloc_getDisplayCountry_nid_postfix("_YU", "en", text, 64, &ok); }), "a deprecated country code throws");
    Require(NotImplemented([&] { uloc_getDisplayName_nid_postfix("in_BU", "de", text, 64, &ok); }), "deprecated codes throw in getDisplayName");
    Require(NotImplemented([&] { uloc_getTableStringWithFallback_nid_postfix("ICUDATA-region", "en", "Countries", nullptr, "UK", nullptr, &ok); }), "a deprecated country throws in getTableStringWithFallback");
    Require(NotImplemented([&] { uloc_getDisplayKeyword_nid_postfix("calendar", "xx", text, 64, &ok); }), "getDisplayKeyword throws for a display locale without data");
    Require(NotImplemented([&] { uloc_getDisplayKeywordValue_nid_postfix("en@calendar=x", "calendar", "xx", text, 64, &ok); }), "getDisplayKeywordValue throws for a display locale without data");
    Require(NotImplemented([&] { uloc_getDisplayKeywordValue_nid_postfix("en@currency=usd", "currency", "xx", text, 64, &ok); }), "a currency display locale without data throws");
    Require(NotImplemented([&] { uloc_getDisplayKeywordValue_nid_postfix("en@currency=USD", "currency", "en_GB", text, 64, &ok); }), "a currency name from a parent bundle throws");
    Require(NotImplemented([&] { uloc_getDisplayName_nid_postfix("en@currency=USD", "en_GB", text, 64, &ok); }), "a currency keyword in getDisplayName throws");
    Require(NotImplemented([&] { uloc_getCharacterOrientation_nid_postfix("xx", &ok); }), "an orientation without data throws");
    Require(NotImplemented([&] { uloc_getDisplayKeywordValue_nid_postfix("de@currency=USD", "currency", "de_CH", text, 64, &ok); }), "a currency name found through fallback throws");
    CompareUChars("a currency name in the display locale's own table", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayKeywordValue_nid_postfix("de@currency=EUR", "currency", "de_CH", d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayKeywordValue("de@currency=EUR", "currency", "de_CH", d, c, s); });
    for (const char* display : {static_cast<const char*>(nullptr), "xx"}) {
        CompareUChars(std::string("a numeric language never opens ") + Name(display), [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayLanguage_nid_postfix("123", display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayLanguage("123", display, d, c, s); });
        CompareUChars(std::string("an empty country never opens ") + Name(display), [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayCountry_nid_postfix("de", display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayCountry("de", display, d, c, s); });
    }
    for (const char* locale : {"i-default@calendar=gregorian", "i-default_US@x=y", "i-default-x@a=b"}) {
        Require(NotImplemented([&] { uloc_getName_nid_postfix(locale, chars, 64, &ok); }), std::string("i-default throws in getName: ") + locale);
        Require(NotImplemented([&] { uloc_addLikelySubtags_nid_postfix(locale, chars, 64, &ok); }), std::string("i-default throws in addLikelySubtags: ") + locale);
        Require(NotImplemented([&] { uloc_toLanguageTag_nid_postfix(locale, chars, 64, 0, &ok); }), std::string("i-default throws in toLanguageTag: ") + locale);
        Require(NotImplemented([&] { ulocimp_isCanonicalizedLocaleForTest_nid_postfix(locale); }), std::string("i-default throws in isCanonicalizedLocaleForTest: ") + locale);
        Require(NotImplemented([&] { ulocimp_getRegionForSupplementalData_nid_postfix(locale, 1, chars, 64, &ok); }), std::string("i-default throws when the region is inferred: ") + locale);
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        char host[64];
        Require(GuestLocale::getParent(locale, chars, 64, &status) == uloc_getParent(locale, host, 64, &hostStatus) && status == hostStatus && std::strcmp(chars, host) == 0, std::string("getParent never canonicalizes: ") + locale);
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(ulocimp_getRegionForSupplementalData_nid_postfix(locale, 0, chars, 64, &status) == ulocimp_getRegionForSupplementalData(locale, 0, host, 64, &hostStatus) && status == hostStatus, std::string("getRegionForSupplementalData without inference never canonicalizes: ") + locale);
    }
    for (const char* locale : {"en_US", "de", "ar_EG", "i-default", "zh_Hant_TW", "ja@calendar=japanese"}) {
        for (const char* display : {"en", "de", "ja", "", "ar"}) {
            const std::string what = std::string("no default ") + locale + " in " + display;
            CompareUChars(what + " language", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayLanguage_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayLanguage(locale, display, d, c, s); });
            CompareUChars(what + " name", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayName_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayName(locale, display, d, c, s); });
        }
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        char host[64];
        Require(uloc_getName_nid_postfix(locale, chars, 64, &status) == uloc_getName(locale, host, 64, &hostStatus) && status == hostStatus && std::strcmp(chars, host) == 0, std::string("getName without a default: ") + locale);
    }
    for (const char* fallback : {"de", "ar_EG", "fa", "xx", "en_GB"}) {
        SetDefaults(fallback);
        for (const char* locale : {static_cast<const char*>(nullptr), "xx", "iw", "in_BU", "_YU", "i-default@calendar=gregorian", "en@currency=usd", "ja_JP@currency=eur;calendar=japanese", "UK"}) {
            for (const char* display : {static_cast<const char*>(nullptr), "xx", "en_GB", "i-default@x=y", "de_CH"}) {
                const std::string what = std::string("default ") + fallback + " " + Name(locale) + " in " + Name(display);
                CompareUChars(what + " language", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayLanguage_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayLanguage(locale, display, d, c, s); });
                CompareUChars(what + " country", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayCountry_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayCountry(locale, display, d, c, s); });
                CompareUChars(what + " name", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayName_nid_postfix(locale, display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayName(locale, display, d, c, s); });
                CompareUChars(what + " currency", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayKeywordValue_nid_postfix(locale, "currency", display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayKeywordValue(locale, "currency", display, d, c, s); });
                CompareUChars(what + " keyword", [&](char16_t* d, std::int32_t c, std::int32_t* s) { return uloc_getDisplayKeyword_nid_postfix("calendar", display, d, c, s); }, [&](char16_t* d, std::int32_t c, UErrorCode* s) { return uloc_getDisplayKeyword("calendar", display, d, c, s); });
                std::int32_t length = 0;
                std::int32_t hostLength = 0;
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                const char16_t* table = uloc_getTableStringWithFallback_nid_postfix("ICUDATA-region", display, "Countries", nullptr, locale ? locale : "UK", &length, &status);
                const UChar* hostTable = uloc_getTableStringWithFallback("ICUDATA-region", display, "Countries", nullptr, locale ? locale : "UK", &hostLength, &hostStatus);
                Require(status == hostStatus && length == hostLength && (table == nullptr) == (hostTable == nullptr), what + " table string");
            }
            CompareChars(std::string("default name ") + fallback + " " + Name(locale), [&](char* d, std::int32_t c, std::int32_t* s) { return uloc_getName_nid_postfix(locale, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return uloc_getName(locale, d, c, s); });
            CompareChars(std::string("default likely ") + fallback + " " + Name(locale), [&](char* d, std::int32_t c, std::int32_t* s) { return uloc_addLikelySubtags_nid_postfix(locale, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return uloc_addLikelySubtags(locale, d, c, s); });
            CompareChars(std::string("default region ") + fallback + " " + Name(locale), [&](char* d, std::int32_t c, std::int32_t* s) { return ulocimp_getRegionForSupplementalData_nid_postfix(locale, 1, d, c, s); }, [&](char* d, std::int32_t c, UErrorCode* s) { return ulocimp_getRegionForSupplementalData(locale, 1, d, c, s); });
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            Require(uloc_getCharacterOrientation_nid_postfix(locale, &status) == uloc_getCharacterOrientation(locale, &hostStatus) && status == hostStatus, std::string("default orientation ") + fallback + " " + Name(locale));
            if (!locale) Require(Name(GuestLocale::getISO3Country(locale)) == Name(uloc_getISO3Country(locale)), std::string("default ISO3 country ") + fallback);
        }
    }
    SetDefaults(nullptr);
}

void CheckErrors() {
    Require(Throws<std::invalid_argument>([] { char buffer[8]; uloc_getName_nid_postfix("en", buffer, 8, nullptr); }), "a NULL status throws in getName");
    Require(Throws<std::invalid_argument>([] { char buffer[8]; uloc_addLikelySubtags_nid_postfix("en", buffer, 8, nullptr); }), "a NULL status throws in addLikelySubtags");
    Require(Throws<std::invalid_argument>([] { char buffer[8]; uloc_minimizeSubtags_nid_postfix("en", buffer, 8, nullptr); }), "a NULL status throws in minimizeSubtags");
    Require(Throws<std::invalid_argument>([] { char buffer[8]; uloc_toLanguageTag_nid_postfix("en", buffer, 8, 0, nullptr); }), "a NULL status throws in toLanguageTag");
    Require(Throws<std::invalid_argument>([] { uloc_getCharacterOrientation_nid_postfix("en", nullptr); }), "a NULL status throws in getCharacterOrientation");
    Require(Throws<std::invalid_argument>([] { uloc_getLineOrientation_nid_postfix("en", nullptr); }), "a NULL status throws in getLineOrientation");
    Require(Throws<std::invalid_argument>([] { char buffer[8]; ulocimp_getRegionForSupplementalData_nid_postfix("en", 0, buffer, 8, nullptr); }), "a NULL status throws in getRegionForSupplementalData");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, nullptr, 8, &ok); }), "a NULL region buffer throws");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; char buffer[8]; ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, buffer, -1, &ok); }), "a negative region capacity throws");
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    status = U_INVALID_FORMAT_ERROR;
    hostStatus = U_INVALID_FORMAT_ERROR;
    Require(uloc_getCharacterOrientation_nid_postfix(nullptr, &status) == uloc_getCharacterOrientation(nullptr, &hostStatus) && status == U_INVALID_FORMAT_ERROR, "a failed status returns before the locale check in getCharacterOrientation");
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckErrors();
    CheckDefaults();
    SetDefaults("en_US");
    CheckStrings();
    CheckLists();
    CheckDisplayNames();
    CheckTableStrings();
    SetDefaults(nullptr);
    std::puts("guest_icu_locale_info passed");
    return 0;
}

#include "prx/libSceIcu/Currency.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/uclean.h>
#include <unicode/ucurr.h>
#include <unicode/udata.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include "ulocimp.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU CURRENCY: %s\n", message.c_str());
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

struct Code {
    char16_t text[6];
    const char* label;
};

const Code Codes[] = {
    {{u'U', u'S', u'D', 0, 0, 0}, "USD"}, {{u'E', u'U', u'R', 0, 0, 0}, "EUR"}, {{u'J', u'P', u'Y', 0, 0, 0}, "JPY"}, {{u'C', u'H', u'F', 0, 0, 0}, "CHF"},
    {{u'u', u's', u'd', 0, 0, 0}, "usd"}, {{u'X', u'X', u'X', 0, 0, 0}, "XXX"}, {{u'Q', u'Q', u'Q', 0, 0, 0}, "QQQ"}, {{u'U', u'S', 0, 0, 0, 0}, "US"},
    {{0, u'X', u'Y', 0, 0, 0}, "empty"}, {{u'B', u'Y', u'N', 0, 0, 0}, "BYN"}, {{u'D', u'E', u'M', 0, 0, 0}, "DEM"}, {{u'U', u'S', u'D', u'X', 0, 0}, "USDX"},
    {{u'I', u'N', u'R', 0, 0, 0}, "INR"}, {{u'C', u'L', u'F', 0, 0, 0}, "CLF"}, {{u'S', u'E', u'K', 0, 0, 0}, "SEK"},
};

const char* const NameLocales[] = {"en", "de", "de_CH", "ja", "en_GB", "en_IN", "fr_CA", "", "root", "ru", "ar", "zh_Hant", "sr_Latn", "en_US_POSIX", "de@currency=CHF"};

std::u16string Text(const char16_t* text, std::int32_t length) {
    return text ? std::u16string(text, static_cast<std::size_t>(std::max<std::int32_t>(length, 0))) : std::u16string(u"(null)");
}

bool Crashes(const char* locale, double date, std::int32_t index, bool all) {
    char id[ULOC_FULLNAME_CAPACITY];
    UErrorCode status = U_ZERO_ERROR;
    ulocimp_getRegionForSupplementalData(locale, false, id, sizeof(id), &status);
    if (char* variant = std::strchr(id, '_')) *variant = 0;
    UResourceBundle* data = ures_openDirect("ICUDATA-curr", "supplementalData", &status);
    UResourceBundle* map = ures_getByKey(data, "CurrencyMap", nullptr, &status);
    UResourceBundle* region = ures_getByKey(map, id, nullptr, &status);
    bool crash = false;
    if (U_SUCCESS(status) && (all || (index > 0 && index <= ures_getSize(region)))) {
        std::int32_t matched = 0;
        for (std::int32_t item = 0; item < ures_getSize(region) && !crash; ++item) {
            UErrorCode entryStatus = U_ZERO_ERROR;
            UResourceBundle* entry = ures_getByIndex(region, item, nullptr, &entryStatus);
            UResourceBundle* from = ures_getByKey(entry, "from", nullptr, &entryStatus);
            std::int32_t length = 0;
            const std::int32_t* fromParts = ures_getIntVector(from, &length, &entryStatus);
            bool valid = false;
            if (!fromParts) {
                crash = true;
            } else {
                valid = static_cast<double>((static_cast<std::int64_t>(fromParts[0]) << 32) | static_cast<std::uint32_t>(fromParts[1])) <= date;
                if (ures_getSize(entry) > 2) {
                    UResourceBundle* to = ures_getByKey(entry, "to", nullptr, &entryStatus);
                    const std::int32_t* toParts = ures_getIntVector(to, &length, &entryStatus);
                    if (!toParts) crash = true;
                    else valid = valid && date < static_cast<double>((static_cast<std::int64_t>(toParts[0]) << 32) | static_cast<std::uint32_t>(toParts[1]));
                    ures_close(to);
                }
            }
            ures_close(from);
            ures_close(entry);
            if (!crash && !all && valid && ++matched == index) break;
        }
    }
    ures_close(region);
    ures_close(map);
    ures_close(data);
    return crash;
}

void CheckForLocale() {
    const char* const locales[] = {"en_US", "de_CH", "zh_CN", "es_BO", "es_MX", "en_AQ", "de_DE", "ja_JP", "en_US@currency=EUR", "pt_BR@currency=usd", "es_ES_PREEURO", "en_US_POSIX", "und", "", "sr_Latn_RS", "en@rg=gbzzzz",
        "zz_XX", "de_DE@currency=x", "en_US@currency=ABCD", "fr_FR@currency=e1r", "de_AT_PREEURO", "en_150", "es_419", "xx_YY", "zh_Hant_TW", "ar_EG", "en", "@currency=jpy", "i-default@currency=EUR"};
    for (const char* locale : locales) {
        for (std::int32_t capacity : {0, 1, 3, 4, 10}) {
            char16_t buffer[12];
            char16_t hostBuffer[12];
            std::fill(std::begin(buffer), std::end(buffer), u'x');
            std::fill(std::begin(hostBuffer), std::end(hostBuffer), u'x');
            for (std::int32_t incoming : {Ok, static_cast<std::int32_t>(U_USING_DEFAULT_WARNING)}) {
                std::int32_t status = incoming;
                UErrorCode hostStatus = static_cast<UErrorCode>(incoming);
                const std::int32_t length = ucurr_forLocale_nid_postfix(locale, capacity ? buffer : nullptr, capacity, &status);
                const std::int32_t hostLength = ucurr_forLocale(locale, capacity ? hostBuffer : nullptr, capacity, &hostStatus);
                Require(length == hostLength && status == hostStatus && std::memcmp(buffer, hostBuffer, sizeof(buffer)) == 0, std::string("forLocale ") + locale + " " + std::to_string(capacity));
            }
        }
        for (double date : {0.0, -1.0e13, 1.0e12, 1.6e12, 4.1e12, -8.6e15, 8.6e15}) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            if (Crashes(locale, date, 0, true)) {
                Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucurr_countCurrencies_nid_postfix(locale, date, &ok); }), std::string("countCurrencies throws where ICU 68.2 reads a missing date: ") + locale);
            } else {
                Require(ucurr_countCurrencies_nid_postfix(locale, date, &status) == ucurr_countCurrencies(locale, date, &hostStatus) && status == hostStatus, std::string("countCurrencies ") + locale);
            }
            for (std::int32_t index = -1; index <= 4; ++index) {
                if (Crashes(locale, date, index, false)) {
                    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; char16_t text[8]; ucurr_forLocaleAndDate_nid_postfix(locale, date, index, text, 8, &ok); }), std::string("forLocaleAndDate throws where ICU 68.2 reads a missing date: ") + locale + " " + std::to_string(index));
                    continue;
                }
                for (std::int32_t capacity : {0, 2, 3, 4, 8, -1}) {
                    char16_t buffer[10];
                    char16_t hostBuffer[10];
                    std::fill(std::begin(buffer), std::end(buffer), u'x');
                    std::fill(std::begin(hostBuffer), std::end(hostBuffer), u'x');
                    status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    const std::int32_t length = ucurr_forLocaleAndDate_nid_postfix(locale, date, index, capacity ? buffer : nullptr, capacity, &status);
                    const std::int32_t hostLength = ucurr_forLocaleAndDate(locale, date, index, capacity ? hostBuffer : nullptr, capacity, &hostStatus);
                    Require(length == hostLength && status == hostStatus && std::memcmp(buffer, hostBuffer, sizeof(buffer)) == 0, std::string("forLocaleAndDate ") + locale + " " + std::to_string(index) + " " + std::to_string(capacity));
                }
            }
        }
    }
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(ucurr_forLocale_nid_postfix("en_US", nullptr, 4, &status) == ucurr_forLocale("en_US", nullptr, 4, &hostStatus) && status == hostStatus && status == U_ILLEGAL_ARGUMENT_ERROR, "forLocale rejects a NULL buffer with a capacity");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    char16_t buffer[4];
    Require(ucurr_forLocaleAndDate_nid_postfix("en_US", 0, 1, nullptr, 4, &status) == ucurr_forLocaleAndDate("en_US", 0, 1, nullptr, 4, &hostStatus) && status == hostStatus && status == U_ILLEGAL_ARGUMENT_ERROR, "forLocaleAndDate rejects a NULL buffer with a capacity");
    Require(ucurr_forLocaleAndDate_nid_postfix("en_US", 0, 1, buffer, 4, nullptr) == 0 && ucurr_countCurrencies_nid_postfix("en_US", 0, nullptr) == 0, "a NULL status returns as in ICU");
    status = U_INVALID_FORMAT_ERROR;
    Require(ucurr_forLocale_nid_postfix(nullptr, buffer, 4, &status) == 0 && status == U_INVALID_FORMAT_ERROR, "a failed status returns before the locale check");
    Require(Throws<std::invalid_argument>([] { char16_t text[4]; ucurr_forLocale_nid_postfix("en_US", text, 4, nullptr); }), "forLocale throws for a NULL status");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucurr_countCurrencies_nid_postfix("en_US", 1.6e12, &ok); }), "countCurrencies throws for the US, whose USN entry has no date");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucurr_countCurrencies_nid_postfix("zh_CN", 1.6e12, &ok); }), "countCurrencies throws for China, whose CNH entry has no end date");
    char16_t usd[8] = {};
    status = Ok;
    Require(ucurr_forLocaleAndDate_nid_postfix("en_US", 1.6e12, 1, usd, 8, &status) == 3 && status == Ok && std::u16string(usd) == u"USD", "forLocaleAndDate stops at USD before the undated entries");
    status = Ok;
    Require(ucurr_countCurrencies_nid_postfix("de_DE", 1.6e12, &status) == 1 && status == Ok, "Germany has one currency in 2020");
}

void CheckRegistry() {
    const char16_t qqq[] = u"QQQ";
    const char16_t zzz[] = u"ZZZ";
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const void* key = ucurr_register_nid_postfix(qqq, "xx_YY", &status);
    UCurrRegistryKey hostKey = ucurr_register(qqq, "xx_YY", &hostStatus);
    Require(key && hostKey && status == hostStatus && status == Ok, "register returns a key");
    const void* second = ucurr_register_nid_postfix(zzz, "de_DE_BERLIN", &status);
    ucurr_register(zzz, "de_DE_BERLIN", &hostStatus);
    for (const char* locale : {"xx_YY", "en_YY", "de_DE_BERLIN", "de_DE", "xx_YY@currency=EUR", "xx_YY_VAR"}) {
        char16_t buffer[8] = {};
        char16_t hostBuffer[8] = {};
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const std::int32_t length = ucurr_forLocale_nid_postfix(locale, buffer, 8, &status);
        const std::int32_t hostLength = ucurr_forLocale(locale, hostBuffer, 8, &hostStatus);
        Require(length == hostLength && status == hostStatus && std::u16string(buffer) == std::u16string(hostBuffer), std::string("a registered currency is found: ") + locale);
    }
    char16_t buffer[8] = {};
    status = Ok;
    ucurr_forLocale_nid_postfix("en_YY", buffer, 8, &status);
    Require(std::u16string(buffer) == u"QQQ", "the registration is keyed by region");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(ucurr_unregister_nid_postfix(key, &status) == 1 && ucurr_unregister(hostKey, &hostStatus) == 1, "unregister removes a key");
    Require(ucurr_unregister_nid_postfix(key, &status) == 0, "unregister reports a removed key");
    Require(ucurr_unregister_nid_postfix(second, nullptr) == 0, "unregister with a NULL status does nothing");
    status = U_INVALID_FORMAT_ERROR;
    Require(ucurr_unregister_nid_postfix(second, &status) == 0 && status == U_INVALID_FORMAT_ERROR, "unregister with a failed status does nothing");
    status = Ok;
    Require(ucurr_unregister_nid_postfix(second, &status) == 1, "unregister removes the second key");
    hostStatus = U_ZERO_ERROR;
    ucurr_unregister(ucurr_register(zzz, "de_DE_BERLIN", &hostStatus), &hostStatus);
    status = Ok;
    std::fill(std::begin(buffer), std::end(buffer), 0);
    ucurr_forLocale_nid_postfix("en_YY", buffer, 8, &status);
    Require(status == U_MISSING_RESOURCE_ERROR, "an unregistered region is missing again");
    status = Ok;
    Require(ucurr_register_nid_postfix(nullptr, "en", &status) == nullptr && status == Ok, "register ignores a NULL code as ICU does");
    Require(ucurr_register_nid_postfix(qqq, "en", nullptr) == nullptr, "register with a NULL status does nothing");
    status = U_INVALID_FORMAT_ERROR;
    Require(ucurr_register_nid_postfix(qqq, nullptr, &status) == nullptr && status == U_INVALID_FORMAT_ERROR, "register with a failed status does nothing");
    status = Ok;
    const void* registered = ucurr_register_nid_postfix(qqq, "fr_QQ", &status);
    SetDefaults("en_US");
    status = Ok;
    Require(ucurr_unregister_nid_postfix(registered, &status) == 0, "u_cleanup drops registrations");
}

void CheckNames() {
    for (const Code& code : Codes) {
        for (const char* locale : NameLocales) {
            for (std::int32_t style = -1; style <= 5; ++style) {
                for (int withLength = 0; withLength < 2; ++withLength) {
                    std::int32_t length = -7;
                    std::int32_t hostLength = -7;
                    std::int8_t choice = 5;
                    UBool hostChoice = 5;
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    const std::string what = std::string("getName ") + code.label + " " + locale + " " + std::to_string(style) + " " + std::to_string(withLength);
                    const char16_t* hostName = ucurr_getName(code.text, locale, static_cast<UCurrNameStyle>(style), &hostChoice, &hostLength, &hostStatus);
                    if (!withLength && hostName == code.text) {
                        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucurr_getName_nid_postfix(code.text, locale, style, &choice, nullptr, &ok); }), what + ": a NULL length throws where ICU writes the ISO code length");
                        continue;
                    }
                    const char16_t* name = ucurr_getName_nid_postfix(code.text, locale, style, &choice, withLength ? &length : nullptr, &status);
                    Require(status == hostStatus && choice == hostChoice && (name == code.text) == (hostName == code.text) && (name == nullptr) == (hostName == nullptr), what);
                    if (withLength) Require(length == hostLength && Text(name, length) == Text(hostName, hostLength), what + " text");
                }
            }
            for (const char* plural : {"one", "other", "few", "many", "zero", static_cast<const char*>(nullptr)}) {
                std::int32_t length = -7;
                std::int32_t hostLength = -7;
                std::int8_t choice = 5;
                UBool hostChoice = 5;
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                const std::string what = std::string("getPluralName ") + code.label + " " + locale + " " + Name(plural);
                const char16_t* name = ucurr_getPluralName_nid_postfix(code.text, locale, &choice, plural, &length, &status);
                const char16_t* hostName = ucurr_getPluralName(code.text, locale, &hostChoice, plural, &hostLength, &hostStatus);
                Require(status == hostStatus && choice == hostChoice && length == hostLength && Text(name, length) == Text(hostName, hostLength) && (name == code.text) == (hostName == code.text), what);
            }
        }
    }
    const char16_t usd[] = u"USD";
    std::int32_t length = 3;
    std::int32_t status = Ok;
    Require(ucurr_getName_nid_postfix(usd, "en", UCURR_LONG_NAME, nullptr, nullptr, &status) && status == Ok, "getName accepts NULL outputs when it finds a name");
    status = U_INVALID_FORMAT_ERROR;
    Require(!ucurr_getName_nid_postfix(nullptr, nullptr, 9, nullptr, &length, &status) && status == U_INVALID_FORMAT_ERROR && length == 3, "a failed status returns before any check");
    status = Ok;
    Require(!ucurr_getName_nid_postfix(nullptr, nullptr, 9, nullptr, &length, &status) && status == U_ILLEGAL_ARGUMENT_ERROR && length == 3, "a bad style returns before the locale and currency checks");
    status = Ok;
    std::string tooLong(200, 'a');
    Require(!ucurr_getName_nid_postfix(nullptr, tooLong.c_str(), UCURR_LONG_NAME, nullptr, &length, &status) && status == U_ILLEGAL_ARGUMENT_ERROR && length == 3, "a long locale returns before the currency check");
    status = Ok;
    Require(!ucurr_getPluralName_nid_postfix(nullptr, tooLong.c_str(), nullptr, "one", &length, &status) && status == U_ILLEGAL_ARGUMENT_ERROR && length == 3, "getPluralName rejects a long locale before the currency check");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; std::int32_t size = 0; ucurr_getName_nid_postfix(nullptr, "en", UCURR_LONG_NAME, nullptr, &size, &ok); }), "a NULL currency throws in getName");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; std::int32_t size = 0; ucurr_getPluralName_nid_postfix(nullptr, "en", nullptr, "one", &size, &ok); }), "a NULL currency throws in getPluralName");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; const char16_t qqq[] = u"QQQ"; ucurr_getPluralName_nid_postfix(qqq, "en", nullptr, "one", nullptr, &ok); }), "a NULL length throws in getPluralName for the ISO code");
    Require(Throws<std::invalid_argument>([] { std::int32_t size = 0; const char16_t code[] = u"USD"; ucurr_getName_nid_postfix(code, "en", UCURR_LONG_NAME, nullptr, &size, nullptr); }), "a NULL status throws in getName");
    Require(Throws<std::invalid_argument>([] { std::int32_t size = 0; const char16_t code[] = u"USD"; ucurr_getPluralName_nid_postfix(code, "en", nullptr, "one", &size, nullptr); }), "a NULL status throws in getPluralName");
}

void CheckMetadata() {
    std::vector<const char16_t*> codes;
    for (const Code& code : Codes) codes.push_back(code.text);
    codes.push_back(nullptr);
    for (const char16_t* code : codes) {
        Require(ucurr_getNumericCode_nid_postfix(code) == ucurr_getNumericCode(code), "getNumericCode");
        for (std::int32_t incoming : {Ok, static_cast<std::int32_t>(U_INVALID_FORMAT_ERROR)}) {
            std::int32_t status = incoming;
            UErrorCode hostStatus = static_cast<UErrorCode>(incoming);
            Require(ucurr_getDefaultFractionDigits_nid_postfix(code, &status) == ucurr_getDefaultFractionDigits(code, &hostStatus) && status == hostStatus, "getDefaultFractionDigits");
            status = incoming;
            hostStatus = static_cast<UErrorCode>(incoming);
            const double increment = ucurr_getRoundingIncrement_nid_postfix(code, &status);
            const double hostIncrement = ucurr_getRoundingIncrement(code, &hostStatus);
            Require(increment == hostIncrement && status == hostStatus, "getRoundingIncrement");
            for (std::int32_t usage = -1; usage <= 2; ++usage) {
                status = incoming;
                hostStatus = static_cast<UErrorCode>(incoming);
                Require(ucurr_getDefaultFractionDigitsForUsage_nid_postfix(code, usage, &status) == ucurr_getDefaultFractionDigitsForUsage(code, static_cast<UCurrencyUsage>(usage), &hostStatus) && status == hostStatus, "getDefaultFractionDigitsForUsage");
                status = incoming;
                hostStatus = static_cast<UErrorCode>(incoming);
                const double usageIncrement = ucurr_getRoundingIncrementForUsage_nid_postfix(code, usage, &status);
                const double hostUsageIncrement = ucurr_getRoundingIncrementForUsage(code, static_cast<UCurrencyUsage>(usage), &hostStatus);
                Require(usageIncrement == hostUsageIncrement && status == hostStatus, "getRoundingIncrementForUsage");
            }
            for (double from : {-1.0e15, 0.0, 9.5e11, 1.6e12}) {
                for (double to : {-1.0e15, 0.0, 1.0e12, 4.1e12, U_DATE_MAX}) {
                    status = incoming;
                    hostStatus = static_cast<UErrorCode>(incoming);
                    Require(ucurr_isAvailable_nid_postfix(code, from, to, &status) == ucurr_isAvailable(code, from, to, &hostStatus) && status == hostStatus, "isAvailable");
                }
            }
        }
    }
    std::int32_t status = Ok;
    const char16_t chf[] = u"CHF";
    const char16_t jpy[] = u"JPY";
    const char16_t dem[] = u"DEM";
    Require(ucurr_getDefaultFractionDigits_nid_postfix(jpy, &status) == 0 && ucurr_getDefaultFractionDigitsForUsage_nid_postfix(chf, UCURR_USAGE_CASH, &status) == 2, "fraction digits match ICU's data");
    Require(std::fabs(ucurr_getRoundingIncrementForUsage_nid_postfix(chf, UCURR_USAGE_CASH, &status) - 0.05) < 1e-12 && status == Ok, "the CHF cash increment is 0.05");
    Require(ucurr_getNumericCode_nid_postfix(chf) == 756 && ucurr_getNumericCode_nid_postfix(jpy) == 392, "numeric codes match ISO 4217");
    Require(ucurr_isAvailable_nid_postfix(dem, 0.0, 1.0e12, &status) == 1 && ucurr_isAvailable_nid_postfix(dem, 1.1e12, 1.2e12, &status) == 0, "DEM is available until 2002");
    Require(Throws<std::invalid_argument>([] { const char16_t code[] = u"USD"; ucurr_getDefaultFractionDigits_nid_postfix(code, nullptr); }), "getDefaultFractionDigits throws for a NULL status");
    Require(Throws<std::invalid_argument>([] { const char16_t code[] = u"USD"; ucurr_getDefaultFractionDigitsForUsage_nid_postfix(code, 0, nullptr); }), "getDefaultFractionDigitsForUsage throws for a NULL status");
    Require(Throws<std::invalid_argument>([] { const char16_t code[] = u"USD"; ucurr_getRoundingIncrement_nid_postfix(code, nullptr); }), "getRoundingIncrement throws for a NULL status");
    Require(Throws<std::invalid_argument>([] { const char16_t code[] = u"USD"; ucurr_getRoundingIncrementForUsage_nid_postfix(code, 0, nullptr); }), "getRoundingIncrementForUsage throws for a NULL status");
    Require(Throws<std::invalid_argument>([] { const char16_t code[] = u"USD"; ucurr_isAvailable_nid_postfix(code, 0, 1, nullptr); }), "isAvailable throws for a NULL status");
}

void CheckDefaults() {
    SetDefaults(nullptr);
    const char16_t usd[] = u"USD";
    const char16_t eur[] = u"EUR";
    char16_t buffer[8];
    std::int32_t length = 0;
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_forLocale_nid_postfix(nullptr, buffer, 8, &ok); }), "forLocale throws for NULL");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_register_nid_postfix(usd, nullptr, &ok); }), "register throws for NULL");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_forLocaleAndDate_nid_postfix(nullptr, 0, 1, buffer, 8, &ok); }), "forLocaleAndDate throws for NULL");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_countCurrencies_nid_postfix(nullptr, 0, &ok); }), "countCurrencies throws for NULL");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getName_nid_postfix(usd, nullptr, UCURR_LONG_NAME, nullptr, &length, &ok); }), "getName throws for NULL");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getPluralName_nid_postfix(usd, nullptr, nullptr, "one", &length, &ok); }), "getPluralName throws for NULL");
    for (const char* locale : {"xx", "xx_YY", "i-default@currency=EUR"}) {
        Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getName_nid_postfix(usd, locale, UCURR_SYMBOL_NAME, nullptr, &length, &ok); }), std::string("getName throws for a locale without data: ") + locale);
        Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getPluralName_nid_postfix(usd, locale, nullptr, "one", &length, &ok); }), std::string("getPluralName throws for a locale without data: ") + locale);
    }
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getName_nid_postfix(usd, "de_CH", UCURR_LONG_NAME, nullptr, &length, &ok); }), "a name found through fallback throws");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getName_nid_postfix(usd, "en_GB", UCURR_LONG_NAME, nullptr, &length, &ok); }), "a Currencies table from a parent throws");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getName_nid_postfix(usd, "de_CH", UCURR_NARROW_SYMBOL_NAME, nullptr, &length, &ok); }), "a narrow symbol found through fallback throws");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getPluralName_nid_postfix(usd, "de_CH", nullptr, "one", &length, &ok); }), "a plural name found through fallback throws");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ucurr_getPluralName_nid_postfix(usd, "de_LU", nullptr, "one", &length, &ok); }), "a CurrencyPlurals table from a parent throws");
    const char16_t byn[] = u"BYN";
    struct Case {
        const char16_t* code;
        const char* locale;
        std::int32_t style;
    };
    for (const Case& item : {Case{eur, "de_CH", UCURR_LONG_NAME}, Case{eur, "de_CH", UCURR_NARROW_SYMBOL_NAME}, Case{usd, "en", UCURR_SYMBOL_NAME}, Case{usd, "root", UCURR_LONG_NAME}, Case{eur, "de", UCURR_FORMAL_SYMBOL_NAME}}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        std::int32_t hostLength = 0;
        const char16_t* name = ucurr_getName_nid_postfix(item.code, item.locale, item.style, nullptr, &length, &status);
        const char16_t* hostName = ucurr_getName(item.code, item.locale, static_cast<UCurrNameStyle>(item.style), nullptr, &hostLength, &hostStatus);
        Require(status == hostStatus && length == hostLength && Text(name, length) == Text(hostName, hostLength), std::string("a name in the locale's own table matches without a default: ") + item.locale);
    }
    {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        std::int32_t hostLength = 0;
        const char16_t* name = ucurr_getPluralName_nid_postfix(byn, "de_CH", nullptr, "one", &length, &status);
        const char16_t* hostName = ucurr_getPluralName(byn, "de_CH", nullptr, "one", &hostLength, &hostStatus);
        Require(status == hostStatus && length == hostLength && Text(name, length) == Text(hostName, hostLength), "a plural name in the locale's own table matches without a default");
    }
    for (const char* locale : {"i-default@currency=EUR", "en_US", "xx_YY", "de_CH"}) {
        char16_t hostBuffer[8];
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(ucurr_forLocale_nid_postfix(locale, buffer, 8, &status) == ucurr_forLocale(locale, hostBuffer, 8, &hostStatus) && status == hostStatus, std::string("forLocale never reads the default for a locale: ") + locale);
    }
    for (const char* fallback : {"de", "xx", "en_GB", "ja_JP"}) {
        SetDefaults(fallback);
        for (const char* locale : {static_cast<const char*>(nullptr), "xx", "de_CH", "en_GB", "i-default@x=y"}) {
            for (const char16_t* code : {usd, eur, byn}) {
                for (std::int32_t style = 0; style <= UCURR_VARIANT_SYMBOL_NAME; ++style) {
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    std::int32_t hostLength = 0;
                    const char16_t* name = ucurr_getName_nid_postfix(code, locale, style, nullptr, &length, &status);
                    const char16_t* hostName = ucurr_getName(code, locale, static_cast<UCurrNameStyle>(style), nullptr, &hostLength, &hostStatus);
                    Require(status == hostStatus && length == hostLength && Text(name, length) == Text(hostName, hostLength), std::string("getName follows the guest default ") + fallback + " " + Name(locale));
                }
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                std::int32_t hostLength = 0;
                const char16_t* name = ucurr_getPluralName_nid_postfix(code, locale, nullptr, "few", &length, &status);
                const char16_t* hostName = ucurr_getPluralName(code, locale, nullptr, "few", &hostLength, &hostStatus);
                Require(status == hostStatus && length == hostLength && Text(name, length) == Text(hostName, hostLength), std::string("getPluralName follows the guest default ") + fallback + " " + Name(locale));
            }
            char16_t hostBuffer[8];
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            Require(ucurr_forLocale_nid_postfix(locale, buffer, 8, &status) == ucurr_forLocale(locale, hostBuffer, 8, &hostStatus) && status == hostStatus, std::string("forLocale follows the guest default ") + fallback + " " + Name(locale));
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            if (Crashes(locale, 1.6e12, 0, true)) {
                Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucurr_countCurrencies_nid_postfix(locale, 1.6e12, &ok); }), std::string("countCurrencies throws for an undated entry under the guest default ") + fallback);
            } else {
                Require(ucurr_countCurrencies_nid_postfix(locale, 1.6e12, &status) == ucurr_countCurrencies(locale, 1.6e12, &hostStatus) && status == hostStatus, std::string("countCurrencies follows the guest default ") + fallback);
            }
        }
    }
    SetDefaults(nullptr);
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckDefaults();
    SetDefaults("en_US");
    CheckForLocale();
    CheckRegistry();
    CheckNames();
    CheckMetadata();
    SetDefaults(nullptr);
    std::puts("guest_icu_currency passed");
    return 0;
}

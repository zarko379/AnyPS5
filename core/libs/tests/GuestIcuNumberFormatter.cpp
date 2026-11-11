#include "prx/libSceIcu/NumberFormatter.hpp"
#include "prx/libSceIcu/NumberFormat.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uloc.h>
#include <unicode/unumberformatter.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU NUMBER FORMATTER: %s\n", message.c_str());
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

struct Output {
    std::u16string text;
    std::int32_t length = 0;
    std::vector<std::int32_t> statuses;
    std::vector<std::int32_t> fields;
    bool operator==(const Output&) const = default;
};

Output Guest(const UNumberFormatter* formatter, const double* value, const char* decimal, std::int32_t decimalLength) {
    Output out;
    std::int32_t status = Ok;
    UFormattedNumber* result = unumf_openResult_nid_postfix(&status);
    if (value) unumf_formatDouble_nid_postfix(formatter, *value, result, &status);
    else unumf_formatDecimal_nid_postfix(formatter, decimal, decimalLength, result, &status);
    out.statuses.push_back(status);
    char16_t buffer[256] = {};
    status = Ok;
    out.length = unumf_resultToString_nid_postfix(result, buffer, 256, &status);
    out.statuses.push_back(status);
    out.text = out.length >= 0 ? std::u16string(buffer, static_cast<std::size_t>(out.length < 256 ? out.length : 256)) : std::u16string();
    status = Ok;
    UFieldPositionIterator* iterator = ufieldpositer_open_nid_postfix(&status);
    unumf_resultGetAllFieldPositions_nid_postfix(result, iterator, &status);
    out.statuses.push_back(status);
    std::int32_t begin = 0;
    std::int32_t end = 0;
    for (std::int32_t field = ufieldpositer_next_nid_postfix(iterator, &begin, &end); field >= 0; field = ufieldpositer_next_nid_postfix(iterator, &begin, &end)) {
        out.fields.push_back(field);
        out.fields.push_back(begin);
        out.fields.push_back(end);
    }
    ufieldpositer_close_nid_postfix(iterator);
    unumf_closeResult_nid_postfix(result);
    return out;
}

Output Host(const UNumberFormatter* formatter, const double* value, const char* decimal, std::int32_t decimalLength) {
    Output out;
    UErrorCode status = U_ZERO_ERROR;
    UFormattedNumber* result = unumf_openResult(&status);
    if (value) unumf_formatDouble(formatter, *value, result, &status);
    else unumf_formatDecimal(formatter, decimal, decimalLength, result, &status);
    out.statuses.push_back(status);
    char16_t buffer[256] = {};
    status = U_ZERO_ERROR;
    out.length = unumf_resultToString(result, buffer, 256, &status);
    out.statuses.push_back(status);
    out.text = out.length >= 0 ? std::u16string(buffer, static_cast<std::size_t>(out.length < 256 ? out.length : 256)) : std::u16string();
    status = U_ZERO_ERROR;
    UFieldPositionIterator* iterator = ufieldpositer_open(&status);
    unumf_resultGetAllFieldPositions(result, iterator, &status);
    out.statuses.push_back(status);
    std::int32_t begin = 0;
    std::int32_t end = 0;
    for (std::int32_t field = ufieldpositer_next(iterator, &begin, &end); field >= 0; field = ufieldpositer_next(iterator, &begin, &end)) {
        out.fields.push_back(field);
        out.fields.push_back(begin);
        out.fields.push_back(end);
    }
    ufieldpositer_close(iterator);
    unumf_closeResult(result);
    return out;
}

const char16_t* const Skeletons[] = {u"", u"percent", u"currency/EUR unit-width-full-name", u"measure-unit/length-meter unit-width-full-name", u"compact-short", u"compact-long", u"scientific", u".00", u"@@@", u"sign-always", u"numbering-system/arab", u"group-off", u"rounding-mode-floor .0", u"integer-width/000", u"scale/100", u"currency/JPY unit-width-iso-code", u"measure-unit/temperature-celsius unit-width-short", u"permille sign-accounting-except-zero", u"bogus skeleton!", u"currency/XXY"};
const double Values[] = {0.0, -1.5, 2.0, 0.5, 1234567.891, 1e-7, 1e21, -42.125, 999.995, 12345};
const char* const Decimals[] = {"1234.5678", "-0", "1e3", "12345678901234567890.123456789012345678901234567890", "abc", "", "0.000001"};

void Sweep(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        for (const char16_t* skeleton : Skeletons) {
            for (std::int32_t length : {-1, 5}) {
                if (length > 0 && std::char_traits<char16_t>::length(skeleton) < static_cast<std::size_t>(length)) continue;
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                UNumberFormatter* guest = unumf_openForSkeletonAndLocale_nid_postfix(skeleton, length, locale, &status);
                UNumberFormatter* host = unumf_openForSkeletonAndLocale(skeleton, length, locale, &hostStatus);
                const std::string what = label + " " + (locale ? locale : "NULL") + " skeleton " + std::to_string(std::char_traits<char16_t>::length(skeleton)) + " " + std::to_string(length);
                Require(status == hostStatus && !guest == !host, what + " opens like ICU");
                for (double value : Values) {
                    Require(Guest(guest, &value, nullptr, 0) == Host(host, &value, nullptr, 0), what + " formats " + std::to_string(value) + " like ICU");
                }
                for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
                    Require(Guest(guest, &value, nullptr, 0) == Host(host, &value, nullptr, 0), what + " formats a special value like ICU");
                }
                for (const char* decimal : Decimals) {
                    Require(Guest(guest, nullptr, decimal, -1) == Host(host, nullptr, decimal, -1), what + " formats the decimal " + decimal + " like ICU");
                    if (std::strlen(decimal) >= 2) Require(Guest(guest, nullptr, decimal, 2) == Host(host, nullptr, decimal, 2), what + " formats a decimal prefix like ICU");
                }
                unumf_close_nid_postfix(guest);
                unumf_close(host);
            }
        }
    }
}

void CheckErrors() {
    SetDefaults(nullptr);
    Require(Throws<std::invalid_argument>([] { unumf_openForSkeletonAndLocale_nid_postfix(u"percent", -1, "en", nullptr); }), "a null status throws in openForSkeletonAndLocale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; unumf_openForSkeletonAndLocale_nid_postfix(u"percent", -1, nullptr, &ok); }), "a NULL locale without a guest default throws");
    Require(Throws<std::runtime_error>([] { std::int32_t failed = IllegalArgument; unumf_openForSkeletonAndLocale_nid_postfix(u"percent", -1, nullptr, &failed); }), "a NULL locale throws with a failed status too, since ICU still builds a usable formatter");
    for (const char* locale : {"xx", "qq_Latn", "xx_YY"}) {
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; unumf_openForSkeletonAndLocale_nid_postfix(u"compact-long", -1, locale, &ok); }), std::string("a locale without data throws: ") + locale);
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; unumf_openForSkeletonAndLocale_nid_postfix(u"", 0, locale, &ok); }), std::string("a second open still throws: ") + locale);
    }
    std::int32_t status = IllegalArgument;
    UErrorCode hostStatus = U_ILLEGAL_ARGUMENT_ERROR;
    UNumberFormatter* failed = unumf_openForSkeletonAndLocale_nid_postfix(u"percent", -1, "de", &status);
    UNumberFormatter* hostFailed = unumf_openForSkeletonAndLocale(u"percent", -1, "de", &hostStatus);
    const double half = 0.5;
    Require(!failed == !hostFailed && status == hostStatus && Guest(failed, &half, nullptr, 0) == Host(hostFailed, &half, nullptr, 0), "a failed status opens the formatter ICU builds");
    unumf_close_nid_postfix(failed);
    unumf_close(hostFailed);
    status = Ok;
    UNumberFormatter* formatter = unumf_openForSkeletonAndLocale_nid_postfix(u"currency/USD", -1, "en_US", &status);
    UFormattedNumber* result = unumf_openResult_nid_postfix(&status);
    unumf_formatDouble_nid_postfix(formatter, -1234.5, result, &status);
    char16_t buffer[32] = {};
    Require(unumf_resultToString_nid_postfix(result, buffer, 32, &status) == 10 && std::u16string(buffer) == u"-$1,234.50", "a known currency format");
    status = Ok;
    Require(unumf_resultToString_nid_postfix(result, nullptr, 0, &status) == 10 && status == BufferOverflow, "resultToString preflights");
    status = Ok;
    Require(unumf_resultToString_nid_postfix(result, nullptr, 3, &status) == 0 && status == IllegalArgument, "a NULL buffer with a capacity is illegal");
    status = Ok;
    Require(unumf_resultToString_nid_postfix(result, buffer, -1, &status) == 0 && status == IllegalArgument, "a negative capacity is illegal");
    status = Ok;
    Require(unumf_resultToString_nid_postfix(nullptr, buffer, 32, &status) == 0 && status == IllegalArgument, "a NULL result is illegal in resultToString");
    alignas(16) unsigned char zeros[512] = {};
    status = Ok;
    Require(unumf_resultToString_nid_postfix(reinterpret_cast<UFormattedNumber*>(zeros), buffer, 32, &status) == 0 && status == InvalidFormat, "a foreign result is an invalid format");
    status = IllegalArgument;
    Require(unumf_resultToString_nid_postfix(result, buffer, 32, &status) == 0 && status == IllegalArgument, "resultToString returns on a failed status");
    Require(Throws<std::invalid_argument>([&] { unumf_resultToString_nid_postfix(result, buffer, 32, nullptr); }), "a null status throws in resultToString");
    status = Ok;
    unumf_formatDouble_nid_postfix(nullptr, 1, result, &status);
    Require(status == IllegalArgument, "a NULL formatter is illegal in formatDouble");
    status = Ok;
    unumf_formatDouble_nid_postfix(reinterpret_cast<UNumberFormatter*>(zeros), 1, result, &status);
    Require(status == InvalidFormat, "a foreign formatter is an invalid format");
    status = Ok;
    unumf_formatDouble_nid_postfix(formatter, 1, nullptr, &status);
    Require(status == IllegalArgument, "a NULL result is illegal in formatDouble");
    status = Ok;
    unumf_formatDecimal_nid_postfix(formatter, "1", 1, reinterpret_cast<UFormattedNumber*>(zeros), &status);
    Require(status == InvalidFormat, "a foreign result is an invalid format in formatDecimal");
    for (std::int32_t length : {-1, 0, 3}) {
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        UNumberFormatter* host = unumf_openForSkeletonAndLocale(u"currency/USD", -1, "en_US", &hostStatus);
        Require(Guest(formatter, nullptr, nullptr, length) == Host(host, nullptr, nullptr, length), "a NULL decimal string matches ICU");
        unumf_close(host);
    }
    Require(Throws<std::invalid_argument>([&] { unumf_formatDouble_nid_postfix(formatter, 1, result, nullptr); }), "a null status throws in formatDouble");
    Require(Throws<std::invalid_argument>([&] { unumf_formatDecimal_nid_postfix(formatter, "1", 1, result, nullptr); }), "a null status throws in formatDecimal");
    status = Ok;
    unumf_resultGetAllFieldPositions_nid_postfix(result, nullptr, &status);
    Require(status == IllegalArgument, "a NULL iterator is illegal");
    status = Ok;
    UFieldPositionIterator* iterator = ufieldpositer_open_nid_postfix(&status);
    unumf_resultGetAllFieldPositions_nid_postfix(nullptr, iterator, &status);
    Require(status == IllegalArgument, "a NULL result is illegal in resultGetAllFieldPositions");
    Require(Throws<std::invalid_argument>([&] { unumf_resultGetAllFieldPositions_nid_postfix(result, iterator, nullptr); }), "a null status throws in resultGetAllFieldPositions");
    ufieldpositer_close_nid_postfix(iterator);
    status = IllegalArgument;
    Require(!unumf_openResult_nid_postfix(&status) && status == IllegalArgument, "openResult returns on a failed status");
    Require(Throws<std::invalid_argument>([] { unumf_openResult_nid_postfix(nullptr); }), "a null status throws in openResult");
    unumf_closeResult_nid_postfix(result);
    unumf_closeResult_nid_postfix(nullptr);
    unumf_closeResult_nid_postfix(reinterpret_cast<UFormattedNumber*>(zeros));
    unumf_close_nid_postfix(formatter);
    unumf_close_nid_postfix(nullptr);
    unumf_close_nid_postfix(reinterpret_cast<UNumberFormatter*>(zeros));
}

void CheckDefaults() {
    SetDefaults("ar_EG");
    std::int32_t status = Ok;
    UNumberFormatter* fallen = unumf_openForSkeletonAndLocale_nid_postfix(u"compact-long", -1, "xx", &status);
    UNumberFormatter* own = unumf_openForSkeletonAndLocale_nid_postfix(u"compact-long", -1, "en", &status);
    UNumberFormatter* implicit = unumf_openForSkeletonAndLocale_nid_postfix(u"", 0, nullptr, &status);
    Require(fallen && own && implicit && status <= Ok, "formatters open under a guest default");
    const double one = 1;
    const Output arabic = Guest(fallen, &one, nullptr, 0);
    Require(!arabic.text.empty() && arabic.text[0] == 0x0661, "xx falls back to the guest default ar_EG");
    Require(Guest(implicit, &one, nullptr, 0).text == u"١", "a NULL locale uses the guest default");
    uloc_setDefault_nid_postfix(nullptr, &status);
    UFormattedNumber* result = unumf_openResult_nid_postfix(&status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; unumf_formatDouble_nid_postfix(fallen, 1, result, &ok); }), "a formatter that fell back to a cleared default throws");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; unumf_formatDecimal_nid_postfix(fallen, "1", 1, result, &ok); }), "formatDecimal throws too");
    std::int32_t failed = IllegalArgument;
    unumf_formatDouble_nid_postfix(fallen, 1, result, &failed);
    Require(failed == IllegalArgument, "a failed status returns before the formatter is used");
    Require(Guest(own, &one, nullptr, 0).text == u"1", "a formatter with its own data still works");
    status = Ok;
    uloc_setDefault_nid_postfix("ar_EG", &status);
    Require(Guest(fallen, &one, nullptr, 0) == arabic, "a guest default again allows it");
    unumf_closeResult_nid_postfix(result);
    unumf_close_nid_postfix(fallen);
    unumf_close_nid_postfix(own);
    unumf_close_nid_postfix(implicit);
    SetDefaults(nullptr);
}

void CompareSkeleton(const char* locale, const std::string& what) {
    for (const char16_t* skeleton : {u"", u"compact-long", u"currency/EUR"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UNumberFormatter* guest = unumf_openForSkeletonAndLocale_nid_postfix(skeleton, -1, locale, &status);
        UNumberFormatter* host = unumf_openForSkeletonAndLocale(skeleton, -1, locale, &hostStatus);
        Require(guest && host && status == hostStatus, what + ": the formatter opens as in ICU");
        for (double value : {1.0, 2.5, 1234567.0}) Require(Guest(guest, &value, nullptr, 0) == Host(host, &value, nullptr, 0), what + ": the formatter formats as ICU's");
        unumf_close_nid_postfix(guest);
        unumf_close(host);
    }
}

void CheckIDefault() {
    SetDefaults(nullptr);
    for (const char* locale : {"i-default@numbers=arab", "i-default_US@x=y", "i-default-x@a=b", "i-default.utf8"}) {
        Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; unumf_openForSkeletonAndLocale_nid_postfix(u"compact-long", -1, locale, &ok); }), std::string("an i-default locale that ICU canonicalizes to the default throws: ") + locale);
    }
    for (const char* locale : {"i-default", "ar_EG", "en@numbers=arab"}) CompareSkeleton(locale, std::string("no default ") + locale);
    SetDefaults("ar_EG");
    for (const char* locale : {"i-default@numbers=latn", "i-default_US@x=y", "i-default.utf8"}) CompareSkeleton(locale, std::string("default ar_EG ") + locale);
    std::int32_t status = Ok;
    UNumberFormatter* arabic = unumf_openForSkeletonAndLocale_nid_postfix(u"", 0, "i-default@x=y", &status);
    const double one = 1;
    Require(arabic && Guest(arabic, &one, nullptr, 0).text == std::u16string(1, static_cast<char16_t>(0x0661)), "an i-default locale formats with the guest default");
    unumf_close_nid_postfix(arabic);
    SetDefaults(nullptr);
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckErrors();
    CheckDefaults();
    CheckIDefault();
    SetDefaults("en_US");
    Sweep({"en", "de", "ar_EG", "ja", "hi", "", "fr_CH", "root", "ru", "en_IN", "bn"}, "sweep");
    for (const char* fallback : {"de", "ar_EG", "fa"}) {
        SetDefaults(fallback);
        Sweep({"xx", "xx_YY", nullptr}, fallback);
    }
    SetDefaults(nullptr);
    std::puts("guest_icu_number_formatter passed");
    return 0;
}

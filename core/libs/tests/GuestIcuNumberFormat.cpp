#include "prx/libSceIcu/NumberFormat.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <unicode/parseerr.h>
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uenum.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uloc.h>
#include <unicode/umisc.h>
#include <unicode/unum.h>
#include <unicode/unumsys.h>
#include <unicode/upluralrules.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

bool GuestEnumerationLayout(void* enumeration);
std::int32_t GuestEnumerationCount(void* enumeration, std::int32_t* status);
const char* GuestEnumerationNext(void* enumeration, std::int32_t* length, std::int32_t* status);
void GuestEnumerationClose(void* enumeration);

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t NotTerminated = -124;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::logic_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::logic_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected posix_memalign"); }

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU NUMBER FORMAT: %s\n", message.c_str());
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

const char16_t RulePattern[] = u"%main:\n-x: minus >>;\nx.x: << point >>;\nInf: infinity;\nNaN: not a number;\n0: zero;\n1: one;\n2: =#,##0=;\n";
const char16_t DecimalPattern[] = u"#,##0.0##;(#)";

const std::int32_t Styles[] = {UNUM_DECIMAL, UNUM_CURRENCY, UNUM_PERCENT, UNUM_SCIENTIFIC, UNUM_SPELLOUT, UNUM_ORDINAL, UNUM_DURATION, UNUM_NUMBERING_SYSTEM, UNUM_PATTERN_RULEBASED, UNUM_CURRENCY_ISO, UNUM_CURRENCY_PLURAL, UNUM_CURRENCY_ACCOUNTING, UNUM_CASH_CURRENCY, UNUM_DECIMAL_COMPACT_SHORT, UNUM_DECIMAL_COMPACT_LONG, UNUM_CURRENCY_STANDARD, UNUM_PATTERN_DECIMAL};
const double Values[] = {0.0, -1.5, 2.0, 0.5, 1234567.891, 1e-7, 1e21, -42.125, 999.995};

const char16_t* PatternFor(std::int32_t style) {
    if (style == UNUM_PATTERN_RULEBASED) return RulePattern;
    if (style == UNUM_PATTERN_DECIMAL) return DecimalPattern;
    return nullptr;
}

std::u16string Text(const char16_t* buffer, std::int32_t length, std::int32_t capacity = 256) {
    return length >= 0 ? std::u16string(buffer, static_cast<std::size_t>(length < capacity ? length : capacity)) : std::u16string();
}

struct Formatted {
    std::u16string text;
    std::int32_t length = 0;
    std::int32_t status = 0;
    std::int32_t begin = 0;
    std::int32_t end = 0;
    std::vector<std::int32_t> fields;
    bool operator==(const Formatted&) const = default;
};

Formatted GuestFormat(void* format, double value, std::int32_t field) {
    Formatted out;
    char16_t buffer[256] = {};
    UFieldPosition position{field, 0, 0};
    out.length = unum_formatDouble_nid_postfix(format, value, buffer, 256, &position, &out.status);
    out.text = Text(buffer, out.length);
    out.begin = position.beginIndex;
    out.end = position.endIndex;
    std::int32_t status = Ok;
    UFieldPositionIterator* iterator = ufieldpositer_open_nid_postfix(&status);
    char16_t other[256] = {};
    const std::int32_t length = unum_formatDoubleForFields_nid_postfix(format, value, other, 256, iterator, &status);
    out.fields.push_back(status);
    out.fields.push_back(length);
    std::int32_t begin = 0;
    std::int32_t end = 0;
    for (std::int32_t kind = ufieldpositer_next_nid_postfix(iterator, &begin, &end); kind >= 0; kind = ufieldpositer_next_nid_postfix(iterator, &begin, &end)) {
        out.fields.push_back(kind);
        out.fields.push_back(begin);
        out.fields.push_back(end);
    }
    ufieldpositer_close_nid_postfix(iterator);
    return out;
}

Formatted HostFormat(UNumberFormat* format, double value, std::int32_t field) {
    Formatted out;
    char16_t buffer[256] = {};
    UFieldPosition position{field, 0, 0};
    UErrorCode status = U_ZERO_ERROR;
    out.length = unum_formatDouble(format, value, buffer, 256, &position, &status);
    out.status = status;
    out.text = Text(buffer, out.length);
    out.begin = position.beginIndex;
    out.end = position.endIndex;
    UErrorCode iteratorStatus = U_ZERO_ERROR;
    UFieldPositionIterator* iterator = ufieldpositer_open(&iteratorStatus);
    char16_t other[256] = {};
    const std::int32_t length = unum_formatDoubleForFields(format, value, other, 256, iterator, &iteratorStatus);
    out.fields.push_back(iteratorStatus);
    out.fields.push_back(length);
    std::int32_t begin = 0;
    std::int32_t end = 0;
    for (std::int32_t kind = ufieldpositer_next(iterator, &begin, &end); kind >= 0; kind = ufieldpositer_next(iterator, &begin, &end)) {
        out.fields.push_back(kind);
        out.fields.push_back(begin);
        out.fields.push_back(end);
    }
    ufieldpositer_close(iterator);
    return out;
}

std::u16string GuestDecimal(void* format, const char* number, std::int32_t length, std::int32_t* status) {
    char16_t buffer[256] = {};
    UFieldPosition position{UNUM_FRACTION_FIELD, 0, 0};
    const std::int32_t written = unum_formatDecimal_nid_postfix(format, number, length, buffer, 256, &position, status);
    return Text(buffer, written) + u"|" + std::u16string(1, static_cast<char16_t>(u'0' + position.beginIndex % 10)) + std::u16string(1, static_cast<char16_t>(u'0' + position.endIndex % 10));
}

std::u16string HostDecimal(UNumberFormat* format, const char* number, std::int32_t length, UErrorCode* status) {
    char16_t buffer[256] = {};
    UFieldPosition position{UNUM_FRACTION_FIELD, 0, 0};
    const std::int32_t written = unum_formatDecimal(format, number, length, buffer, 256, &position, status);
    return Text(buffer, written) + u"|" + std::u16string(1, static_cast<char16_t>(u'0' + position.beginIndex % 10)) + std::u16string(1, static_cast<char16_t>(u'0' + position.endIndex % 10));
}

void CompareFormats(void* guest, UNumberFormat* host, const std::string& what, bool integers = false) {
    for (double value : Values) {
        if (integers && value != std::floor(value)) continue;
        for (std::int32_t field : {UNUM_INTEGER_FIELD, UNUM_FRACTION_FIELD, UNUM_GROUPING_SEPARATOR_FIELD, UNUM_CURRENCY_FIELD}) {
            Require(GuestFormat(guest, value, field) == HostFormat(host, value, field), what + " formats " + std::to_string(value) + " like ICU");
        }
    }
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        if (integers) break;
        Require(GuestFormat(guest, value, UNUM_INTEGER_FIELD) == HostFormat(host, value, UNUM_INTEGER_FIELD), what + " formats a special value like ICU");
    }
    for (const char* number : {"1234.5678", "-0", "1e3", "12345678901234567890.123456789012345678901234567890", "abc", "", "0.000001"}) {
        if (integers && std::strchr(number, '.')) continue;
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(GuestDecimal(guest, number, -1, &status) == HostDecimal(host, number, -1, &hostStatus) && status == hostStatus, what + " formats the decimal " + number + " like ICU");
        if (std::strlen(number) < 2) continue;
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(GuestDecimal(guest, number, 2, &status) == HostDecimal(host, number, 2, &hostStatus) && status == hostStatus, what + " formats a decimal prefix like ICU");
    }
    for (std::int32_t tag = UNUM_POSITIVE_PREFIX; tag <= UNUM_PUBLIC_RULESETS + 1; ++tag) {
        char16_t buffer[512] = {};
        char16_t hostBuffer[512] = {};
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t length = unum_getTextAttribute_nid_postfix(guest, tag, buffer, 512, &status);
        const std::int32_t hostLength = unum_getTextAttribute(host, static_cast<UNumberFormatTextAttribute>(tag), hostBuffer, 512, &hostStatus);
        Require(length == hostLength && status == hostStatus && Text(buffer, length, 512) == Text(hostBuffer, hostLength, 512), what + " text attribute " + std::to_string(tag) + " matches ICU");
    }
    for (std::int32_t symbol = -1; symbol <= UNUM_FORMAT_SYMBOL_COUNT; ++symbol) {
        char16_t buffer[64] = {};
        char16_t hostBuffer[64] = {};
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t length = unum_getSymbol_nid_postfix(guest, symbol, buffer, 64, &status);
        const std::int32_t hostLength = unum_getSymbol(host, static_cast<UNumberFormatSymbol>(symbol), hostBuffer, 64, &hostStatus);
        Require(length == hostLength && status == hostStatus && Text(buffer, length, 64) == Text(hostBuffer, hostLength, 64), what + " symbol " + std::to_string(symbol) + " matches ICU");
    }
}

void Sweep(const std::vector<const char*>& locales, const char* label) {
    for (const char* locale : locales) {
        for (std::int32_t style : Styles) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            UParseError guestError{};
            UParseError hostError{};
            void* guest = unum_open_nid_postfix(style, PatternFor(style), -1, locale, &guestError, &status);
            UNumberFormat* host = unum_open(static_cast<UNumberFormatStyle>(style), PatternFor(style), -1, locale, &hostError, &hostStatus);
            const std::string what = std::string(label) + " " + locale + " style " + std::to_string(style);
            Require(status == hostStatus && !guest == !host, what + " opens like ICU");
            if (!guest) continue;
            CompareFormats(guest, host, what, style == UNUM_ORDINAL);
            std::int32_t cloneStatus = Ok;
            void* clone = unum_clone_nid_postfix(guest, &cloneStatus);
            UErrorCode hostCloneStatus = U_ZERO_ERROR;
            UNumberFormat* hostClone = unum_clone(host, &hostCloneStatus);
            Require(clone && cloneStatus == Ok && hostClone, what + " clones");
            unum_close_nid_postfix(guest);
            Require(GuestFormat(clone, 1234.5, UNUM_INTEGER_FIELD) == HostFormat(hostClone, 1234.5, UNUM_INTEGER_FIELD), what + " clone formats like ICU");
            unum_close_nid_postfix(clone);
            unum_close(hostClone);
            unum_close(host);
        }
    }
}

void CheckNumberingSystems() {
    SetDefaults(nullptr);
    for (const char* locale : {"en", "ar_EG", "ar_MA", "fa", "th_TH@numbers=thai", "zh@numbers=traditional", "zh_Hant@numbers=finance", "ta@numbers=native", "en@numbers=roman", "", "root", "hi_IN", "my", "xx@numbers=thai", "xx@numbers=bogus"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UNumberingSystem* guest = unumsys_open_nid_postfix(locale, &status);
        UNumberingSystem* host = unumsys_open(locale, &hostStatus);
        Require(status == hostStatus && !guest == !host, std::string("unumsys_open ") + locale + " matches ICU");
        if (!guest) continue;
        Require(std::strcmp(unumsys_getName_nid_postfix(guest), unumsys_getName(host)) == 0, std::string("unumsys_getName ") + locale + " matches ICU");
        Require(unumsys_isAlgorithmic_nid_postfix(guest) == unumsys_isAlgorithmic(host), std::string("unumsys_isAlgorithmic ") + locale + " matches ICU");
        unumsys_close_nid_postfix(guest);
        unumsys_close(host);
    }
    for (const char* name : {"latn", "arab", "roman", "hanidec", "jpan", "thai", "default", "bogus", "", static_cast<const char*>(nullptr)}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UNumberingSystem* guest = unumsys_openByName_nid_postfix(name, &status);
        UNumberingSystem* host = unumsys_openByName(name, &hostStatus);
        Require(status == hostStatus && !guest == !host, std::string("unumsys_openByName ") + (name ? name : "NULL") + " matches ICU");
        if (!guest) continue;
        Require(std::strcmp(unumsys_getName_nid_postfix(guest), unumsys_getName(host)) == 0 && unumsys_isAlgorithmic_nid_postfix(guest) == unumsys_isAlgorithmic(host), "an opened numbering system matches ICU");
        unumsys_close_nid_postfix(guest);
        unumsys_close(host);
    }
    std::int32_t status = Ok;
    UNumberingSystem* roman = unumsys_openByName_nid_postfix("roman", &status);
    Require(roman && unumsys_isAlgorithmic_nid_postfix(roman) && std::strcmp(unumsys_getName_nid_postfix(roman), "roman") == 0, "roman is algorithmic");
    unumsys_close_nid_postfix(roman);
    UNumberingSystem* arab = unumsys_open_nid_postfix("ar_EG", &status);
    Require(arab && std::strcmp(unumsys_getName_nid_postfix(arab), "arab") == 0 && !unumsys_isAlgorithmic_nid_postfix(arab), "ar_EG uses arab digits");
    unumsys_close_nid_postfix(arab);
    unumsys_close_nid_postfix(nullptr);
    status = IllegalArgument;
    Require(!unumsys_open_nid_postfix(nullptr, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(!unumsys_openByName_nid_postfix("latn", &status) && status == IllegalArgument, "a failed status returns NULL in unumsys_openByName");
    Require(Throws<std::invalid_argument>([] { unumsys_open_nid_postfix("en", nullptr); }), "a null status throws in unumsys_open");
    Require(Throws<std::invalid_argument>([] { unumsys_openByName_nid_postfix("latn", nullptr); }), "a null status throws in unumsys_openByName");
    Require(Throws<std::invalid_argument>([] { unumsys_getName_nid_postfix(nullptr); }), "a null numbering system throws in getName");
    Require(Throws<std::invalid_argument>([] { unumsys_isAlgorithmic_nid_postfix(nullptr); }), "a null numbering system throws in isAlgorithmic");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; unumsys_open_nid_postfix(nullptr, &ok); }), "a NULL locale without a guest default throws");
    for (const char* locale : {"xx", "qq_Latn", "xx_YY", "xx@numbers=native", "zz@numbers=default"}) {
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; unumsys_open_nid_postfix(locale, &ok); }), std::string("a locale that falls back to the default throws: ") + locale);
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; unumsys_open_nid_postfix(locale, &ok); }), std::string("a second open still throws: ") + locale);
    }
    for (const char* fallback : {"ar_EG", "fa", "de"}) {
        SetDefaults(fallback);
        for (const char* locale : {"xx", "und", static_cast<const char*>(nullptr), "xx@numbers=native"}) {
            std::int32_t ok = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            UNumberingSystem* guest = unumsys_open_nid_postfix(locale, &ok);
            UNumberingSystem* host = unumsys_open(locale, &hostStatus);
            Require(guest && host && ok == hostStatus && std::strcmp(unumsys_getName_nid_postfix(guest), unumsys_getName(host)) == 0, std::string("a guest default ") + fallback + " resolves like ICU");
            unumsys_close_nid_postfix(guest);
            unumsys_close(host);
        }
    }
    SetDefaults("ar_EG");
    status = Ok;
    UNumberingSystem* fallen = unumsys_open_nid_postfix("xx", &status);
    Require(fallen && std::strcmp(unumsys_getName_nid_postfix(fallen), "arab") == 0, "xx falls back to the guest default ar_EG");
    unumsys_close_nid_postfix(fallen);
    SetDefaults(nullptr);
}

void CheckFieldIterator() {
    SetDefaults(nullptr);
    std::int32_t status = IllegalArgument;
    Require(!ufieldpositer_open_nid_postfix(&status) && status == IllegalArgument, "a failed status returns NULL in ufieldpositer_open");
    Require(Throws<std::invalid_argument>([] { ufieldpositer_open_nid_postfix(nullptr); }), "a null status throws in ufieldpositer_open");
    Require(Throws<std::invalid_argument>([] { ufieldpositer_next_nid_postfix(nullptr, nullptr, nullptr); }), "a null iterator throws in ufieldpositer_next");
    ufieldpositer_close_nid_postfix(nullptr);
    status = Ok;
    UFieldPositionIterator* iterator = ufieldpositer_open_nid_postfix(&status);
    Require(iterator && status == Ok, "ufieldpositer_open");
    Require(ufieldpositer_next_nid_postfix(iterator, nullptr, nullptr) == -1, "an unused iterator is empty");
    void* format = unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, "en_US", nullptr, &status);
    char16_t buffer[32] = {};
    Require(unum_formatDoubleForFields_nid_postfix(format, -12345.678, buffer, 32, iterator, &status) == 11 && std::u16string(buffer) == u"-12,345.678", "a known format with fields");
    const std::int32_t expected[][3] = {{UNUM_SIGN_FIELD, 0, 1}, {UNUM_GROUPING_SEPARATOR_FIELD, 3, 4}, {UNUM_INTEGER_FIELD, 1, 7}, {UNUM_DECIMAL_SEPARATOR_FIELD, 7, 8}, {UNUM_FRACTION_FIELD, 8, 11}};
    std::vector<std::array<std::int32_t, 3>> seen;
    std::int32_t begin = -5;
    std::int32_t end = -5;
    for (std::int32_t kind = ufieldpositer_next_nid_postfix(iterator, &begin, &end); kind >= 0; kind = ufieldpositer_next_nid_postfix(iterator, &begin, &end)) seen.push_back({kind, begin, end});
    Require(seen.size() == 5, "five fields");
    for (const auto& field : expected) {
        bool found = false;
        for (const auto& item : seen) found = found || (item[0] == field[0] && item[1] == field[1] && item[2] == field[2]);
        Require(found, "a known field position");
    }
    unum_formatDoubleForFields_nid_postfix(format, 7, buffer, 32, iterator, &status);
    Require(ufieldpositer_next_nid_postfix(iterator, nullptr, &end) == UNUM_INTEGER_FIELD && end == 1, "a reused iterator holds the new fields and accepts a null begin");
    Require(ufieldpositer_next_nid_postfix(iterator, &begin, nullptr) == -1, "the iterator ends");
    ufieldpositer_close_nid_postfix(iterator);
    unum_close_nid_postfix(format);
}

void CheckNumberFormatErrors() {
    SetDefaults(nullptr);
    std::int32_t status = Ok;
    Require(!unum_open_nid_postfix(99, nullptr, 0, nullptr, nullptr, &status) && status == Unsupported, "an unknown style is unsupported before the default locale");
    status = IllegalArgument;
    Require(!unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, nullptr, nullptr, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::invalid_argument>([] { unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, "en", nullptr, nullptr); }), "a null status throws in unum_open");
    for (std::int32_t style : Styles) {
        Require(Throws<std::runtime_error>([style] { std::int32_t ok = Ok; unum_open_nid_postfix(style, PatternFor(style), -1, nullptr, nullptr, &ok); }), "a NULL locale without a guest default throws for every style");
        for (const char* locale : {"xx", "qq_Latn", "xx_YY"}) {
            Require(Throws<std::runtime_error>([style, locale] { std::int32_t ok = Ok; unum_open_nid_postfix(style, PatternFor(style), -1, locale, nullptr, &ok); }), std::string("a locale without data throws: ") + locale + " " + std::to_string(style));
        }
    }
    for (const char* locale : {"en@compat=host", "de_DE@compat=host;numbers=latn", "@compat=host"}) {
        for (std::int32_t style : {std::int32_t{UNUM_DECIMAL}, std::int32_t{UNUM_CURRENCY}, std::int32_t{UNUM_SPELLOUT}}) {
            Require(Throws<std::runtime_error>([style, locale] { std::int32_t ok = Ok; unum_open_nid_postfix(style, nullptr, 0, locale, nullptr, &ok); }), std::string("a request for the host's own formats throws: ") + locale);
        }
    }
    for (const char* locale : {"en@compat=hostx", "en@compat=other"}) {
        std::int32_t ok = Ok;
        void* format = unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, locale, nullptr, &ok);
        Require(format && ok <= Ok, std::string("another compat value is accepted: ") + locale);
        unum_close_nid_postfix(format);
    }
    for (std::int32_t style : {UNUM_SPELLOUT, UNUM_ORDINAL, UNUM_DURATION, UNUM_NUMBERING_SYSTEM}) {
        Require(Throws<std::runtime_error>([style] { std::int32_t ok = Ok; unum_open_nid_postfix(style, nullptr, 0, "bn", nullptr, &ok); }), "a locale without rule-based data throws for the rule-based styles");
    }
    for (std::int32_t style : {UNUM_DECIMAL, UNUM_PATTERN_RULEBASED, UNUM_CURRENCY_PLURAL, UNUM_DECIMAL_COMPACT_LONG}) {
        std::int32_t ok = Ok;
        UErrorCode hostOk = U_ZERO_ERROR;
        void* format = unum_open_nid_postfix(style, PatternFor(style), -1, "bn", nullptr, &ok);
        UNumberFormat* hostFormat = unum_open(static_cast<UNumberFormatStyle>(style), PatternFor(style), -1, "bn", nullptr, &hostOk);
        Require(format && ok <= Ok && ok == hostOk, "bn has data for the other styles " + std::to_string(style) + " " + std::to_string(ok));
        CompareFormats(format, hostFormat, "bn without a default");
        unum_close_nid_postfix(format);
        unum_close(hostFormat);
    }
    UParseError guestError{};
    UParseError hostError{};
    UErrorCode hostStatus = U_ZERO_ERROR;
    status = Ok;
    for (const char16_t* pattern : {u"#,##0.0#0", u"0.0.0", u"''#'", u"#;#;#", u"*"}) {
        guestError = UParseError{};
        hostError = UParseError{};
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        void* guest = unum_open_nid_postfix(UNUM_PATTERN_DECIMAL, pattern, -1, "en", &guestError, &status);
        UNumberFormat* host = unum_open(UNUM_PATTERN_DECIMAL, pattern, -1, "en", &hostError, &hostStatus);
        Require(status == hostStatus && !guest == !host && std::memcmp(&guestError, &hostError, sizeof(UParseError)) == 0, "a decimal pattern error matches ICU");
        unum_close_nid_postfix(guest);
        unum_close(host);
    }
    for (const char16_t* rules : {u"%main: 0: zero; 1: ;", u"0: x >>> y;", u"%a: 0: =%b=;", u""}) {
        guestError = UParseError{};
        hostError = UParseError{};
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        void* guest = unum_open_nid_postfix(UNUM_PATTERN_RULEBASED, rules, -1, "en", &guestError, &status);
        UNumberFormat* host = unum_open(UNUM_PATTERN_RULEBASED, rules, -1, "en", &hostError, &hostStatus);
        Require(status == hostStatus && !guest == !host && std::memcmp(&guestError, &hostError, sizeof(UParseError)) == 0, "a rule error matches ICU");
        unum_close_nid_postfix(guest);
        unum_close(host);
    }
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    void* format = unum_open_nid_postfix(UNUM_PATTERN_DECIMAL, DecimalPattern, 7, "de", nullptr, &status);
    UNumberFormat* host = unum_open(UNUM_PATTERN_DECIMAL, DecimalPattern, 7, "de", nullptr, &hostStatus);
    Require(format && GuestFormat(format, -12345.6789, UNUM_INTEGER_FIELD) == HostFormat(host, -12345.6789, UNUM_INTEGER_FIELD), "a pattern length is honoured");
    char16_t buffer[64] = {};
    status = Ok;
    Require(unum_formatDouble_nid_postfix(format, 1234.5, nullptr, 0, nullptr, &status) == 7 && status == BufferOverflow, "formatDouble preflights");
    status = Ok;
    Require(unum_formatDouble_nid_postfix(format, 1234.5, buffer, 7, nullptr, &status) == 7 && status == NotTerminated && std::u16string(buffer, 7) == u"1.234,5", "formatDouble fills an exact buffer");
    status = Ok;
    Require(unum_formatDouble_nid_postfix(format, 1234.5, buffer, 3, nullptr, &status) == 7 && status == BufferOverflow, "formatDouble reports an overflow");
    status = Ok;
    Require(unum_formatDouble_nid_postfix(format, 1234.5, buffer, -1, nullptr, &status) == unum_formatDouble(host, 1234.5, buffer, -1, nullptr, &hostStatus) && status == hostStatus, "a negative capacity matches ICU");
    status = IllegalArgument;
    Require(unum_formatDouble_nid_postfix(nullptr, 1, buffer, 64, nullptr, &status) == -1 && status == IllegalArgument, "a failed status returns -1 before the format");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unum_formatDouble_nid_postfix(nullptr, 1, buffer, 64, nullptr, &ok); }), "a null format throws in formatDouble");
    Require(Throws<std::invalid_argument>([&] { unum_formatDouble_nid_postfix(format, 1, buffer, 64, nullptr, nullptr); }), "a null status throws in formatDouble");
    status = Ok;
    Require(unum_formatDoubleForFields_nid_postfix(nullptr, 1, nullptr, 5, nullptr, &status) == -1 && status == IllegalArgument, "formatDoubleForFields checks the buffer before the format");
    status = Ok;
    Require(unum_formatDoubleForFields_nid_postfix(format, 1, buffer, -1, nullptr, &status) == -1 && status == IllegalArgument, "formatDoubleForFields rejects a negative capacity");
    status = Ok;
    Require(unum_formatDoubleForFields_nid_postfix(format, 1234.5, buffer, 64, nullptr, &status) == 7 && status == Ok && std::u16string(buffer) == u"1.234,5", "formatDoubleForFields takes a null iterator");
    status = Ok;
    Require(unum_formatDoubleForFields_nid_postfix(format, 1234.5, nullptr, 0, nullptr, &status) == 7 && status == BufferOverflow, "formatDoubleForFields preflights");
    status = IllegalArgument;
    Require(unum_formatDoubleForFields_nid_postfix(nullptr, 1, buffer, 64, nullptr, &status) == -1 && status == IllegalArgument, "formatDoubleForFields returns on a failed status");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unum_formatDoubleForFields_nid_postfix(nullptr, 1, buffer, 64, nullptr, &ok); }), "a null format throws in formatDoubleForFields");
    Require(Throws<std::invalid_argument>([&] { unum_formatDoubleForFields_nid_postfix(format, 1, buffer, 64, nullptr, nullptr); }), "a null status throws in formatDoubleForFields");
    status = Ok;
    Require(unum_formatDecimal_nid_postfix(nullptr, nullptr, -1, nullptr, 5, nullptr, &status) == -1 && status == IllegalArgument, "formatDecimal checks the buffer first");
    status = Ok;
    Require(unum_formatDecimal_nid_postfix(format, "1", 1, buffer, -1, nullptr, &status) == -1 && status == IllegalArgument, "formatDecimal rejects a negative capacity");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(GuestDecimal(format, nullptr, 0, &status) == HostDecimal(host, nullptr, 0, &hostStatus) && status == hostStatus, "a NULL number of length zero matches ICU");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unum_formatDecimal_nid_postfix(format, nullptr, -1, buffer, 64, nullptr, &ok); }), "a NULL number ICU would measure throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unum_formatDecimal_nid_postfix(format, nullptr, 3, buffer, 64, nullptr, &ok); }), "a NULL number ICU would read throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unum_formatDecimal_nid_postfix(nullptr, "1", 1, buffer, 64, nullptr, &ok); }), "a null format throws in formatDecimal");
    Require(Throws<std::invalid_argument>([&] { unum_formatDecimal_nid_postfix(format, "1", 1, buffer, 64, nullptr, nullptr); }), "a null status throws in formatDecimal");
    status = IllegalArgument;
    Require(unum_formatDecimal_nid_postfix(nullptr, nullptr, -1, buffer, 64, nullptr, &status) == -1, "formatDecimal returns on a failed status");
    Require(Throws<std::invalid_argument>([&] { unum_clone_nid_postfix(format, nullptr); }), "a null status throws in unum_clone");
    status = IllegalArgument;
    Require(!unum_clone_nid_postfix(nullptr, &status) && status == IllegalArgument, "a failed status returns NULL in unum_clone");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; unum_clone_nid_postfix(nullptr, &ok); }), "a null format throws in unum_clone");
    Require(Throws<std::invalid_argument>([] { unum_setAttribute_nid_postfix(nullptr, UNUM_MAX_FRACTION_DIGITS, 1); }), "a null format throws in setAttribute");
    status = IllegalArgument;
    Require(unum_getTextAttribute_nid_postfix(nullptr, UNUM_POSITIVE_PREFIX, buffer, 64, &status) == -1, "getTextAttribute returns on a failed status");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unum_getTextAttribute_nid_postfix(nullptr, UNUM_POSITIVE_PREFIX, buffer, 64, &ok); }), "a null format throws in getTextAttribute");
    Require(Throws<std::invalid_argument>([&] { unum_getTextAttribute_nid_postfix(format, UNUM_POSITIVE_PREFIX, buffer, 64, nullptr); }), "a null status throws in getTextAttribute");
    status = IllegalArgument;
    unum_setTextAttribute_nid_postfix(nullptr, UNUM_POSITIVE_PREFIX, u"x", 1, &status);
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; unum_setTextAttribute_nid_postfix(nullptr, UNUM_POSITIVE_PREFIX, u"x", 1, &ok); }), "a null format throws in setTextAttribute");
    Require(Throws<std::invalid_argument>([&] { unum_setTextAttribute_nid_postfix(format, UNUM_POSITIVE_PREFIX, u"x", 1, nullptr); }), "a null status throws in setTextAttribute");
    Require(unum_getSymbol_nid_postfix(format, UNUM_DECIMAL_SEPARATOR_SYMBOL, buffer, 64, nullptr) == 0, "a null status returns 0 in getSymbol, as in ICU");
    status = Ok;
    Require(unum_getSymbol_nid_postfix(nullptr, UNUM_DECIMAL_SEPARATOR_SYMBOL, buffer, 64, &status) == 0 && status == IllegalArgument, "a null format is an illegal argument in getSymbol");
    status = Ok;
    Require(unum_getSymbol_nid_postfix(format, UNUM_DECIMAL_SEPARATOR_SYMBOL, nullptr, 0, &status) == 1 && status == BufferOverflow, "getSymbol preflights");

    unum_setAttribute_nid_postfix(format, UNUM_MIN_FRACTION_DIGITS, 3);
    unum_setAttribute(host, UNUM_MIN_FRACTION_DIGITS, 3);
    unum_setAttribute_nid_postfix(format, UNUM_GROUPING_USED, 0);
    unum_setAttribute(host, UNUM_GROUPING_USED, 0);
    unum_setAttribute_nid_postfix(format, UNUM_ROUNDING_MODE, UNUM_ROUND_CEILING);
    unum_setAttribute(host, UNUM_ROUNDING_MODE, UNUM_ROUND_CEILING);
    CompareFormats(format, host, "attributes");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    unum_setTextAttribute_nid_postfix(format, UNUM_POSITIVE_PREFIX, u"+x", -1, &status);
    unum_setTextAttribute(host, UNUM_POSITIVE_PREFIX, u"+x", -1, &hostStatus);
    unum_setTextAttribute_nid_postfix(format, UNUM_NEGATIVE_SUFFIX, u"-y", 1, &status);
    unum_setTextAttribute(host, UNUM_NEGATIVE_SUFFIX, u"-y", 1, &hostStatus);
    unum_setTextAttribute_nid_postfix(format, UNUM_DEFAULT_RULESET, u"z", 1, &status);
    unum_setTextAttribute(host, UNUM_DEFAULT_RULESET, u"z", 1, &hostStatus);
    Require(status == Unsupported && status == hostStatus, "an unsupported text attribute matches ICU");
    CompareFormats(format, host, "text attributes");
    unum_close_nid_postfix(format);
    unum_close(host);

    status = Ok;
    hostStatus = U_ZERO_ERROR;
    format = unum_open_nid_postfix(UNUM_CURRENCY, nullptr, 0, "en_US", nullptr, &status);
    host = unum_open(UNUM_CURRENCY, nullptr, 0, "en_US", nullptr, &hostStatus);
    unum_setTextAttribute_nid_postfix(format, UNUM_CURRENCY_CODE, u"EUR", 3, &status);
    unum_setTextAttribute(host, UNUM_CURRENCY_CODE, u"EUR", 3, &hostStatus);
    Require(GuestFormat(format, 1234.5, UNUM_CURRENCY_FIELD).text == u"€1,234.50", "a currency code changes the symbol");
    CompareFormats(format, host, "currency code");
    unum_close_nid_postfix(format);
    unum_close(host);

    status = Ok;
    hostStatus = U_ZERO_ERROR;
    format = unum_open_nid_postfix(UNUM_SPELLOUT, nullptr, 0, "en", nullptr, &status);
    host = unum_open(UNUM_SPELLOUT, nullptr, 0, "en", nullptr, &hostStatus);
    Require(GuestFormat(format, 42, UNUM_INTEGER_FIELD).text == u"forty-two", "a spellout format");
    unum_setTextAttribute_nid_postfix(format, UNUM_DEFAULT_RULESET, u"%spellout-ordinal", -1, &status);
    unum_setTextAttribute(host, UNUM_DEFAULT_RULESET, u"%spellout-ordinal", -1, &hostStatus);
    Require(status == hostStatus && GuestFormat(format, 42, UNUM_INTEGER_FIELD).text == u"forty-second", "a rule set change " + std::to_string(status));
    unum_setTextAttribute_nid_postfix(format, UNUM_POSITIVE_PREFIX, u"x", 1, &status);
    unum_setTextAttribute(host, UNUM_POSITIVE_PREFIX, u"x", 1, &hostStatus);
    Require(status == Unsupported && hostStatus == U_UNSUPPORTED_ERROR, "a decimal attribute is unsupported for rules");
    unum_close_nid_postfix(format);
    unum_close(host);
}

void CheckDefaults() {
    for (const char* fallback : {"de", "ar_EG", "fa"}) {
        SetDefaults(fallback);
        Sweep({"xx", "und", "xx_YY", "bn"}, fallback);
        for (std::int32_t style : {std::int32_t{UNUM_DECIMAL}, std::int32_t{UNUM_SPELLOUT}}) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            void* guest = unum_open_nid_postfix(style, nullptr, 0, nullptr, nullptr, &status);
            UNumberFormat* host = unum_open(static_cast<UNumberFormatStyle>(style), nullptr, 0, nullptr, nullptr, &hostStatus);
            Require(guest && status == hostStatus, "a NULL locale uses the guest default");
            CompareFormats(guest, host, std::string("default ") + fallback);
            unum_close_nid_postfix(guest);
            unum_close(host);
        }
    }
    SetDefaults("ar_EG");
    std::int32_t status = Ok;
    void* fallen = unum_open_nid_postfix(UNUM_DECIMAL_COMPACT_LONG, nullptr, 0, "xx", nullptr, &status);
    void* plain = unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, "en", nullptr, &status);
    void* clone = unum_clone_nid_postfix(fallen, &status);
    Require(fallen && plain && clone && status <= Ok, "formats open under a guest default");
    status = Ok;
    char16_t buffer[64] = {};
    Require(unum_formatDouble_nid_postfix(fallen, 1, buffer, 64, nullptr, &status) > 0 && buffer[0] == 0x0661, "xx falls back to the guest default ar_EG");
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; unum_formatDouble_nid_postfix(fallen, 1, buffer, 64, nullptr, &ok); }), "a format that fell back to a cleared default throws");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; unum_formatDoubleForFields_nid_postfix(clone, 1, buffer, 64, nullptr, &ok); }), "its clone throws too");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; unum_formatDecimal_nid_postfix(fallen, "1", 1, buffer, 64, nullptr, &ok); }), "formatDecimal throws too");
    Require(Throws<std::runtime_error>([&] { unum_setAttribute_nid_postfix(fallen, UNUM_MIN_FRACTION_DIGITS, 1); }), "setAttribute throws too");
    status = Ok;
    UPluralRules* rules = uplrules_openForType_nid_postfix("en", UPLURAL_TYPE_CARDINAL, &status);
    char16_t keyword[16] = {};
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uplrules_selectWithFormat_nid_postfix(rules, 1, fallen, keyword, 16, &ok); }), "selecting through such a format throws");
    status = IllegalArgument;
    Require(uplrules_selectWithFormat_nid_postfix(rules, 1, fallen, keyword, 16, &status) == 0 && status == IllegalArgument, "a failed status returns before the format is used");
    uplrules_close_nid_postfix(rules);
    status = Ok;
    Require(unum_formatDouble_nid_postfix(plain, 1, buffer, 64, nullptr, &status) == 1 && buffer[0] == u'1', "a format with its own data still works");
    status = Ok;
    uloc_setDefault_nid_postfix("ar_EG", &status);
    Require(unum_formatDouble_nid_postfix(fallen, 1, buffer, 64, nullptr, &status) > 0, "a guest default again allows it");
    unum_close_nid_postfix(fallen);
    unum_close_nid_postfix(clone);
    unum_close_nid_postfix(plain);
    SetDefaults(nullptr);
}

void CheckPluralRules() {
    SetDefaults(nullptr);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    void* integer = unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, "en", nullptr, &status);
    void* fixed = unum_open_nid_postfix(UNUM_PATTERN_DECIMAL, u"0.00", -1, "en", nullptr, &status);
    UNumberFormat* hostInteger = unum_open(UNUM_DECIMAL, nullptr, 0, "en", nullptr, &hostStatus);
    UNumberFormat* hostFixed = unum_open(UNUM_PATTERN_DECIMAL, u"0.00", -1, "en", nullptr, &hostStatus);
    void* spellout = unum_open_nid_postfix(UNUM_SPELLOUT, nullptr, 0, "en", nullptr, &status);
    UNumberFormat* hostSpellout = unum_open(UNUM_SPELLOUT, nullptr, 0, "en", nullptr, &hostStatus);
    Require(status <= Ok && U_SUCCESS(hostStatus), "the formats open");
    status = Ok;
    UPluralRules* english = uplrules_openForType_nid_postfix("en", UPLURAL_TYPE_CARDINAL, &status);
    char16_t keyword[16] = {};
    Require(uplrules_selectWithFormat_nid_postfix(english, 1, integer, keyword, 16, &status) == 3 && std::u16string(keyword) == u"one", "1 is one");
    Require(uplrules_selectWithFormat_nid_postfix(english, 1, fixed, keyword, 16, &status) == 5 && std::u16string(keyword) == u"other", "1.00 is other");
    uplrules_close_nid_postfix(english);
    UPluralRules* ordinal = uplrules_openForType_nid_postfix("en", UPLURAL_TYPE_ORDINAL, &status);
    Require(uplrules_selectWithFormat_nid_postfix(ordinal, 22, integer, keyword, 16, &status) == 3 && std::u16string(keyword) == u"two", "22nd is two");
    uplrules_close_nid_postfix(ordinal);
    for (const char* locale : {"en", "fr", "ru", "ar", "ja", "pl", "cy", "lv", "", "root", "xx", "und", "br", "sl"}) {
        for (std::int32_t type : {std::int32_t{UPLURAL_TYPE_CARDINAL}, std::int32_t{UPLURAL_TYPE_ORDINAL}, std::int32_t{UPLURAL_TYPE_COUNT}, std::int32_t{-1}}) {
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            UPluralRules* guest = uplrules_openForType_nid_postfix(locale, type, &status);
            UPluralRules* host = uplrules_openForType(locale, static_cast<UPluralType>(type), &hostStatus);
            const std::string what = std::string("plural rules ") + locale + " " + std::to_string(type);
            Require(status == hostStatus && !guest == !host, what + " open like ICU");
            if (!guest) continue;
            for (double value : {0.0, 1.0, 2.0, 3.0, 5.0, 11.0, 21.0, 22.0, 101.0, 1.5, 0.1, 1e6, -1.0}) {
                for (const auto& [guestFormat, hostFormat] : {std::pair{integer, hostInteger}, std::pair{fixed, hostFixed}, std::pair{spellout, hostSpellout}}) {
                    char16_t buffer[16] = {};
                    char16_t hostBuffer[16] = {};
                    std::int32_t selectStatus = Ok;
                    UErrorCode hostSelectStatus = U_ZERO_ERROR;
                    const std::int32_t length = uplrules_selectWithFormat_nid_postfix(guest, value, guestFormat, buffer, 16, &selectStatus);
                    const std::int32_t hostLength = uplrules_selectWithFormat(host, value, hostFormat, hostBuffer, 16, &hostSelectStatus);
                    Require(length == hostLength && selectStatus == hostSelectStatus && Text(buffer, length, 16) == Text(hostBuffer, hostLength, 16), what + " selects like ICU");
                }
            }
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            UEnumeration* keywords = uplrules_getKeywords_nid_postfix(guest, &status);
            UEnumeration* hostKeywords = uplrules_getKeywords(host, &hostStatus);
            Require(keywords && GuestEnumerationLayout(keywords) && status == hostStatus, what + " keywords open");
            std::int32_t countStatus = Ok;
            Require(GuestEnumerationCount(keywords, &countStatus) == uenum_count(hostKeywords, &hostStatus), what + " keyword count matches ICU");
            for (;;) {
                std::int32_t length = 0;
                std::int32_t hostLength = 0;
                const char* name = GuestEnumerationNext(keywords, &length, &countStatus);
                const char* hostName = uenum_next(hostKeywords, &hostLength, &hostStatus);
                Require(!name == !hostName && (!name || (length == hostLength && std::strcmp(name, hostName) == 0)), what + " keyword matches ICU");
                if (!name) break;
            }
            GuestEnumerationClose(keywords);
            uenum_close(hostKeywords);
            uplrules_close_nid_postfix(guest);
            uplrules_close(host);
        }
    }
    status = Ok;
    UPluralRules* rules = uplrules_openForType_nid_postfix("en", UPLURAL_TYPE_CARDINAL, &status);
    status = Ok;
    Require(uplrules_selectWithFormat_nid_postfix(rules, 1, integer, nullptr, 0, &status) == 3 && status == BufferOverflow, "selectWithFormat preflights");
    status = Ok;
    Require(uplrules_selectWithFormat_nid_postfix(rules, 1, integer, nullptr, 4, &status) == 0 && status == IllegalArgument, "a NULL keyword with a capacity is an illegal argument");
    status = Ok;
    Require(uplrules_selectWithFormat_nid_postfix(rules, 1, integer, keyword, -1, &status) == 0 && status == IllegalArgument, "a negative capacity is an illegal argument");
    status = Ok;
    Require(uplrules_selectWithFormat_nid_postfix(nullptr, 1, integer, keyword, 16, &status) == 0 && status == IllegalArgument, "a NULL rule set is an illegal argument");
    status = Ok;
    Require(uplrules_selectWithFormat_nid_postfix(rules, 1, nullptr, keyword, 16, &status) == 0 && status == IllegalArgument, "a NULL format is an illegal argument");
    status = IllegalArgument;
    Require(uplrules_selectWithFormat_nid_postfix(rules, 1, integer, keyword, 16, &status) == 0 && status == IllegalArgument, "a failed status returns 0");
    Require(Throws<std::invalid_argument>([&] { uplrules_selectWithFormat_nid_postfix(rules, 1, integer, keyword, 16, nullptr); }), "a null status throws in selectWithFormat");
    status = Ok;
    Require(!uplrules_getKeywords_nid_postfix(nullptr, &status) && status == IllegalArgument, "a NULL rule set is an illegal argument in getKeywords");
    status = IllegalArgument;
    Require(!uplrules_getKeywords_nid_postfix(rules, &status) && status == IllegalArgument, "a failed status returns NULL in getKeywords");
    Require(Throws<std::invalid_argument>([&] { uplrules_getKeywords_nid_postfix(rules, nullptr); }), "a null status throws in getKeywords");
    status = IllegalArgument;
    Require(!uplrules_openForType_nid_postfix(nullptr, UPLURAL_TYPE_CARDINAL, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::invalid_argument>([] { uplrules_openForType_nid_postfix("en", UPLURAL_TYPE_CARDINAL, nullptr); }), "a null status throws in openForType");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; uplrules_openForType_nid_postfix(nullptr, UPLURAL_TYPE_CARDINAL, &ok); }), "a NULL locale without a guest default throws");
    uplrules_close_nid_postfix(rules);
    uplrules_close_nid_postfix(nullptr);
    SetDefaults("ru");
    status = Ok;
    UPluralRules* russian = uplrules_openForType_nid_postfix(nullptr, UPLURAL_TYPE_CARDINAL, &status);
    Require(uplrules_selectWithFormat_nid_postfix(russian, 3, integer, keyword, 16, &status) == 3 && std::u16string(keyword) == u"few", "a NULL locale uses the guest default ru");
    uplrules_close_nid_postfix(russian);
    unum_close_nid_postfix(integer);
    unum_close_nid_postfix(fixed);
    unum_close_nid_postfix(spellout);
    unum_close(hostInteger);
    unum_close(hostFixed);
    unum_close(hostSpellout);
    SetDefaults(nullptr);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckNumberingSystems();
    CheckFieldIterator();
    CheckNumberFormatErrors();
    SetDefaults("en_US");
    Sweep({"en", "de", "fr_CH", "ar_EG", "hi_IN", "ja", "zh_Hant", "th_TH@numbers=thai", "", "root", "sr_Latn", "es_419", "bn", "en@numbers=arab", "ru"}, "sweep");
    CheckDefaults();
    CheckPluralRules();
    std::puts("guest_icu_number_format passed");
    return 0;
}

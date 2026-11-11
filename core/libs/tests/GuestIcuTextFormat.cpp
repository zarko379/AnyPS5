#include "prx/libSceIcu/TextFormat.hpp"
#include "prx/libSceIcu/NumberFormat.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/formattedvalue.h>
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/udisplaycontext.h>
#include <unicode/uldnames.h>
#include <unicode/ulistformatter.h>
#include <unicode/uloc.h>
#include <unicode/unum.h>
#include <unicode/ureldatefmt.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
        std::fprintf(stderr, "ICU TEXT FORMAT: %s\n", message.c_str());
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

std::u16string Text(const char16_t* buffer, std::int32_t length, std::int32_t capacity) {
    return length >= 0 ? std::u16string(buffer, static_cast<std::size_t>(length < capacity ? length : capacity)) : std::u16string();
}

struct Result {
    std::u16string text;
    std::int32_t length = 0;
    std::int32_t status = 0;
    bool operator==(const Result&) const = default;
};

const char16_t* const Items[] = {u"apples", u"Bananen", u"אב", u"", u"oranges and lemons", u"一二"};
const std::int32_t ItemLengths[] = {6, 3, -1, 0, 7, 1};

void CheckLists(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        for (std::int32_t type = -1; type <= ULISTFMT_TYPE_UNITS + 1; ++type) {
            for (std::int32_t width = -1; width <= ULISTFMT_WIDTH_NARROW + 1; ++width) {
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                UListFormatter* guest = ulistfmt_openForType_nid_postfix(locale, type, width, &status);
                UListFormatter* host = ulistfmt_openForType(locale, static_cast<UListFormatterType>(type), static_cast<UListFormatterWidth>(width), &hostStatus);
                const std::string what = label + " list " + (locale ? locale : "NULL") + " " + std::to_string(type) + " " + std::to_string(width);
                Require(status == hostStatus && !guest == !host, what + " opens like ICU");
                if (!guest) continue;
                for (std::int32_t count = 0; count <= 6; ++count) {
                    for (const std::int32_t* lengths : {static_cast<const std::int32_t*>(nullptr), ItemLengths}) {
                        char16_t buffer[128] = {};
                        char16_t hostBuffer[128] = {};
                        std::int32_t formatStatus = Ok;
                        UErrorCode hostFormatStatus = U_ZERO_ERROR;
                        Result mine{{}, ulistfmt_format_nid_postfix(guest, Items, lengths, count, buffer, 128, &formatStatus), 0};
                        mine.status = formatStatus;
                        mine.text = Text(buffer, mine.length, 128);
                        Result theirs{{}, ulistfmt_format(host, Items, lengths, count, hostBuffer, 128, &hostFormatStatus), hostFormatStatus};
                        theirs.text = Text(hostBuffer, theirs.length, 128);
                        Require(mine == theirs, what + " formats " + std::to_string(count) + " items like ICU");
                        formatStatus = Ok;
                        hostFormatStatus = U_ZERO_ERROR;
                        UFormattedList* result = ulistfmt_openResult_nid_postfix(&formatStatus);
                        UFormattedList* hostResult = ulistfmt_openResult(&hostFormatStatus);
                        ulistfmt_formatStringsToResult_nid_postfix(guest, Items, lengths, count, result, &formatStatus);
                        ulistfmt_formatStringsToResult(host, Items, lengths, count, hostResult, &hostFormatStatus);
                        const UFormattedValue* value = ulistfmt_resultAsValue_nid_postfix(result, &formatStatus);
                        const UFormattedValue* hostValue = ulistfmt_resultAsValue(hostResult, &hostFormatStatus);
                        std::int32_t length = -5;
                        std::int32_t hostLength = -5;
                        const char16_t* string = ufmtval_getString_nid_postfix(value, &length, &formatStatus);
                        const char16_t* hostString = ufmtval_getString(hostValue, &hostLength, &hostFormatStatus);
                        Require(formatStatus == hostFormatStatus && length == hostLength && !string == !hostString && (!string || std::u16string(string, length) == std::u16string(hostString, hostLength)), what + " formats to a value like ICU");
                        Require(!string || std::u16string(string, length) == mine.text, what + " value matches the string");
                        ulistfmt_closeResult_nid_postfix(result);
                        ulistfmt_closeResult(hostResult);
                    }
                }
                ulistfmt_close_nid_postfix(guest);
                ulistfmt_close(host);
            }
        }
    }
}

void CheckListErrors() {
    SetDefaults(nullptr);
    std::int32_t status = Ok;
    Require(!ulistfmt_openForType_nid_postfix(nullptr, 7, ULISTFMT_WIDTH_WIDE, &status) && status == IllegalArgument, "an unknown type is illegal before the default locale");
    status = Ok;
    Require(!ulistfmt_openForType_nid_postfix("xx", ULISTFMT_TYPE_AND, 9, &status) && status == IllegalArgument, "an unknown width is illegal before the locale data");
    status = IllegalArgument;
    Require(!ulistfmt_openForType_nid_postfix(nullptr, ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::invalid_argument>([] { ulistfmt_openForType_nid_postfix("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, nullptr); }), "a null status throws in openForType");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ulistfmt_openForType_nid_postfix(nullptr, ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, &ok); }), "a NULL locale without a guest default throws");
    for (const char* locale : {"xx", "qq_Latn", "xx_YY"}) {
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; ulistfmt_openForType_nid_postfix(locale, ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_SHORT, &ok); }), std::string("a list locale without data throws: ") + locale);
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; ulistfmt_openForType_nid_postfix(locale, ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_SHORT, &ok); }), std::string("a second list open still throws: ") + locale);
    }
    status = Ok;
    UListFormatter* list = ulistfmt_openForType_nid_postfix("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, &status);
    char16_t buffer[64] = {};
    Require(ulistfmt_format_nid_postfix(list, Items, nullptr, 3, buffer, 64, &status) == 23 && std::u16string(buffer) == u"apples, Bananen, and אב", "a known list");
    status = Ok;
    Require(ulistfmt_format_nid_postfix(list, Items, nullptr, 2, nullptr, 0, &status) == 18 && status == BufferOverflow, "format preflights");
    status = Ok;
    Require(ulistfmt_format_nid_postfix(nullptr, Items, nullptr, 2, nullptr, 3, &status) == -1 && status == IllegalArgument, "format checks the buffer before the formatter");
    status = Ok;
    Require(ulistfmt_format_nid_postfix(nullptr, Items, nullptr, -1, buffer, 64, &status) == -1 && status == IllegalArgument, "format checks the count before the formatter");
    status = Ok;
    Require(ulistfmt_format_nid_postfix(nullptr, nullptr, nullptr, 1, buffer, 64, &status) == -1 && status == IllegalArgument, "format checks the strings before the formatter");
    status = Ok;
    Require(ulistfmt_format_nid_postfix(list, nullptr, nullptr, 0, buffer, 64, &status) == 0 && status == Ok, "an empty list without strings");
    status = IllegalArgument;
    Require(ulistfmt_format_nid_postfix(nullptr, nullptr, nullptr, 1, buffer, 64, &status) == -1, "format returns on a failed status");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ulistfmt_format_nid_postfix(nullptr, Items, nullptr, 2, buffer, 64, &ok); }), "a null formatter throws in format");
    Require(Throws<std::invalid_argument>([&] { ulistfmt_format_nid_postfix(list, Items, nullptr, 2, buffer, 64, nullptr); }), "a null status throws in format");
    status = Ok;
    UFormattedList* result = ulistfmt_openResult_nid_postfix(&status);
    alignas(16) unsigned char zeros[256] = {};
    status = Ok;
    ulistfmt_formatStringsToResult_nid_postfix(nullptr, Items, nullptr, 2, nullptr, &status);
    Require(status == IllegalArgument, "a NULL result is illegal before the formatter");
    status = Ok;
    ulistfmt_formatStringsToResult_nid_postfix(nullptr, Items, nullptr, 2, reinterpret_cast<UFormattedList*>(zeros), &status);
    Require(status == InvalidFormat, "a foreign result is an invalid format before the formatter");
    status = Ok;
    ulistfmt_formatStringsToResult_nid_postfix(nullptr, Items, nullptr, -2, result, &status);
    Require(status == IllegalArgument, "a negative count is illegal before the formatter");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ulistfmt_formatStringsToResult_nid_postfix(nullptr, Items, nullptr, 2, result, &ok); }), "a null formatter throws in formatStringsToResult");
    Require(Throws<std::invalid_argument>([&] { ulistfmt_formatStringsToResult_nid_postfix(list, Items, nullptr, 2, result, nullptr); }), "a null status throws in formatStringsToResult");
    status = IllegalArgument;
    ulistfmt_formatStringsToResult_nid_postfix(nullptr, Items, nullptr, 2, nullptr, &status);
    Require(status == IllegalArgument, "formatStringsToResult returns on a failed status");
    status = Ok;
    Require(!ulistfmt_resultAsValue_nid_postfix(nullptr, &status) && status == IllegalArgument, "resultAsValue rejects NULL");
    status = Ok;
    Require(!ulistfmt_resultAsValue_nid_postfix(reinterpret_cast<UFormattedList*>(zeros), &status) && status == InvalidFormat, "resultAsValue rejects a foreign object");
    Require(Throws<std::invalid_argument>([&] { ulistfmt_resultAsValue_nid_postfix(result, nullptr); }), "a null status throws in resultAsValue");
    status = Ok;
    const UFormattedValue* value = ulistfmt_resultAsValue_nid_postfix(result, &status);
    UErrorCode hostStatus = U_ZERO_ERROR;
    UFormattedList* hostResult = ulistfmt_openResult(&hostStatus);
    const UFormattedValue* hostValue = ulistfmt_resultAsValue(hostResult, &hostStatus);
    Require(value && !ufmtval_getString_nid_postfix(value, nullptr, &status) == !ufmtval_getString(hostValue, nullptr, &hostStatus) && status == hostStatus && status == U_INVALID_STATE_ERROR, "an unformatted result is in an invalid state, as in ICU");
    ulistfmt_closeResult(hostResult);
    status = Ok;
    ulistfmt_formatStringsToResult_nid_postfix(list, Items, nullptr, 1, result, &status);
    Require(ufmtval_getString_nid_postfix(value, nullptr, &status) && status == Ok, "a null length is allowed");
    status = Ok;
    Require(!ufmtval_getString_nid_postfix(nullptr, nullptr, &status) && status == IllegalArgument, "getString rejects NULL");
    status = Ok;
    Require(!ufmtval_getString_nid_postfix(reinterpret_cast<UFormattedValue*>(zeros), nullptr, &status) && status == InvalidFormat, "getString rejects a foreign object");
    status = IllegalArgument;
    Require(!ufmtval_getString_nid_postfix(value, nullptr, &status) && status == IllegalArgument, "getString returns on a failed status");
    Require(Throws<std::invalid_argument>([&] { ufmtval_getString_nid_postfix(value, nullptr, nullptr); }), "a null status throws in getString");
    status = IllegalArgument;
    Require(!ulistfmt_openResult_nid_postfix(&status), "openResult returns on a failed status");
    Require(Throws<std::invalid_argument>([] { ulistfmt_openResult_nid_postfix(nullptr); }), "a null status throws in openResult");
    ulistfmt_closeResult_nid_postfix(result);
    ulistfmt_closeResult_nid_postfix(nullptr);
    ulistfmt_close_nid_postfix(list);
    ulistfmt_close_nid_postfix(nullptr);
}

const char* const DisplayLocales[] = {"en_US", "zh_Hant_TW", "sr_Latn_RS", "de@collation=phonebook;calendar=buddhist", "xx_YY", "en_US_POSIX", "und", "es_419", "nl_BE", "en_GB", "pt_PT", "fr-CA", "ja_JP_TRADITIONAL", ""};
const char* const Scripts[] = {"Latn", "Cyrl", "Hant", "Qaaa", "xx", "", "Zzzz"};
const char* const Regions[] = {"US", "419", "DE", "ZZ", "xx", "", "HK", "MO"};
const char* const Keys[][2] = {{"calendar", "japanese"}, {"currency", "EUR"}, {"currency", "XYZ"}, {"collation", "phonebook"}, {"numbers", "arab"}, {"foo", "bar"}, {"", ""}, {"hc", "h12"}};

std::vector<std::vector<std::int32_t>> Contexts() {
    return {{}, {UDISPCTX_DIALECT_NAMES}, {UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE}, {UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU, UDISPCTX_LENGTH_SHORT}, {UDISPCTX_CAPITALIZATION_FOR_STANDALONE, UDISPCTX_NO_SUBSTITUTE}, {UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, 0x7777}};
}

template<typename TGuest, typename THost>
void CompareName(TGuest guest, THost host, const std::string& what) {
    char16_t buffer[128] = {};
    char16_t hostBuffer[128] = {};
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Result mine{{}, guest(buffer, 128, &status), 0};
    mine.status = status;
    mine.text = Text(buffer, mine.length, 128);
    Result theirs{{}, host(hostBuffer, 128, &hostStatus), hostStatus};
    theirs.text = Text(hostBuffer, theirs.length, 128);
    Require(mine == theirs, what + " matches ICU");
}

void CheckDisplayNames(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        for (auto contexts : Contexts()) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            ULocaleDisplayNames* guest = uldn_openForContext_nid_postfix(locale, contexts.empty() ? nullptr : contexts.data(), static_cast<std::int32_t>(contexts.size()), &status);
            ULocaleDisplayNames* host = uldn_openForContext(locale, contexts.empty() ? nullptr : reinterpret_cast<UDisplayContext*>(contexts.data()), static_cast<std::int32_t>(contexts.size()), &hostStatus);
            const std::string what = label + " names " + (locale ? locale : "NULL") + " " + std::to_string(contexts.size());
            Require(status == hostStatus && !guest == !host, what + " open like ICU");
            for (const char* id : DisplayLocales) {
                CompareName([&](char16_t* b, std::int32_t c, std::int32_t* s) { return uldn_localeDisplayName_nid_postfix(guest, id, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return uldn_localeDisplayName(host, id, b, c, s); }, what + " locale " + id);
            }
            for (const char* script : Scripts) {
                CompareName([&](char16_t* b, std::int32_t c, std::int32_t* s) { return uldn_scriptDisplayName_nid_postfix(guest, script, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return uldn_scriptDisplayName(host, script, b, c, s); }, what + " script " + script);
            }
            for (const char* region : Regions) {
                CompareName([&](char16_t* b, std::int32_t c, std::int32_t* s) { return uldn_regionDisplayName_nid_postfix(guest, region, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return uldn_regionDisplayName(host, region, b, c, s); }, what + " region " + region);
            }
            for (const auto& key : Keys) {
                CompareName([&](char16_t* b, std::int32_t c, std::int32_t* s) { return uldn_keyValueDisplayName_nid_postfix(guest, key[0], key[1], b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return uldn_keyValueDisplayName(host, key[0], key[1], b, c, s); }, what + " key " + key[0]);
            }
            uldn_close_nid_postfix(guest);
            uldn_close(host);
        }
    }
}

void CheckDisplayNameErrors() {
    SetDefaults(nullptr);
    std::int32_t status = IllegalArgument;
    Require(!uldn_openForContext_nid_postfix(nullptr, nullptr, 0, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::invalid_argument>([] { uldn_openForContext_nid_postfix("en", nullptr, 0, nullptr); }), "a null status throws in openForContext");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; uldn_openForContext_nid_postfix(nullptr, nullptr, 0, &ok); }), "a NULL locale without a guest default throws");
    for (const char* locale : {"xx", "qq_Latn", "xx_YY"}) {
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; uldn_openForContext_nid_postfix(locale, nullptr, 0, &ok); }), std::string("a display locale without data throws: ") + locale);
    }
    status = Ok;
    std::int32_t contexts[] = {UDISPCTX_DIALECT_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE};
    ULocaleDisplayNames* names = uldn_openForContext_nid_postfix("en", contexts, -3, &status);
    char16_t buffer[64] = {};
    Require(uldn_localeDisplayName_nid_postfix(names, "en_GB", buffer, 64, &status) == 24 && std::u16string(buffer) == u"English (United Kingdom)", "a negative length ignores the contexts");
    uldn_close_nid_postfix(names);
    names = uldn_openForContext_nid_postfix("en", contexts, 2, &status);
    Require(uldn_localeDisplayName_nid_postfix(names, "en_GB", buffer, 64, &status) == 15 && std::u16string(buffer) == u"British English", "dialect names");
    Require(uldn_scriptDisplayName_nid_postfix(names, "Cyrl", buffer, 64, &status) == 8 && std::u16string(buffer) == u"Cyrillic", "a script name");
    Require(uldn_regionDisplayName_nid_postfix(names, "419", buffer, 64, &status) == 13 && std::u16string(buffer) == u"Latin America", "a region name");
    Require(uldn_keyValueDisplayName_nid_postfix(names, "calendar", "japanese", buffer, 64, &status) == 17 && std::u16string(buffer) == u"Japanese Calendar", "a key value name");
    status = Ok;
    Require(uldn_regionDisplayName_nid_postfix(names, "419", nullptr, 0, &status) == 13 && status == BufferOverflow, "display names preflight");
    for (std::int32_t which = 0; which < 4; ++which) {
        auto call = [&](const ULocaleDisplayNames* ldn, const char* text, char16_t* out, std::int32_t size, std::int32_t* code) {
            switch (which) {
            case 0: return uldn_localeDisplayName_nid_postfix(ldn, text, out, size, code);
            case 1: return uldn_scriptDisplayName_nid_postfix(ldn, text, out, size, code);
            case 2: return uldn_regionDisplayName_nid_postfix(ldn, text, out, size, code);
            default: return uldn_keyValueDisplayName_nid_postfix(ldn, "calendar", text, out, size, code);
            }
        };
        status = Ok;
        Require(call(nullptr, "en", buffer, 64, &status) == 0 && status == IllegalArgument, "a NULL object is an illegal argument");
        status = Ok;
        Require(call(names, nullptr, buffer, 64, &status) == 0 && status == IllegalArgument, "a NULL name is an illegal argument");
        status = Ok;
        Require(call(names, "en", nullptr, 4, &status) == 0 && status == IllegalArgument, "a NULL buffer with a size is an illegal argument");
        status = Ok;
        Require(call(names, "en", buffer, -1, &status) == 0 && status == IllegalArgument, "a negative size is an illegal argument");
        status = IllegalArgument;
        Require(call(nullptr, nullptr, nullptr, -1, &status) == 0 && status == IllegalArgument, "a failed status returns 0");
        Require(Throws<std::invalid_argument>([&] { call(names, "en", buffer, 64, nullptr); }), "a null status throws");
    }
    status = Ok;
    Require(uldn_keyValueDisplayName_nid_postfix(names, nullptr, "x", buffer, 64, &status) == 0 && status == IllegalArgument, "a NULL key is an illegal argument");
    uldn_close_nid_postfix(names);
    uldn_close_nid_postfix(nullptr);
    SetDefaults("de");
    status = Ok;
    names = uldn_openForContext_nid_postfix("xx", nullptr, 0, &status);
    ULocaleDisplayNames* own = uldn_openForContext_nid_postfix("fr", nullptr, 0, &status);
    Require(names && own && uldn_regionDisplayName_nid_postfix(names, "DE", buffer, 64, &status) == 11 && std::u16string(buffer) == u"Deutschland", "xx falls back to the guest default de");
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uldn_regionDisplayName_nid_postfix(names, "DE", buffer, 64, &ok); }), "names that fell back to a cleared default throw");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uldn_localeDisplayName_nid_postfix(names, "de", buffer, 64, &ok); }), "every display call throws");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uldn_scriptDisplayName_nid_postfix(names, "Latn", buffer, 64, &ok); }), "script names throw");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uldn_keyValueDisplayName_nid_postfix(names, "calendar", "japanese", buffer, 64, &ok); }), "key value names throw");
    status = Ok;
    Require(uldn_regionDisplayName_nid_postfix(own, "DE", buffer, 64, &status) == 9 && std::u16string(buffer) == u"Allemagne", "names with their own data still work");
    uldn_close_nid_postfix(names);
    uldn_close_nid_postfix(own);
    SetDefaults(nullptr);
}

const double Offsets[] = {-2, -1, 0, 1, 2, 1.5, -0.5, 3600, -0.0};

void CompareRelative(URelativeDateTimeFormatter* guest, URelativeDateTimeFormatter* host, const std::string& what) {
    for (std::int32_t unit : {-1, std::int32_t{UDAT_REL_UNIT_COUNT}}) {
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; char16_t b[8]; ureldatefmt_format_nid_postfix(guest, 1, unit, b, 8, &ok); }), what + " an out-of-range unit ICU would index with throws in format");
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; char16_t b[8]; ureldatefmt_formatNumeric_nid_postfix(guest, 1, unit, b, 8, &ok); }), what + " an out-of-range unit throws in formatNumeric");
    }
    for (std::int32_t unit = 0; unit < UDAT_REL_UNIT_COUNT; ++unit) {
        for (double offset : Offsets) {
            CompareName([&](char16_t* b, std::int32_t c, std::int32_t* s) { return ureldatefmt_format_nid_postfix(guest, offset, unit, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return ureldatefmt_format(host, offset, static_cast<URelativeDateTimeUnit>(unit), b, c, s); }, what + " format " + std::to_string(unit));
            CompareName([&](char16_t* b, std::int32_t c, std::int32_t* s) { return ureldatefmt_formatNumeric_nid_postfix(guest, offset, unit, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return ureldatefmt_formatNumeric(host, offset, static_cast<URelativeDateTimeUnit>(unit), b, c, s); }, what + " formatNumeric " + std::to_string(unit));
        }
    }
}

void CheckRelative(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        for (std::int32_t width = UDAT_STYLE_LONG; width < UDAT_STYLE_COUNT; ++width) {
            for (std::int32_t context : {std::int32_t{UDISPCTX_CAPITALIZATION_NONE}, std::int32_t{UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE}, std::int32_t{UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU}, std::int32_t{UDISPCTX_STANDARD_NAMES}}) {
                for (bool adopt : {false, true}) {
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    void* number = adopt ? unum_open_nid_postfix(UNUM_PATTERN_DECIMAL, u"0.00", -1, "en", nullptr, &status) : nullptr;
                    UNumberFormat* hostNumber = adopt ? unum_open(UNUM_PATTERN_DECIMAL, u"0.00", -1, "en", nullptr, &hostStatus) : nullptr;
                    URelativeDateTimeFormatter* guest = ureldatefmt_open_nid_postfix(locale, number, width, context, &status);
                    URelativeDateTimeFormatter* host = ureldatefmt_open(locale, hostNumber, static_cast<UDateRelativeDateTimeFormatterStyle>(width), static_cast<UDisplayContext>(context), &hostStatus);
                    const std::string what = label + " relative " + (locale ? locale : "NULL") + " " + std::to_string(width) + " " + std::to_string(context) + (adopt ? " adopted" : "");
                    Require(status == hostStatus && !guest == !host, what + " opens like ICU");
                    if (!guest) {
                        if (context == UDISPCTX_STANDARD_NAMES) {
                            unum_close_nid_postfix(number);
                            unum_close(hostNumber);
                        }
                        continue;
                    }
                    CompareRelative(guest, host, what);
                    ureldatefmt_close_nid_postfix(guest);
                    ureldatefmt_close(host);
                }
            }
        }
    }
}

void CheckRelativeErrors() {
    SetDefaults(nullptr);
    std::int32_t status = IllegalArgument;
    Require(!ureldatefmt_open_nid_postfix(nullptr, nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::invalid_argument>([] { ureldatefmt_open_nid_postfix("en", nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, nullptr); }), "a null status throws in ureldatefmt_open");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ureldatefmt_open_nid_postfix(nullptr, nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &ok); }), "a NULL locale without a guest default throws");
    for (const char* locale : {"xx", "qq_Latn", "xx_YY"}) {
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; ureldatefmt_open_nid_postfix(locale, nullptr, UDAT_STYLE_SHORT, UDISPCTX_CAPITALIZATION_NONE, &ok); }), std::string("a relative locale without data throws: ") + locale);
    }
    for (std::int32_t width : {-1, std::int32_t{UDAT_STYLE_COUNT}}) {
        Require(Throws<std::invalid_argument>([width] { std::int32_t ok = Ok; ureldatefmt_open_nid_postfix("en", nullptr, width, UDISPCTX_CAPITALIZATION_NONE, &ok); }), "a width ICU would index with throws");
        status = Ok;
        Require(!ureldatefmt_open_nid_postfix("en", nullptr, width, UDISPCTX_STANDARD_NAMES, &status) && status == IllegalArgument, "a bad context fails before the width is used");
    }
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ureldatefmt_open_nid_postfix("en@compat=host", nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &ok); }), "a request for the host's own number format throws");
    status = Ok;
    URelativeDateTimeFormatter* formatter = ureldatefmt_open_nid_postfix("en", nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, &status);
    char16_t buffer[64] = {};
    Require(ureldatefmt_format_nid_postfix(formatter, -1, UDAT_REL_UNIT_DAY, buffer, 64, &status) == 9 && std::u16string(buffer) == u"Yesterday", "a known relative date");
    Require(ureldatefmt_formatNumeric_nid_postfix(formatter, 3, UDAT_REL_UNIT_WEEK, buffer, 64, &status) == 10 && std::u16string(buffer) == u"In 3 weeks", "a known numeric relative date");
    status = Ok;
    Require(ureldatefmt_format_nid_postfix(formatter, 1, UDAT_REL_UNIT_DAY, nullptr, 0, &status) == 8 && status == BufferOverflow, "format preflights");
    for (bool numeric : {false, true}) {
        auto call = [numeric](const URelativeDateTimeFormatter* f, char16_t* out, std::int32_t size, std::int32_t* code) {
            return numeric ? ureldatefmt_formatNumeric_nid_postfix(f, 1, UDAT_REL_UNIT_DAY, out, size, code) : ureldatefmt_format_nid_postfix(f, 1, UDAT_REL_UNIT_DAY, out, size, code);
        };
        status = Ok;
        Require(call(nullptr, nullptr, 4, &status) == 0 && status == IllegalArgument, "a NULL buffer with a size is illegal before the formatter");
        status = Ok;
        Require(call(nullptr, buffer, -1, &status) == 0 && status == IllegalArgument, "a negative size is illegal before the formatter");
        status = IllegalArgument;
        Require(call(nullptr, buffer, 64, &status) == 0 && status == IllegalArgument, "a failed status returns 0");
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; call(nullptr, buffer, 64, &ok); }), "a null formatter throws");
        Require(Throws<std::invalid_argument>([&] { call(formatter, buffer, 64, nullptr); }), "a null status throws");
    }
    ureldatefmt_close_nid_postfix(formatter);
    ureldatefmt_close_nid_postfix(nullptr);

    SetDefaults("ar_EG");
    status = Ok;
    formatter = ureldatefmt_open_nid_postfix("xx", nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &status);
    void* number = unum_open_nid_postfix(UNUM_DECIMAL, nullptr, 0, "xx", nullptr, &status);
    URelativeDateTimeFormatter* adopted = ureldatefmt_open_nid_postfix("en", number, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &status);
    URelativeDateTimeFormatter* own = ureldatefmt_open_nid_postfix("en", nullptr, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &status);
    Require(formatter && adopted && own && status <= Ok, "relative formatters open under a guest default");
    status = Ok;
    Require(ureldatefmt_formatNumeric_nid_postfix(adopted, 3, UDAT_REL_UNIT_DAY, buffer, 64, &status) > 0 && std::u16string(buffer).find(u'٣') != std::u16string::npos, "an adopted format that fell back uses the guest default digits");
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ureldatefmt_format_nid_postfix(formatter, 1, UDAT_REL_UNIT_DAY, buffer, 64, &ok); }), "a relative formatter that fell back to a cleared default throws");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ureldatefmt_formatNumeric_nid_postfix(adopted, 1, UDAT_REL_UNIT_DAY, buffer, 64, &ok); }), "a formatter whose adopted number format fell back throws");
    status = Ok;
    Require(ureldatefmt_formatNumeric_nid_postfix(own, 3, UDAT_REL_UNIT_DAY, buffer, 64, &status) == 9 && std::u16string(buffer) == u"in 3 days", "a formatter with its own data still works");
    ureldatefmt_close_nid_postfix(formatter);
    ureldatefmt_close_nid_postfix(adopted);
    ureldatefmt_close_nid_postfix(own);
    SetDefaults(nullptr);
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckListErrors();
    CheckDisplayNameErrors();
    CheckRelativeErrors();
    SetDefaults("en_US");
    CheckLists({"en", "de", "es", "he", "ja", "ar", "", "root", "it", "th", "zh_Hant", "ur_IN", "el", "en_IN"}, "sweep");
    CheckDisplayNames({"en", "de", "fr_CA", "ja", "zh_Hant", "", "ar", "sr_Latn", "ru"}, "sweep");
    CheckRelative({"en", "de", "ja", "ar", "", "ru", "es_MX"}, "sweep");
    for (const char* fallback : {"de", "ar_EG", "fa"}) {
        SetDefaults(fallback);
        CheckLists({"xx", "xx_YY", nullptr}, fallback);
        CheckDisplayNames({"xx", nullptr}, fallback);
        CheckRelative({"xx", nullptr}, fallback);
    }
    SetDefaults(nullptr);
    std::puts("guest_icu_text_format passed");
    return 0;
}

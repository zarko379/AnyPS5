#include "prx/libSceIcu/CaseMap.hpp"
#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/udata.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>
#include "ucasemap_imp.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU CASE MAP: %s\n", message);
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

enum class Mapping { Upper, Lower, Title, Fold };

std::int32_t Guest(Mapping mapping, char16_t* dest, std::int32_t capacity, const char16_t* src, std::int32_t length, const char* locale, std::uint32_t options, std::int32_t* status) {
    switch (mapping) {
    case Mapping::Upper: return u_strToUpper_nid_postfix(dest, capacity, src, length, locale, status);
    case Mapping::Lower: return u_strToLower_nid_postfix(dest, capacity, src, length, locale, status);
    case Mapping::Title: return u_strToTitle_nid_postfix(dest, capacity, src, length, nullptr, locale, status);
    case Mapping::Fold: return u_strFoldCase_nid_postfix(dest, capacity, src, length, options, status);
    }
    return 0;
}

std::int32_t Host(Mapping mapping, char16_t* dest, std::int32_t capacity, const char16_t* src, std::int32_t length, const char* locale, std::uint32_t options, UErrorCode* status) {
    switch (mapping) {
    case Mapping::Upper: return u_strToUpper(dest, capacity, src, length, locale, status);
    case Mapping::Lower: return u_strToLower(dest, capacity, src, length, locale, status);
    case Mapping::Title: return u_strToTitle(dest, capacity, src, length, nullptr, locale, status);
    case Mapping::Fold: return u_strFoldCase(dest, capacity, src, length, options, status);
    }
    return 0;
}

std::u16string Map(Mapping mapping, const std::u16string& text, const char* locale, std::uint32_t options = 0) {
    char16_t buffer[128]{};
    std::int32_t status = Ok;
    const std::int32_t length = Guest(mapping, buffer, 128, text.c_str(), -1, locale, options, &status);
    Require(status <= Ok && length >= 0 && length < 128, "a mapping succeeds, possibly with a root break iterator warning");
    return std::u16string(buffer, static_cast<std::size_t>(length));
}

void CompareMapping(Mapping mapping, const std::u16string& text, const char* locale, std::uint32_t options, std::int32_t capacity) {
    char16_t guest[96];
    char16_t host[96];
    std::fill(std::begin(guest), std::end(guest), 0x7777);
    std::fill(std::begin(host), std::end(host), 0x7777);
    std::int32_t guestStatus = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t guestLength = Guest(mapping, capacity ? guest : nullptr, capacity, text.data(), static_cast<std::int32_t>(text.size()), locale, options, &guestStatus);
    const std::int32_t hostLength = Host(mapping, capacity ? host : nullptr, capacity, text.data(), static_cast<std::int32_t>(text.size()), locale, options, &hostStatus);
    if (guestLength != hostLength || guestStatus != hostStatus || std::memcmp(guest, host, sizeof(guest)) != 0) {
        std::fprintf(stderr, "ICU CASE MAP: mapping %d in %s differs for text starting %X\n", static_cast<int>(mapping), locale ? locale : "(null)", static_cast<unsigned>(text.empty() ? 0 : text[0]));
        std::abort();
    }
}

void CheckKnownMappings() {
    Require(Map(Mapping::Upper, u"istanbul", "tr") == u"\u0130STANBUL" && Map(Mapping::Upper, u"istanbul", "en") == u"ISTANBUL", "Turkish dotted capital I");
    Require(Map(Mapping::Lower, u"\u0130", "tr") == u"i" && Map(Mapping::Lower, u"\u0130", "en") == u"i\u0307", "lowercase dotted capital I");
    Require(Map(Mapping::Lower, u"\u039F\u0394\u039F\u03A3", "el") == u"\u03BF\u03B4\u03BF\u03C2", "final sigma");
    Require(Map(Mapping::Upper, u"stra\u00DFe", "") == u"STRASSE" && Map(Mapping::Upper, u"\u03AC\u03BB\u03C6\u03B1", "el") == u"\u0391\u039B\u03A6\u0391", "full uppercase and Greek accent removal");
    Require(Map(Mapping::Title, u"hello wORLD", "en") == u"Hello World" && Map(Mapping::Title, u"ijssel", "nl") == u"IJssel", "title case with a word iterator");
    Require(Map(Mapping::Fold, u"Stra\u00DFe", nullptr) == u"strasse" && Map(Mapping::Fold, u"I", nullptr, U_FOLD_CASE_EXCLUDE_SPECIAL_I) == u"\u0131", "case folding");

    char16_t buffer[8];
    std::int32_t status = Ok;
    Require(u_strToUpper_nid_postfix(nullptr, 0, u"abc", -1, "", &status) == 3 && status == BufferOverflow, "preflight");
    status = Ok;
    std::fill(std::begin(buffer), std::end(buffer), 0x7777);
    Require(u_strToUpper_nid_postfix(buffer, 3, u"abc", -1, "", &status) == 3 && status == NotTerminated && buffer[3] == 0x7777, "exact fit is not terminated");
    status = Ok;
    Require(u_strToUpper_nid_postfix(buffer, 2, u"abc", -1, "", &status) == 3 && status == BufferOverflow, "overflow");
    char16_t inPlace[8] = u"abc";
    status = Ok;
    Require(u_strToUpper_nid_postfix(inPlace, 8, inPlace, -1, "", &status) == 3 && status == Ok && std::u16string(inPlace) == u"ABC", "overlapping source and destination");
}

void CheckMappingErrors() {
    char16_t buffer[8]{};
    const Mapping mappings[] = {Mapping::Upper, Mapping::Lower, Mapping::Title, Mapping::Fold};
    for (const Mapping mapping : mappings) {
        Require(Throws<std::invalid_argument>([&] { Guest(mapping, buffer, 8, u"a", -1, "", 0, nullptr); }), "a null status throws");
        std::int32_t status = BufferOverflow;
        Require(Guest(mapping, buffer, 8, u"a", -1, nullptr, 0, &status) == 0 && status == BufferOverflow, "a failed status returns before the locale");
        status = Ok;
        Require(Guest(mapping, buffer, -1, u"a", -1, nullptr, 0, &status) == 0 && status == IllegalArgument, "a negative capacity");
        status = Ok;
        Require(Guest(mapping, nullptr, 2, u"a", -1, nullptr, 0, &status) == 0 && status == IllegalArgument, "a null destination with capacity");
        status = Ok;
        Require(Guest(mapping, buffer, 8, nullptr, 1, nullptr, 0, &status) == 0 && status == IllegalArgument, "a null source");
        status = Ok;
        Require(Guest(mapping, buffer, 8, u"a", -2, nullptr, 0, &status) == 0 && status == IllegalArgument, "a length below -1");
    }
    for (const Mapping mapping : {Mapping::Upper, Mapping::Lower, Mapping::Title}) {
        Require(Throws<std::runtime_error>([&] { std::int32_t status = Ok; Guest(mapping, buffer, 8, u"a", -1, nullptr, 0, &status); }), "the console's default locale is unknown");
    }
    std::int32_t status = Ok;
    Require(u_strFoldCase_nid_postfix(buffer, 8, u"A", -1, 0, &status) == 1 && status == Ok && buffer[0] == u'a', "folding takes no locale");
    auto* iterator = reinterpret_cast<UBreakIterator*>(buffer);
    status = BufferOverflow;
    Require(u_strToTitle_nid_postfix(buffer, 8, u"a", -1, iterator, "", &status) == 0 && status == BufferOverflow, "a failed status returns before the break iterator");

    status = Ok;
    uloc_setDefault_nid_postfix("tr", &status);
    Require(status == Ok, "the guest sets a default locale");
    Require(Map(Mapping::Upper, u"istanbul", nullptr) == u"\u0130STANBUL" && Map(Mapping::Lower, u"I", nullptr) == u"\u0131" && Map(Mapping::Title, u"istanbul", nullptr) == u"\u0130stanbul", "a null locale uses the guest's default");
    status = Ok;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; u_strToLower_nid_postfix(buffer, 8, u"a", -1, nullptr, &ok); }), "the default is unknown again");
}

void CheckMappingSweep() {
    const char* locales[] = {"", "en", "tr", "az", "lt", "el", "nl", "und", "xx_YY"};
    for (std::int32_t start = 0; start <= 0x10FFFF; start += 0x89) {
        std::u16string text;
        for (std::int32_t c = start; c < start + 12 && c <= 0x10FFFF; ++c) {
            if (c >= 0xD800 && c <= 0xDFFF) continue;
            if (c > 0xFFFF) {
                text.push_back(static_cast<char16_t>(0xD7C0 + (c >> 10)));
                text.push_back(static_cast<char16_t>(0xDC00 | (c & 0x3FF)));
            } else {
                text.push_back(static_cast<char16_t>(c));
            }
            if (c % 5 == 0) text.push_back(u' ');
        }
        const bool common = start < 0x3000 || (start & 0xFFF) < 0x100;
        for (const char* locale : locales) {
            if (!common && locale[0] != 0) continue;
            CompareMapping(Mapping::Upper, text, locale, 0, 96);
            CompareMapping(Mapping::Lower, text, locale, 0, 96);
            CompareMapping(Mapping::Title, text, locale, 0, 96);
        }
        CompareMapping(Mapping::Fold, text, nullptr, U_FOLD_CASE_DEFAULT, 96);
        CompareMapping(Mapping::Fold, text, nullptr, U_FOLD_CASE_EXCLUDE_SPECIAL_I, 96);
        CompareMapping(Mapping::Upper, text, "tr", 0, 5);
        CompareMapping(Mapping::Fold, text, nullptr, 0, 0);
    }
}

void CheckComparisons() {
    const std::u16string words[] = {u"", u"a", u"A", u"abc", u"ABD", u"Stra\u00DFe", u"STRASSE", u"strasse", u"I", u"\u0131", u"i", u"\u0130", u"\U0001D400", u"\uFB03", u"ffi", u"aB", u"\uFF61", std::u16string{u'a', 0, u'b'}, std::u16string{u'A', 0, u'c'}};
    const std::uint32_t options[] = {0, U_FOLD_CASE_EXCLUDE_SPECIAL_I, U_COMPARE_CODE_POINT_ORDER, U_FOLD_CASE_EXCLUDE_SPECIAL_I | U_COMPARE_CODE_POINT_ORDER};
    for (const auto& first : words) {
        for (const auto& second : words) {
            for (const std::uint32_t option : options) {
                const auto* s1 = first.c_str();
                const auto* s2 = second.c_str();
                const auto l1 = static_cast<std::int32_t>(first.size());
                const auto l2 = static_cast<std::int32_t>(second.size());
                std::int32_t guestStatus = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                Require(u_strCaseCompare_nid_postfix(s1, l1, s2, l2, option, &guestStatus) == u_strCaseCompare(s1, l1, s2, l2, option, &hostStatus) && guestStatus == hostStatus, "u_strCaseCompare matches ICU");
                guestStatus = Ok;
                hostStatus = U_ZERO_ERROR;
                Require(u_strCaseCompare_nid_postfix(s1, -1, s2, -1, option, &guestStatus) == u_strCaseCompare(s1, -1, s2, -1, option, &hostStatus) && guestStatus == hostStatus, "terminated u_strCaseCompare matches ICU");
                Require(u_strcasecmp_nid_postfix(s1, s2, option) == u_strcasecmp(s1, s2, option), "u_strcasecmp matches ICU");
                const std::int32_t shorter = l1 < l2 ? l1 : l2;
                for (std::int32_t n = 0; n <= shorter; ++n) {
                    Require(u_strncasecmp_nid_postfix(s1, s2, n, option) == u_strncasecmp(s1, s2, n, option), "u_strncasecmp matches ICU");
                    Require(u_memcasecmp_nid_postfix(s1, s2, n, option) == u_memcasecmp(s1, s2, n, option), "u_memcasecmp matches ICU");
                }
                std::int32_t guestMatch1 = -9;
                std::int32_t guestMatch2 = -9;
                std::int32_t hostMatch1 = -9;
                std::int32_t hostMatch2 = -9;
                guestStatus = Ok;
                hostStatus = U_ZERO_ERROR;
                u_caseInsensitivePrefixMatch_nid_postfix(s1, l1, s2, l2, option, &guestMatch1, &guestMatch2, &guestStatus);
                u_caseInsensitivePrefixMatch(s1, l1, s2, l2, option, &hostMatch1, &hostMatch2, &hostStatus);
                Require(guestMatch1 == hostMatch1 && guestMatch2 == hostMatch2 && guestStatus == hostStatus, "prefix matches match ICU");
            }
        }
    }
    Require(u_strcasecmp_nid_postfix(u"ABC", u"abc", 0) == 0 && u_strcasecmp_nid_postfix(u"abc", u"abd", 0) < 0 && u_strcasecmp_nid_postfix(u"Stra\u00DFe", u"STRASSE", 0) == 0, "case-insensitive comparison");
    Require(u_strcasecmp_nid_postfix(u"I", u"\u0131", 0) != 0 && u_strcasecmp_nid_postfix(u"I", u"\u0131", U_FOLD_CASE_EXCLUDE_SPECIAL_I) == 0, "Turkic folding option");
    const char16_t nulFirst[] = {u'a', 0, u'b'};
    const char16_t nulSecond[] = {u'A', 0, u'c'};
    Require(u_strncasecmp_nid_postfix(nulFirst, nulSecond, 3, 0) == 0 && u_memcasecmp_nid_postfix(nulFirst, nulSecond, 3, 0) < 0, "u_strncasecmp stops at NUL and u_memcasecmp does not");
    std::int32_t orderStatus = Ok;
    Require(u_strCaseCompare_nid_postfix(u"\uFF61", -1, u"\U00010000", -1, 0, &orderStatus) > 0 && u_strCaseCompare_nid_postfix(u"\uFF61", -1, u"\U00010000", -1, U_COMPARE_CODE_POINT_ORDER, &orderStatus) < 0 && orderStatus == Ok, "code unit and code point order");
    Require(u_strncasecmp_nid_postfix(u"abcX", u"ABCy", 3, 0) == 0 && u_memcasecmp_nid_postfix(u"aXb", u"Ayb", 2, 0) != 0, "counted comparisons");
    std::int32_t match1 = 0;
    std::int32_t match2 = 0;
    std::int32_t status = Ok;
    u_caseInsensitivePrefixMatch_nid_postfix(u"Stra\u00DFenbahn", -1, u"STRASSE", -1, 0, &match1, &match2, &status);
    Require(status == Ok && match1 == 6 && match2 == 7, "a prefix match through a full folding");

    Require(u_strcasecmp_nid_postfix(nullptr, nullptr, 0) == u_strcasecmp(nullptr, nullptr, 0) && u_strcasecmp_nid_postfix(nullptr, u"a", 0) == u_strcasecmp(nullptr, u"a", 0), "null terminated strings compare as empty, as in ICU");
    Require(u_memcasecmp_nid_postfix(nullptr, nullptr, 0, 0) == 0 && u_strncasecmp_nid_postfix(nullptr, u"a", 0, 0) == 0 && u_memcasecmp_nid_postfix(nullptr, u"a", -1, 0) == u_memcasecmp(nullptr, u"a", -1, 0), "null strings with no length to read");
    Require(Throws<std::invalid_argument>([] { u_memcasecmp_nid_postfix(nullptr, u"abc", 3, 0); }), "a null first string with a length throws in u_memcasecmp");
    Require(Throws<std::invalid_argument>([] { u_memcasecmp_nid_postfix(u"abc", nullptr, 3, 0); }), "a null second string with a length throws in u_memcasecmp");
    Require(Throws<std::invalid_argument>([] { u_strncasecmp_nid_postfix(u"abc", nullptr, 2, 0); }), "a null string with a length throws in u_strncasecmp");
    status = Ok;
    Require(u_strCaseCompare_nid_postfix(nullptr, 0, u"a", 1, 0, &status) == 0 && status == IllegalArgument, "u_strCaseCompare rejects a null string");
    status = Ok;
    Require(u_strCaseCompare_nid_postfix(u"a", -2, u"a", 1, 0, &status) == 0 && status == IllegalArgument, "u_strCaseCompare rejects a length below -1");
    status = IllegalArgument;
    Require(u_strCaseCompare_nid_postfix(u"a", 1, u"b", 1, 0, &status) == 0 && status == IllegalArgument, "u_strCaseCompare keeps a failed status");
    Require(u_strCaseCompare_nid_postfix(u"a", 1, u"b", 1, 0, nullptr) == 0, "u_strCaseCompare returns 0 for a null status, as in ICU");

    match1 = -9;
    match2 = -9;
    status = BufferOverflow;
    u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, &match1, &match2, &status);
    Require(match1 == -9 && status == BufferOverflow, "a failed prefix status writes nothing");
    Require(Throws<std::invalid_argument>([] { std::int32_t match = 0; u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, &match, &match, nullptr); }), "a null prefix status throws");
    Require(Throws<std::invalid_argument>([] { std::int32_t match = 0; std::int32_t ok = Ok; u_caseInsensitivePrefixMatch_nid_postfix(u"a", 1, u"a", 1, 0, &match, nullptr, &ok); }), "a first match length without a second throws");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; u_caseInsensitivePrefixMatch_nid_postfix(nullptr, 2, u"a", 1, 0, nullptr, nullptr, &ok); }), "a null counted prefix string throws");
    status = Ok;
    match2 = -9;
    u_caseInsensitivePrefixMatch_nid_postfix(u"ab", 2, u"AB", 2, 0, nullptr, &match2, &status);
    Require(status == Ok && match2 == -9, "without a first match length nothing is written");
    status = Ok;
    u_caseInsensitivePrefixMatch_nid_postfix(nullptr, -1, u"a", -1, 0, &match1, &match2, &status);
    Require(status == Ok && match1 == 0 && match2 == 0, "a null terminated prefix string is empty, as in ICU");
}


std::u16string GuestTitle(const char16_t* source, UBreakIterator* iterator, const char* locale, std::int32_t& status) {
    char16_t buffer[128] = {};
    const std::int32_t length = u_strToTitle_nid_postfix(buffer, 128, source, -1, iterator, locale, &status);
    return std::u16string(buffer, static_cast<std::size_t>(length > 0 && length <= 128 ? length : 0));
}

std::u16string HostTitle(const char16_t* source, UBreakIterator* iterator, const char* locale, UErrorCode& status) {
    char16_t buffer[128] = {};
    const std::int32_t length = u_strToTitle(buffer, 128, source, -1, iterator, locale, &status);
    return std::u16string(buffer, static_cast<std::size_t>(length > 0 && length <= 128 ? length : 0));
}

void CheckTitleIterators() {
    const char16_t* sources[] = {u"hello wORLD. foo bar", u"ijssel ijs", u"ßtraße i̇stanbul", u"one-two three's", u""};
    for (const std::int32_t type : {UBRK_CHARACTER, UBRK_WORD, UBRK_SENTENCE, UBRK_LINE, UBRK_TITLE}) {
        for (const char* locale : {"en", "nl", "tr", "el"}) {
            for (const char16_t* source : sources) {
                std::int32_t status = Ok;
                UBreakIterator* guestIterator = ubrk_open_nid_postfix(type, "en", nullptr, 0, &status);
                UErrorCode hostStatus = U_ZERO_ERROR;
                UBreakIterator* hostIterator = ubrk_open(static_cast<UBreakIteratorType>(type), "en", nullptr, 0, &hostStatus);
                Require(guestIterator && hostIterator, "the title break iterators open");
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                Require(GuestTitle(source, guestIterator, locale, status) == HostTitle(source, hostIterator, locale, hostStatus) && status == hostStatus, "title case with a guest break iterator matches ICU");
                ubrk_close_nid_postfix(guestIterator);
                ubrk_close(hostIterator);
            }
        }
    }
    std::int32_t status = Ok;
    UBreakIterator* characters = ubrk_open_nid_postfix(UBRK_CHARACTER, "en", nullptr, 0, &status);
    status = Ok;
    Require(GuestTitle(u"hello world", characters, "en", status) == u"HELLO WORLD" && status == Ok, "a character iterator title-cases every letter");
    UBreakIterator* sentences = ubrk_open_nid_postfix(UBRK_SENTENCE, "en", nullptr, 0, &status);
    status = Ok;
    Require(GuestTitle(u"hello wORLD? foo bar", sentences, "en", status) == u"Hello world? Foo bar" && status == Ok, "a sentence iterator title-cases each sentence");
    status = Ok;
    Require(GuestTitle(u"ijssel", sentences, "nl", status) == u"IJssel" && status == Ok, "the locale still drives case mapping with a guest iterator");
    status = Ok;
    Require(Throws<std::runtime_error>([&] { GuestTitle(u"abc", sentences, nullptr, status); }), "a NULL locale throws with a guest iterator too");
    ubrk_close_nid_postfix(characters);
    ubrk_close_nid_postfix(sentences);
}

void CheckIDefault() {
    const char16_t* source = u"ijssel ijs. hello wORLD";
    for (const char* locale : {"i-default@x=y", "i-default_US@x=y", "i-default-x@a=b", "i-default.utf8"}) {
        std::int32_t status = Ok;
        Require(Throws<std::runtime_error>([&] { GuestTitle(source, nullptr, locale, status); }), "a title break iterator for an i-default locale that ICU canonicalizes to the default throws");
        for (const Mapping mapping : {Mapping::Upper, Mapping::Lower}) {
            char16_t guest[64]{};
            char16_t host[64]{};
            status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            const std::int32_t guestLength = Guest(mapping, guest, 64, source, -1, locale, 0, &status);
            const std::int32_t hostLength = mapping == Mapping::Upper ? u_strToUpper(host, 64, source, -1, locale, &hostStatus) : u_strToLower(host, 64, source, -1, locale, &hostStatus);
            Require(guestLength == hostLength && status == hostStatus && std::u16string(guest) == host, "upper and lower case never canonicalize the locale");
        }
        status = Ok;
        UBreakIterator* words = ubrk_open_nid_postfix(UBRK_WORD, "en", nullptr, 0, &status);
        UErrorCode hostStatus = U_ZERO_ERROR;
        UBreakIterator* hostWords = ubrk_open(UBRK_WORD, "en", nullptr, 0, &hostStatus);
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(GuestTitle(source, words, locale, status) == HostTitle(source, hostWords, locale, hostStatus) && status == hostStatus, "title case with a caller's iterator never canonicalizes the locale");
        ubrk_close_nid_postfix(words);
        ubrk_close(hostWords);
        char16_t buffer[8]{};
        status = Ok;
        Require(u_strToTitle_nid_postfix(buffer, -1, source, -1, nullptr, locale, &status) == 0 && status == IllegalArgument, "an illegal argument does not depend on the default");
    }
    for (const char* locale : {"i-default", "nl", "tr@x=y"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(GuestTitle(source, nullptr, locale, status) == HostTitle(source, nullptr, locale, hostStatus) && status == hostStatus, "title case for a locale ICU canonicalizes without the default matches ICU");
    }
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    uloc_setDefault_nid_postfix("nl", &status);
    uloc_setDefault("nl", &hostStatus);
    for (const char* locale : {"i-default@x=y", "i-default_US@x=y", "i-default.utf8"}) {
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(GuestTitle(source, nullptr, locale, status) == HostTitle(source, nullptr, locale, hostStatus) && status == hostStatus, "title case for an i-default locale uses the guest default's word breaks");
    }
    uloc_setDefault_nid_postfix(nullptr, &status);
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckKnownMappings();
    CheckMappingErrors();
    CheckMappingSweep();
    CheckComparisons();
    CheckTitleIterators();
    CheckIDefault();
    return 0;
}

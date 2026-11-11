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

std::int32_t Guest(char16_t* dest, std::int32_t capacity, const char16_t* src, std::int32_t length, const char* locale, std::int32_t* status) {
    return u_strToTitle_nid_postfix(dest, capacity, src, length, nullptr, locale, status);
}

std::int32_t Host(char16_t* dest, std::int32_t capacity, const char16_t* src, std::int32_t length, const char* locale, UErrorCode* status) {
    return u_strToTitle(dest, capacity, src, length, nullptr, locale, status);
}

std::u16string Map(const std::u16string& text, const char* locale) {
    char16_t buffer[128]{};
    std::int32_t status = Ok;
    const std::int32_t length = Guest(buffer, 128, text.c_str(), -1, locale, &status);
    Require(status <= Ok && length >= 0 && length < 128, "a mapping succeeds, possibly with a root break iterator warning");
    return std::u16string(buffer, static_cast<std::size_t>(length));
}

void CompareMapping(const std::u16string& text, const char* locale, std::int32_t capacity) {
    char16_t guest[96];
    char16_t host[96];
    std::fill(std::begin(guest), std::end(guest), 0x7777);
    std::fill(std::begin(host), std::end(host), 0x7777);
    std::int32_t guestStatus = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t guestLength = Guest(capacity ? guest : nullptr, capacity, text.data(), static_cast<std::int32_t>(text.size()), locale, &guestStatus);
    const std::int32_t hostLength = Host(capacity ? host : nullptr, capacity, text.data(), static_cast<std::int32_t>(text.size()), locale, &hostStatus);
    if (guestLength != hostLength || guestStatus != hostStatus || std::memcmp(guest, host, sizeof(guest)) != 0) {
        std::fprintf(stderr, "ICU CASE MAP: title case in %s differs for text starting %X\n", locale ? locale : "(null)", static_cast<unsigned>(text.empty() ? 0 : text[0]));
        std::abort();
    }
}

void CheckKnownMappings() {
    Require(Map(u"hello wORLD", "en") == u"Hello World" && Map(u"ijssel", "nl") == u"IJssel", "title case with a word iterator");
}

void CheckMappingErrors() {
    char16_t buffer[8]{};
    Require(Throws<std::invalid_argument>([&] { Guest(buffer, 8, u"a", -1, "", nullptr); }), "a null status throws");
    std::int32_t status = BufferOverflow;
    Require(Guest(buffer, 8, u"a", -1, nullptr, &status) == 0 && status == BufferOverflow, "a failed status returns before the locale");
    status = Ok;
    Require(Guest(buffer, -1, u"a", -1, nullptr, &status) == 0 && status == IllegalArgument, "a negative capacity");
    status = Ok;
    Require(Guest(nullptr, 2, u"a", -1, nullptr, &status) == 0 && status == IllegalArgument, "a null destination with capacity");
    status = Ok;
    Require(Guest(buffer, 8, nullptr, 1, nullptr, &status) == 0 && status == IllegalArgument, "a null source");
    status = Ok;
    Require(Guest(buffer, 8, u"a", -2, nullptr, &status) == 0 && status == IllegalArgument, "a length below -1");
    Require(Throws<std::runtime_error>([&] { std::int32_t unknown = Ok; Guest(buffer, 8, u"a", -1, nullptr, &unknown); }), "the console's default locale is unknown");
    auto* iterator = reinterpret_cast<UBreakIterator*>(buffer);
    status = BufferOverflow;
    Require(u_strToTitle_nid_postfix(buffer, 8, u"a", -1, iterator, "", &status) == 0 && status == BufferOverflow, "a failed status returns before the break iterator");

    status = Ok;
    uloc_setDefault_nid_postfix("tr", &status);
    Require(status == Ok, "the guest sets a default locale");
    Require(Map(u"istanbul", nullptr) == u"\u0130stanbul", "a null locale uses the guest's default");
    status = Ok;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; Guest(buffer, 8, u"a", -1, nullptr, &ok); }), "the default is unknown again");
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
            CompareMapping(text, locale, 96);
        }
        CompareMapping(text, "tr", 5);
        CompareMapping(text, "", 0);
    }
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
    CheckTitleIterators();
    CheckIDefault();
    return 0;
}

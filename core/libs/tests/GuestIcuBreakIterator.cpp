#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/parseerr.h>
#include <unicode/ubrk.h>
#include <unicode/udata.h>
#include <unicode/uloc.h>
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
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t UsingDefault = -127;
constexpr std::int32_t SafeCloneAllocated = -126;
constexpr std::int32_t Done = -1;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU BREAK ITERATOR: %s\n", message);
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

const char16_t* const Texts[] = {
    u"Hello, world! This is a test. Isn't it? Yes: 3.14 apples, $5 each.",
    u"The quick (\"brown\") fox can't jump 32.3 feet, right? Mr. Smith went to Washington.",
    u"ภาษาไทยไม่มีการเว้นวรรคระหว่างคำ แต่มีระหว่างประโยค",
    u"日本語の文章には単語の区切りがありません。これはテストです！",
    u"中文没有空格。这是一个测试。",
    u"éx 👍🏽 🇪🇸 a‍b",
    u"Ünïcödé text with soft­hyphen and non breaking space—dash",
    u"",
    u"etc. U.S.A. is big. Dr. No. z.B. ein Test. Bzw. noch einer.",
};

const char* const Locales[] = {"en", "", "th", "ja", "zh", "de", "sv", "fi", "en@lb=strict", "ja@lb=loose", "ja@lb=normal", "de@ss=standard", "en_US@ss=standard", "xx", "el"};

struct Walk {
    std::vector<std::int32_t> boundaries;
    std::vector<std::int32_t> statuses;
    std::vector<std::int32_t> vectors;
    std::vector<std::int32_t> backward;
    std::vector<std::int32_t> following;
    std::vector<std::int32_t> preceding;
    std::vector<std::int32_t> isBoundary;
    std::int32_t current = 0;

    bool operator==(const Walk&) const = default;
};

template<typename TFirst, typename TNext, typename TLast, typename TPrevious, typename TStatus, typename TVector, typename TFollowing, typename TPreceding, typename TBoundary, typename TCurrent>
Walk Iterate(std::int32_t length, TFirst first, TNext next, TLast last, TPrevious previous, TStatus status, TVector vector, TFollowing following, TPreceding preceding, TBoundary boundary, TCurrent current) {
    Walk walk;
    for (std::int32_t position = first(); position != Done; position = next()) {
        walk.boundaries.push_back(position);
        walk.statuses.push_back(status());
        std::int32_t values[8] = {};
        const std::int32_t count = vector(values, 8);
        walk.vectors.push_back(count);
        for (std::int32_t index = 0; index < count && index < 8; ++index) walk.vectors.push_back(values[index]);
    }
    for (std::int32_t position = last(); position != Done; position = previous()) walk.backward.push_back(position);
    for (std::int32_t offset = -1; offset <= length + 1; ++offset) {
        walk.following.push_back(following(offset));
        walk.preceding.push_back(preceding(offset));
        walk.isBoundary.push_back(boundary(offset));
        walk.isBoundary.push_back(current());
    }
    walk.current = current();
    return walk;
}

Walk GuestWalk(UBreakIterator* iterator, std::int32_t length) {
    return Iterate(length,
        [&] { return ubrk_first_nid_postfix(iterator); },
        [&] { return ubrk_next_nid_postfix(iterator); },
        [&] { return ubrk_last_nid_postfix(iterator); },
        [&] { return ubrk_previous_nid_postfix(iterator); },
        [&] { return ubrk_getRuleStatus_nid_postfix(iterator); },
        [&](std::int32_t* values, std::int32_t capacity) { std::int32_t status = Ok; const std::int32_t count = ubrk_getRuleStatusVec_nid_postfix(iterator, values, capacity, &status); Require(status == Ok, "guest rule status vector"); return count; },
        [&](std::int32_t offset) { return ubrk_following_nid_postfix(iterator, offset); },
        [&](std::int32_t offset) { return ubrk_preceding_nid_postfix(iterator, offset); },
        [&](std::int32_t offset) { return static_cast<std::int32_t>(ubrk_isBoundary_nid_postfix(iterator, offset)); },
        [&] { return ubrk_current_nid_postfix(iterator); });
}

Walk HostWalk(UBreakIterator* iterator, std::int32_t length) {
    return Iterate(length,
        [&] { return ubrk_first(iterator); },
        [&] { return ubrk_next(iterator); },
        [&] { return ubrk_last(iterator); },
        [&] { return ubrk_previous(iterator); },
        [&] { return ubrk_getRuleStatus(iterator); },
        [&](std::int32_t* values, std::int32_t capacity) { UErrorCode status = U_ZERO_ERROR; const std::int32_t count = ubrk_getRuleStatusVec(iterator, values, capacity, &status); Require(U_SUCCESS(status), "host rule status vector"); return count; },
        [&](std::int32_t offset) { return ubrk_following(iterator, offset); },
        [&](std::int32_t offset) { return ubrk_preceding(iterator, offset); },
        [&](std::int32_t offset) { return static_cast<std::int32_t>(ubrk_isBoundary(iterator, offset)); },
        [&] { return ubrk_current(iterator); });
}

std::int32_t Length(const char16_t* text) {
    return static_cast<std::int32_t>(std::char_traits<char16_t>::length(text));
}

std::vector<std::int32_t> Boundaries(UBreakIterator* iterator) {
    std::vector<std::int32_t> boundaries;
    for (std::int32_t position = ubrk_first_nid_postfix(iterator); position != Done; position = ubrk_next_nid_postfix(iterator)) boundaries.push_back(position);
    return boundaries;
}

UBreakIterator* Open(std::int32_t type, const char* locale, const char16_t* text) {
    std::int32_t status = Ok;
    UBreakIterator* iterator = ubrk_open_nid_postfix(type, locale, text, -1, &status);
    Require(iterator && status <= Ok, "a break iterator opens");
    return iterator;
}

void CheckKnown() {
    const char16_t* text = u"Hello, world! How are you? Fine.";
    UBreakIterator* word = Open(UBRK_WORD, "en", text);
    Require(Boundaries(word) == std::vector<std::int32_t>{0, 5, 6, 7, 12, 13, 14, 17, 18, 21, 22, 25, 26, 27, 31, 32}, "word boundaries");
    ubrk_first_nid_postfix(word);
    Require(ubrk_next_nid_postfix(word) == 5 && ubrk_getRuleStatus_nid_postfix(word) == UBRK_WORD_LETTER, "a word has the letter status");
    Require(ubrk_next_nid_postfix(word) == 6 && ubrk_getRuleStatus_nid_postfix(word) == UBRK_WORD_NONE, "punctuation has no word status");
    UBreakIterator* line = Open(UBRK_LINE, "en", text);
    Require(Boundaries(line) == std::vector<std::int32_t>{0, 7, 14, 18, 22, 27, 32}, "line boundaries");
    UBreakIterator* sentence = Open(UBRK_SENTENCE, "en", text);
    Require(Boundaries(sentence) == std::vector<std::int32_t>{0, 14, 27, 32}, "sentence boundaries");
    const char16_t combining[] = {u'e', 0x0301, u'x', 0xD83D, 0xDC4D, 0};
    UBreakIterator* character = Open(UBRK_CHARACTER, "en", combining);
    Require(Boundaries(character) == std::vector<std::int32_t>{0, 2, 3, 5}, "grapheme cluster boundaries");
    UBreakIterator* thai = Open(UBRK_WORD, "th", u"ภาษาไทย");
    Require(Boundaries(thai) == std::vector<std::int32_t>{0, 4, 7}, "Thai words come from the dictionary");
    UBreakIterator* abbreviation = Open(UBRK_SENTENCE, "en@ss=standard", u"Mr. Smith left. He came back.");
    Require(Boundaries(abbreviation) == std::vector<std::int32_t>{0, 16, 29}, "sentence break suppressions skip abbreviations");
    UBreakIterator* plain = Open(UBRK_SENTENCE, "en", u"Mr. Smith left. He came back.");
    Require(Boundaries(plain) == std::vector<std::int32_t>{0, 4, 16, 29}, "without suppressions an abbreviation ends a sentence");
    for (UBreakIterator* iterator : {word, line, sentence, character, thai, abbreviation, plain}) ubrk_close_nid_postfix(iterator);
}

void CheckSweep() {
    for (std::int32_t type = UBRK_CHARACTER; type <= UBRK_TITLE; ++type) {
        for (const char* locale : Locales) {
            for (const char16_t* text : Texts) {
                const std::int32_t length = Length(text);
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                UBreakIterator* guest = ubrk_open_nid_postfix(type, locale, text, length, &status);
                UBreakIterator* host = ubrk_open(static_cast<UBreakIteratorType>(type), locale, text, length, &hostStatus);
                Require(guest && host && status == hostStatus, "ubrk_open matches ICU");
                Require(GuestWalk(guest, length) == HostWalk(host, length), "every boundary query matches ICU");
                ubrk_close_nid_postfix(guest);
                ubrk_close(host);
            }
        }
    }
}

void CheckOpen() {
    Require(Throws<std::invalid_argument>([] { ubrk_open_nid_postfix(UBRK_WORD, "en", nullptr, 0, nullptr); }), "a null status throws in ubrk_open");
    std::int32_t status = BufferOverflow;
    Require(!ubrk_open_nid_postfix(UBRK_WORD, nullptr, nullptr, 0, &status) && status == BufferOverflow, "a failed status returns before the default locale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ubrk_open_nid_postfix(UBRK_WORD, nullptr, nullptr, 0, &ok); }), "a NULL locale without a guest default throws");
    for (std::int32_t type : {5, -1, 100}) {
        status = Ok;
        Require(!ubrk_open_nid_postfix(type, nullptr, u"a", -1, &status) && status == IllegalArgument, "an unknown type is an illegal argument without reading the default locale");
    }
    status = Ok;
    UBreakIterator* empty = ubrk_open_nid_postfix(UBRK_WORD, "en", nullptr, 0, &status);
    UErrorCode emptyStatus = U_ZERO_ERROR;
    UBreakIterator* hostEmpty = ubrk_open(UBRK_WORD, "en", nullptr, 0, &emptyStatus);
    Require(empty && status == emptyStatus && GuestWalk(empty, 0) == HostWalk(hostEmpty, 0), "an iterator opened without text matches ICU");
    ubrk_close_nid_postfix(empty);
    ubrk_close(hostEmpty);
    status = Ok;
    UBreakIterator* ignoredText = ubrk_open_nid_postfix(UBRK_WORD, "en", nullptr, 3, &status);
    Require(ignoredText && status <= Ok && ubrk_last_nid_postfix(ignoredText) == 0, "a NULL text is not set, whatever its length, as in ICU");
    ubrk_close_nid_postfix(ignoredText);
    status = Ok;
    UBreakIterator* badLength = ubrk_open_nid_postfix(UBRK_WORD, "en", u"abc", -2, &status);
    Require(badLength && status == IllegalArgument, "a length below -1 fails to set the text but returns the iterator, as in ICU");
    ubrk_close_nid_postfix(badLength);

    status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    UBreakIterator* unknown = ubrk_open_nid_postfix(UBRK_WORD, "xx", u"a b", -1, &status);
    UBreakIterator* hostUnknown = ubrk_open(UBRK_WORD, "xx", u"a b", -1, &hostStatus);
    Require(unknown && status == UsingDefault && hostStatus == U_USING_DEFAULT_WARNING, "an unknown locale uses root without consulting the default locale");
    ubrk_close_nid_postfix(unknown);
    ubrk_close(hostUnknown);

    const char16_t* text = u"Mr. Smith arrived at 3 p.m. today. Dr. Who left. Prof. X stayed. Etc. etc.";
    std::vector<std::vector<std::int32_t>> filtered;
    for (const char* defaultLocale : {"de", "en", "fr"}) {
        status = Ok;
        uloc_setDefault_nid_postfix(defaultLocale, &status);
        UBreakIterator* iterator = ubrk_open_nid_postfix(UBRK_SENTENCE, "xx@ss=standard", text, -1, &status);
        Require(iterator && status <= Ok, "a filtered sentence iterator for an unknown locale opens");
        filtered.push_back(Boundaries(iterator));
        ubrk_close_nid_postfix(iterator);
    }
    Require(filtered[0] == filtered[1] && filtered[1] == filtered[2], "the filtered iterator does not depend on the default locale");
    hostStatus = U_ZERO_ERROR;
    UBreakIterator* hostRoot = ubrk_open(UBRK_SENTENCE, "root", text, -1, &hostStatus);
    std::vector<std::int32_t> rootBoundaries;
    for (std::int32_t position = ubrk_first(hostRoot); position != Done; position = ubrk_next(hostRoot)) rootBoundaries.push_back(position);
    ubrk_close(hostRoot);
    Require(filtered[0] == rootBoundaries, "the filtered iterator of an unknown locale suppresses nothing");

    status = Ok;
    uloc_setDefault_nid_postfix("ja", &status);
    uloc_setDefault("ja", &hostStatus);
    const char16_t* japanese = Texts[3];
    UBreakIterator* guest = ubrk_open_nid_postfix(UBRK_LINE, nullptr, japanese, -1, &status);
    UBreakIterator* host = ubrk_open(UBRK_LINE, nullptr, japanese, -1, &hostStatus);
    Require(guest && host && status == hostStatus && GuestWalk(guest, Length(japanese)) == HostWalk(host, Length(japanese)), "a NULL locale uses the guest default");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(std::strcmp(ubrk_getLocaleByType_nid_postfix(guest, ULOC_ACTUAL_LOCALE, &status), ubrk_getLocaleByType(host, ULOC_ACTUAL_LOCALE, &hostStatus)) == 0 && std::strncmp(ubrk_getLocaleByType_nid_postfix(guest, ULOC_ACTUAL_LOCALE, &status), "ja", 2) == 0 && status == Ok, "the default's locale is reported");
    ubrk_close_nid_postfix(guest);
    ubrk_close(host);
    uloc_setDefault_nid_postfix(nullptr, &status);
    ubrk_close_nid_postfix(nullptr);
}

void CheckIDefault() {
    const char16_t* japanese = Texts[3];
    for (const char* locale : {"i-default@lb=strict", "i-default_US@x=y", "i-default-x@a=b", "i-default.utf8", "I-DEFAULT@lb=strict", "i_default_US"}) {
        for (std::int32_t type : {UBRK_CHARACTER, UBRK_WORD, UBRK_LINE, UBRK_SENTENCE}) {
            Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ubrk_open_nid_postfix(type, locale, japanese, -1, &ok); }), "an i-default locale that ICU canonicalizes to the default throws");
        }
        std::int32_t status = Ok;
        Require(!ubrk_open_nid_postfix(5, locale, japanese, -1, &status) && status == IllegalArgument, "an unknown type never canonicalizes the locale");
    }
    for (const char* locale : {"i-default", "I-DEFAULT", "i-defaults@lb=strict", "ja", "ja@lb=strict"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UBreakIterator* guest = ubrk_open_nid_postfix(UBRK_LINE, locale, japanese, -1, &status);
        UBreakIterator* host = ubrk_open(UBRK_LINE, locale, japanese, -1, &hostStatus);
        Require(guest && host && status == hostStatus && GuestWalk(guest, Length(japanese)) == HostWalk(host, Length(japanese)), "a locale ICU canonicalizes without the default matches ICU");
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(std::strcmp(ubrk_getLocaleByType_nid_postfix(guest, ULOC_ACTUAL_LOCALE, &status), ubrk_getLocaleByType(host, ULOC_ACTUAL_LOCALE, &hostStatus)) == 0, "the same break data opens");
        ubrk_close_nid_postfix(guest);
        ubrk_close(host);
    }
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    uloc_setDefault_nid_postfix("ja", &status);
    uloc_setDefault("ja", &hostStatus);
    for (const char* locale : {"i-default@lb=strict", "i-default_US@x=y", "i-default.utf8"}) {
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        UBreakIterator* guest = ubrk_open_nid_postfix(UBRK_LINE, locale, japanese, -1, &status);
        UBreakIterator* host = ubrk_open(UBRK_LINE, locale, japanese, -1, &hostStatus);
        Require(guest && host && status == hostStatus && GuestWalk(guest, Length(japanese)) == HostWalk(host, Length(japanese)), "an i-default locale uses the guest default");
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(std::strcmp(ubrk_getLocaleByType_nid_postfix(guest, ULOC_ACTUAL_LOCALE, &status), ubrk_getLocaleByType(host, ULOC_ACTUAL_LOCALE, &hostStatus)) == 0, "the guest default's break data opens as in ICU");
        Require(std::strchr(locale, '@') == nullptr || std::strncmp(ubrk_getLocaleByType_nid_postfix(guest, ULOC_ACTUAL_LOCALE, &status), "ja", 2) == 0, "an i-default locale with keywords opens the guest default's data");
        ubrk_close_nid_postfix(guest);
        ubrk_close(host);
    }
    uloc_setDefault_nid_postfix(nullptr, &status);
}

void CheckText() {
    UBreakIterator* iterator = Open(UBRK_WORD, "en", u"one two");
    Require(Throws<std::invalid_argument>([&] { ubrk_setText_nid_postfix(iterator, u"a", -1, nullptr); }), "a null status throws in ubrk_setText");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ubrk_setText_nid_postfix(nullptr, u"a", -1, &ok); }), "a null iterator throws in ubrk_setText");
    std::int32_t status = BufferOverflow;
    ubrk_setText_nid_postfix(iterator, u"three four five", -1, &status);
    Require(status == BufferOverflow && Boundaries(iterator) == std::vector<std::int32_t>{0, 3, 4, 7}, "a failed status keeps the old text");
    status = Ok;
    ubrk_setText_nid_postfix(iterator, nullptr, 2, &status);
    Require(status == IllegalArgument && Boundaries(iterator) == std::vector<std::int32_t>{0, 3, 4, 7}, "a NULL text with a length keeps the old text");
    status = Ok;
    const char16_t text[] = u"three four five";
    ubrk_setText_nid_postfix(iterator, text, 10, &status);
    Require(status == Ok && Boundaries(iterator) == std::vector<std::int32_t>{0, 5, 6, 10}, "a counted text replaces the old one");
    status = Ok;
    ubrk_setText_nid_postfix(iterator, nullptr, 0, &status);
    Require(status == Ok && Boundaries(iterator) == std::vector<std::int32_t>{0}, "an empty text");
    ubrk_close_nid_postfix(iterator);

    Require(Throws<std::invalid_argument>([] { ubrk_current_nid_postfix(nullptr); }), "a null iterator throws in ubrk_current");
    Require(Throws<std::invalid_argument>([] { ubrk_first_nid_postfix(nullptr); }), "a null iterator throws in ubrk_first");
    Require(Throws<std::invalid_argument>([] { ubrk_last_nid_postfix(nullptr); }), "a null iterator throws in ubrk_last");
    Require(Throws<std::invalid_argument>([] { ubrk_next_nid_postfix(nullptr); }), "a null iterator throws in ubrk_next");
    Require(Throws<std::invalid_argument>([] { ubrk_previous_nid_postfix(nullptr); }), "a null iterator throws in ubrk_previous");
    Require(Throws<std::invalid_argument>([] { ubrk_following_nid_postfix(nullptr, 0); }), "a null iterator throws in ubrk_following");
    Require(Throws<std::invalid_argument>([] { ubrk_preceding_nid_postfix(nullptr, 0); }), "a null iterator throws in ubrk_preceding");
    Require(Throws<std::invalid_argument>([] { ubrk_isBoundary_nid_postfix(nullptr, 0); }), "a null iterator throws in ubrk_isBoundary");
    Require(Throws<std::invalid_argument>([] { ubrk_getRuleStatus_nid_postfix(nullptr); }), "a null iterator throws in ubrk_getRuleStatus");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; std::int32_t value = 0; ubrk_getRuleStatusVec_nid_postfix(nullptr, &value, 1, &ok); }), "a null iterator throws in ubrk_getRuleStatusVec");
}

void CheckRuleStatusVector() {
    std::int32_t status = Ok;
    UBreakIterator* rules = ubrk_openRules_nid_postfix(u"$a = [a-z]; $a+ {100}; $a+ {200}; [0-9]+ {300}; .;", -1, u"abc 123", -1, nullptr, &status);
    Require(rules && status == Ok, "rules with two statuses compile");
    Require(ubrk_first_nid_postfix(rules) == 0 && ubrk_next_nid_postfix(rules) == 3, "the rule iterator reaches its first boundary");
    std::int32_t values[4] = {-7, -7, -7, -7};
    Require(ubrk_getRuleStatusVec_nid_postfix(rules, values, 4, &status) == 2 && status == Ok && values[0] == 100 && values[1] == 200 && values[2] == -7, "both statuses are returned");
    Require(ubrk_getRuleStatus_nid_postfix(rules) == 200, "the largest status is the rule status");
    values[0] = values[1] = -7;
    Require(ubrk_getRuleStatusVec_nid_postfix(rules, values, 2, &status) == 2 && status == Ok && values[0] == 100 && values[1] == 200, "an exact capacity holds every status");
    values[0] = -7;
    Require(ubrk_getRuleStatusVec_nid_postfix(rules, values, 1, &status) == 2 && status == BufferOverflow && values[0] == 100 && values[1] == 200, "a short vector overflows after the values that fit");
    status = Ok;
    Require(ubrk_getRuleStatusVec_nid_postfix(rules, nullptr, 0, &status) == 2 && status == BufferOverflow, "a zero capacity preflights");
    status = Ok;
    Require(ubrk_getRuleStatusVec_nid_postfix(rules, values, -1, &status) == 2 && status == BufferOverflow, "a negative capacity overflows");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ubrk_getRuleStatusVec_nid_postfix(rules, nullptr, 2, &ok); }), "a NULL vector that ICU would write throws");
    Require(Throws<std::invalid_argument>([&] { ubrk_getRuleStatusVec_nid_postfix(rules, values, 4, nullptr); }), "a null status throws in ubrk_getRuleStatusVec");
    status = IllegalArgument;
    Require(ubrk_getRuleStatusVec_nid_postfix(rules, nullptr, 2, &status) == 0 && status == IllegalArgument, "a failed status returns 0 before the vector");
    ubrk_close_nid_postfix(rules);
}

void CheckRules() {
    const char16_t* rules = u"!!chain; $L = [:L:]; $N = [:N:]; $L+ {100}; $N+ {200}; $N+ ($L $N+)+ {300}; [\\p{WSpace}]+; .;";
    const char16_t* text = u"abc 123 x4y5 ++ Ωμέγα 7";
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    UParseError guestError{};
    UParseError hostError{};
    UBreakIterator* guest = ubrk_openRules_nid_postfix(rules, -1, text, -1, &guestError, &status);
    UBreakIterator* host = ubrk_openRules(rules, -1, text, -1, &hostError, &hostStatus);
    Require(guest && host && status == hostStatus, "custom rules compile as in ICU");
    Require(GuestWalk(guest, Length(text)) == HostWalk(host, Length(text)), "custom rules break as in ICU");

    std::vector<std::uint8_t> binary;
    status = Ok;
    const std::int32_t size = ubrk_getBinaryRules_nid_postfix(guest, nullptr, 0, &status);
    hostStatus = U_ZERO_ERROR;
    Require(size > 0 && status == Ok && size == ubrk_getBinaryRules(host, nullptr, 0, &hostStatus), "binary rules preflight");
    binary.resize(static_cast<std::size_t>(size));
    std::vector<std::uint8_t> hostBinary(binary.size());
    Require(ubrk_getBinaryRules_nid_postfix(guest, binary.data(), size, &status) == size && status == Ok, "binary rules are copied");
    ubrk_getBinaryRules(host, hostBinary.data(), size, &hostStatus);
    Require(binary == hostBinary, "the binary rules match ICU");
    std::vector<std::uint8_t> shortBuffer(binary.size(), 0xAB);
    Require(ubrk_getBinaryRules_nid_postfix(guest, shortBuffer.data(), size - 1, &status) == size && status == BufferOverflow && shortBuffer[0] == 0xAB, "a short buffer overflows without a copy");
    status = Ok;
    Require(ubrk_getBinaryRules_nid_postfix(guest, nullptr, 4, &status) == 0 && status == IllegalArgument, "a NULL buffer with a capacity is an illegal argument");
    status = Ok;
    Require(ubrk_getBinaryRules_nid_postfix(guest, binary.data(), -1, &status) == 0 && status == IllegalArgument, "a negative capacity is an illegal argument");
    status = Ok;
    Require(ubrk_getBinaryRules_nid_postfix(nullptr, nullptr, 0, &status) == 0 && status == IllegalArgument, "a NULL iterator is not rule based");
    status = Ok;
    UBreakIterator* filtered = ubrk_open_nid_postfix(UBRK_SENTENCE, "en@ss=standard", u"a", -1, &status);
    Require(ubrk_getBinaryRules_nid_postfix(filtered, nullptr, 0, &status) == 0 && status == IllegalArgument, "a filtered sentence iterator is not rule based");
    ubrk_close_nid_postfix(filtered);
    status = BufferOverflow;
    Require(ubrk_getBinaryRules_nid_postfix(nullptr, nullptr, -5, &status) == 0 && status == BufferOverflow, "a failed status returns first");
    Require(Throws<std::invalid_argument>([&] { ubrk_getBinaryRules_nid_postfix(guest, nullptr, 0, nullptr); }), "a null status throws in ubrk_getBinaryRules");

    status = Ok;
    UBreakIterator* fromBinary = ubrk_openBinaryRules_nid_postfix(binary.data(), size, text, -1, &status);
    Require(fromBinary && status == Ok && GuestWalk(fromBinary, Length(text)) == HostWalk(host, Length(text)), "binary rules break as the source rules");
    status = Ok;
    Require(!ubrk_openBinaryRules_nid_postfix(binary.data(), -1, nullptr, 0, &status) && status == IllegalArgument, "a negative binary length is an illegal argument");
    status = Ok;
    Require(!ubrk_openBinaryRules_nid_postfix(nullptr, 0, nullptr, 0, &status) && status == IllegalArgument, "NULL binary rules are an illegal argument");
    status = Ok;
    Require(!ubrk_openBinaryRules_nid_postfix(binary.data(), 8, nullptr, 0, &status) && status == IllegalArgument, "binary rules shorter than their header are an illegal argument");
    status = Ok;
    Require(!ubrk_openBinaryRules_nid_postfix(binary.data(), size - 1, nullptr, 0, &status) && status == IllegalArgument, "binary rules shorter than their recorded length are an illegal argument");
    status = BufferOverflow;
    Require(!ubrk_openBinaryRules_nid_postfix(nullptr, -1, nullptr, 0, &status) && status == BufferOverflow, "a failed status opens no binary rules");
    Require(Throws<std::invalid_argument>([&] { ubrk_openBinaryRules_nid_postfix(binary.data(), size, nullptr, 0, nullptr); }), "a null status throws in ubrk_openBinaryRules");
    ubrk_close_nid_postfix(fromBinary);

    const char16_t* broken = u"$a = [a-z; $a+;";
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    guestError = UParseError{};
    hostError = UParseError{};
    Require(!ubrk_openRules_nid_postfix(broken, -1, nullptr, 0, &guestError, &status) && !ubrk_openRules(broken, -1, nullptr, 0, &hostError, &hostStatus), "broken rules open nothing");
    Require(status == hostStatus && status > Ok && guestError.line == hostError.line && guestError.offset == hostError.offset && std::memcmp(guestError.preContext, hostError.preContext, sizeof(hostError.preContext)) == 0 && std::memcmp(guestError.postContext, hostError.postContext, sizeof(hostError.postContext)) == 0, "a rule error and its position match ICU");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(!ubrk_openRules_nid_postfix(broken, -1, nullptr, 0, nullptr, &status) && !ubrk_openRules(broken, -1, nullptr, 0, nullptr, &hostStatus) && status == hostStatus, "a NULL parse error is allowed");
    Require(!ubrk_openRules_nid_postfix(rules, -1, nullptr, 0, nullptr, nullptr), "a null status returns NULL in ubrk_openRules, as in ICU");
    status = IllegalArgument;
    Require(!ubrk_openRules_nid_postfix(rules, -1, nullptr, 0, nullptr, &status) && status == IllegalArgument, "a failed status compiles nothing");
    ubrk_close_nid_postfix(guest);
    ubrk_close(host);
}

void CheckClone() {
    const char16_t* text = u"one two three";
    UBreakIterator* iterator = Open(UBRK_WORD, "en", text);
    ubrk_following_nid_postfix(iterator, 5);
    std::int32_t status = Ok;
    UBreakIterator* clone = ubrk_safeClone_nid_postfix(iterator, nullptr, nullptr, &status);
    Require(clone && clone != iterator && status == SafeCloneAllocated, "a clone is allocated with a warning");
    Require(ubrk_current_nid_postfix(clone) == 7 && Boundaries(clone) == Boundaries(iterator), "a clone keeps the position and text");
    std::int32_t size = 0;
    status = Ok;
    Require(!ubrk_safeClone_nid_postfix(iterator, nullptr, &size, &status) && size == 1 && status == Ok, "a zero buffer size preflights");
    size = 512;
    char buffer[512];
    UBreakIterator* sized = ubrk_safeClone_nid_postfix(iterator, buffer, &size, &status);
    Require(sized && reinterpret_cast<char*>(sized) != buffer && size == 1 && status == SafeCloneAllocated, "the stack buffer is ignored");
    status = Ok;
    Require(!ubrk_safeClone_nid_postfix(nullptr, nullptr, nullptr, &status) && status == IllegalArgument, "a NULL iterator is an illegal argument");
    Require(!ubrk_safeClone_nid_postfix(iterator, nullptr, nullptr, nullptr), "a null status returns NULL in ubrk_safeClone, as in ICU");
    status = BufferOverflow;
    size = 0;
    Require(!ubrk_safeClone_nid_postfix(nullptr, nullptr, &size, &status) && status == BufferOverflow && size == 0, "a failed status returns first");
    for (UBreakIterator* each : {iterator, clone, sized}) ubrk_close_nid_postfix(each);
}

void CheckLocales() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    for (const char* locale : {"de_CH", "en_US_POSIX", "ja@lb=strict", "xx", "", "zh_Hant_TW", "sr_Latn"}) {
        for (std::int32_t type : {UBRK_WORD, UBRK_LINE, UBRK_SENTENCE}) {
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            UBreakIterator* guest = ubrk_open_nid_postfix(type, locale, nullptr, 0, &status);
            UBreakIterator* host = ubrk_open(static_cast<UBreakIteratorType>(type), locale, nullptr, 0, &hostStatus);
            for (std::int32_t which : {ULOC_ACTUAL_LOCALE, ULOC_VALID_LOCALE}) {
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                const char* guestLocale = ubrk_getLocaleByType_nid_postfix(guest, which, &status);
                const char* hostLocale = ubrk_getLocaleByType(host, static_cast<ULocDataLocaleType>(which), &hostStatus);
                Require(status == hostStatus && std::string(guestLocale ? guestLocale : "(null)") == (hostLocale ? hostLocale : "(null)"), "the iterator's locale matches ICU");
            }
            status = Ok;
            Require(!ubrk_getLocaleByType_nid_postfix(guest, 7, &status) && status == IllegalArgument, "an unknown locale type is an illegal argument");
            ubrk_close_nid_postfix(guest);
            ubrk_close(host);
        }
    }
    status = Ok;
    Require(!ubrk_getLocaleByType_nid_postfix(nullptr, ULOC_VALID_LOCALE, &status) && status == IllegalArgument, "a NULL iterator is an illegal argument");
    status = BufferOverflow;
    Require(!ubrk_getLocaleByType_nid_postfix(nullptr, ULOC_VALID_LOCALE, &status) && status == BufferOverflow, "a NULL iterator keeps a failed status");
    Require(Throws<std::invalid_argument>([] { ubrk_getLocaleByType_nid_postfix(nullptr, ULOC_VALID_LOCALE, nullptr); }), "a null status throws in ubrk_getLocaleByType");

    const std::int32_t count = ubrk_countAvailable_nid_postfix();
    Require(count > 100 && count == ubrk_countAvailable(), "the available count matches ICU");
    for (std::int32_t index = 0; index < count; ++index) Require(std::strcmp(ubrk_getAvailable_nid_postfix(index), ubrk_getAvailable(index)) == 0, "each available locale matches ICU");
    Require(!ubrk_getAvailable_nid_postfix(count + 1) && !ubrk_getAvailable_nid_postfix(count + 1000), "an index past the end returns NULL");
    Require(Throws<std::invalid_argument>([] { ubrk_getAvailable_nid_postfix(-1); }), "a negative index throws");
    Require(Throws<std::invalid_argument>([&] { ubrk_getAvailable_nid_postfix(count); }), "the index ICU reads past its array throws");
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckKnown();
    CheckOpen();
    CheckIDefault();
    CheckText();
    CheckRuleStatusVector();
    CheckRules();
    CheckClone();
    CheckLocales();
    CheckSweep();
    std::puts("guest_icu_break_iterator passed");
    return 0;
}

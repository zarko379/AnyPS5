#include "prx/libSceIcu/StringSearch.hpp"
#include "prx/libSceIcu/Collation.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/ucol.h>
#include <unicode/udata.h>
#include <unicode/usearch.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t UsingDefault = -127;
constexpr std::int32_t Done = -1;
constexpr std::size_t MatchLimit = 64;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU STRING SEARCH: %s\n", message);
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

using Matches = std::vector<std::pair<std::int32_t, std::int32_t>>;

UStringSearch* Open(const char16_t* pattern, const char16_t* text, const char* locale) {
    std::int32_t status = Ok;
    UStringSearch* search = usearch_open_nid_postfix(pattern, -1, text, -1, locale, nullptr, &status);
    Require(search && status <= Ok, "a search opens");
    return search;
}

Matches All(UStringSearch* search) {
    Matches matches;
    while (matches.size() < MatchLimit) {
        std::int32_t status = Ok;
        const std::int32_t index = usearch_next_nid_postfix(search, &status);
        Require(status == Ok, "usearch_next succeeds");
        if (index == Done) break;
        matches.emplace_back(index, usearch_getMatchedLength_nid_postfix(search));
    }
    return matches;
}

Matches HostAll(UStringSearch* search) {
    Matches matches;
    while (matches.size() < MatchLimit) {
        UErrorCode status = U_ZERO_ERROR;
        const std::int32_t index = usearch_next(search, &status);
        Require(U_SUCCESS(status), "host usearch_next succeeds");
        if (index == Done) break;
        matches.emplace_back(index, usearch_getMatchedLength(search));
    }
    return matches;
}

void CheckOpen() {
    Require(Throws<std::invalid_argument>([] { usearch_open_nid_postfix(u"a", -1, u"a", -1, "", nullptr, nullptr); }), "a null status throws in usearch_open");
    auto* iterator = reinterpret_cast<UBreakIterator*>(16);
    std::int32_t status = IllegalArgument;
    Require(!usearch_open_nid_postfix(u"a", -1, u"a", -1, "", iterator, &status) && status == IllegalArgument, "a failed status returns NULL before the break iterator is read");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; usearch_open_nid_postfix(u"a", -1, u"a", -1, "", iterator, &ok); }), "a guest break iterator throws");
    status = Ok;
    Require(!usearch_open_nid_postfix(u"a", -1, u"a", -1, nullptr, nullptr, &status) && status == IllegalArgument, "a NULL locale is an illegal argument, not the default locale");
    status = Ok;
    Require(!usearch_open_nid_postfix(nullptr, -1, u"a", -1, "", nullptr, &status) && status == IllegalArgument, "a NULL pattern is an illegal argument");
    status = Ok;
    Require(!usearch_open_nid_postfix(u"a", -1, nullptr, -1, "", nullptr, &status) && status == IllegalArgument, "a NULL text is an illegal argument");
    status = Ok;
    Require(!usearch_open_nid_postfix(u"a", -1, u"a", 0, "", nullptr, &status) && status == IllegalArgument, "an empty text is an illegal argument");
    status = Ok;
    Require(!usearch_open_nid_postfix(u"", -1, u"a", -1, "", nullptr, &status) && status == IllegalArgument, "an empty pattern is an illegal argument");
    status = Ok;
    Require(!usearch_open_nid_postfix(u"a", -1, u"a", -1, "de@colNumeric=yes", nullptr, &status) && status == Unsupported, "numeric collation is unsupported");
    status = Ok;
    UStringSearch* unknown = usearch_open_nid_postfix(u"b", -1, u"abc", -1, "xx", nullptr, &status);
    Require(unknown && status == UsingDefault, "an unknown locale searches with root without consulting the default locale");
    status = Ok;
    Require(usearch_next_nid_postfix(unknown, &status) == 1 && status == Ok && usearch_getMatchedLength_nid_postfix(unknown) == 1, "the root search finds the match");
    Require(usearch_getCollator_nid_postfix(nullptr) == nullptr && usearch_getMatchedLength_nid_postfix(nullptr) == Done, "a null search has no collator and no match");
    usearch_reset_nid_postfix(nullptr);
    status = Ok;
    Require(usearch_next_nid_postfix(nullptr, &status) == Done && status == Ok, "a null search finds nothing");
}

void CheckKnownMatches() {
    UStringSearch* fox = Open(u"fox", u"The quick brown fox jumps over the lazy fox", "en");
    Require(All(fox) == Matches{{16, 3}, {40, 3}}, "fox is found twice");
    UStringSearch* resume = Open(u"resume", u"r\u00E9sum\u00E9 resume RESUME", "");
    Require(All(resume) == Matches{{7, 6}}, "tertiary strength matches case and accents exactly");
    UCollator* collator = usearch_getCollator_nid_postfix(resume);
    Require(collator != nullptr, "the search owns a collator");
    ucol_setStrength_nid_postfix(collator, UCOL_PRIMARY);
    usearch_reset_nid_postfix(resume);
    Require(All(resume) == Matches{{0, 6}, {7, 6}, {14, 6}}, "primary strength ignores case and accents after a reset");
    UStringSearch* overlap = Open(u"aa", u"aaaa", "");
    Require(All(overlap) == Matches{{0, 2}, {2, 2}}, "matches do not overlap by default");
    usearch_reset_nid_postfix(overlap);
    std::int32_t status = Ok;
    usearch_setAttribute_nid_postfix(overlap, USEARCH_OVERLAP, USEARCH_ON, &status);
    Require(status == Ok && All(overlap) == Matches{{0, 2}, {1, 2}, {2, 2}}, "overlapping matches");
    usearch_reset_nid_postfix(overlap);
    Require(All(overlap) == Matches{{0, 2}, {2, 2}}, "a reset turns overlapping off again");
    UStringSearch* canonical = Open(u"\u00E5", u"a\u030A \u00E5", "");
    Require(All(canonical) == Matches{{0, 2}, {3, 1}}, "canonically equivalent text matches");
    status = Ok;
    usearch_setPattern_nid_postfix(fox, u"lazy", -1, &status);
    Require(status == Ok && All(fox).empty(), "a new pattern is searched from the current offset");
    usearch_reset_nid_postfix(fox);
    Require(All(fox) == Matches{{35, 4}}, "a new pattern is searched after a reset");
    status = Ok;
    usearch_setText_nid_postfix(fox, u"lazy dogs are lazy", -1, &status);
    Require(status == Ok && All(fox) == Matches{{0, 4}, {14, 4}}, "a new text is searched");
    status = Ok;
    usearch_setOffset_nid_postfix(fox, 5, &status);
    std::int32_t nextStatus = Ok;
    Require(status == Ok && usearch_next_nid_postfix(fox, &nextStatus) == 14 && usearch_getMatchedLength_nid_postfix(fox) == 4, "the search continues at the offset");
    status = Ok;
    usearch_setOffset_nid_postfix(fox, 19, &status);
    Require(status == IndexOutOfBounds, "an offset past the text is out of bounds");
    status = Ok;
    usearch_setOffset_nid_postfix(fox, -1, &status);
    Require(status == IndexOutOfBounds, "a negative offset is out of bounds");
}

void CheckErrors() {
    UStringSearch* search = Open(u"b", u"abc", "");
    Require(Throws<std::invalid_argument>([&] { usearch_next_nid_postfix(search, nullptr); }), "a null status throws in usearch_next");
    Require(Throws<std::invalid_argument>([&] { usearch_setAttribute_nid_postfix(search, USEARCH_OVERLAP, USEARCH_ON, nullptr); }), "a null status throws in usearch_setAttribute");
    Require(Throws<std::invalid_argument>([&] { usearch_setOffset_nid_postfix(search, 0, nullptr); }), "a null status throws in usearch_setOffset");
    Require(Throws<std::invalid_argument>([&] { usearch_setPattern_nid_postfix(search, u"a", -1, nullptr); }), "a null status throws in usearch_setPattern");
    Require(Throws<std::invalid_argument>([&] { usearch_setText_nid_postfix(search, u"a", -1, nullptr); }), "a null status throws in usearch_setText");
    std::int32_t status = IllegalArgument;
    Require(usearch_next_nid_postfix(search, &status) == Done && status == IllegalArgument, "a failed status finds nothing");
    usearch_setOffset_nid_postfix(search, 99, &status);
    usearch_setPattern_nid_postfix(search, u"zz", -7, &status);
    usearch_setText_nid_postfix(search, nullptr, 0, &status);
    usearch_setAttribute_nid_postfix(search, USEARCH_OVERLAP, USEARCH_ON, &status);
    Require(status == IllegalArgument, "a failed status is kept");
    status = Ok;
    Require(usearch_next_nid_postfix(search, &status) == 1 && status == Ok, "nothing changed under a failed status");
    status = Ok;
    usearch_setAttribute_nid_postfix(nullptr, USEARCH_OVERLAP, USEARCH_ATTRIBUTE_VALUE_COUNT, &status);
    Require(status == IllegalArgument, "the value count is an illegal argument even without a search");
    status = Unsupported;
    usearch_setAttribute_nid_postfix(search, USEARCH_OVERLAP, USEARCH_ATTRIBUTE_VALUE_COUNT, &status);
    Require(status == IllegalArgument, "the value count overwrites a failed status, as in ICU");
    status = Ok;
    usearch_setAttribute_nid_postfix(search, USEARCH_ATTRIBUTE_COUNT, USEARCH_ON, &status);
    Require(status == IllegalArgument, "an unknown attribute is an illegal argument");
    status = Ok;
    usearch_setAttribute_nid_postfix(nullptr, USEARCH_OVERLAP, USEARCH_ON, &status);
    usearch_setOffset_nid_postfix(nullptr, 0, &status);
    Require(status == Ok, "a null search is ignored by the setters");
    usearch_setPattern_nid_postfix(nullptr, u"a", -1, &status);
    Require(status == IllegalArgument, "a null search is an illegal argument for a pattern");
    status = Ok;
    usearch_setPattern_nid_postfix(search, nullptr, -1, &status);
    Require(status == IllegalArgument, "a null pattern is an illegal argument");
    status = Ok;
    usearch_setPattern_nid_postfix(search, u"", -1, &status);
    Require(status == IllegalArgument, "an empty pattern is an illegal argument");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; usearch_setPattern_nid_postfix(search, u"a", -2, &ok); }), "a pattern length ICU would read before the pattern throws");
    status = Ok;
    usearch_setText_nid_postfix(nullptr, u"a", -1, &status);
    Require(status == IllegalArgument, "a null search is an illegal argument for a text");
    for (std::int32_t length : {0, -2}) {
        status = Ok;
        usearch_setText_nid_postfix(search, u"a", length, &status);
        Require(status == IllegalArgument, "an empty or negative text length is an illegal argument");
    }
    status = Ok;
    usearch_setText_nid_postfix(search, nullptr, 1, &status);
    Require(status == IllegalArgument, "a null text is an illegal argument");
}

void CheckSweep() {
    const std::vector<std::pair<std::u16string, std::u16string>> cases{
        {u"fox", u"The quick brown fox jumps over the lazy fox"},
        {u"resume", u"r\u00E9sum\u00E9 resume RESUME Resume"},
        {u"aa", u"aaaaa a\u0308a\u0308 \u00E4\u00E4"},
        {u"\u00E5", u"a\u030A \u00E5 A\u030A \u00C5 a \u212B"},
        {u"strasse", u"Ich wei\u00DF, Stra\u00DFe STRASSE strasse"},
        {u"ch", u"chico cocina chocolate CH"},
        {u"\u0E01", u"\u0E40\u0E01\u0E01\u0E32 \u0E01\u0E34"},
        {u"\u304B", u"\u304B\u304C\u30AB\u30AC \u304B\u3099"},
        {u"a\u0301", u"\u00E1 a\u0301 a \u0301 A\u0301b"},
        {u"\U0001F600", u"x\U0001F600y\U0001F600"},
        {u"b", u"a-b ab a b"}};
    for (const char* locale : {"", "de", "es@collation=traditional", "th", "ja", "da"}) {
        for (const auto& [pattern, text] : cases) {
            UStringSearch* guest = Open(pattern.c_str(), text.c_str(), locale);
            UErrorCode openStatus = U_ZERO_ERROR;
            UStringSearch* host = usearch_open(pattern.c_str(), -1, text.c_str(), -1, locale, nullptr, &openStatus);
            Require(host != nullptr, "a host search opens");
            for (std::int32_t strength : {UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_IDENTICAL}) {
                for (std::int32_t overlap : {USEARCH_OFF, USEARCH_ON}) {
                    for (std::int32_t canonical : {USEARCH_OFF, USEARCH_ON}) {
                        for (std::int32_t comparison : {USEARCH_STANDARD_ELEMENT_COMPARISON, USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD, USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD}) {
                            ucol_setStrength_nid_postfix(usearch_getCollator_nid_postfix(guest), strength);
                            ucol_setStrength(usearch_getCollator(host), static_cast<UCollationStrength>(strength));
                            usearch_reset_nid_postfix(guest);
                            usearch_reset(host);
                            std::int32_t status = Ok;
                            UErrorCode hostStatus = U_ZERO_ERROR;
                            usearch_setAttribute_nid_postfix(guest, USEARCH_OVERLAP, overlap, &status);
                            usearch_setAttribute_nid_postfix(guest, USEARCH_CANONICAL_MATCH, canonical, &status);
                            usearch_setAttribute_nid_postfix(guest, USEARCH_ELEMENT_COMPARISON, comparison, &status);
                            usearch_setAttribute(host, USEARCH_OVERLAP, static_cast<USearchAttributeValue>(overlap), &hostStatus);
                            usearch_setAttribute(host, USEARCH_CANONICAL_MATCH, static_cast<USearchAttributeValue>(canonical), &hostStatus);
                            usearch_setAttribute(host, USEARCH_ELEMENT_COMPARISON, static_cast<USearchAttributeValue>(comparison), &hostStatus);
                            Require(status == Ok && U_SUCCESS(hostStatus), "attributes are set");
                            Require(All(guest) == HostAll(host), "matches agree with ICU");
                            const std::int32_t offset = static_cast<std::int32_t>(text.size() / 2);
                            usearch_setOffset_nid_postfix(guest, offset, &status);
                            usearch_setOffset(host, offset, &hostStatus);
                            Require(status == hostStatus && All(guest) == HostAll(host), "matches from an offset agree with ICU");
                        }
                    }
                }
            }
            usearch_close(host);
        }
    }
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckOpen();
    CheckKnownMatches();
    CheckErrors();
    CheckSweep();
    return 0;
}

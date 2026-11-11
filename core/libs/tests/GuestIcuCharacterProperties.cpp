#include "prx/libSceIcu/CharacterProperties.hpp"
#include "prx/libSceIcu/Set.hpp"
#include <unicode/uchar.h>
#include <unicode/ucpmap.h>
#include <unicode/udata.h>
#include <unicode/uset.h>
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
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IllegalCharFound = 12;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU CHARACTER PROPERTIES: %s\n", message);
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

void CheckCharacterNames() {
    char buffer[128]{};
    std::int32_t status = Ok;
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, buffer, 128, &status) == 22 && status == Ok && std::strcmp(buffer, "LATIN CAPITAL LETTER A") == 0, "a character name");
    status = Ok;
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, nullptr, 0, &status) == 22 && status == BufferOverflow, "preflight");
    status = Ok;
    std::memset(buffer, 'x', sizeof(buffer));
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, buffer, 5, &status) == 22 && status == BufferOverflow, "overflow");
    status = Ok;
    std::memset(buffer, 'x', sizeof(buffer));
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, buffer, 22, &status) == 22 && status == NotTerminated && buffer[22] == 'x', "exact fit is not terminated");
    status = Ok;
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, buffer, -1, &status) == 0 && status == IllegalArgument, "negative capacity");
    status = Ok;
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, nullptr, 4, &status) == 0 && status == IllegalArgument, "null buffer with capacity");
    status = Ok;
    Require(u_charName_nid_postfix(0x41, U_CHAR_NAME_CHOICE_COUNT, buffer, 128, &status) == 0 && status == IllegalArgument, "unknown name choice");
    status = IllegalArgument;
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, buffer, 128, &status) == 0 && status == IllegalArgument, "a failed status stays");
    Require(u_charName_nid_postfix(0x41, U_UNICODE_CHAR_NAME, buffer, 128, nullptr) == 0, "a null status returns 0 as in ICU");
    status = Ok;
    buffer[0] = 'x';
    Require(u_charName_nid_postfix(0x110000, U_UNICODE_CHAR_NAME, buffer, 128, &status) == 0 && status == Ok && buffer[0] == 0, "no name beyond U+10FFFF");
    status = Ok;
    Require(u_charName_nid_postfix(0x9, U_EXTENDED_CHAR_NAME, buffer, 128, &status) > 0 && std::strcmp(buffer, "<control-0009>") == 0, "extended names");
    status = Ok;
    Require(u_charName_nid_postfix(0x4E00, U_UNICODE_CHAR_NAME, buffer, 128, &status) > 0 && std::strcmp(buffer, "CJK UNIFIED IDEOGRAPH-4E00") == 0, "algorithmic names");
    status = Ok;
    Require(u_charName_nid_postfix(0x01A2, U_CHAR_NAME_ALIAS, buffer, 128, &status) > 0 && std::strcmp(buffer, "LATIN CAPITAL LETTER GHA") == 0, "name aliases");

    status = Ok;
    Require(u_charFromName_nid_postfix(U_UNICODE_CHAR_NAME, "LATIN SMALL LETTER A", &status) == 0x61 && status == Ok, "a code point from its name");
    status = Ok;
    Require(u_charFromName_nid_postfix(U_EXTENDED_CHAR_NAME, "<control-0009>", &status) == 9 && status == Ok, "a code point from an extended name");
    status = Ok;
    Require(u_charFromName_nid_postfix(U_UNICODE_CHAR_NAME, "NO SUCH CHARACTER NAME", &status) == 0xFFFF && status == IllegalCharFound, "an unknown name");
    status = Ok;
    Require(u_charFromName_nid_postfix(U_UNICODE_CHAR_NAME, "", &status) == 0xFFFF && status == IllegalArgument, "an empty name");
    status = Ok;
    Require(u_charFromName_nid_postfix(U_UNICODE_CHAR_NAME, nullptr, &status) == 0xFFFF && status == IllegalArgument, "a null name");
    status = Ok;
    Require(u_charFromName_nid_postfix(U_CHAR_NAME_CHOICE_COUNT, "LATIN SMALL LETTER A", &status) == 0xFFFF && status == IllegalArgument, "an unknown name choice");
    Require(u_charFromName_nid_postfix(U_UNICODE_CHAR_NAME, "LATIN SMALL LETTER A", nullptr) == 0xFFFF, "a null status returns U+FFFF as in ICU");

    for (std::int32_t choice = 0; choice < U_CHAR_NAME_CHOICE_COUNT; ++choice) {
        for (std::int32_t c = 0; c <= 0x10FFFF; c += 37) {
            char guest[128]{};
            char host[128]{};
            std::int32_t guestStatus = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            const std::int32_t guestLength = u_charName_nid_postfix(c, choice, guest, 128, &guestStatus);
            const std::int32_t hostLength = u_charName(c, static_cast<UCharNameChoice>(choice), host, 128, &hostStatus);
            Require(guestLength == hostLength && guestStatus == hostStatus && std::strcmp(guest, host) == 0, "character names match ICU");
            if (guestLength > 0) {
                guestStatus = Ok;
                hostStatus = U_ZERO_ERROR;
                Require(u_charFromName_nid_postfix(choice, guest, &guestStatus) == u_charFromName(static_cast<UCharNameChoice>(choice), host, &hostStatus) && guestStatus == hostStatus, "names map back as in ICU");
            }
        }
    }
}

void CheckClosure() {
    const struct {
        std::int32_t c;
        std::u16string closure;
    } expected[] = {{0x00C4, u""}, {0x037A, u" \u03B9"}, {0x20A8, u"rs"}, {0x2121, u"tel"}, {0x1D5DB, u"h"}, {0x0061, u""}};
    for (const auto& item : expected) {
        char16_t buffer[16]{};
        std::int32_t status = Ok;
        const std::int32_t length = u_getFC_NFKC_Closure_nid_postfix(item.c, buffer, 16, &status);
        Require(status == Ok && std::u16string(buffer, static_cast<std::size_t>(length)) == item.closure, "FC_NFKC closures from ICU's cnormtst");
    }
    char16_t buffer[16]{};
    std::int32_t status = Ok;
    Require(u_getFC_NFKC_Closure_nid_postfix(0x2121, nullptr, 0, &status) == 3 && status == BufferOverflow, "closure preflight");
    status = Ok;
    Require(u_getFC_NFKC_Closure_nid_postfix(0x2121, buffer, -1, &status) == 0 && status == IllegalArgument, "negative closure capacity");
    status = Ok;
    Require(u_getFC_NFKC_Closure_nid_postfix(0x2121, nullptr, 3, &status) == 0 && status == IllegalArgument, "null closure buffer");
    status = BufferOverflow;
    Require(u_getFC_NFKC_Closure_nid_postfix(0x2121, buffer, 16, &status) == 0 && status == BufferOverflow, "a failed closure status stays");
    Require(u_getFC_NFKC_Closure_nid_postfix(0x2121, buffer, 16, nullptr) == 0, "a null closure status returns 0 as in ICU");
    for (std::int32_t c = 0; c <= 0x10FFFF; c += 3) {
        char16_t guest[16]{};
        char16_t host[16]{};
        std::int32_t guestStatus = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t guestLength = u_getFC_NFKC_Closure_nid_postfix(c, guest, 16, &guestStatus);
        const std::int32_t hostLength = u_getFC_NFKC_Closure(c, host, 16, &hostStatus);
        Require(guestLength == hostLength && guestStatus == hostStatus && std::memcmp(guest, host, sizeof(guest)) == 0, "closures match ICU");
    }
}


struct NameRecord {
    std::int32_t code;
    std::int32_t choice;
    std::string name;
    bool operator==(const NameRecord&) const = default;
};

struct NameCollector {
    std::vector<NameRecord> records;
    std::size_t stopAfter = static_cast<std::size_t>(-1);
};

std::int8_t APS5_VABI GuestName(void* context, std::int32_t code, std::int32_t choice, const char* name, std::int32_t length) {
    auto* collector = static_cast<NameCollector*>(context);
    collector->records.push_back({code, choice, std::string(name, static_cast<std::size_t>(length))});
    return collector->records.size() < collector->stopAfter;
}

UBool U_CALLCONV HostName(void* context, UChar32 code, UCharNameChoice choice, const char* name, int32_t length) {
    auto* collector = static_cast<NameCollector*>(context);
    collector->records.push_back({code, choice, std::string(name, static_cast<std::size_t>(length))});
    return collector->records.size() < collector->stopAfter;
}

std::vector<NameRecord> GuestNames(std::int32_t start, std::int32_t limit, std::int32_t choice, std::size_t stopAfter = static_cast<std::size_t>(-1)) {
    NameCollector collector;
    collector.stopAfter = stopAfter;
    std::int32_t status = Ok;
    u_enumCharNames_nid_postfix(start, limit, GuestName, &collector, choice, &status);
    Require(status == Ok, "guest name enumeration succeeds");
    return collector.records;
}

std::vector<NameRecord> HostNames(std::int32_t start, std::int32_t limit, std::int32_t choice, std::size_t stopAfter = static_cast<std::size_t>(-1)) {
    NameCollector collector;
    collector.stopAfter = stopAfter;
    UErrorCode status = U_ZERO_ERROR;
    u_enumCharNames(start, limit, HostName, &collector, static_cast<UCharNameChoice>(choice), &status);
    Require(U_SUCCESS(status), "host name enumeration succeeds");
    return collector.records;
}

void CheckNameEnumeration() {
    const std::vector<NameRecord> latin = GuestNames(0x41, 0x44, U_UNICODE_CHAR_NAME);
    Require(latin == std::vector<NameRecord>{{0x41, U_UNICODE_CHAR_NAME, "LATIN CAPITAL LETTER A"}, {0x42, U_UNICODE_CHAR_NAME, "LATIN CAPITAL LETTER B"}, {0x43, U_UNICODE_CHAR_NAME, "LATIN CAPITAL LETTER C"}}, "names of A to C");
    const std::pair<std::int32_t, std::int32_t> ranges[] = {{0, 0x300}, {0x3400, 0x3410}, {0xAC00, 0xAC20}, {0x1F600, 0x1F650}, {0x10FFF0, 0x200000}, {-5, 0x20}};
    for (const auto& [start, limit] : ranges) {
        for (std::int32_t choice = 0; choice < U_CHAR_NAME_CHOICE_COUNT; ++choice) {
            Require(GuestNames(start, limit, choice) == HostNames(start, limit, choice), "enumerated names match ICU");
        }
    }
    Require(GuestNames(0x20, 0x100, U_UNICODE_CHAR_NAME, 5).size() == 5 && GuestNames(0x4E00, 0x4F00, U_UNICODE_CHAR_NAME, 3) == HostNames(0x4E00, 0x4F00, U_UNICODE_CHAR_NAME, 3), "a false callback result stops the enumeration");
    Require(GuestNames(0x50, 0x50, U_UNICODE_CHAR_NAME).empty() && GuestNames(0x60, 0x50, U_UNICODE_CHAR_NAME).empty(), "empty ranges call nothing");

    NameCollector collector;
    std::int32_t status = Ok;
    u_enumCharNames_nid_postfix(0x41, 0x50, nullptr, &collector, U_UNICODE_CHAR_NAME, &status);
    Require(status == IllegalArgument, "a null callback is an illegal argument");
    status = Ok;
    u_enumCharNames_nid_postfix(0x41, 0x50, GuestName, &collector, U_CHAR_NAME_CHOICE_COUNT, &status);
    Require(status == IllegalArgument && collector.records.empty(), "an unknown name choice is an illegal argument");
    status = IllegalArgument;
    u_enumCharNames_nid_postfix(0x41, 0x50, GuestName, &collector, U_UNICODE_CHAR_NAME, &status);
    Require(status == IllegalArgument && collector.records.empty(), "a failed status calls nothing");
    u_enumCharNames_nid_postfix(0x41, 0x50, GuestName, &collector, U_UNICODE_CHAR_NAME, nullptr);
    Require(collector.records.empty(), "a null status calls nothing, as in ICU");
}

std::vector<std::pair<std::int32_t, std::int32_t>> GuestItems(const USet* set) {
    std::vector<std::pair<std::int32_t, std::int32_t>> items;
    const std::int32_t count = uset_getItemCount_nid_postfix(set);
    for (std::int32_t index = 0; index < count; ++index) {
        std::int32_t start = 0;
        std::int32_t end = 0;
        std::int32_t status = Ok;
        Require(uset_getItem_nid_postfix(set, index, &start, &end, nullptr, 0, &status) == 0 && status == Ok, "guest set items are ranges");
        items.emplace_back(start, end);
    }
    return items;
}

std::vector<std::pair<std::int32_t, std::int32_t>> HostItems(const USet* set) {
    std::vector<std::pair<std::int32_t, std::int32_t>> items;
    const std::int32_t count = uset_getItemCount(set);
    for (std::int32_t index = 0; index < count; ++index) {
        UChar32 start = 0;
        UChar32 end = 0;
        UErrorCode status = U_ZERO_ERROR;
        Require(uset_getItem(set, index, &start, &end, nullptr, 0, &status) == 0 && U_SUCCESS(status), "host set items are ranges");
        items.emplace_back(start, end);
    }
    return items;
}

void CheckPropertySets() {
    for (std::int32_t property = UCHAR_BINARY_START; property < UCHAR_BINARY_LIMIT; ++property) {
        std::int32_t guestStatus = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const USet* guest = u_getBinaryPropertySet_nid_postfix(property, &guestStatus);
        const USet* host = u_getBinaryPropertySet(static_cast<UProperty>(property), &hostStatus);
        Require(guestStatus == Ok && U_SUCCESS(hostStatus) && guest && GuestItems(guest) == HostItems(host), "binary property sets match ICU");
        guestStatus = Ok;
        Require(u_getBinaryPropertySet_nid_postfix(property, &guestStatus) == guest, "the same set is returned again");
    }
    std::int32_t status = Ok;
    const USet* alphabetic = u_getBinaryPropertySet_nid_postfix(UCHAR_ALPHABETIC, &status);
    Require(uset_contains_nid_postfix(alphabetic, 0x41) && !uset_contains_nid_postfix(alphabetic, 0x31), "the alphabetic set");
    status = Ok;
    Require(u_getBinaryPropertySet_nid_postfix(UCHAR_BINARY_LIMIT, &status) == nullptr && status == IllegalArgument, "a non-binary property is an illegal argument");
    status = Ok;
    Require(u_getBinaryPropertySet_nid_postfix(-1, &status) == nullptr && status == IllegalArgument, "a negative property is an illegal argument");
    status = BufferOverflow;
    Require(u_getBinaryPropertySet_nid_postfix(UCHAR_ALPHABETIC, &status) == nullptr && status == BufferOverflow, "a failed status returns null");
    Require(Throws<std::invalid_argument>([] { u_getBinaryPropertySet_nid_postfix(UCHAR_ALPHABETIC, nullptr); }), "a null status throws");
}

std::uint32_t APS5_VABI GuestUppercaseFilter(const void* context, std::uint32_t value) {
    return value == *static_cast<const std::uint32_t*>(context) ? 1 : 0;
}

uint32_t U_CALLCONV HostUppercaseFilter(const void* context, uint32_t value) {
    return value == *static_cast<const std::uint32_t*>(context) ? 1 : 0;
}

void CheckPropertyMaps() {
    for (std::int32_t property = UCHAR_INT_START; property < UCHAR_INT_LIMIT; ++property) {
        std::int32_t guestStatus = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const UCPMap* guest = u_getIntPropertyMap_nid_postfix(property, &guestStatus);
        const UCPMap* host = u_getIntPropertyMap(static_cast<UProperty>(property), &hostStatus);
        Require(guestStatus == Ok && U_SUCCESS(hostStatus) && guest, "integer property maps load");
        for (std::int32_t c = -1; c <= 0x110000; c += 11) Require(ucpmap_get_nid_postfix(guest, c) == ucpmap_get(host, c), "map values match ICU");
        for (std::int32_t option = UCPMAP_RANGE_NORMAL; option <= UCPMAP_RANGE_FIXED_ALL_SURROGATES; ++option) {
            std::int32_t start = 0;
            while (true) {
                std::uint32_t guestValue = 0xDEAD;
                uint32_t hostValue = 0xBEEF;
                const std::int32_t guestEnd = ucpmap_getRange_nid_postfix(guest, start, option, 7, nullptr, nullptr, &guestValue);
                const std::int32_t hostEnd = ucpmap_getRange(host, start, static_cast<UCPMapRangeOption>(option), 7, nullptr, nullptr, &hostValue);
                Require(guestEnd == hostEnd && (guestEnd < 0 || guestValue == hostValue), "map ranges match ICU");
                if (guestEnd < 0) break;
                start = guestEnd + 1;
            }
        }
    }
    std::int32_t status = Ok;
    const UCPMap* category = u_getIntPropertyMap_nid_postfix(UCHAR_GENERAL_CATEGORY, &status);
    Require(ucpmap_get_nid_postfix(category, 0x41) == U_UPPERCASE_LETTER && ucpmap_get_nid_postfix(category, 0x110000) == 0, "general category map values");
    const std::uint32_t uppercase = U_UPPERCASE_LETTER;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const UCPMap* hostCategory = u_getIntPropertyMap(UCHAR_GENERAL_CATEGORY, &hostStatus);
    std::int32_t start = 0;
    std::int32_t filteredRanges = 0;
    std::int32_t plainRanges = 0;
    while (true) {
        std::uint32_t guestValue = 0;
        uint32_t hostValue = 0;
        const std::int32_t guestEnd = ucpmap_getRange_nid_postfix(category, start, UCPMAP_RANGE_NORMAL, 0, GuestUppercaseFilter, &uppercase, &guestValue);
        const std::int32_t hostEnd = ucpmap_getRange(hostCategory, start, UCPMAP_RANGE_NORMAL, 0, HostUppercaseFilter, &uppercase, &hostValue);
        Require(guestEnd == hostEnd && (guestEnd < 0 || guestValue == hostValue), "filtered map ranges match ICU");
        if (guestEnd < 0) break;
        Require(guestValue <= 1, "the filter result is the range value");
        start = guestEnd + 1;
        ++filteredRanges;
    }
    for (start = 0; start >= 0; ++plainRanges) {
        const std::int32_t end = ucpmap_getRange_nid_postfix(category, start, UCPMAP_RANGE_NORMAL, 0, nullptr, nullptr, nullptr);
        start = end < 0 ? -1 : end + 1;
    }
    Require(filteredRanges > 100 && filteredRanges < plainRanges, "the filter merges ranges by uppercase or not");
    Require(ucpmap_getRange_nid_postfix(category, 0x41, UCPMAP_RANGE_NORMAL, 0, nullptr, nullptr, nullptr) == 0x5A, "a null value pointer is allowed");
    Require(ucpmap_getRange_nid_postfix(category, 0x110000, UCPMAP_RANGE_NORMAL, 0, nullptr, nullptr, nullptr) == -1, "a start beyond U+10FFFF has no range");
    status = Ok;
    Require(u_getIntPropertyMap_nid_postfix(UCHAR_ALPHABETIC, &status) == nullptr && status == IllegalArgument, "a binary property has no integer map");
    status = Ok;
    Require(u_getIntPropertyMap_nid_postfix(UCHAR_INT_LIMIT, &status) == nullptr && status == IllegalArgument, "a property beyond the integer range has no map");
    status = BufferOverflow;
    Require(u_getIntPropertyMap_nid_postfix(UCHAR_GENERAL_CATEGORY, &status) == nullptr && status == BufferOverflow, "a failed status returns no map");
    Require(Throws<std::invalid_argument>([] { u_getIntPropertyMap_nid_postfix(UCHAR_GENERAL_CATEGORY, nullptr); }), "a null map status throws");
    Require(Throws<std::invalid_argument>([] { ucpmap_get_nid_postfix(nullptr, 0x41); }), "a null map throws in ucpmap_get");
    Require(Throws<std::invalid_argument>([] { ucpmap_getRange_nid_postfix(nullptr, 0, UCPMAP_RANGE_NORMAL, 0, nullptr, nullptr, nullptr); }), "a null map throws in ucpmap_getRange");
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckCharacterNames();
    CheckClosure();
    CheckNameEnumeration();
    CheckPropertySets();
    CheckPropertyMaps();
    return 0;
}

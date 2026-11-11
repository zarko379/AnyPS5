#include "prx/libSceIcu/Properties.hpp"
#include "prx/libSceIcu/Set.hpp"
#include <unicode/uchar.h>
#include <unicode/ucpmap.h>
#include <unicode/udata.h>
#include <unicode/uset.h>
#include <unicode/uscript.h>
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
        std::fprintf(stderr, "ICU PROPERTIES: %s\n", message);
        std::abort();
    }
}

bool SameName(const char* guest, const char* host) {
    if (!guest || !host) return guest == host;
    return std::strcmp(guest, host) == 0;
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

template<typename TCheck>
void Sweep(const char* message, TCheck check) {
    for (std::int32_t c = -2; c <= 0x110001; ++c) {
        if (!check(c)) {
            std::fprintf(stderr, "ICU PROPERTIES: %s differs at %X\n", message, static_cast<unsigned>(c));
            std::abort();
        }
    }
}

void CheckPredicates() {
    Sweep("u_isalnum", [](std::int32_t c) { return u_isalnum_nid_postfix(c) == u_isalnum(c); });
    Sweep("u_isalpha", [](std::int32_t c) { return u_isalpha_nid_postfix(c) == u_isalpha(c); });
    Sweep("u_isbase", [](std::int32_t c) { return u_isbase_nid_postfix(c) == u_isbase(c); });
    Sweep("u_iscntrl", [](std::int32_t c) { return u_iscntrl_nid_postfix(c) == u_iscntrl(c); });
    Sweep("u_isdefined", [](std::int32_t c) { return u_isdefined_nid_postfix(c) == u_isdefined(c); });
    Sweep("u_isdigit", [](std::int32_t c) { return u_isdigit_nid_postfix(c) == u_isdigit(c); });
    Sweep("u_isgraph", [](std::int32_t c) { return u_isgraph_nid_postfix(c) == u_isgraph(c); });
    Sweep("u_islower", [](std::int32_t c) { return u_islower_nid_postfix(c) == u_islower(c); });
    Sweep("u_isprint", [](std::int32_t c) { return u_isprint_nid_postfix(c) == u_isprint(c); });
    Sweep("u_ispunct", [](std::int32_t c) { return u_ispunct_nid_postfix(c) == u_ispunct(c); });
    Sweep("u_istitle", [](std::int32_t c) { return u_istitle_nid_postfix(c) == u_istitle(c); });
    Sweep("u_isupper", [](std::int32_t c) { return u_isupper_nid_postfix(c) == u_isupper(c); });
    Sweep("u_isxdigit", [](std::int32_t c) { return u_isxdigit_nid_postfix(c) == u_isxdigit(c); });
    Sweep("u_isIDIgnorable", [](std::int32_t c) { return u_isIDIgnorable_nid_postfix(c) == u_isIDIgnorable(c); });
    Sweep("u_isIDPart", [](std::int32_t c) { return u_isIDPart_nid_postfix(c) == u_isIDPart(c); });
    Sweep("u_isIDStart", [](std::int32_t c) { return u_isIDStart_nid_postfix(c) == u_isIDStart(c); });
    Sweep("u_isJavaIDPart", [](std::int32_t c) { return u_isJavaIDPart_nid_postfix(c) == u_isJavaIDPart(c); });
    Sweep("u_isJavaIDStart", [](std::int32_t c) { return u_isJavaIDStart_nid_postfix(c) == u_isJavaIDStart(c); });
    Sweep("u_isMirrored", [](std::int32_t c) { return u_isMirrored_nid_postfix(c) == u_isMirrored(c); });
    Sweep("u_isUAlphabetic", [](std::int32_t c) { return u_isUAlphabetic_nid_postfix(c) == u_isUAlphabetic(c); });
    Sweep("u_isULowercase", [](std::int32_t c) { return u_isULowercase_nid_postfix(c) == u_isULowercase(c); });
    Sweep("u_isUUppercase", [](std::int32_t c) { return u_isUUppercase_nid_postfix(c) == u_isUUppercase(c); });

    Require(u_isalpha_nid_postfix(0x41) && !u_isalpha_nid_postfix(0x31) && u_isdigit_nid_postfix(0x0663) && u_isxdigit_nid_postfix(0xFF21), "letters, digits and fullwidth hex digits");
    Require(u_islower_nid_postfix(0x61) && u_isupper_nid_postfix(0x41) && u_istitle_nid_postfix(0x01C5) && !u_istitle_nid_postfix(0x01C4), "case predicates");
    Require(u_isIDIgnorable_nid_postfix(0x200B) && u_isIDIgnorable_nid_postfix(0x0008) && !u_isIDIgnorable_nid_postfix(0x0041), "identifier-ignorable characters");
    Require(u_isJavaIDStart_nid_postfix(0x24) && !u_isIDStart_nid_postfix(0x24) && u_isIDPart_nid_postfix(0x5F), "Java and Unicode identifiers");
    Require(u_isMirrored_nid_postfix(0x28) && !u_isMirrored_nid_postfix(0x41) && u_iscntrl_nid_postfix(0x7F) && u_ispunct_nid_postfix(0x21), "mirrored, control and punctuation");
    Require(u_isdefined_nid_postfix(0x41) && !u_isdefined_nid_postfix(0x0378) && !u_isdefined_nid_postfix(0x110000), "defined code points");
}

void CheckScalars() {
    Sweep("u_charType", [](std::int32_t c) { return u_charType_nid_postfix(c) == u_charType(c); });
    Sweep("u_charDirection", [](std::int32_t c) { return u_charDirection_nid_postfix(c) == u_charDirection(c); });
    Sweep("u_charMirror", [](std::int32_t c) { return u_charMirror_nid_postfix(c) == u_charMirror(c); });
    Sweep("u_getBidiPairedBracket", [](std::int32_t c) { return u_getBidiPairedBracket_nid_postfix(c) == u_getBidiPairedBracket(c); });
    Sweep("u_getCombiningClass", [](std::int32_t c) { return u_getCombiningClass_nid_postfix(c) == u_getCombiningClass(c); });
    Sweep("u_getNumericValue", [](std::int32_t c) { return u_getNumericValue_nid_postfix(c) == u_getNumericValue(c); });
    Sweep("u_foldCase default", [](std::int32_t c) { return u_foldCase_nid_postfix(c, U_FOLD_CASE_DEFAULT) == u_foldCase(c, U_FOLD_CASE_DEFAULT); });
    Sweep("u_foldCase Turkic", [](std::int32_t c) { return u_foldCase_nid_postfix(c, U_FOLD_CASE_EXCLUDE_SPECIAL_I) == u_foldCase(c, U_FOLD_CASE_EXCLUDE_SPECIAL_I); });
    Sweep("u_tolower", [](std::int32_t c) { return u_tolower_nid_postfix(c) == u_tolower(c); });
    Sweep("u_toupper", [](std::int32_t c) { return u_toupper_nid_postfix(c) == u_toupper(c); });
    Sweep("u_totitle", [](std::int32_t c) { return u_totitle_nid_postfix(c) == u_totitle(c); });
    Sweep("ublock_getCode", [](std::int32_t c) { return ublock_getCode_nid_postfix(c) == ublock_getCode(c); });
    Sweep("u_charAge", [](std::int32_t c) {
        std::uint8_t guest[4]{9, 9, 9, 9};
        UVersionInfo host{7, 7, 7, 7};
        u_charAge_nid_postfix(c, guest);
        u_charAge(c, host);
        return std::memcmp(guest, host, 4) == 0;
    });

    Require(u_charType_nid_postfix(0x41) == U_UPPERCASE_LETTER && u_charType_nid_postfix(0x0378) == U_UNASSIGNED && u_charType_nid_postfix(0x110000) == U_UNASSIGNED, "general categories");
    Require(u_charDirection_nid_postfix(0x05D0) == U_RIGHT_TO_LEFT && u_charDirection_nid_postfix(0x0627) == U_RIGHT_TO_LEFT_ARABIC && u_charDirection_nid_postfix(0x41) == U_LEFT_TO_RIGHT, "bidi classes");
    Require(u_charMirror_nid_postfix(0x28) == 0x29 && u_charMirror_nid_postfix(0x2264) == 0x2265 && u_charMirror_nid_postfix(0x41) == 0x41, "mirrored code points");
    Require(u_getBidiPairedBracket_nid_postfix(0x5B) == 0x5D && u_getBidiPairedBracket_nid_postfix(0x2264) == 0x2264, "paired brackets");
    Require(u_getCombiningClass_nid_postfix(0x0301) == 230 && u_getCombiningClass_nid_postfix(0x0334) == 1 && u_getCombiningClass_nid_postfix(0x41) == 0, "combining classes");
    Require(u_getNumericValue_nid_postfix(0x00BD) == 0.5 && u_getNumericValue_nid_postfix(0x216B) == 12.0 && u_getNumericValue_nid_postfix(0x61) == U_NO_NUMERIC_VALUE, "numeric values");
    Require(u_foldCase_nid_postfix(0x49, U_FOLD_CASE_DEFAULT) == 0x69 && u_foldCase_nid_postfix(0x49, U_FOLD_CASE_EXCLUDE_SPECIAL_I) == 0x131 && u_foldCase_nid_postfix(0x130, U_FOLD_CASE_DEFAULT) == 0x130, "simple case folding with and without Turkic I");
    Require(u_tolower_nid_postfix(0x41) == 0x61 && u_toupper_nid_postfix(0xFF) == 0x178 && u_totitle_nid_postfix(0x01C6) == 0x01C5 && u_toupper_nid_postfix(0x01C6) == 0x01C4, "simple case mappings");
    Require(ublock_getCode_nid_postfix(0x41) == UBLOCK_BASIC_LATIN && ublock_getCode_nid_postfix(0x0400) == UBLOCK_CYRILLIC && ublock_getCode_nid_postfix(0x110000) == UBLOCK_NO_BLOCK, "blocks");
    std::uint8_t age[4]{};
    u_charAge_nid_postfix(0x20AC, age);
    Require(age[0] == 2 && age[1] == 1 && age[2] == 0 && age[3] == 0, "the euro sign is from Unicode 2.1");
    u_charAge_nid_postfix(0x41, nullptr);
}

const std::vector<std::int32_t>& PropertyIds() {
    static const std::vector<std::int32_t> ids = [] {
        std::vector<std::int32_t> list{-1, UCHAR_BINARY_LIMIT, UCHAR_INT_LIMIT, UCHAR_GENERAL_CATEGORY_MASK, UCHAR_NUMERIC_VALUE, UCHAR_AGE, UCHAR_SCRIPT_EXTENSIONS, 0x8000};
        for (std::int32_t id = UCHAR_BINARY_START; id < UCHAR_BINARY_LIMIT; ++id) list.push_back(id);
        for (std::int32_t id = UCHAR_INT_START; id < UCHAR_INT_LIMIT; ++id) list.push_back(id);
        return list;
    }();
    return ids;
}

void CheckIntProperties() {
    for (const std::int32_t id : PropertyIds()) {
        Require(u_getIntPropertyMinValue_nid_postfix(id) == u_getIntPropertyMinValue(static_cast<UProperty>(id)), "minimum values match ICU");
        Require(u_getIntPropertyMaxValue_nid_postfix(id) == u_getIntPropertyMaxValue(static_cast<UProperty>(id)), "maximum values match ICU");
        for (std::int32_t c = -1; c <= 0x110000; c += 7) {
            if (u_getIntPropertyValue_nid_postfix(c, id) != u_getIntPropertyValue(c, static_cast<UProperty>(id))) {
                std::fprintf(stderr, "ICU PROPERTIES: u_getIntPropertyValue %d differs at %X\n", id, static_cast<unsigned>(c));
                std::abort();
            }
            if (u_hasBinaryProperty_nid_postfix(c, id) != u_hasBinaryProperty(c, static_cast<UProperty>(id))) {
                std::fprintf(stderr, "ICU PROPERTIES: u_hasBinaryProperty %d differs at %X\n", id, static_cast<unsigned>(c));
                std::abort();
            }
        }
    }
    Require(u_hasBinaryProperty_nid_postfix(0x41, UCHAR_ALPHABETIC) && !u_hasBinaryProperty_nid_postfix(0x41, UCHAR_WHITE_SPACE) && !u_hasBinaryProperty_nid_postfix(0x41, -1), "binary properties");
    Require(u_hasBinaryProperty_nid_postfix(0x1F600, UCHAR_EMOJI) && u_hasBinaryProperty_nid_postfix(0x00E0, UCHAR_CHANGES_WHEN_NFKC_CASEFOLDED) == 0, "emoji and normalization-based properties");
    Require(u_getIntPropertyValue_nid_postfix(0x41, UCHAR_GENERAL_CATEGORY) == U_UPPERCASE_LETTER && u_getIntPropertyValue_nid_postfix(0x41, UCHAR_GENERAL_CATEGORY_MASK) == U_GC_LU_MASK, "general category and its mask");
    Require(u_getIntPropertyValue_nid_postfix(0x0915, UCHAR_SCRIPT) == USCRIPT_DEVANAGARI && u_getIntPropertyValue_nid_postfix(0x41, UCHAR_ALPHABETIC) == 1, "script and a binary property as an integer");
    Require(u_getIntPropertyValue_nid_postfix(0x41, UCHAR_INT_LIMIT) == 0 && u_getIntPropertyValue_nid_postfix(0x41, UCHAR_NUMERIC_VALUE) == 0, "out-of-range and double properties give 0");
    Require(u_getIntPropertyMaxValue_nid_postfix(UCHAR_BIDI_PAIRED_BRACKET_TYPE) == U_BPT_CLOSE && u_getIntPropertyMinValue_nid_postfix(UCHAR_SCRIPT) == 0 && u_getIntPropertyMaxValue_nid_postfix(UCHAR_ALPHABETIC) == 1, "property ranges");
    Require(u_getIntPropertyMaxValue_nid_postfix(-1) == -1 && u_getIntPropertyMaxValue_nid_postfix(UCHAR_INT_LIMIT) == -1, "unknown properties have maximum -1");
}

void CheckNames() {
    Require(u_getPropertyEnum_nid_postfix("Alphabetic") == UCHAR_ALPHABETIC && u_getPropertyEnum_nid_postfix("gc") == UCHAR_GENERAL_CATEGORY && u_getPropertyEnum_nid_postfix("line_break") == UCHAR_LINE_BREAK, "property enums with loose matching");
    Require(u_getPropertyEnum_nid_postfix("NoSuchProperty") == UCHAR_INVALID_CODE && u_getPropertyEnum_nid_postfix(nullptr) == UCHAR_INVALID_CODE, "unknown property names");
    Require(std::strcmp(u_getPropertyName_nid_postfix(UCHAR_ALPHABETIC, U_SHORT_PROPERTY_NAME), "Alpha") == 0 && std::strcmp(u_getPropertyName_nid_postfix(UCHAR_ALPHABETIC, U_LONG_PROPERTY_NAME), "Alphabetic") == 0, "property names");
    Require(u_getPropertyName_nid_postfix(UCHAR_INT_LIMIT, U_SHORT_PROPERTY_NAME) == nullptr && u_getPropertyName_nid_postfix(UCHAR_ALPHABETIC, 9) == nullptr, "missing property names are null");
    Require(u_getPropertyValueEnum_nid_postfix(UCHAR_GENERAL_CATEGORY, "Lu") == U_UPPERCASE_LETTER && u_getPropertyValueEnum_nid_postfix(UCHAR_SCRIPT, "Latn") == USCRIPT_LATIN, "property value enums");
    Require(u_getPropertyValueEnum_nid_postfix(UCHAR_SCRIPT, "Nope") == UCHAR_INVALID_CODE && u_getPropertyValueEnum_nid_postfix(UCHAR_INT_LIMIT, "Lu") == UCHAR_INVALID_CODE && u_getPropertyValueEnum_nid_postfix(UCHAR_SCRIPT, nullptr) == UCHAR_INVALID_CODE, "unknown property values");
    Require(std::strcmp(u_getPropertyValueName_nid_postfix(UCHAR_SCRIPT, USCRIPT_LATIN, U_SHORT_PROPERTY_NAME), "Latn") == 0 && std::strcmp(u_getPropertyValueName_nid_postfix(UCHAR_GENERAL_CATEGORY, U_UPPERCASE_LETTER, U_LONG_PROPERTY_NAME), "Uppercase_Letter") == 0, "property value names");
    Require(u_getPropertyValueName_nid_postfix(UCHAR_SCRIPT, -5, U_SHORT_PROPERTY_NAME) == nullptr, "missing value names are null");
    for (const std::int32_t id : PropertyIds()) {
        for (std::int32_t choice = 0; choice < 3; ++choice) {
            Require(SameName(u_getPropertyName_nid_postfix(id, choice), u_getPropertyName(static_cast<UProperty>(id), static_cast<UPropertyNameChoice>(choice))), "property names are ICU's");
            for (std::int32_t value = -1; value < 40; ++value) {
                const char* name = u_getPropertyValueName_nid_postfix(id, value, choice);
                Require(SameName(name, u_getPropertyValueName(static_cast<UProperty>(id), value, static_cast<UPropertyNameChoice>(choice))), "property value names are ICU's");
                if (name) Require(u_getPropertyValueEnum_nid_postfix(id, name) == u_getPropertyValueEnum(static_cast<UProperty>(id), name), "value names map back as in ICU");
            }
            const char* name = u_getPropertyName_nid_postfix(id, choice);
            if (name) Require(u_getPropertyEnum_nid_postfix(name) == u_getPropertyEnum(name), "property names map back as in ICU");
        }
    }
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
    } expected[] = {{0x00C4, u""}, {0x037A, u" ι"}, {0x20A8, u"rs"}, {0x2121, u"tel"}, {0x1D5DB, u"h"}, {0x0061, u""}};
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

struct TypeRange {
    std::int32_t start;
    std::int32_t limit;
    std::int32_t type;
    bool operator==(const TypeRange&) const = default;
};

struct TypeCollector {
    std::vector<TypeRange> ranges;
    std::size_t stopAfter = static_cast<std::size_t>(-1);
};

std::int8_t APS5_VABI GuestTypes(const void* context, std::int32_t start, std::int32_t limit, std::int32_t type) {
    auto* collector = const_cast<TypeCollector*>(static_cast<const TypeCollector*>(context));
    collector->ranges.push_back({start, limit, type});
    return collector->ranges.size() < collector->stopAfter;
}

UBool U_CALLCONV HostTypes(const void* context, UChar32 start, UChar32 limit, UCharCategory type) {
    auto* collector = const_cast<TypeCollector*>(static_cast<const TypeCollector*>(context));
    collector->ranges.push_back({start, limit, type});
    return collector->ranges.size() < collector->stopAfter;
}

void CheckTypeEnumeration() {
    TypeCollector guest;
    TypeCollector host;
    u_enumCharTypes_nid_postfix(GuestTypes, &guest);
    u_enumCharTypes(HostTypes, &host);
    Require(!guest.ranges.empty() && guest.ranges == host.ranges, "category ranges match ICU");
    Require(guest.ranges.front().start == 0 && guest.ranges.back().limit == 0x110000 && guest.ranges.front().type == U_CONTROL_CHAR, "category ranges cover the code space");
    TypeCollector stopped;
    stopped.stopAfter = 4;
    u_enumCharTypes_nid_postfix(GuestTypes, &stopped);
    Require(stopped.ranges.size() == 4, "a false callback result stops the category enumeration");
    u_enumCharTypes_nid_postfix(nullptr, &stopped);
    Require(stopped.ranges.size() == 4, "a null callback does nothing");
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
    CheckPredicates();
    CheckScalars();
    CheckIntProperties();
    CheckNames();
    CheckCharacterNames();
    CheckClosure();
    CheckNameEnumeration();
    CheckTypeEnumeration();
    CheckPropertySets();
    CheckPropertyMaps();
    return 0;
}

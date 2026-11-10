#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU C string: %s\n", message);
    std::abort();
}

template<class TCall>
bool Rejects(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<class TValue>
void CheckFormat(TValue value, int radix) {
    std::array<char, 80> expected{};
    const auto converted = radix == 10
        ? std::to_chars(expected.data(), expected.data() + expected.size(), value, radix)
        : std::to_chars(expected.data(), expected.data() + expected.size(), static_cast<std::make_unsigned_t<TValue>>(value), radix);
    Require(converted.ec == std::errc{}, "standard formatting oracle succeeds");
    for (char* cursor = expected.data(); cursor != converted.ptr; ++cursor)
        if (*cursor >= 'a' && *cursor <= 'f') *cursor = static_cast<char>(*cursor - ('a' - 'A'));
    const std::string_view wanted(expected.data(), converted.ptr - expected.data());
    std::array<char, 80> actual;
    actual.fill('@');
    std::int32_t length;
    if constexpr (sizeof(TValue) == 4) length = T_CString_integerToString_nid_postfix(actual.data() + 1, value, radix);
    else length = T_CString_int64ToString_nid_postfix(actual.data() + 1, value, static_cast<std::uint32_t>(radix));
    Require(length == static_cast<std::int32_t>(wanted.size()), "format length excludes the terminator");
    Require(std::string_view(actual.data() + 1, wanted.size()) == wanted, "format matches independent standard oracle");
    Require(actual[1 + wanted.size()] == 0, "format terminates the string");
    Require(actual.front() == '@', "format preserves the prefix canary");
    Require(std::all_of(actual.begin() + wanted.size() + 2, actual.end(), [](char value) { return value == '@'; }), "format preserves the suffix canary");
}

void Formatting() {
    constexpr std::array<std::int32_t, 9> values32{0, 1, -1, 15, -15, 431, -431, INT32_MIN, INT32_MAX};
    constexpr std::array<std::int64_t, 11> values64{0, 1, -1, 15, -15, 431, -431, INT32_MIN, INT32_MAX, INT64_MIN, INT64_MAX};
    for (int radix = 2; radix <= 16; ++radix) {
        for (auto value : values32) CheckFormat(value, radix);
        for (auto value : values64) CheckFormat(value, radix);
    }
    std::uint64_t random = 0x18cb3a2ef4796d05;
    for (int sample = 0; sample < 1024; ++sample) {
        random ^= random << 13;
        random ^= random >> 7;
        random ^= random << 17;
        for (int radix = 2; radix <= 16; ++radix) {
            CheckFormat(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(random)), radix);
            CheckFormat(std::bit_cast<std::int64_t>(random), radix);
        }
    }
}

void InvalidFormatting() {
    std::array<char, 80> buffer;
    buffer.fill('@');
    for (int radix : {-1, 0, 1, 17, 36, INT32_MAX}) {
        Require(Rejects([&] { T_CString_integerToString_nid_postfix(buffer.data(), 42, radix); }), "invalid 32-bit format radix throws");
        Require(Rejects([&] { T_CString_int64ToString_nid_postfix(buffer.data(), 42, static_cast<std::uint32_t>(radix)); }), "invalid 64-bit format radix throws");
        Require(std::all_of(buffer.begin(), buffer.end(), [](char value) { return value == '@'; }), "invalid radix leaves the buffer unchanged");
    }
    Require(Rejects([] { T_CString_integerToString_nid_postfix(nullptr, 42, 10); }), "null 32-bit format buffer throws");
    Require(Rejects([] { T_CString_int64ToString_nid_postfix(nullptr, 42, 10); }), "null 64-bit format buffer throws");
}

void Parsing() {
    struct Case {
        const char* text;
        int radix;
        std::int32_t value;
        int error;
    };
    constexpr int SavedError = 1234;
    constexpr Case cases[] = {
        {"", 10, 0, SavedError},
        {"words", 10, 0, SavedError},
        {" +", 10, 0, SavedError},
        {"0", 10, 0, SavedError},
        {" \t\n\r\f\v-42tail", 10, -42, SavedError},
        {"+42", 10, 42, SavedError},
        {"2147483647", 10, INT32_MAX, SavedError},
        {"2147483648", 10, INT32_MIN, SavedError},
        {"-2147483648", 10, INT32_MIN, SavedError},
        {"4294967295", 10, -1, SavedError},
        {"4294967296", 10, 0, SavedError},
        {"4294967297", 10, 1, SavedError},
        {"-4294967297", 10, -1, SavedError},
        {"9223372036854775808", 10, 0, SavedError},
        {"18446744073709551615", 10, -1, SavedError},
        {"-18446744073709551615", 10, 1, SavedError},
        {"18446744073709551616", 10, -1, 34},
        {"-18446744073709551616", 10, -1, 34},
        {"999999999999999999999999999999", 10, -1, 34},
        {"101101tail", 2, 45, SavedError},
        {"0b11", 0, 0, SavedError},
        {"0b11", 2, 0, SavedError},
        {"077", 0, 63, SavedError},
        {"077", 10, 77, SavedError},
        {"0x2A", 0, 42, SavedError},
        {"0X2a", 16, 42, SavedError},
        {"0x", 16, 0, SavedError},
        {"FFFFFFFF", 16, -1, SavedError},
        {"100000001", 16, 1, SavedError},
        {"zZ", 36, 1295, SavedError},
        {"42", 1, 0, 22},
        {"42", 37, 0, 22},
        {"42", -1, 0, 22},
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = SavedError;
        const auto actual = T_CString_stringToInteger_nid_postfix(test.text, test.radix);
        const int error = *__error_nid_postfix();
        Require(actual == test.value, "parser preserves LP64 strtoul results before narrowing");
        Require(error == test.error, "parser preserves strtoul error behavior");
    }
    for (int radix = 2; radix <= 36; ++radix) {
        for (std::uint64_t value : std::array<std::uint64_t, 8>{0, 1, 35, 431, UINT32_MAX, std::uint64_t{UINT32_MAX} + 1, INT64_MAX, UINT64_MAX}) {
            std::array<char, 80> text{};
            const auto converted = std::to_chars(text.data(), text.data() + text.size() - 1, value, radix);
            Require(converted.ec == std::errc{}, "standard parse input generation succeeds");
            const auto expected = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
            Require(T_CString_stringToInteger_nid_postfix(text.data(), radix) == expected, "parser accepts every supported radix");
        }
    }
    Require(Rejects([] { T_CString_stringToInteger_nid_postfix(nullptr, 10); }), "null parse source throws");
}

void CaseConversion() {
    Require(T_CString_toLowerCase_nid_postfix(nullptr) == nullptr, "null lowercase input returns null");
    Require(T_CString_toUpperCase_nid_postfix(nullptr) == nullptr, "null uppercase input returns null");
    for (int value = 0; value <= 255; ++value) {
        const char input = static_cast<char>(value);
        std::array<char, 4> lower{input, 0, 'A', 'Z'};
        std::array<char, 4> upper{input, 0, 'a', 'z'};
        Require(T_CString_toLowerCase_nid_postfix(lower.data()) == lower.data(), "lowercase returns the original pointer");
        Require(T_CString_toUpperCase_nid_postfix(upper.data()) == upper.data(), "uppercase returns the original pointer");
        Require(lower[0] == (value >= 65 && value <= 90 ? static_cast<char>(value + 32) : input), "only ASCII uppercase letters become lowercase");
        Require(upper[0] == (value >= 97 && value <= 122 ? static_cast<char>(value - 32) : input), "only ASCII lowercase letters become uppercase");
        Require(lower[1] == 0 && lower[2] == 'A' && lower[3] == 'Z', "lowercase stops at the terminator");
        Require(upper[1] == 0 && upper[2] == 'a' && upper[3] == 'z', "uppercase stops at the terminator");
    }
    std::string mixed = "aBcD-zZ [\\]^_` @09\tI-i";
    T_CString_toLowerCase_nid_postfix(mixed.data());
    Require(mixed == "abcd-zz [\\]^_` @09\ti-i", "lowercase traverses complete strings");
    T_CString_toUpperCase_nid_postfix(mixed.data());
    Require(mixed == "ABCD-ZZ [\\]^_` @09\tI-I", "uppercase traverses complete strings");
}

int FoldedByte(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? value + 32 : value;
}

void CharacterHelpers() {
    for (int value = 0; value <= 255; ++value) {
        const char input = static_cast<char>(value);
        const bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        Require(uprv_isASCIILetter_nid_postfix(input) == (letter ? 1 : 0), "only ASCII letters are letters");
        Require(uprv_toupper_nid_postfix(input) == (value >= 'a' && value <= 'z' ? static_cast<char>(value - 32) : input), "only ASCII lowercase letters become uppercase");
        Require(uprv_asciitolower_nid_postfix(input) == (value >= 'A' && value <= 'Z' ? static_cast<char>(value + 32) : input), "only ASCII uppercase letters become lowercase");
        const bool ebcdicUpper = (value >= 0xc1 && value <= 0xc9) || (value >= 0xd1 && value <= 0xd9) || (value >= 0xe2 && value <= 0xe9);
        Require(uprv_ebcdictolower_nid_postfix(input) == (ebcdicUpper ? static_cast<char>(value - 0x40) : input), "only EBCDIC uppercase letters become lowercase");
    }
    Require(uprv_ebcdictolower_nid_postfix(static_cast<char>(0xc1)) == static_cast<char>(0x81), "EBCDIC A becomes a");
    Require(uprv_ebcdictolower_nid_postfix(static_cast<char>(0xe9)) == static_cast<char>(0xa9), "EBCDIC Z becomes z");
    Require(uprv_ebcdictolower_nid_postfix(static_cast<char>(0xe1)) == static_cast<char>(0xe1), "EBCDIC gap before S is kept");
    Require(uprv_ebcdictolower_nid_postfix(static_cast<char>(0xd0)) == static_cast<char>(0xd0), "EBCDIC gap before J is kept");
    Require(uprv_ebcdictolower_nid_postfix(static_cast<char>(0x81)) == static_cast<char>(0x81), "EBCDIC lowercase is kept");
    Require(uprv_isASCIILetter_nid_postfix(static_cast<char>(0xc1)) == 0, "EBCDIC letters are not ASCII letters");
}

void CaselessComparison() {
    Require(uprv_stricmp_nid_postfix(nullptr, nullptr) == 0, "two null strings are equal");
    Require(uprv_stricmp_nid_postfix(nullptr, "a") == -1, "a null first string is smaller");
    Require(uprv_stricmp_nid_postfix("a", nullptr) == 1, "a null second string is larger");
    Require(uprv_strnicmp_nid_postfix(nullptr, nullptr, 5) == 0, "two null prefixes are equal");
    Require(uprv_strnicmp_nid_postfix(nullptr, "a", 0) == -1, "nulls are checked before the count");
    Require(uprv_strnicmp_nid_postfix("a", nullptr, 0) == 1, "nulls are checked before the count");
    for (int first = 1; first <= 255; ++first) {
        for (int second = 1; second <= 255; ++second) {
            const char left[] = {static_cast<char>(first), 'x', 0};
            const char right[] = {static_cast<char>(second), 'y', 0};
            const int expected = FoldedByte(static_cast<unsigned char>(first)) - FoldedByte(static_cast<unsigned char>(second));
            Require(uprv_stricmp_nid_postfix(left, right) == (expected != 0 ? expected : 'x' - 'y'), "bytes compare unsigned after ASCII lowercasing");
            Require(uprv_strnicmp_nid_postfix(left, right, 1) == expected, "a one-byte prefix compares the first byte");
        }
    }
    Require(uprv_stricmp_nid_postfix("HeLLo", "hEllO") == 0, "ASCII case is ignored");
    Require(uprv_stricmp_nid_postfix("", "") == 0, "empty strings are equal");
    Require(uprv_stricmp_nid_postfix("ab", "ABC") == -1, "a shorter first string returns -1");
    Require(uprv_stricmp_nid_postfix("ABC", "ab") == 1, "a shorter second string returns 1");
    Require(uprv_stricmp_nid_postfix("", "\xff") == -1, "an empty first string returns -1 whatever follows");
    Require(uprv_stricmp_nid_postfix("A", "[") == 'a' - '[', "uppercase letters compare as lowercase");
    Require(uprv_stricmp_nid_postfix("\xc1", "\x81") == 0xc1 - 0x81, "EBCDIC letters are not folded");
    Require(uprv_stricmp_nid_postfix("\xe9", "a") == 0xe9 - 'a', "high bytes compare as unsigned");
    Require(uprv_strnicmp_nid_postfix("abX", "ABy", 0) == 0, "a zero count compares nothing");
    Require(uprv_strnicmp_nid_postfix("abX", "ABy", 2) == 0, "differences after the count are ignored");
    Require(uprv_strnicmp_nid_postfix("abX", "ABy", 3) == 'x' - 'y', "the last counted byte is compared");
    Require(uprv_strnicmp_nid_postfix("ab", "ABC", 3) == -1, "a terminator inside the count returns -1");
    Require(uprv_strnicmp_nid_postfix("abc", "AB", 3) == 1, "a terminator inside the count returns 1");
    Require(uprv_strnicmp_nid_postfix("ab", "AB", std::numeric_limits<std::uint32_t>::max()) == 0, "equal strings stop at the terminator");
    Require(uprv_strnicmp_nid_postfix("ab", "AC", std::numeric_limits<std::uint32_t>::max()) == 'b' - 'c', "the largest count acts as unlimited");
}

}

int main() {
    Formatting();
    InvalidFormatting();
    Parsing();
    CaseConversion();
    CharacterHelpers();
    CaselessComparison();
}

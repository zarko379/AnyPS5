#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
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
    Require(Rejects([] { T_CString_integerToString_nid_postfix(reinterpret_cast<char*>(0x10), 42, 10); }), "unwritable 32-bit format buffer throws");
    Require(Rejects([] { T_CString_int64ToString_nid_postfix(nullptr, 42, 10); }), "null 64-bit format buffer throws");
    Require(Rejects([] { T_CString_int64ToString_nid_postfix(reinterpret_cast<char*>(0x10), 42, 10); }), "unwritable 64-bit format buffer throws");
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
    constexpr Case staleErrors[] = {
        {"-18446744073709551615", 10, 1, ERANGE},
        {"42", 10, 42, ERANGE},
        {"0", 10, 0, ERANGE},
        {"", 10, 0, ERANGE},
        {"words", 10, 0, ERANGE},
        {" +", 10, 0, ERANGE}
    };
    for (const auto& test : staleErrors) {
        *__error_nid_postfix() = test.error;
        Require(T_CString_stringToInteger_nid_postfix(test.text, test.radix) == test.value, "incoming ERANGE does not saturate valid or absent integers");
        Require(*__error_nid_postfix() == test.error, "valid and absent integers preserve incoming ERANGE");
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

void IntegerToUnits() {
    std::array<char16_t, 16> buffer{};
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 0xbeef, 16, 0) == 4, "hex length");
    Require(std::u16string_view(buffer.data(), 5) == std::u16string_view(u"BEEF\0", 5), "hex digits are uppercase and terminated");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 42, 10, 5) == 5, "zero padding counts");
    Require(std::u16string_view(buffer.data(), 6) == std::u16string_view(u"00042\0", 6), "padding goes before the digits");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 0, 10, 0) == 1 && buffer[0] == u'0' && buffer[1] == 0, "zero writes one digit");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 0xffffffffu, 2, 0) == 16 && buffer[0] == u'1' && buffer[15] == u'1', "digits stop at the capacity");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 3, 12345, 10, 0) == 3, "no terminator at the capacity");
    Require(std::u16string_view(buffer.data(), 4) == std::u16string_view(u"345#", 4), "the low digits are kept in reading order");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 0, 7, 10, 0) == 1 && buffer[0] == u'7' && buffer[1] == u'#', "a zero capacity still writes one digit");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), -4, 98, 10, 3) == 3, "padding ignores the capacity");
    Require(std::u16string_view(buffer.data(), 4) == std::u16string_view(u"008#", 4), "a negative capacity writes one digit and the padding");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 35, 36, 0) == 1 && buffer[0] == u'Z', "base 36 uses uppercase letters");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 36, 37, 0) == 1 && buffer[0] == u'[', "digits past Z continue in code order");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 4, 5, 1, 0) == 4 && std::u16string_view(buffer.data(), 5) == std::u16string_view(u"0000#", 5), "radix one stops at the capacity");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 0xfffffff0u, 0xfffffff1u, 0) == 1 && buffer[0] == static_cast<char16_t>(0xfffffff0u + 0x30u), "a digit above INT32_MAX wraps as ICU's int does");
    buffer.fill(u'#');
    Require(uprv_itou_nid_postfix(buffer.data(), 16, 0x7ffffff0u, 0x7ffffff1u, 0) == 1 && buffer[0] == static_cast<char16_t>(0x7ffffff0u + 0x37u), "a large positive digit adds the letter offset");
    Require(Rejects([&] { uprv_itou_nid_postfix(buffer.data(), 16, 1, 0, 0); }), "radix zero rejected");
    Require(Rejects([] { uprv_itou_nid_postfix(nullptr, 0, 1, 10, 0); }), "null buffer rejected");
}

UHashTok Pointer(const void* pointer) {
    UHashTok key{};
    key.pointer = const_cast<void*>(pointer);
    return key;
}

UHashTok Integer(std::int32_t value, std::uint32_t high) {
    UHashTok key{};
    key.pointer = reinterpret_cast<void*>((static_cast<std::uintptr_t>(high) << 32) | static_cast<std::uint32_t>(value));
    return key;
}

void HashTableKeys() {
    const char narrow[] = "Hello, World";
    const char16_t wide[] = u"Hello, World";
    Require(uhash_hashChars_nid_postfix(Pointer(narrow)) == ustr_hashCharsN_nid_postfix(narrow, 12), "byte key hash covers the whole string");
    Require(uhash_hashIChars_nid_postfix(Pointer(narrow)) == ustr_hashICharsN_nid_postfix(narrow, 12), "caseless key hash covers the whole string");
    Require(uhash_hashUChars_nid_postfix(Pointer(wide)) == ustr_hashUCharsN_nid_postfix(reinterpret_cast<const std::uint16_t*>(wide), 12), "unit key hash covers the whole string");
    Require(uhash_hashIChars_nid_postfix(Pointer("HELLO, WORLD")) == uhash_hashChars_nid_postfix(Pointer("hello, world")), "caseless key hash folds ASCII");
    Require(uhash_hashChars_nid_postfix(Pointer("")) == 0 && uhash_hashUChars_nid_postfix(Pointer(u"")) == 0, "empty keys hash to zero");
    Require(uhash_hashChars_nid_postfix(Pointer(nullptr)) == 0 && uhash_hashIChars_nid_postfix(Pointer(nullptr)) == 0 && uhash_hashUChars_nid_postfix(Pointer(nullptr)) == 0, "null keys hash to zero");
    std::array<char, 101> longText{};
    longText.fill('q');
    longText[100] = 0;
    Require(uhash_hashChars_nid_postfix(Pointer(longText.data())) == ustr_hashCharsN_nid_postfix(longText.data(), 100), "long keys use the sampled hash");
    Require(uhash_hashLong_nid_postfix(Integer(-5, 0xdeadbeef)) == -5, "integer hash is the low 32 bits");
    Require(uhash_compareLong_nid_postfix(Integer(7, 1), Integer(7, 2)) == 1, "integer keys compare the low 32 bits");
    Require(uhash_compareLong_nid_postfix(Integer(7, 1), Integer(8, 1)) == 0, "different integers differ");
    Require(uhash_compareChars_nid_postfix(Pointer(narrow), Pointer("Hello, World")) == 1, "equal byte keys");
    Require(uhash_compareChars_nid_postfix(Pointer(narrow), Pointer("hello, world")) == 0, "byte keys are case sensitive");
    Require(uhash_compareChars_nid_postfix(Pointer("ab"), Pointer("abc")) == 0 && uhash_compareChars_nid_postfix(Pointer("abc"), Pointer("ab")) == 0, "prefixes differ");
    Require(uhash_compareIChars_nid_postfix(Pointer(narrow), Pointer("hELLO, wORLD")) == 1, "caseless keys fold ASCII");
    Require(uhash_compareIChars_nid_postfix(Pointer("ab"), Pointer("AB\x01")) == 0, "caseless prefixes differ");
    Require(uhash_compareIChars_nid_postfix(Pointer("\xc1"), Pointer("\x81")) == 0, "caseless keys fold only ASCII");
    Require(uhash_compareIChars_nid_postfix(Pointer("["), Pointer("{")) == 0, "brackets are not letters");
    Require(uhash_compareUChars_nid_postfix(Pointer(wide), Pointer(u"Hello, World")) == 1, "equal unit keys");
    Require(uhash_compareUChars_nid_postfix(Pointer(u"a\xffff"), Pointer(u"a\xfffe")) == 0, "unit keys compare every unit");
    Require(uhash_compareUChars_nid_postfix(Pointer(u"ab"), Pointer(u"a")) == 0, "unit prefixes differ");
    for (const auto compare : {uhash_compareChars_nid_postfix, uhash_compareIChars_nid_postfix, uhash_compareUChars_nid_postfix}) {
        Require(compare(Pointer(nullptr), Pointer(nullptr)) == 1, "two null keys are equal");
        Require(compare(Pointer(nullptr), Pointer(u"")) == 0 && compare(Pointer(u""), Pointer(nullptr)) == 0, "one null key differs");
        const char unreadable = 'x';
        Require(compare(Pointer(&unreadable), Pointer(&unreadable)) == 1, "identical pointers are equal without reading");
    }
}

void CharacterHelpers() {
    for (int value = 0; value <= 255; ++value) {
        const char input = static_cast<char>(value);
        const bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        Require(uprv_isASCIILetter_nid_postfix(input) == (letter ? 1 : 0), "only ASCII letters are letters");
    }
    Require(uprv_isASCIILetter_nid_postfix(static_cast<char>(0xc1)) == 0, "EBCDIC letters are not ASCII letters");
}


}

int main() {
    Formatting();
    InvalidFormatting();
    Parsing();
    CaseConversion();
    CharacterHelpers();
    IntegerToUnits();
    HashTableKeys();
}

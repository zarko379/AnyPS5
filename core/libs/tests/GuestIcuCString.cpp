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

template<class TText>
std::uint32_t ReferenceHash(const TText* text, int length, int stride, bool fold) {
    std::uint32_t hash = 0;
    for (int index = 0; index < length; index += stride) {
        std::uint32_t unit = static_cast<std::make_unsigned_t<TText>>(text[index]);
        if (fold && unit >= 'A' && unit <= 'Z') unit += 32;
        hash = hash * 37u + unit;
    }
    return hash;
}

void Hashes() {
    Require(ustr_hashCharsN_nid_postfix("abc", 3) == 136518, "three bytes hash by base 37");
    Require(ustr_hashICharsN_nid_postfix("ABC", 3) == 136518, "caseless hash lowercases ASCII");
    Require(ustr_hashUCharsN_nid_postfix(u"abc", 3) == 136518, "three units hash by base 37");
    Require(ustr_hashCharsN_nid_postfix("\xff", 1) == 255 && ustr_hashICharsN_nid_postfix("\xff", 1) == 255, "bytes are unsigned");
    Require(ustr_hashICharsN_nid_postfix("\xc1", 1) == 0xc1, "caseless hash folds only ASCII");
    Require(ustr_hashUCharsN_nid_postfix(u"\xffff", 1) == 0xffff, "units are unsigned");
    for (const std::int32_t length : {0, -1, -32, std::numeric_limits<std::int32_t>::min()}) {
        Require(ustr_hashCharsN_nid_postfix("abc", length) == 0, "nonpositive byte length hashes nothing");
        Require(ustr_hashICharsN_nid_postfix("abc", length) == 0, "nonpositive caseless length hashes nothing");
        Require(ustr_hashUCharsN_nid_postfix(u"abc", length) == 0, "nonpositive unit length hashes nothing");
    }
    Require(ustr_hashCharsN_nid_postfix(nullptr, 5) == 0 && ustr_hashICharsN_nid_postfix(nullptr, 5) == 0 && ustr_hashUCharsN_nid_postfix(nullptr, 5) == 0, "null text hashes to zero");
    std::array<char, 200> bytes{};
    std::array<char16_t, 200> units{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<char>("AbCdEfGhIjKlMnOpQrStUvWxYz\x80\xff"[index % 28]);
        units[index] = static_cast<char16_t>(0x41 + index * 331);
    }
    for (const int length : {1, 2, 31, 32, 33, 63, 64, 65, 95, 96, 100, 127, 128, 200}) {
        const int stride = length < 64 ? 1 : length < 96 ? 2 : length < 128 ? 3 : length < 160 ? 4 : length < 192 ? 5 : 6;
        Require(static_cast<std::uint32_t>(ustr_hashCharsN_nid_postfix(bytes.data(), length)) == ReferenceHash(bytes.data(), length, stride, false), "sampled byte hash");
        Require(static_cast<std::uint32_t>(ustr_hashICharsN_nid_postfix(bytes.data(), length)) == ReferenceHash(bytes.data(), length, stride, true), "sampled caseless hash");
        Require(static_cast<std::uint32_t>(ustr_hashUCharsN_nid_postfix(units.data(), length)) == ReferenceHash(units.data(), length, stride, false), "sampled unit hash");
    }
    std::array<char, 64> sampled{};
    sampled.fill('a');
    const auto before = ustr_hashCharsN_nid_postfix(sampled.data(), 64);
    const auto shorter = ustr_hashCharsN_nid_postfix(sampled.data(), 63);
    sampled[1] = 'z';
    sampled[63] = 'z';
    Require(ustr_hashCharsN_nid_postfix(sampled.data(), 64) == before, "a 64-byte hash skips odd positions");
    Require(ustr_hashCharsN_nid_postfix(sampled.data(), 63) != shorter, "a 63-byte hash reads every byte");
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
    Require(uhash_hashUChars_nid_postfix(Pointer(wide)) == ustr_hashUCharsN_nid_postfix(wide, 12), "unit key hash covers the whole string");
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
    Hashes();
    IntegerToUnits();
    HashTableKeys();
}

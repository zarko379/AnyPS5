#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

template<class TValue>
std::int32_t FormatInteger(char* buffer, TValue value, std::uint32_t radix) {
    using TUnsigned = std::make_unsigned_t<TValue>;
    auto magnitude = static_cast<TUnsigned>(value);
    const bool negative = value < 0 && radix == 10;
    std::int32_t length = 0;
    if (negative) {
        magnitude = TUnsigned{0} - magnitude;
        buffer[length++] = '-';
    }
    const std::int32_t firstDigit = length;
    do {
        buffer[length++] = "0123456789ABCDEF"[magnitude % radix];
        magnitude /= radix;
    } while (magnitude);
    std::reverse(buffer + firstDigit, buffer + length);
    buffer[length] = 0;
    return length;
}

char* ConvertCase(char* text, char first, char last, int offset) {
    if (!text) return nullptr;
    for (char* cursor = text;; ++cursor) {
        const char value = *cursor;
        *cursor = value >= first && value <= last ? static_cast<char>(value + offset) : value;
        if (!value) return text;
    }
}

char AsciiLower(char value) {
    return value >= 0x41 && value <= 0x5a ? static_cast<char>(value + 0x20) : value;
}

template<class TUnit>
std::int32_t HashSampled(const void* text, std::int32_t length, TUnit unit) {
    std::uint32_t hash = 0;
    if (text && length > 0) {
        const std::int32_t step = (length - 32) / 32 + 1;
        for (std::int64_t index = 0; index < length; index += step) hash = hash * 37 + unit(index);
    }
    return std::bit_cast<std::int32_t>(hash);
}

template<class TText, class TFold>
std::int8_t EqualKeys(const void* first, const void* second, TFold fold) {
    auto* left = static_cast<const TText*>(first);
    auto* right = static_cast<const TText*>(second);
    if (left == right) return 1;
    if (!left || !right) return 0;
    while (*left != 0 && fold(*left) == fold(*right)) {
        ++left;
        ++right;
    }
    return *left == *right;
}

std::int32_t TextLength(const char* text) {
    std::size_t length = 0;
    while (text[length] != 0) ++length;
    if (length > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::overflow_error("ICU: string length exceeds int32");
    return static_cast<std::int32_t>(length);
}

int CompareFolded(const char* first, const char* second, std::uint64_t limit) {
    if (!first) return second ? -1 : 0;
    if (!second) return 1;
    for (std::uint64_t index = 0; index < limit; ++index) {
        const auto left = static_cast<unsigned char>(first[index]);
        const auto right = static_cast<unsigned char>(second[index]);
        if (left == 0) return right == 0 ? 0 : -1;
        if (right == 0) return 1;
        const int difference = static_cast<unsigned char>(AsciiLower(static_cast<char>(left))) -
                               static_cast<unsigned char>(AsciiLower(static_cast<char>(right)));
        if (difference != 0) return difference;
    }
    return 0;
}

}

extern "C" {

std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t value, std::int32_t radix) {
    if (!buffer || radix < 2 || radix > 16) APS5_INVALID_ARG_EX;
    return FormatInteger(buffer, value, static_cast<std::uint32_t>(radix));
}

std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t value, std::uint32_t radix) {
    if (!buffer || radix < 2 || radix > 16) APS5_INVALID_ARG_EX;
    return FormatInteger(buffer, value, radix);
}

std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix) {
    if (!text) APS5_INVALID_ARG_EX;
    const auto value = static_cast<std::uint32_t>(strtoul_nid_postfix(text, nullptr, radix));
    return std::bit_cast<std::int32_t>(value);
}

char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text) {
    return ConvertCase(text, 'A', 'Z', 'a' - 'A');
}

char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text) {
    return ConvertCase(text, 'a', 'z', 'A' - 'a');
}

std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

char APS5_VABI uprv_toupper_nid_postfix(char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value + ('A' - 'a')) : value;
}

char APS5_VABI uprv_asciitolower_nid_postfix(char value) {
    return AsciiLower(value);
}

char APS5_VABI uprv_ebcdictolower_nid_postfix(char value) {
    const auto unit = static_cast<std::uint8_t>(value);
    if ((unit >= 0xc1 && unit <= 0xc9) || (unit >= 0xd1 && unit <= 0xd9) || (unit >= 0xe2 && unit <= 0xe9))
        return static_cast<char>(unit - 0x40);
    return value;
}

int APS5_VABI uprv_stricmp_nid_postfix(const char* first, const char* second) {
    return CompareFolded(first, second, std::numeric_limits<std::uint64_t>::max());
}

int APS5_VABI uprv_strnicmp_nid_postfix(const char* first, const char* second, std::uint32_t count) {
    return CompareFolded(first, second, count);
}

std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* text, std::int32_t length) {
    return HashSampled(text, length, [text](std::int64_t index) { return static_cast<std::uint32_t>(text[index]); });
}

std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* text, std::int32_t length) {
    return HashSampled(text, length, [text](std::int64_t index) { return static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[index])); });
}

std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* text, std::int32_t length) {
    return HashSampled(text, length, [text](std::int64_t index) { return static_cast<std::uint32_t>(static_cast<std::uint8_t>(AsciiLower(text[index]))); });
}

std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value, std::uint32_t radix, std::int32_t minimumWidth) {
    if (!buffer || radix == 0) APS5_INVALID_ARG_EX;
    std::int32_t length = 0;
    do {
        const auto digit = static_cast<std::int32_t>(value % radix);
        buffer[length++] = static_cast<char16_t>(static_cast<std::uint32_t>(digit) + (digit <= 9 ? 0x30u : 0x37u));
        value /= radix;
    } while (value != 0 && length < capacity);
    while (length < minimumWidth) buffer[length++] = u'0';
    if (length < capacity) buffer[length] = 0;
    std::reverse(buffer, buffer + length);
    return length;
}

std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(UHashTok key) {
    const auto* text = static_cast<const char16_t*>(key.pointer);
    return text ? ustr_hashUCharsN_nid_postfix(text, u_strlen_nid_postfix(text)) : 0;
}

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(UHashTok key) {
    const auto* text = static_cast<const char*>(key.pointer);
    return text ? ustr_hashCharsN_nid_postfix(text, TextLength(text)) : 0;
}

std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(UHashTok key) {
    const auto* text = static_cast<const char*>(key.pointer);
    return text ? ustr_hashICharsN_nid_postfix(text, TextLength(text)) : 0;
}

std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(UHashTok first, UHashTok second) {
    return EqualKeys<char16_t>(first.pointer, second.pointer, [](char16_t unit) { return unit; });
}

std::int8_t APS5_VABI uhash_compareChars_nid_postfix(UHashTok first, UHashTok second) {
    return EqualKeys<char>(first.pointer, second.pointer, [](char unit) { return unit; });
}

std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(UHashTok first, UHashTok second) {
    return EqualKeys<char>(first.pointer, second.pointer, AsciiLower);
}

std::int32_t APS5_VABI uhash_hashLong_nid_postfix(UHashTok key) {
    return key.integer;
}

std::int8_t APS5_VABI uhash_compareLong_nid_postfix(UHashTok first, UHashTok second) {
    return first.integer == second.integer;
}

}

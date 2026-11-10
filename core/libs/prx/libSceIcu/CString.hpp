#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

union UHashTok {
    void* pointer;
    std::int32_t integer;
};

static_assert(sizeof(UHashTok) == 8);

extern "C" {
std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t value, std::int32_t radix);
std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t value, std::uint32_t radix);
std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix);
char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text);
char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text);
std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char value);
char APS5_VABI uprv_toupper_nid_postfix(char value);
char APS5_VABI uprv_asciitolower_nid_postfix(char value);
char APS5_VABI uprv_ebcdictolower_nid_postfix(char value);
int APS5_VABI uprv_stricmp_nid_postfix(const char* first, const char* second);
int APS5_VABI uprv_strnicmp_nid_postfix(const char* first, const char* second, std::uint32_t count);
std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* text, std::int32_t length);
std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* text, std::int32_t length);
std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* text, std::int32_t length);
std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value, std::uint32_t radix, std::int32_t minimumWidth);
std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(UHashTok key);
std::int32_t APS5_VABI uhash_hashChars_nid_postfix(UHashTok key);
std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(UHashTok key);
std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(UHashTok first, UHashTok second);
std::int8_t APS5_VABI uhash_compareChars_nid_postfix(UHashTok first, UHashTok second);
std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(UHashTok first, UHashTok second);
std::int32_t APS5_VABI uhash_hashLong_nid_postfix(UHashTok key);
std::int8_t APS5_VABI uhash_compareLong_nid_postfix(UHashTok first, UHashTok second);
}

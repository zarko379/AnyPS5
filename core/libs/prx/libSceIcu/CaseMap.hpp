#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UBreakIterator;

extern "C" {
std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, const char* locale, std::int32_t* status);
std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, const char* locale, std::int32_t* status);
std::int32_t APS5_VABI u_strToTitle_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, UBreakIterator* titleIter, const char* locale, std::int32_t* status);
std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, std::uint32_t options, std::int32_t* status);
std::int32_t APS5_VABI u_strCaseCompare_nid_postfix(const char16_t* s1, std::int32_t length1, const char16_t* s2, std::int32_t length2, std::uint32_t options, std::int32_t* status);
std::int32_t APS5_VABI u_strcasecmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::uint32_t options);
std::int32_t APS5_VABI u_strncasecmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::int32_t n, std::uint32_t options);
std::int32_t APS5_VABI u_memcasecmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::int32_t length, std::uint32_t options);
void APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix(const char16_t* s1, std::int32_t length1, const char16_t* s2, std::int32_t length2, std::uint32_t options, std::int32_t* matchLen1, std::int32_t* matchLen2, std::int32_t* status);
}

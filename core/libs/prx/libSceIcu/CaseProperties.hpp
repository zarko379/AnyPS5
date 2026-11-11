#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

using GuestCaseContextIterator = std::int32_t (APS5_VABI*)(void* context, std::int8_t dir);

extern "C" {
std::int32_t APS5_VABI ucase_tolower_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI ucase_toupper_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI ucase_totitle_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI ucase_fold_nid_postfix(std::int32_t c, std::uint32_t options);
std::int32_t APS5_VABI ucase_getType_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI ucase_getTypeOrIgnorable_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI ucase_isSoftDotted_nid_postfix(std::int32_t c);
std::int8_t APS5_VABI ucase_isCaseSensitive_nid_postfix(std::int32_t c);
std::int32_t APS5_VABI ucase_toFullLower_nid_postfix(std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale);
std::int32_t APS5_VABI ucase_toFullUpper_nid_postfix(std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale);
std::int32_t APS5_VABI ucase_toFullTitle_nid_postfix(std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale);
std::int32_t APS5_VABI ucase_toFullFolding_nid_postfix(std::int32_t c, const char16_t** pString, std::uint32_t options);
}

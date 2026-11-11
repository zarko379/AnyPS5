#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UBreakIterator;
struct UParseError;

extern "C" {
UBreakIterator* APS5_VABI ubrk_open_nid_postfix(std::int32_t type, const char* locale, const char16_t* text, std::int32_t textLength, std::int32_t* status);
UBreakIterator* APS5_VABI ubrk_openRules_nid_postfix(const char16_t* rules, std::int32_t rulesLength, const char16_t* text, std::int32_t textLength, UParseError* parseErr, std::int32_t* status);
UBreakIterator* APS5_VABI ubrk_openBinaryRules_nid_postfix(const std::uint8_t* binaryRules, std::int32_t rulesLength, const char16_t* text, std::int32_t textLength, std::int32_t* status);
std::int32_t APS5_VABI ubrk_getBinaryRules_nid_postfix(UBreakIterator* bi, std::uint8_t* binaryRules, std::int32_t rulesCapacity, std::int32_t* status);
UBreakIterator* APS5_VABI ubrk_safeClone_nid_postfix(const UBreakIterator* bi, void* stackBuffer, std::int32_t* pBufferSize, std::int32_t* status);
void APS5_VABI ubrk_close_nid_postfix(UBreakIterator* bi);
void APS5_VABI ubrk_setText_nid_postfix(UBreakIterator* bi, const char16_t* text, std::int32_t textLength, std::int32_t* status);
std::int32_t APS5_VABI ubrk_current_nid_postfix(const UBreakIterator* bi);
std::int32_t APS5_VABI ubrk_first_nid_postfix(UBreakIterator* bi);
std::int32_t APS5_VABI ubrk_last_nid_postfix(UBreakIterator* bi);
std::int32_t APS5_VABI ubrk_next_nid_postfix(UBreakIterator* bi);
std::int32_t APS5_VABI ubrk_previous_nid_postfix(UBreakIterator* bi);
std::int32_t APS5_VABI ubrk_following_nid_postfix(UBreakIterator* bi, std::int32_t offset);
std::int32_t APS5_VABI ubrk_preceding_nid_postfix(UBreakIterator* bi, std::int32_t offset);
std::int8_t APS5_VABI ubrk_isBoundary_nid_postfix(UBreakIterator* bi, std::int32_t offset);
std::int32_t APS5_VABI ubrk_getRuleStatus_nid_postfix(UBreakIterator* bi);
std::int32_t APS5_VABI ubrk_getRuleStatusVec_nid_postfix(UBreakIterator* bi, std::int32_t* fillInVec, std::int32_t capacity, std::int32_t* status);
const char* APS5_VABI ubrk_getLocaleByType_nid_postfix(const UBreakIterator* bi, std::int32_t type, std::int32_t* status);
std::int32_t APS5_VABI ubrk_countAvailable_nid_postfix();
const char* APS5_VABI ubrk_getAvailable_nid_postfix(std::int32_t index);
}

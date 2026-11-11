#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UStringSearch;
struct UCollator;
struct UBreakIterator;

extern "C" {
UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength, const char* locale, UBreakIterator* breakIterator, std::int32_t* status);
UCollator* APS5_VABI usearch_getCollator_nid_postfix(const UStringSearch* search);
std::int32_t APS5_VABI usearch_getMatchedLength_nid_postfix(const UStringSearch* search);
std::int32_t APS5_VABI usearch_next_nid_postfix(UStringSearch* search, std::int32_t* status);
void APS5_VABI usearch_reset_nid_postfix(UStringSearch* search);
void APS5_VABI usearch_setAttribute_nid_postfix(UStringSearch* search, std::int32_t attribute, std::int32_t value, std::int32_t* status);
void APS5_VABI usearch_setOffset_nid_postfix(UStringSearch* search, std::int32_t position, std::int32_t* status);
void APS5_VABI usearch_setPattern_nid_postfix(UStringSearch* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status);
void APS5_VABI usearch_setText_nid_postfix(UStringSearch* search, const char16_t* text, std::int32_t textLength, std::int32_t* status);
}

#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UEnumeration;
struct UFieldPosition;
struct UFieldPositionIterator;
struct UNumberingSystem;
struct UParseError;
struct UPluralRules;

extern "C" {
UNumberingSystem* APS5_VABI unumsys_open_nid_postfix(const char* locale, std::int32_t* status);
UNumberingSystem* APS5_VABI unumsys_openByName_nid_postfix(const char* name, std::int32_t* status);
void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* unumsys);
const char* APS5_VABI unumsys_getName_nid_postfix(const UNumberingSystem* unumsys);
std::int8_t APS5_VABI unumsys_isAlgorithmic_nid_postfix(const UNumberingSystem* unumsys);
UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t* status);
void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator* fpositer);
std::int32_t APS5_VABI ufieldpositer_next_nid_postfix(UFieldPositionIterator* fpositer, std::int32_t* beginIndex, std::int32_t* endIndex);
void* APS5_VABI unum_open_nid_postfix(std::int32_t style, const char16_t* pattern, std::int32_t patternLength, const char* locale, UParseError* parseErr, std::int32_t* status);
void APS5_VABI unum_close_nid_postfix(void* fmt);
void* APS5_VABI unum_clone_nid_postfix(const void* fmt, std::int32_t* status);
std::int32_t APS5_VABI unum_formatDouble_nid_postfix(const void* fmt, double number, char16_t* result, std::int32_t resultLength, UFieldPosition* pos, std::int32_t* status);
std::int32_t APS5_VABI unum_formatDoubleForFields_nid_postfix(const void* format, double number, char16_t* result, std::int32_t resultLength, UFieldPositionIterator* fpositer, std::int32_t* status);
std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const void* fmt, const char* number, std::int32_t length, char16_t* result, std::int32_t resultLength, UFieldPosition* pos, std::int32_t* status);
void APS5_VABI unum_setAttribute_nid_postfix(void* fmt, std::int32_t attr, std::int32_t newValue);
std::int32_t APS5_VABI unum_getTextAttribute_nid_postfix(const void* fmt, std::int32_t tag, char16_t* result, std::int32_t resultLength, std::int32_t* status);
void APS5_VABI unum_setTextAttribute_nid_postfix(void* fmt, std::int32_t tag, const char16_t* newValue, std::int32_t newValueLength, std::int32_t* status);
std::int32_t APS5_VABI unum_getSymbol_nid_postfix(const void* fmt, std::int32_t symbol, char16_t* buffer, std::int32_t size, std::int32_t* status);
UPluralRules* APS5_VABI uplrules_openForType_nid_postfix(const char* locale, std::int32_t type, std::int32_t* status);
void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules);
std::int32_t APS5_VABI uplrules_selectWithFormat_nid_postfix(const UPluralRules* uplrules, double number, const void* fmt, char16_t* keyword, std::int32_t capacity, std::int32_t* status);
UEnumeration* APS5_VABI uplrules_getKeywords_nid_postfix(const UPluralRules* uplrules, std::int32_t* status);
}

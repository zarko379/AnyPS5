#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFormattedList;
struct UFormattedValue;
struct UListFormatter;
struct ULocaleDisplayNames;
struct URelativeDateTimeFormatter;

extern "C" {
UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char* locale, std::int32_t type, std::int32_t width, std::int32_t* status);
void APS5_VABI ulistfmt_close_nid_postfix(UListFormatter* listfmt);
UFormattedList* APS5_VABI ulistfmt_openResult_nid_postfix(std::int32_t* ec);
const UFormattedValue* APS5_VABI ulistfmt_resultAsValue_nid_postfix(const UFormattedList* uresult, std::int32_t* ec);
void APS5_VABI ulistfmt_closeResult_nid_postfix(UFormattedList* uresult);
std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[], const std::int32_t* stringLengths, std::int32_t stringCount, char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[], const std::int32_t* stringLengths, std::int32_t stringCount, UFormattedList* uresult, std::int32_t* status);
const char16_t* APS5_VABI ufmtval_getString_nid_postfix(const UFormattedValue* ufmtval, std::int32_t* pLength, std::int32_t* ec);
ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char* locale, std::int32_t* contexts, std::int32_t length, std::int32_t* pErrorCode);
void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* ldn);
std::int32_t APS5_VABI uldn_localeDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* locale, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode);
std::int32_t APS5_VABI uldn_scriptDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* script, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode);
std::int32_t APS5_VABI uldn_regionDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* region, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode);
std::int32_t APS5_VABI uldn_keyValueDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* key, const char* value, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode);
URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char* locale, void* nfToAdopt, std::int32_t width, std::int32_t capitalizationContext, std::int32_t* status);
void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt);
std::int32_t APS5_VABI ureldatefmt_formatNumeric_nid_postfix(const URelativeDateTimeFormatter* reldatefmt, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter* reldatefmt, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
}

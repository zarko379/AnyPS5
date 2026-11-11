#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConverter;

namespace Icu {
struct ConverterFromUnicodeArgs;
struct ConverterToUnicodeArgs;
using ConverterFromUCallback = void (APS5_VABI*)(const void*, ConverterFromUnicodeArgs*, const char16_t*, std::int32_t, std::int32_t, std::int32_t, std::int32_t*);
using ConverterToUCallback = void (APS5_VABI*)(const void*, ConverterToUnicodeArgs*, const char*, std::int32_t, std::int32_t, std::int32_t*);
} // namespace Icu

extern "C" {
UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status);
UConverter* APS5_VABI ucnv_openU_nid_postfix(const char16_t* name, std::int32_t* status);
UConverter* APS5_VABI ucnv_openCCSID_nid_postfix(std::int32_t codepage, std::int32_t platform, std::int32_t* status);
UConverter* APS5_VABI ucnv_openPackage_nid_postfix(const char* packageName, const char* converterName, std::int32_t* status);
UConverter* APS5_VABI ucnv_createConverter_nid_postfix(UConverter* fillIn, const char* name, std::int32_t* status);
UConverter* APS5_VABI ucnv_safeClone_nid_postfix(const UConverter* converter, void* stackBuffer, std::int32_t* bufferSize, std::int32_t* status);
void APS5_VABI ucnv_close_nid_postfix(UConverter* converter);
void APS5_VABI ucnv_getSubstChars_nid_postfix(const UConverter* converter, char* subChars, std::int8_t* length, std::int32_t* status);
void APS5_VABI ucnv_setSubstChars_nid_postfix(UConverter* converter, const char* subChars, std::int8_t length, std::int32_t* status);
void APS5_VABI ucnv_setSubstString_nid_postfix(UConverter* converter, const char16_t* text, std::int32_t length, std::int32_t* status);
void APS5_VABI ucnv_getInvalidChars_nid_postfix(const UConverter* converter, char* bytes, std::int8_t* length, std::int32_t* status);
void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const UConverter* converter, char16_t* units, std::int8_t* length, std::int32_t* status);
void APS5_VABI ucnv_reset_nid_postfix(UConverter* converter);
void APS5_VABI ucnv_resetToUnicode_nid_postfix(UConverter* converter);
void APS5_VABI ucnv_resetFromUnicode_nid_postfix(UConverter* converter);
std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const UConverter* converter);
std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const UConverter* converter);
const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_getPlatform_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_getType_nid_postfix(const UConverter* converter);
void APS5_VABI ucnv_getStarters_nid_postfix(const UConverter* converter, std::int8_t* starters, std::int32_t* status);
void APS5_VABI ucnv_fixFileSeparator_nid_postfix(const UConverter* converter, char16_t* source, std::int32_t length);
std::int8_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const UConverter* converter);
void APS5_VABI ucnv_setFallback_nid_postfix(UConverter* converter, std::int8_t usesFallback);
std::int8_t APS5_VABI ucnv_usesFallback_nid_postfix(const UConverter* converter);
std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int8_t APS5_VABI ucnv_isFixedWidth_nid_postfix(UConverter* converter, std::int32_t* status);
void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter* converter, char** target, const char* targetLimit, const char16_t** source, const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status);
void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter* converter, char16_t** target, const char16_t* targetLimit, const char** source, const char* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status);
std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter* converter, char* destination, std::int32_t capacity, const char16_t* source, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(UConverter* converter, char16_t* destination, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(UConverter* converter, const char** source, const char* sourceLimit, std::int32_t* status);
void APS5_VABI ucnv_convertEx_nid_postfix(UConverter* targetConverter, UConverter* sourceConverter, char** target, const char* targetLimit, const char** source, const char* sourceLimit, char16_t* pivotStart, char16_t** pivotSource, char16_t** pivotTarget, const char16_t* pivotLimit, std::int8_t reset, std::int8_t flush, std::int32_t* status);
std::int32_t APS5_VABI ucnv_convert_nid_postfix(const char* toName, const char* fromName, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI ucnv_toAlgorithmic_nid_postfix(std::int32_t type, UConverter* converter, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI ucnv_fromAlgorithmic_nid_postfix(UConverter* converter, std::int32_t type, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status);
void APS5_VABI ucnv_getFromUCallBack_nid_postfix(const UConverter* converter, Icu::ConverterFromUCallback* action, const void** context);
void APS5_VABI ucnv_getToUCallBack_nid_postfix(const UConverter* converter, Icu::ConverterToUCallback* action, const void** context);
void APS5_VABI ucnv_setFromUCallBack_nid_postfix(UConverter* converter, Icu::ConverterFromUCallback action, const void* context, Icu::ConverterFromUCallback* oldAction, const void** oldContext, std::int32_t* status);
void APS5_VABI ucnv_setToUCallBack_nid_postfix(UConverter* converter, Icu::ConverterToUCallback action, const void* context, Icu::ConverterToUCallback* oldAction, const void** oldContext, std::int32_t* status);
void APS5_VABI UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* status);
void APS5_VABI UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* status);
void APS5_VABI UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* status);
void APS5_VABI UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* status);
void APS5_VABI ucnv_cbFromUWriteBytes_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, const char* source, std::int32_t length, std::int32_t offsetIndex, std::int32_t* status);
void APS5_VABI ucnv_cbFromUWriteSub_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, std::int32_t offsetIndex, std::int32_t* status);
void APS5_VABI ucnv_cbFromUWriteUChars_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, const char16_t** source, const char16_t* sourceLimit, std::int32_t offsetIndex, std::int32_t* status);
void APS5_VABI ucnv_cbToUWriteUChars_nid_postfix(Icu::ConverterToUnicodeArgs* arguments, const char16_t* source, std::int32_t length, std::int32_t offsetIndex, std::int32_t* status);
void APS5_VABI ucnv_cbToUWriteSub_nid_postfix(Icu::ConverterToUnicodeArgs* arguments, std::int32_t offsetIndex, std::int32_t* status);
}

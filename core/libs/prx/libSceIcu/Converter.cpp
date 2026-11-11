#include "prx/libSceIcu/Converter.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#include "ucnv_bld.h"
#include "ucnv_imp.h"
#include <cstdint>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

UErrorCode* Optional(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

bool Proceeds(const std::int32_t* status) {
    return status && *status <= U_ZERO_ERROR;
}

template<typename TConverter>
TConverter* Object(TConverter* converter) {
    IcuBackend::RequireObject(converter);
    return converter;
}

UBool* Flag(std::int8_t* flag) {
    return reinterpret_cast<UBool*>(flag);
}

void OpenUName(const char16_t* name) {
    IcuBackend::Converter(nullptr);
    if (!name) return;
    UErrorCode probe = U_ZERO_ERROR;
    UConverter* converter = ucnv_open(nullptr, &probe);
    if (U_FAILURE(probe)) {
        ucnv_close(converter);
        return;
    }
    const std::int32_t length = ucnv_fromUChars(converter, nullptr, 0, name, -1, &probe);
    ucnv_close(converter);
    if (length >= UCNV_MAX_CONVERTER_NAME_LENGTH) APS5_INVALID_ARG_EX;
}

}

extern "C" {

UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return nullptr;
    IcuBackend::Converter(name);
    return ucnv_open(name, error);
}

UConverter* APS5_VABI ucnv_openU_nid_postfix(const char16_t* name, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return nullptr;
    if (!name || u_strlen(name) < UCNV_MAX_CONVERTER_NAME_LENGTH) OpenUName(name);
    return ucnv_openU(name, error);
}

UConverter* APS5_VABI ucnv_openCCSID_nid_postfix(std::int32_t codepage, std::int32_t platform, std::int32_t* status) {
    return ucnv_openCCSID(codepage, static_cast<UConverterPlatform>(platform), Optional(status));
}

UConverter* APS5_VABI ucnv_openPackage_nid_postfix(const char* packageName, const char* converterName, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::RequireObject(converterName);
    return ucnv_openPackage(IcuBackend::Package(packageName), converterName, error);
}

UConverter* APS5_VABI ucnv_createConverter_nid_postfix(UConverter* fillIn, const char* name, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if (fillIn) NotImplemented_nid_no_patch("libSceIcu: ucnv_createConverter into caller memory (ICU 68.2's internal UConverter layout)");
    IcuBackend::Converter(name);
    return ucnv_createConverter(nullptr, name, error);
}

void APS5_VABI ucnv_close_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_close(converter);
}

void APS5_VABI ucnv_getSubstChars_nid_postfix(const UConverter* converter, char* subChars, std::int8_t* length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    Object(converter);
    Object(length);
    if (converter->subCharLen > 0 && *length >= converter->subCharLen) IcuBackend::RequireObject(subChars);
    ucnv_getSubstChars(converter, subChars, length, error);
}

void APS5_VABI ucnv_setSubstChars_nid_postfix(UConverter* converter, const char* subChars, std::int8_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    const UConverterStaticData* data = Object(converter)->sharedData->staticData;
    if (length <= data->maxBytesPerChar && length >= data->minBytesPerChar && length > 0) IcuBackend::RequireObject(subChars);
    ucnv_setSubstChars(converter, subChars, length, error);
}

void APS5_VABI ucnv_setSubstString_nid_postfix(UConverter* converter, const char16_t* text, std::int32_t length, std::int32_t* status) {
    ucnv_setSubstString(converter, text, length, Status(status));
}

void APS5_VABI ucnv_getInvalidChars_nid_postfix(const UConverter* converter, char* bytes, std::int8_t* length, std::int32_t* status) {
    ucnv_getInvalidChars(converter, bytes, length, Optional(status));
}

void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const UConverter* converter, char16_t* units, std::int8_t* length, std::int32_t* status) {
    ucnv_getInvalidUChars(converter, units, length, Optional(status));
}

void APS5_VABI ucnv_reset_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_reset(converter);
}

void APS5_VABI ucnv_resetToUnicode_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_resetToUnicode(converter);
}

void APS5_VABI ucnv_resetFromUnicode_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_resetFromUnicode(converter);
}

std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const UConverter* converter) {
    return ucnv_getMaxCharSize(Object(converter));
}

std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const UConverter* converter) {
    return ucnv_getMinCharSize(Object(converter));
}

const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return ucnv_getName(Object(converter), error);
}

std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    return ucnv_getCCSID(Object(converter), error);
}

std::int32_t APS5_VABI ucnv_getPlatform_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return UCNV_UNKNOWN;
    return ucnv_getPlatform(Object(converter), error);
}

std::int32_t APS5_VABI ucnv_getType_nid_postfix(const UConverter* converter) {
    return ucnv_getType(Object(converter));
}

void APS5_VABI ucnv_getStarters_nid_postfix(const UConverter* converter, std::int8_t* starters, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return;
    if (Object(converter)->sharedData->impl->getStarters) IcuBackend::RequireObject(starters);
    ucnv_getStarters(converter, Flag(starters), error);
}

void APS5_VABI ucnv_fixFileSeparator_nid_postfix(const UConverter* converter, char16_t* source, std::int32_t length) {
    IcuBackend::Ensure();
    ucnv_fixFileSeparator(converter, source, length);
}

std::int8_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const UConverter* converter) {
    IcuBackend::Ensure();
    return ucnv_isAmbiguous(converter);
}

void APS5_VABI ucnv_setFallback_nid_postfix(UConverter* converter, std::int8_t usesFallback) {
    ucnv_setFallback(Object(converter), usesFallback);
}

std::int8_t APS5_VABI ucnv_usesFallback_nid_postfix(const UConverter* converter) {
    return ucnv_usesFallback(Object(converter));
}

std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status) {
    return ucnv_fromUCountPending(converter, Optional(status));
}

std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status) {
    return ucnv_toUCountPending(converter, Optional(status));
}

std::int8_t APS5_VABI ucnv_isFixedWidth_nid_postfix(UConverter* converter, std::int32_t* status) {
    return ucnv_isFixedWidth(converter, Status(status));
}

void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter* converter, char** target, const char* targetLimit, const char16_t** source, const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status) {
    ucnv_fromUnicode(converter, target, targetLimit, source, sourceLimit, offsets, flush, Optional(status));
}

void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter* converter, char16_t** target, const char16_t* targetLimit, const char** source, const char* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status) {
    ucnv_toUnicode(converter, target, targetLimit, source, sourceLimit, offsets, flush, Optional(status));
}

std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter* converter, char* destination, std::int32_t capacity, const char16_t* source, std::int32_t length, std::int32_t* status) {
    return ucnv_fromUChars(converter, destination, capacity, source, length, Optional(status));
}

std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(UConverter* converter, char16_t* destination, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    return ucnv_toUChars(converter, destination, capacity, source, length, Optional(status));
}

std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(UConverter* converter, const char** source, const char* sourceLimit, std::int32_t* status) {
    return ucnv_getNextUChar(converter, source, sourceLimit, Optional(status));
}

void APS5_VABI ucnv_convertEx_nid_postfix(UConverter* targetConverter, UConverter* sourceConverter, char** target, const char* targetLimit, const char** source, const char* sourceLimit, char16_t* pivotStart, char16_t** pivotSource, char16_t** pivotTarget, const char16_t* pivotLimit, std::int8_t reset, std::int8_t flush, std::int32_t* status) {
    ucnv_convertEx(targetConverter, sourceConverter, target, targetLimit, source, sourceLimit, pivotStart, pivotSource, pivotTarget, pivotLimit, reset, flush, Optional(status));
}

std::int32_t APS5_VABI ucnv_convert_nid_postfix(const char* toName, const char* fromName, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (Proceeds(status) && source && length >= -1 && capacity >= 0 && (capacity == 0 || target) && length != 0 && (length > 0 || *source != 0)) {
        IcuBackend::Converter(fromName);
        if (!toName) {
            UErrorCode probe = U_ZERO_ERROR;
            ucnv_close(ucnv_open(fromName, &probe));
            if (U_SUCCESS(probe)) IcuBackend::Converter(toName);
        }
    }
    return ucnv_convert(toName, fromName, target, capacity, source, length, error);
}

std::int32_t APS5_VABI ucnv_toAlgorithmic_nid_postfix(std::int32_t type, UConverter* converter, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    return ucnv_toAlgorithmic(static_cast<UConverterType>(type), converter, target, capacity, source, length, Optional(status));
}

std::int32_t APS5_VABI ucnv_fromAlgorithmic_nid_postfix(UConverter* converter, std::int32_t type, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    return ucnv_fromAlgorithmic(converter, static_cast<UConverterType>(type), target, capacity, source, length, Optional(status));
}

}

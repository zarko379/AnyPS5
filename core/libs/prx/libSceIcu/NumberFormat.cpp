#include "prx/libSceIcu/NumberFormat.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/locid.h>
#include <unicode/parseerr.h>
#include <unicode/udata.h>
#include <unicode/ufieldpositer.h>
#include <unicode/umisc.h>
#include <unicode/unum.h>
#include <unicode/unumsys.h>
#include <unicode/upluralrules.h>
#include <cstddef>
#include <cstring>
#include <type_traits>

static_assert(sizeof(UFieldPosition) == 12);
static_assert(offsetof(UFieldPosition, endIndex) == 8);
static_assert(sizeof(UParseError) == 72);
static_assert(offsetof(UParseError, postContext) == 40);

namespace {

constexpr const char* RbnfTree = U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "rbnf";

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

template<typename TObject>
TObject* Object(TObject* object) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(object);
    return object;
}

template<typename TFormat>
auto* Format(TFormat* fmt) {
    IcuBackend::Ensure();
    IcuBackend::CheckTracked(fmt);
    if constexpr (std::is_const_v<TFormat>) return static_cast<const UNumberFormat*>(fmt);
    else return static_cast<UNumberFormat*>(fmt);
}

bool NamedNumberingSystem(const icu::Locale& locale) {
    char buffer[ULOC_KEYWORDS_CAPACITY] = "";
    UErrorCode status = U_ZERO_ERROR;
    const std::int32_t count = locale.getKeywordValue("numbers", buffer, sizeof(buffer), status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING || count <= 0) return false;
    buffer[count] = '\0';
    return std::strcmp(buffer, "default") != 0 && std::strcmp(buffer, "native") != 0 && std::strcmp(buffer, "traditional") != 0 && std::strcmp(buffer, "finance") != 0;
}

bool KnownStyle(std::int32_t style) {
    switch (style) {
    case UNUM_DECIMAL:
    case UNUM_CURRENCY:
    case UNUM_PERCENT:
    case UNUM_SCIENTIFIC:
    case UNUM_CURRENCY_ISO:
    case UNUM_CURRENCY_PLURAL:
    case UNUM_CURRENCY_ACCOUNTING:
    case UNUM_CASH_CURRENCY:
    case UNUM_CURRENCY_STANDARD:
    case UNUM_PATTERN_DECIMAL:
    case UNUM_PATTERN_RULEBASED:
    case UNUM_SPELLOUT:
    case UNUM_ORDINAL:
    case UNUM_DURATION:
    case UNUM_NUMBERING_SYSTEM:
    case UNUM_DECIMAL_COMPACT_SHORT:
    case UNUM_DECIMAL_COMPACT_LONG:
        return true;
    default:
        return false;
    }
}

bool RbnfStyle(std::int32_t style) {
    return style == UNUM_SPELLOUT || style == UNUM_ORDINAL || style == UNUM_DURATION || style == UNUM_NUMBERING_SYSTEM;
}

}

extern "C" {

UNumberingSystem* APS5_VABI unumsys_open_nid_postfix(const char* locale, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    const icu::Locale requested(IcuBackend::Locale(locale));
    if (!NamedNumberingSystem(requested)) IcuBackend::LocaleData(nullptr, requested.getName());
    return unumsys_open(locale, error);
}

UNumberingSystem* APS5_VABI unumsys_openByName_nid_postfix(const char* name, std::int32_t* status) {
    return unumsys_openByName(name, Status(status));
}

void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* unumsys) {
    IcuBackend::Ensure();
    unumsys_close(unumsys);
}

const char* APS5_VABI unumsys_getName_nid_postfix(const UNumberingSystem* unumsys) {
    return unumsys_getName(Object(unumsys));
}

std::int8_t APS5_VABI unumsys_isAlgorithmic_nid_postfix(const UNumberingSystem* unumsys) {
    return unumsys_isAlgorithmic(Object(unumsys));
}

UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t* status) {
    return ufieldpositer_open(Status(status));
}

void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator* fpositer) {
    IcuBackend::Ensure();
    ufieldpositer_close(fpositer);
}

std::int32_t APS5_VABI ufieldpositer_next_nid_postfix(UFieldPositionIterator* fpositer, std::int32_t* beginIndex, std::int32_t* endIndex) {
    return ufieldpositer_next(Object(fpositer), beginIndex, endIndex);
}

void* APS5_VABI unum_open_nid_postfix(std::int32_t style, const char16_t* pattern, std::int32_t patternLength, const char* locale, UParseError* parseErr, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    bool needsDefault = false;
    if (KnownStyle(style)) {
        IcuBackend::Locale(locale);
        needsDefault = IcuBackend::LocaleData(nullptr, locale);
        if (RbnfStyle(style)) needsDefault = IcuBackend::LocaleData(RbnfTree, locale) || needsDefault;
    }
    UNumberFormat* format = unum_open(static_cast<UNumberFormatStyle>(style), pattern, patternLength, locale, parseErr, error);
    IcuBackend::Track(format, needsDefault);
    return format;
}

void APS5_VABI unum_close_nid_postfix(void* fmt) {
    IcuBackend::Ensure();
    IcuBackend::Untrack(fmt);
    unum_close(static_cast<UNumberFormat*>(fmt));
}

void* APS5_VABI unum_clone_nid_postfix(const void* fmt, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::RequireObject(fmt);
    UNumberFormat* clone = unum_clone(Format(fmt), error);
    IcuBackend::Track(clone, IcuBackend::Tracked(fmt));
    return clone;
}

std::int32_t APS5_VABI unum_formatDouble_nid_postfix(const void* fmt, double number, char16_t* result, std::int32_t resultLength, UFieldPosition* pos, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    IcuBackend::RequireObject(fmt);
    return unum_formatDouble(Format(fmt), number, result, resultLength, pos, error);
}

std::int32_t APS5_VABI unum_formatDoubleForFields_nid_postfix(const void* format, double number, char16_t* result, std::int32_t resultLength, UFieldPositionIterator* fpositer, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    if (!(result == nullptr ? resultLength != 0 : resultLength < 0)) IcuBackend::RequireObject(format);
    return unum_formatDoubleForFields(Format(format), number, result, resultLength, fpositer, error);
}

std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const void* fmt, const char* number, std::int32_t length, char16_t* result, std::int32_t resultLength, UFieldPosition* pos, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    if (!((result == nullptr && resultLength != 0) || resultLength < 0)) {
        if (!number && length != 0) APS5_INVALID_ARG_EX;
        IcuBackend::RequireObject(fmt);
    }
    return unum_formatDecimal(Format(fmt), number, length, result, resultLength, pos, error);
}

void APS5_VABI unum_setAttribute_nid_postfix(void* fmt, std::int32_t attr, std::int32_t newValue) {
    IcuBackend::RequireObject(fmt);
    unum_setAttribute(Format(fmt), static_cast<UNumberFormatAttribute>(attr), newValue);
}

std::int32_t APS5_VABI unum_getTextAttribute_nid_postfix(const void* fmt, std::int32_t tag, char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    IcuBackend::RequireObject(fmt);
    return unum_getTextAttribute(Format(fmt), static_cast<UNumberFormatTextAttribute>(tag), result, resultLength, error);
}

void APS5_VABI unum_setTextAttribute_nid_postfix(void* fmt, std::int32_t tag, const char16_t* newValue, std::int32_t newValueLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    IcuBackend::RequireObject(fmt);
    unum_setTextAttribute(Format(fmt), static_cast<UNumberFormatTextAttribute>(tag), newValue, newValueLength, error);
}

std::int32_t APS5_VABI unum_getSymbol_nid_postfix(const void* fmt, std::int32_t symbol, char16_t* buffer, std::int32_t size, std::int32_t* status) {
    IcuBackend::Ensure();
    return unum_getSymbol(static_cast<const UNumberFormat*>(fmt), static_cast<UNumberFormatSymbol>(symbol), buffer, size, reinterpret_cast<UErrorCode*>(status));
}

UPluralRules* APS5_VABI uplrules_openForType_nid_postfix(const char* locale, std::int32_t type, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return uplrules_openForType(IcuBackend::Locale(locale), static_cast<UPluralType>(type), error);
}

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules) {
    IcuBackend::Ensure();
    uplrules_close(uplrules);
}

std::int32_t APS5_VABI uplrules_selectWithFormat_nid_postfix(const UPluralRules* uplrules, double number, const void* fmt, char16_t* keyword, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return 0;
    return uplrules_selectWithFormat(uplrules, number, Format(fmt), keyword, capacity, error);
}

UEnumeration* APS5_VABI uplrules_getKeywords_nid_postfix(const UPluralRules* uplrules, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return IcuBackend::GuestEnumeration(uplrules_getKeywords(uplrules, error), status);
}

}

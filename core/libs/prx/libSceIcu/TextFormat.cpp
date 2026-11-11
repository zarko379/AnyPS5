#include "prx/libSceIcu/TextFormat.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/formattedvalue.h>
#include <unicode/udisplaycontext.h>
#include <unicode/uldnames.h>
#include <unicode/ulistformatter.h>
#include <unicode/unum.h>
#include <unicode/ureldatefmt.h>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

bool ListStyle(std::int32_t type, std::int32_t width) {
    const bool knownType = type == ULISTFMT_TYPE_AND || type == ULISTFMT_TYPE_OR || type == ULISTFMT_TYPE_UNITS;
    const bool knownWidth = width == ULISTFMT_WIDTH_WIDE || width == ULISTFMT_WIDTH_SHORT || width == ULISTFMT_WIDTH_NARROW;
    return knownType && knownWidth;
}

bool BadStrings(const char16_t* const strings[], std::int32_t stringCount) {
    return stringCount < 0 || (strings == nullptr && stringCount > 0);
}

bool BadBuffer(const char16_t* result, std::int32_t capacity) {
    return result == nullptr ? capacity != 0 : capacity < 0;
}

void Relative(const URelativeDateTimeFormatter* formatter, std::int32_t unit) {
    IcuBackend::RequireObject(formatter);
    if (unit < 0 || unit >= UDAT_REL_UNIT_COUNT) APS5_INVALID_ARG_EX;
    IcuBackend::CheckTracked(formatter);
}

}

extern "C" {

UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char* locale, std::int32_t type, std::int32_t width, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if (ListStyle(type, width)) {
        IcuBackend::Locale(locale);
        IcuBackend::LocaleData(nullptr, locale);
    }
    return ulistfmt_openForType(locale, static_cast<UListFormatterType>(type), static_cast<UListFormatterWidth>(width), error);
}

void APS5_VABI ulistfmt_close_nid_postfix(UListFormatter* listfmt) {
    IcuBackend::Ensure();
    ulistfmt_close(listfmt);
}

UFormattedList* APS5_VABI ulistfmt_openResult_nid_postfix(std::int32_t* ec) {
    return ulistfmt_openResult(Status(ec));
}

const UFormattedValue* APS5_VABI ulistfmt_resultAsValue_nid_postfix(const UFormattedList* uresult, std::int32_t* ec) {
    return ulistfmt_resultAsValue(uresult, Status(ec));
}

void APS5_VABI ulistfmt_closeResult_nid_postfix(UFormattedList* uresult) {
    IcuBackend::Ensure();
    ulistfmt_closeResult(uresult);
}

std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[], const std::int32_t* stringLengths, std::int32_t stringCount, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && !BadBuffer(result, resultCapacity) && !BadStrings(strings, stringCount)) IcuBackend::RequireObject(listfmt);
    return ulistfmt_format(listfmt, strings, stringLengths, stringCount, result, resultCapacity, error);
}

void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[], const std::int32_t* stringLengths, std::int32_t stringCount, UFormattedList* uresult, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) {
        UErrorCode probe = U_ZERO_ERROR;
        ulistfmt_resultAsValue(uresult, &probe);
        if (U_SUCCESS(probe) && !BadStrings(strings, stringCount)) IcuBackend::RequireObject(listfmt);
    }
    ulistfmt_formatStringsToResult(listfmt, strings, stringLengths, stringCount, uresult, error);
}

const char16_t* APS5_VABI ufmtval_getString_nid_postfix(const UFormattedValue* ufmtval, std::int32_t* pLength, std::int32_t* ec) {
    return ufmtval_getString(ufmtval, pLength, Status(ec));
}

ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char* locale, std::int32_t* contexts, std::int32_t length, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Locale(locale);
    const bool needsDefault = IcuBackend::LocaleData(nullptr, locale);
    ULocaleDisplayNames* names = uldn_openForContext(locale, reinterpret_cast<UDisplayContext*>(contexts), length, error);
    IcuBackend::Track(names, needsDefault);
    return names;
}

void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* ldn) {
    IcuBackend::Ensure();
    IcuBackend::Untrack(ldn);
    uldn_close(ldn);
}

std::int32_t APS5_VABI uldn_localeDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* locale, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CheckTracked(ldn);
    return uldn_localeDisplayName(ldn, locale, result, maxResultSize, error);
}

std::int32_t APS5_VABI uldn_scriptDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* script, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CheckTracked(ldn);
    return uldn_scriptDisplayName(ldn, script, result, maxResultSize, error);
}

std::int32_t APS5_VABI uldn_regionDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* region, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CheckTracked(ldn);
    return uldn_regionDisplayName(ldn, region, result, maxResultSize, error);
}

std::int32_t APS5_VABI uldn_keyValueDisplayName_nid_postfix(const ULocaleDisplayNames* ldn, const char* key, const char* value, char16_t* result, std::int32_t maxResultSize, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::CheckTracked(ldn);
    return uldn_keyValueDisplayName(ldn, key, value, result, maxResultSize, error);
}

URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char* locale, void* nfToAdopt, std::int32_t width, std::int32_t capitalizationContext, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if ((capitalizationContext >> 8) == UDISPCTX_TYPE_CAPITALIZATION && (width < 0 || width >= UDAT_STYLE_COUNT)) APS5_INVALID_ARG_EX;
    IcuBackend::Locale(locale);
    const bool needsDefault = IcuBackend::LocaleData(nullptr, locale);
    const bool numberNeedsDefault = IcuBackend::Tracked(nfToAdopt);
    URelativeDateTimeFormatter* formatter = ureldatefmt_open(locale, static_cast<UNumberFormat*>(nfToAdopt), static_cast<UDateRelativeDateTimeFormatterStyle>(width), static_cast<UDisplayContext>(capitalizationContext), error);
    if (formatter) {
        IcuBackend::Untrack(nfToAdopt);
        IcuBackend::Track(formatter, needsDefault || numberNeedsDefault);
    }
    return formatter;
}

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt) {
    IcuBackend::Ensure();
    IcuBackend::Untrack(reldatefmt);
    ureldatefmt_close(reldatefmt);
}

std::int32_t APS5_VABI ureldatefmt_formatNumeric_nid_postfix(const URelativeDateTimeFormatter* reldatefmt, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && !BadBuffer(result, resultCapacity)) Relative(reldatefmt, unit);
    return ureldatefmt_formatNumeric(reldatefmt, offset, static_cast<URelativeDateTimeUnit>(unit), result, resultCapacity, error);
}

std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter* reldatefmt, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && !BadBuffer(result, resultCapacity)) Relative(reldatefmt, unit);
    return ureldatefmt_format(reldatefmt, offset, static_cast<URelativeDateTimeUnit>(unit), result, resultCapacity, error);
}

}

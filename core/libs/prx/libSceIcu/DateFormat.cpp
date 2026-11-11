#include "prx/libSceIcu/DateFormat.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/formattedvalue.h>
#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/udata.h>
#include <unicode/udateintervalformat.h>
#include <unicode/udatpg.h>
#include <unicode/ufieldpositer.h>
#include <unicode/umisc.h>

namespace {

constexpr const char* ZoneTree = U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "zone";

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

template<typename TObject>
TObject* Object(TObject* object) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(object);
    return object;
}

bool BadBuffer(const char16_t* result, std::int32_t capacity) {
    return result == nullptr ? capacity != 0 : capacity < 0;
}

bool BadText(const char16_t* text, std::int32_t length) {
    return text == nullptr ? length != 0 : length < -1;
}

bool CalendarAttribute(std::int32_t attr) {
    return attr == UCAL_LENIENT || attr == UCAL_FIRST_DAY_OF_WEEK || attr == UCAL_MINIMAL_DAYS_IN_FIRST_WEEK || attr == UCAL_REPEATED_WALL_TIME || attr == UCAL_SKIPPED_WALL_TIME;
}

bool DisplayNameType(std::int32_t type) {
    return type == UCAL_STANDARD || type == UCAL_SHORT_STANDARD || type == UCAL_DST || type == UCAL_SHORT_DST;
}

void DateFormat(const void* format) {
    IcuBackend::RequireObject(format);
    IcuBackend::CheckTracked(format);
}

bool ValidResult(const UFormattedDateInterval* result) {
    UErrorCode probe = U_ZERO_ERROR;
    udtitvfmt_resultAsValue(result, &probe);
    return U_SUCCESS(probe);
}

void IntervalFormat(const UDateIntervalFormat* formatter) {
    IcuBackend::RequireObject(formatter);
    IcuBackend::CheckTracked(formatter);
}

}

extern "C" {

void* APS5_VABI ucal_open_nid_postfix(const char16_t* zoneID, std::int32_t len, const char* locale, std::int32_t type, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Zone(zoneID, len);
    IcuBackend::Locale(locale);
    IcuBackend::LocaleData(nullptr, locale);
    return ucal_open(zoneID, len, locale, static_cast<UCalendarType>(type), error);
}

void APS5_VABI ucal_close_nid_postfix(void* cal) {
    IcuBackend::Ensure();
    ucal_close(static_cast<UCalendar*>(cal));
}

void* APS5_VABI ucal_clone_nid_postfix(const void* cal, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return ucal_clone(static_cast<const UCalendar*>(Object(cal)), error);
}

std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void* cal, std::int32_t type, const char* locale, char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    Object(cal);
    if (DisplayNameType(type)) {
        IcuBackend::Locale(locale);
        IcuBackend::LocaleData(ZoneTree, locale);
    }
    return ucal_getTimeZoneDisplayName(static_cast<const UCalendar*>(cal), static_cast<UCalendarDisplayNameType>(type), locale, result, resultLength, error);
}

void APS5_VABI ucal_setGregorianChange_nid_postfix(void* cal, double date, std::int32_t* pErrorCode) {
    IcuBackend::Ensure();
    ucal_setGregorianChange(static_cast<UCalendar*>(cal), date, Status(pErrorCode));
}

std::int32_t APS5_VABI ucal_getAttribute_nid_postfix(const void* cal, std::int32_t attr) {
    IcuBackend::Ensure();
    if (CalendarAttribute(attr)) IcuBackend::RequireObject(cal);
    return ucal_getAttribute(static_cast<const UCalendar*>(cal), static_cast<UCalendarAttribute>(attr));
}

void APS5_VABI ucal_setMillis_nid_postfix(void* cal, double dateTime, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    ucal_setMillis(static_cast<UCalendar*>(Object(cal)), dateTime, error);
}

std::int32_t APS5_VABI ucal_get_nid_postfix(const void* cal, std::int32_t field, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    Object(cal);
    if (field < 0 || field >= UCAL_FIELD_COUNT) APS5_INVALID_ARG_EX;
    return ucal_get(static_cast<const UCalendar*>(cal), static_cast<UCalendarDateFields>(field), error);
}

std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(const char16_t* id, std::int32_t len, char16_t* result, std::int32_t resultCapacity, std::int8_t* isSystemID, std::int32_t* status) {
    IcuBackend::Ensure();
    return ucal_getCanonicalTimeZoneID(id, len, result, resultCapacity, reinterpret_cast<UBool*>(isSystemID), reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI ucal_getDayOfWeekType_nid_postfix(const void* cal, std::int32_t dayOfWeek, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return UCAL_WEEKDAY;
    return ucal_getDayOfWeekType(static_cast<const UCalendar*>(Object(cal)), static_cast<UCalendarDaysOfWeek>(dayOfWeek), error);
}

void* APS5_VABI udat_open_nid_postfix(std::int32_t timeStyle, std::int32_t dateStyle, const char* locale, const char16_t* tzID, std::int32_t tzIDLength, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Locale(locale);
    const bool needsDefault = IcuBackend::LocaleData(nullptr, locale);
    IcuBackend::Zone(tzID, tzIDLength);
    IcuBackend::PatternGenerator();
    UDateFormat* format = udat_open(static_cast<UDateFormatStyle>(timeStyle), static_cast<UDateFormatStyle>(dateStyle), locale, tzID, tzIDLength, pattern, patternLength, error);
    IcuBackend::Track(format, needsDefault);
    return format;
}

void APS5_VABI udat_close_nid_postfix(void* format) {
    IcuBackend::Ensure();
    IcuBackend::Untrack(format);
    udat_close(static_cast<UDateFormat*>(format));
}

std::int32_t APS5_VABI udat_format_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPosition* position, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && !BadBuffer(result, resultLength)) DateFormat(format);
    return udat_format(static_cast<const UDateFormat*>(format), dateToFormat, result, resultLength, position, error);
}

std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPositionIterator* fpositer, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && !BadBuffer(result, resultLength)) DateFormat(format);
    return udat_formatForFields(static_cast<const UDateFormat*>(format), dateToFormat, result, resultLength, fpositer, error);
}

std::int32_t APS5_VABI udat_toPattern_nid_postfix(const void* fmt, std::int8_t localized, char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    IcuBackend::Ensure();
    return udat_toPattern(static_cast<const UDateFormat*>(fmt), localized, result, resultLength, Status(status));
}

std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t type, std::int32_t symbolIndex, char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    IcuBackend::Ensure();
    if (fmt) {
        IcuBackend::RequireStatus(status);
        if (type != UDAT_LOCALIZED_CHARS && symbolIndex < 0) APS5_INVALID_ARG_EX;
    }
    return udat_getSymbols(static_cast<const UDateFormat*>(fmt), static_cast<UDateFormatSymbolType>(type), symbolIndex, result, resultLength, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI udat_countSymbols_nid_postfix(const void* fmt, std::int32_t type) {
    IcuBackend::Ensure();
    return udat_countSymbols(static_cast<const UDateFormat*>(fmt), static_cast<UDateFormatSymbolType>(type));
}

const void* APS5_VABI udat_getCalendar_nid_postfix(const void* fmt) {
    return udat_getCalendar(static_cast<const UDateFormat*>(Object(fmt)));
}

void* APS5_VABI udatpg_open_nid_postfix(const char* locale, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Locale(locale);
    IcuBackend::LocaleData(nullptr, locale);
    IcuBackend::PatternGenerator();
    return udatpg_open(locale, error);
}

void APS5_VABI udatpg_close_nid_postfix(void* dtpg) {
    IcuBackend::Ensure();
    udatpg_close(static_cast<UDateTimePatternGenerator*>(dtpg));
}

std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(void* dtpg, const char16_t* skeleton, std::int32_t length, std::int32_t options, char16_t* bestPattern, std::int32_t capacity, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_SUCCESS(*error) && !(skeleton == nullptr && length != 0)) Object(dtpg);
    return udatpg_getBestPatternWithOptions(static_cast<UDateTimePatternGenerator*>(dtpg), skeleton, length, static_cast<UDateTimePatternMatchOptions>(options), bestPattern, capacity, error);
}

std::int32_t APS5_VABI udatpg_getBestPattern_nid_postfix(void* dtpg, const char16_t* skeleton, std::int32_t length, char16_t* bestPattern, std::int32_t capacity, std::int32_t* pErrorCode) {
    return udatpg_getBestPatternWithOptions_nid_postfix(dtpg, skeleton, length, UDATPG_MATCH_NO_OPTIONS, bestPattern, capacity, pErrorCode);
}

std::int32_t APS5_VABI udatpg_getFieldDisplayName_nid_postfix(const void* dtpg, std::int32_t field, std::int32_t width, char16_t* fieldName, std::int32_t capacity, std::int32_t* pErrorCode) {
    UErrorCode* error = Status(pErrorCode);
    if (U_SUCCESS(*error) && !BadBuffer(fieldName, capacity)) {
        Object(dtpg);
        if (field < 0 || field >= UDATPG_FIELD_COUNT || width < 0 || width > UDATPG_NARROW) APS5_INVALID_ARG_EX;
    }
    return udatpg_getFieldDisplayName(static_cast<const UDateTimePatternGenerator*>(dtpg), static_cast<UDateTimePatternField>(field), static_cast<UDateTimePGDisplayWidth>(width), fieldName, capacity, error);
}

std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(void* unusedDtpg, const char16_t* pattern, std::int32_t length, char16_t* skeleton, std::int32_t capacity, std::int32_t* pErrorCode) {
    IcuBackend::Ensure();
    return udatpg_getSkeleton(static_cast<UDateTimePatternGenerator*>(unusedDtpg), pattern, length, skeleton, capacity, Status(pErrorCode));
}

UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char* locale, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID, std::int32_t tzIDLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    bool needsDefault = false;
    if (!BadText(skeleton, skeletonLength) && !BadText(tzID, tzIDLength)) {
        IcuBackend::Locale(locale);
        needsDefault = IcuBackend::LocaleData(nullptr, locale);
        IcuBackend::Zone(tzID, tzIDLength);
        IcuBackend::PatternGenerator();
    }
    UDateIntervalFormat* formatter = udtitvfmt_open(locale, skeleton, skeletonLength, tzID, tzIDLength, error);
    IcuBackend::Track(formatter, needsDefault);
    return formatter;
}

void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter) {
    IcuBackend::Ensure();
    IcuBackend::Untrack(formatter);
    udtitvfmt_close(formatter);
}

UFormattedDateInterval* APS5_VABI udtitvfmt_openResult_nid_postfix(std::int32_t* ec) {
    return udtitvfmt_openResult(Status(ec));
}

const UFormattedValue* APS5_VABI udtitvfmt_resultAsValue_nid_postfix(const UFormattedDateInterval* uresult, std::int32_t* ec) {
    return udtitvfmt_resultAsValue(uresult, Status(ec));
}

void APS5_VABI udtitvfmt_closeResult_nid_postfix(UFormattedDateInterval* uresult) {
    IcuBackend::Ensure();
    udtitvfmt_closeResult(uresult);
}

void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat* formatter, double fromDate, double toDate, UFormattedDateInterval* result, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && ValidResult(result)) IntervalFormat(formatter);
    udtitvfmt_formatToResult(formatter, fromDate, toDate, result, error);
}

void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat* formatter, void* fromCalendar, void* toCalendar, UFormattedDateInterval* result, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && ValidResult(result)) {
        IntervalFormat(formatter);
        IcuBackend::RequireObject(fromCalendar);
        IcuBackend::RequireObject(toCalendar);
    }
    udtitvfmt_formatCalendarToResult(formatter, static_cast<UCalendar*>(fromCalendar), static_cast<UCalendar*>(toCalendar), result, error);
}

}

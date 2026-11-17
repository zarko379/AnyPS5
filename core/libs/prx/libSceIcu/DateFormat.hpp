#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateIntervalFormat;
struct UFieldPosition;
struct UFieldPositionIterator;
struct UFormattedDateInterval;
struct UFormattedValue;

extern "C" {
void* APS5_VABI ucal_open_nid_postfix(const char16_t* zoneID, std::int32_t len, const char* locale, std::int32_t type, std::int32_t* status);
void APS5_VABI ucal_close_nid_postfix(void* cal);
void* APS5_VABI ucal_clone_nid_postfix(const void* cal, std::int32_t* status);
std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void* cal, std::int32_t type, const char* locale, char16_t* result, std::int32_t resultLength, std::int32_t* status);
void APS5_VABI ucal_setGregorianChange_nid_postfix(void* cal, double date, std::int32_t* pErrorCode);
std::int32_t APS5_VABI ucal_getAttribute_nid_postfix(const void* cal, std::int32_t attr);
void APS5_VABI ucal_setMillis_nid_postfix(void* cal, double dateTime, std::int32_t* status);
std::int32_t APS5_VABI ucal_get_nid_postfix(const void* cal, std::int32_t field, std::int32_t* status);
std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(const char16_t* id, std::int32_t len, char16_t* result, std::int32_t resultCapacity, std::int8_t* isSystemID, std::int32_t* status);
std::int32_t APS5_VABI ucal_getDayOfWeekType_nid_postfix(const void* cal, std::int32_t dayOfWeek, std::int32_t* status);
void* APS5_VABI udat_open_nid_postfix(std::int32_t timeStyle, std::int32_t dateStyle, const char* locale, const char16_t* tzID, std::int32_t tzIDLength, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status);
void APS5_VABI udat_close_nid_postfix(void* format);
std::int32_t APS5_VABI udat_format_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPosition* position, std::int32_t* status);
std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPositionIterator* fpositer, std::int32_t* status);
std::int32_t APS5_VABI udat_toPattern_nid_postfix(const void* fmt, std::int8_t localized, char16_t* result, std::int32_t resultLength, std::int32_t* status);
std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t type, std::int32_t symbolIndex, char16_t* result, std::int32_t resultLength, std::int32_t* status);
std::int32_t APS5_VABI udat_countSymbols_nid_postfix(const void* fmt, std::int32_t type);
const void* APS5_VABI udat_getCalendar_nid_postfix(const void* fmt);
void* APS5_VABI udatpg_open_nid_postfix(const char* locale, std::int32_t* pErrorCode);
void APS5_VABI udatpg_close_nid_postfix(void* dtpg);
std::int32_t APS5_VABI udatpg_getBestPattern_nid_postfix(void* dtpg, const char16_t* skeleton, std::int32_t length, char16_t* bestPattern, std::int32_t capacity, std::int32_t* pErrorCode);
std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(void* dtpg, const char16_t* skeleton, std::int32_t length, std::int32_t options, char16_t* bestPattern, std::int32_t capacity, std::int32_t* pErrorCode);
std::int32_t APS5_VABI udatpg_getFieldDisplayName_nid_postfix(const void* dtpg, std::int32_t field, std::int32_t width, char16_t* fieldName, std::int32_t capacity, std::int32_t* pErrorCode);
std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(void* unusedDtpg, const char16_t* pattern, std::int32_t length, char16_t* skeleton, std::int32_t capacity, std::int32_t* pErrorCode);
UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char* locale, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID, std::int32_t tzIDLength, std::int32_t* status);
void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter);
UFormattedDateInterval* APS5_VABI udtitvfmt_openResult_nid_postfix(std::int32_t* ec);
const UFormattedValue* APS5_VABI udtitvfmt_resultAsValue_nid_postfix(const UFormattedDateInterval* uresult, std::int32_t* ec);
void APS5_VABI udtitvfmt_closeResult_nid_postfix(UFormattedDateInterval* uresult);
void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat* formatter, double fromDate, double toDate, UFormattedDateInterval* result, std::int32_t* status);
void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat* formatter, void* fromCalendar, void* toCalendar, UFormattedDateInterval* result, std::int32_t* status);
}

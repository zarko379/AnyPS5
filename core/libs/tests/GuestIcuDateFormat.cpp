#include "prx/libSceIcu/DateFormat.hpp"
#include "prx/libSceIcu/NumberFormat.hpp"
#include "prx/libSceIcu/TextFormat.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/formattedvalue.h>
#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/udata.h>
#include <unicode/udateintervalformat.h>
#include <unicode/udatpg.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uloc.h>
#include <unicode/umisc.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t Unsupported = 16;

std::int32_t opened = 0;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU DATE FORMAT: %s\n", message.c_str());
        std::abort();
    }
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    }
    return false;
}

void SetDefaults(const char* locale) {
    std::int32_t status = Ok;
    uloc_setDefault_nid_postfix(locale, &status);
    UErrorCode hostStatus = U_ZERO_ERROR;
    uloc_setDefault(locale ? locale : "en_US_POSIX", &hostStatus);
    Require(status == Ok && U_SUCCESS(hostStatus), std::string("the defaults are set to ") + (locale ? locale : "none"));
}

std::u16string Text(const char16_t* buffer, std::int32_t length, std::int32_t capacity) {
    return length >= 0 ? std::u16string(buffer, static_cast<std::size_t>(length < capacity ? length : capacity)) : std::u16string();
}

struct Result {
    std::u16string text;
    std::int32_t length = 0;
    std::int32_t status = 0;
    bool operator==(const Result&) const = default;
};

template<typename TGuest, typename THost>
void Compare(TGuest guest, THost host, const std::string& what) {
    char16_t buffer[256] = {};
    char16_t hostBuffer[256] = {};
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Result mine{{}, guest(buffer, 256, &status), 0};
    mine.status = status;
    mine.text = Text(buffer, mine.length, 256);
    Result theirs{{}, host(hostBuffer, 256, &hostStatus), hostStatus};
    theirs.text = Text(hostBuffer, theirs.length, 256);
    Require(mine == theirs, what + " matches ICU");
}

const double Dates[] = {0.0, 1.6e12, -1e12, 1.7e12 + 987, 9.5e11, -6e13};
const char16_t* const Zones[] = {u"Europe/Berlin", u"America/Los_Angeles", u"Asia/Kolkata", u"Etc/GMT+5", u"Bogus/Zone", u"", u"PST", u"Australia/Lord_Howe"};

void CompareCalendars(const void* guest, const UCalendar* host, const std::string& what) {
    for (std::int32_t field = 0; field < UCAL_FIELD_COUNT; ++field) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(ucal_get_nid_postfix(guest, field, &status) == ucal_get(host, static_cast<UCalendarDateFields>(field), &hostStatus) && status == hostStatus, what + " field " + std::to_string(field) + " matches ICU");
    }
    for (std::int32_t attr = UCAL_LENIENT; attr <= UCAL_SKIPPED_WALL_TIME + 1; ++attr) {
        Require(ucal_getAttribute_nid_postfix(guest, attr) == ucal_getAttribute(host, static_cast<UCalendarAttribute>(attr)), what + " attribute matches ICU");
    }
    for (std::int32_t day = 0; day <= 8; ++day) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(ucal_getDayOfWeekType_nid_postfix(guest, day, &status) == ucal_getDayOfWeekType(host, static_cast<UCalendarDaysOfWeek>(day), &hostStatus) && status == hostStatus, what + " day of week type matches ICU");
    }
}

void CheckCalendars() {
    SetDefaults("en_US");
    for (const char* locale : {"en_US", "de", "ja_JP@calendar=japanese", "th_TH@calendar=buddhist", "ar_SA@calendar=islamic-umalqura", "zh@calendar=chinese", "he@calendar=hebrew", "fa_IR", "", "root", "en_GB@fw=wed", "fr_CA@calendar=iso8601"}) {
        for (const char16_t* zone : Zones) {
            for (std::int32_t type : {std::int32_t{UCAL_TRADITIONAL}, std::int32_t{UCAL_GREGORIAN}}) {
                for (std::int32_t length : {-1, 3}) {
                    if (length > 0 && std::char_traits<char16_t>::length(zone) < 3) continue;
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    void* guest = ucal_open_nid_postfix(zone, length, locale, type, &status);
                    UCalendar* host = ucal_open(zone, length, locale, static_cast<UCalendarType>(type), &hostStatus);
                    const std::string what = std::string("calendar ") + locale + " " + std::to_string(std::char_traits<char16_t>::length(zone)) + " " + std::to_string(type) + " " + std::to_string(length);
                    Require(status == hostStatus && !guest == !host, what + " opens like ICU");
                    if (guest) ++opened;
                    for (double date : Dates) {
                        status = Ok;
                        hostStatus = U_ZERO_ERROR;
                        ucal_setMillis_nid_postfix(guest, date, &status);
                        ucal_setMillis(host, date, &hostStatus);
                        Require(status == hostStatus, what + " setMillis matches ICU");
                        CompareCalendars(guest, host, what);
                        if (date != Dates[0] && date != Dates[3]) continue;
                        for (std::int32_t nameType = UCAL_STANDARD; nameType <= UCAL_SHORT_DST + 1; ++nameType) {
                            for (const char* nameLocale : {"en", "de", "ja", ""}) {
                                Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return ucal_getTimeZoneDisplayName_nid_postfix(guest, nameType, nameLocale, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return ucal_getTimeZoneDisplayName(host, static_cast<UCalendarDisplayNameType>(nameType), nameLocale, b, c, s); }, what + " zone name " + nameLocale);
                            }
                        }
                    }
                    status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    void* clone = ucal_clone_nid_postfix(guest, &status);
                    UCalendar* hostClone = ucal_clone(host, &hostStatus);
                    Require(clone && status == hostStatus, what + " clones");
                    ucal_close_nid_postfix(guest);
                    CompareCalendars(clone, hostClone, what + " clone");
                    status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    ucal_setGregorianChange_nid_postfix(clone, -1e13, &status);
                    ucal_setGregorianChange(hostClone, -1e13, &hostStatus);
                    Require(status == hostStatus, what + " setGregorianChange matches ICU");
                    ucal_setMillis_nid_postfix(clone, -1.3e13, &status);
                    ucal_setMillis(hostClone, -1.3e13, &hostStatus);
                    CompareCalendars(clone, hostClone, what + " changed");
                    ucal_close_nid_postfix(clone);
                    ucal_close(hostClone);
                    ucal_close(host);
                }
            }
        }
    }
    for (const char16_t* id : {u"PST", u"America/Indianapolis", u"Etc/GMT+5", u"bogus", u"GMT", u"UTC", u"Asia/Calcutta", u"Europe/Kiev", u"GMT+05:30", u"Etc/Unknown"}) {
        char16_t buffer[64] = {};
        char16_t hostBuffer[64] = {};
        std::int8_t system = 7;
        UBool hostSystem = 7;
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t length = ucal_getCanonicalTimeZoneID_nid_postfix(id, -1, buffer, 64, &system, &status);
        const std::int32_t hostLength = ucal_getCanonicalTimeZoneID(id, -1, hostBuffer, 64, &hostSystem, &hostStatus);
        Require(length == hostLength && status == hostStatus && system == hostSystem && Text(buffer, length, 64) == Text(hostBuffer, hostLength, 64), "a canonical zone ID matches ICU");
    }
    char16_t buffer[64] = {};
    std::int32_t status = Ok;
    Require(ucal_getCanonicalTimeZoneID_nid_postfix(u"US/Pacific", 10, buffer, 64, nullptr, &status) == 19 && std::u16string(buffer) == u"America/Los_Angeles", "a known canonical ID without a system flag");
    Require(ucal_getCanonicalTimeZoneID_nid_postfix(u"PST", -1, buffer, 64, nullptr, nullptr) == 0, "a null status returns 0, as in ICU");
    for (auto call : {+[](char16_t* b, std::int32_t* s) { return ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, -1, b, 64, nullptr, s); }, +[](char16_t* b, std::int32_t* s) { return ucal_getCanonicalTimeZoneID_nid_postfix(u"PST", 0, b, 64, nullptr, s); }, +[](char16_t*, std::int32_t* s) { return ucal_getCanonicalTimeZoneID_nid_postfix(u"PST", -1, nullptr, 64, nullptr, s); }, +[](char16_t* b, std::int32_t* s) { return ucal_getCanonicalTimeZoneID_nid_postfix(u"PST", -1, b, 0, nullptr, s); }}) {
        status = Ok;
        Require(call(buffer, &status) == 0 && status == IllegalArgument, "a bad canonical ID argument is illegal");
    }
}

void CheckCalendarErrors() {
    SetDefaults(nullptr);
    std::int32_t status = IllegalArgument;
    Require(!ucal_open_nid_postfix(nullptr, -1, nullptr, UCAL_GREGORIAN, &status) && status == IllegalArgument, "a failed status returns before the default zone and locale");
    Require(Throws<std::invalid_argument>([] { ucal_open_nid_postfix(u"UTC", -1, "en", UCAL_GREGORIAN, nullptr); }), "a null status throws in ucal_open");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucal_open_nid_postfix(nullptr, -1, "en", UCAL_GREGORIAN, &ok); }), "a NULL zone throws");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucal_open_nid_postfix(u"UTC", -1, nullptr, UCAL_TRADITIONAL, &ok); }), "a NULL locale without a guest default throws");
    for (const char* locale : {"xx", "qq_Latn", "xx_US@calendar=gregorian"}) {
        for (std::int32_t type : {std::int32_t{UCAL_TRADITIONAL}, std::int32_t{UCAL_GREGORIAN}}) {
            Require(Throws<std::runtime_error>([locale, type] { std::int32_t ok = Ok; ucal_open_nid_postfix(u"UTC", -1, locale, type, &ok); }), std::string("a calendar locale without data throws: ") + locale);
        }
    }
    status = Ok;
    void* calendar = ucal_open_nid_postfix(u"Asia/Tokyo", -1, "ja_JP", UCAL_GREGORIAN, &status);
    Require(calendar && status <= Ok, "a calendar opens");
    status = Ok;
    Require(ucal_get_nid_postfix(calendar, UCAL_YEAR, &status) >= 2026 && status == Ok, "a new calendar holds the current time");
    ucal_setMillis_nid_postfix(calendar, 1.7e12, &status);
    Require(ucal_get_nid_postfix(calendar, UCAL_YEAR, &status) == 2023 && ucal_get_nid_postfix(calendar, UCAL_MONTH, &status) == UCAL_NOVEMBER && ucal_get_nid_postfix(calendar, UCAL_DATE, &status) == 15 && ucal_get_nid_postfix(calendar, UCAL_HOUR_OF_DAY, &status) == 7, "a known date in Tokyo");
    char16_t buffer[64] = {};
    Require(ucal_getTimeZoneDisplayName_nid_postfix(calendar, UCAL_STANDARD, "en", buffer, 64, &status) == 19 && std::u16string(buffer) == u"Japan Standard Time", "a known zone name");
    status = Ok;
    Require(ucal_getTimeZoneDisplayName_nid_postfix(calendar, UCAL_STANDARD, "en", nullptr, 0, &status) == 19 && status == BufferOverflow, "zone names preflight");
    status = Ok;
    Require(ucal_getTimeZoneDisplayName_nid_postfix(calendar, 9, nullptr, buffer, 64, &status) == 0 && status == Ok, "an unknown name type is empty before the default locale");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ucal_getTimeZoneDisplayName_nid_postfix(calendar, UCAL_DST, nullptr, buffer, 64, &ok); }), "a NULL name locale without a guest default throws");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ucal_getTimeZoneDisplayName_nid_postfix(calendar, UCAL_SHORT_DST, "xx", buffer, 64, &ok); }), "a name locale without data throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucal_getTimeZoneDisplayName_nid_postfix(nullptr, UCAL_DST, "en", buffer, 64, &ok); }), "a null calendar throws in getTimeZoneDisplayName");
    Require(Throws<std::invalid_argument>([&] { ucal_getTimeZoneDisplayName_nid_postfix(calendar, UCAL_DST, "en", buffer, 64, nullptr); }), "a null status throws in getTimeZoneDisplayName");
    status = IllegalArgument;
    Require(ucal_getTimeZoneDisplayName_nid_postfix(nullptr, UCAL_DST, nullptr, buffer, 64, &status) == -1, "getTimeZoneDisplayName returns on a failed status");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucal_clone_nid_postfix(nullptr, &ok); }), "a null calendar throws in clone");
    Require(Throws<std::invalid_argument>([&] { ucal_clone_nid_postfix(calendar, nullptr); }), "a null status throws in clone");
    status = IllegalArgument;
    Require(!ucal_clone_nid_postfix(nullptr, &status), "clone returns on a failed status");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucal_get_nid_postfix(nullptr, UCAL_YEAR, &ok); }), "a null calendar throws in get");
    Require(Throws<std::invalid_argument>([&] { ucal_get_nid_postfix(calendar, UCAL_YEAR, nullptr); }), "a null status throws in get");
    for (std::int32_t field : {-1, std::int32_t{UCAL_FIELD_COUNT}}) {
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucal_get_nid_postfix(calendar, field, &ok); }), "a field ICU would index with throws");
    }
    status = IllegalArgument;
    Require(ucal_get_nid_postfix(nullptr, -1, &status) == -1, "get returns on a failed status");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucal_setMillis_nid_postfix(nullptr, 1, &ok); }), "a null calendar throws in setMillis");
    Require(Throws<std::invalid_argument>([&] { ucal_setMillis_nid_postfix(calendar, 1, nullptr); }), "a null status throws in setMillis");
    status = IllegalArgument;
    ucal_setMillis_nid_postfix(nullptr, 1, &status);
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucal_getDayOfWeekType_nid_postfix(nullptr, UCAL_MONDAY, &ok); }), "a null calendar throws in getDayOfWeekType");
    Require(Throws<std::invalid_argument>([&] { ucal_getDayOfWeekType_nid_postfix(calendar, UCAL_MONDAY, nullptr); }), "a null status throws in getDayOfWeekType");
    status = IllegalArgument;
    Require(ucal_getDayOfWeekType_nid_postfix(nullptr, UCAL_MONDAY, &status) == UCAL_WEEKDAY, "getDayOfWeekType returns on a failed status");
    status = Ok;
    Require(ucal_getDayOfWeekType_nid_postfix(calendar, UCAL_SUNDAY, &status) == UCAL_WEEKEND, "Sunday is a weekend day in Japan");
    Require(Throws<std::invalid_argument>([] { ucal_getAttribute_nid_postfix(nullptr, UCAL_LENIENT); }), "a null calendar throws in getAttribute");
    Require(ucal_getAttribute_nid_postfix(nullptr, 42) == -1, "an unknown attribute returns -1 without the calendar");
    status = Ok;
    ucal_setGregorianChange_nid_postfix(nullptr, 0, &status);
    Require(status == IllegalArgument, "a null calendar is illegal in setGregorianChange");
    Require(Throws<std::invalid_argument>([&] { ucal_setGregorianChange_nid_postfix(calendar, 0, nullptr); }), "a null status throws in setGregorianChange");
    status = Ok;
    ucal_setGregorianChange_nid_postfix(calendar, 3e14, &status);
    ucal_setMillis_nid_postfix(calendar, 9e11, &status);
    Require(status == Ok && ucal_get_nid_postfix(calendar, UCAL_YEAR, &status) == 1998 && ucal_get_nid_postfix(calendar, UCAL_MONTH, &status) == UCAL_JUNE && ucal_get_nid_postfix(calendar, UCAL_DATE, &status) == 27, "a cutover ICU 68.2 counts past 1998's Julian day makes 1998 Julian");
    status = Ok;
    void* japanese = ucal_open_nid_postfix(u"Asia/Tokyo", -1, "ja_JP@calendar=japanese", UCAL_TRADITIONAL, &status);
    status = Ok;
    ucal_setGregorianChange_nid_postfix(japanese, 0, &status);
    Require(status == Unsupported, "setGregorianChange is unsupported for a Japanese calendar");
    status = Ok;
    ucal_setMillis_nid_postfix(japanese, 1.7e12, &status);
    Require(ucal_get_nid_postfix(japanese, UCAL_ERA, &status) == 236 && ucal_get_nid_postfix(japanese, UCAL_YEAR, &status) == 5, "2023 is Reiwa 5");
    ucal_close_nid_postfix(japanese);
    ucal_close_nid_postfix(calendar);
    ucal_close_nid_postfix(nullptr);
    SetDefaults("de");
    status = Ok;
    calendar = ucal_open_nid_postfix(u"Europe/Berlin", -1, nullptr, UCAL_GREGORIAN, &status);
    Require(calendar && ucal_getAttribute_nid_postfix(calendar, UCAL_FIRST_DAY_OF_WEEK) == UCAL_MONDAY, "a NULL locale uses the guest default de");
    status = Ok;
    Require(ucal_getTimeZoneDisplayName_nid_postfix(calendar, UCAL_STANDARD, "qx", buffer, 64, &status) == 28 && std::u16string(buffer) == u"Mitteleuropäische Normalzeit", "a name locale falls back to the guest default");
    ucal_close_nid_postfix(calendar);
    SetDefaults(nullptr);
}

const char16_t* const Patterns[] = {u"yyyy-MM-dd HH:mm:ss.SSS zzzz", u"EEEE d MMMM y G", u"h:mm a vvvv", u"QQQQ w W D F E", u"B h", u"''yy'' r U", u"VVVV OOOO XXX xxx", u"", u"cccc LLLL"};

struct Style {
    std::int32_t time;
    std::int32_t date;
};

const Style Styles[] = {{UDAT_FULL, UDAT_NONE}, {UDAT_SHORT, UDAT_NONE}, {UDAT_NONE, UDAT_FULL}, {UDAT_NONE, UDAT_MEDIUM}, {UDAT_SHORT, UDAT_LONG}, {UDAT_MEDIUM, UDAT_SHORT}, {UDAT_NONE, UDAT_FULL_RELATIVE}, {UDAT_SHORT, UDAT_SHORT_RELATIVE}, {UDAT_NONE, UDAT_NONE}, {7, 9}, {UDAT_PATTERN, UDAT_PATTERN}};

void CompareDateFormats(void* guest, UDateFormat* host, const std::string& what) {
    for (double date : Dates) {
        for (std::int32_t field : {std::int32_t{UDAT_YEAR_FIELD}, std::int32_t{UDAT_TIMEZONE_FIELD}, std::int32_t{UDAT_HOUR1_FIELD}}) {
            UFieldPosition position{field, 0, 0};
            UFieldPosition hostPosition{field, 0, 0};
            Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udat_format_nid_postfix(guest, date, b, c, &position, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udat_format(host, date, b, c, &hostPosition, s); }, what + " format");
            Require(position.beginIndex == hostPosition.beginIndex && position.endIndex == hostPosition.endIndex, what + " field position matches ICU");
        }
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UFieldPositionIterator* iterator = ufieldpositer_open_nid_postfix(&status);
        UFieldPositionIterator* hostIterator = ufieldpositer_open(&hostStatus);
        Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udat_formatForFields_nid_postfix(guest, date, b, c, iterator, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udat_formatForFields(host, date, b, c, hostIterator, s); }, what + " formatForFields");
        for (;;) {
            std::int32_t begin = 0, end = 0, hostBegin = 0, hostEnd = 0;
            const std::int32_t field = ufieldpositer_next_nid_postfix(iterator, &begin, &end);
            Require(field == ufieldpositer_next(hostIterator, &hostBegin, &hostEnd) && begin == hostBegin && end == hostEnd, what + " fields match ICU");
            if (field < 0) break;
        }
        ufieldpositer_close_nid_postfix(iterator);
        ufieldpositer_close(hostIterator);
    }
    for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}}) {
        Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udat_toPattern_nid_postfix(guest, localized, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udat_toPattern(host, localized, b, c, s); }, what + " toPattern");
    }
    for (std::int32_t type = UDAT_ERAS; type <= UDAT_ZODIAC_NAMES_NARROW + 1; ++type) {
        const std::int32_t count = udat_countSymbols_nid_postfix(guest, type);
        Require(count == udat_countSymbols(host, static_cast<UDateFormatSymbolType>(type)), what + " symbol count matches ICU");
        for (std::int32_t index = 0; index <= count; ++index) {
            Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udat_getSymbols_nid_postfix(guest, type, index, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udat_getSymbols(host, static_cast<UDateFormatSymbolType>(type), index, b, c, s); }, what + " symbol " + std::to_string(type) + " " + std::to_string(index));
        }
    }
    const void* calendar = udat_getCalendar_nid_postfix(guest);
    const UCalendar* hostCalendar = udat_getCalendar(host);
    for (std::int32_t attr = UCAL_LENIENT; attr <= UCAL_SKIPPED_WALL_TIME; ++attr) {
        Require(ucal_getAttribute_nid_postfix(calendar, attr) == ucal_getAttribute(hostCalendar, static_cast<UCalendarAttribute>(attr)), what + " calendar attribute matches ICU");
    }
    for (std::int32_t day = UCAL_SUNDAY; day <= UCAL_SATURDAY; ++day) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(ucal_getDayOfWeekType_nid_postfix(calendar, day, &status) == ucal_getDayOfWeekType(hostCalendar, static_cast<UCalendarDaysOfWeek>(day), &hostStatus), what + " calendar weekend matches ICU");
    }
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(ucal_get_nid_postfix(calendar, UCAL_ERA, &status) == ucal_get(hostCalendar, UCAL_ERA, &hostStatus), what + " calendar era matches ICU");
}

void CheckDateFormats(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        for (const char16_t* zone : {u"Europe/Berlin", u"America/New_York", u"Asia/Kolkata"}) {
            for (const Style& style : Styles) {
                for (const char16_t* pattern : Patterns) {
                    if (style.time != UDAT_PATTERN && pattern != Patterns[0]) continue;
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    void* guest = udat_open_nid_postfix(style.time, style.date, locale, zone, -1, pattern, -1, &status);
                    UDateFormat* host = udat_open(static_cast<UDateFormatStyle>(style.time), static_cast<UDateFormatStyle>(style.date), locale, zone, -1, pattern, -1, &hostStatus);
                    const std::string what = label + " date " + (locale ? locale : "NULL") + " " + std::to_string(style.time) + "/" + std::to_string(style.date) + " " + std::to_string(std::char_traits<char16_t>::length(pattern));
                    Require(status == hostStatus && !guest == !host, what + " opens like ICU");
                    if (!guest) continue;
                    ++opened;
                    CompareDateFormats(guest, host, what);
                    udat_close_nid_postfix(guest);
                    udat_close(host);
                }
            }
        }
    }
}

void CheckDateFormatErrors() {
    SetDefaults(nullptr);
    std::int32_t status = IllegalArgument;
    Require(!udat_open_nid_postfix(UDAT_SHORT, UDAT_SHORT, nullptr, nullptr, -1, nullptr, 0, &status) && status == IllegalArgument, "a failed status returns before the defaults");
    Require(Throws<std::invalid_argument>([] { udat_open_nid_postfix(UDAT_SHORT, UDAT_SHORT, "en", u"UTC", -1, nullptr, 0, nullptr); }), "a null status throws in udat_open");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udat_open_nid_postfix(UDAT_SHORT, UDAT_SHORT, "en", nullptr, -1, nullptr, 0, &ok); }), "a NULL zone throws");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udat_open_nid_postfix(UDAT_PATTERN, UDAT_PATTERN, "en@timezone=Europe/Paris", nullptr, -1, u"zzzz", -1, &ok); }), "a NULL zone throws even with a timezone keyword");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udat_open_nid_postfix(UDAT_SHORT, UDAT_SHORT, nullptr, u"UTC", -1, nullptr, 0, &ok); }), "a NULL locale without a guest default throws");
    for (const char* locale : {"xx", "qq_Latn", "xx_YY", "en@compat=host"}) {
        Require(Throws<std::runtime_error>([locale] { std::int32_t ok = Ok; udat_open_nid_postfix(UDAT_LONG, UDAT_LONG, locale, u"UTC", -1, nullptr, 0, &ok); }), std::string("a date locale without data or asking for the host throws: ") + locale);
    }
    status = Ok;
    void* format = udat_open_nid_postfix(UDAT_PATTERN, UDAT_PATTERN, "en_US", u"America/New_York", -1, u"EEE, MMM d, y h:mm a zzz", -1, &status);
    char16_t buffer[64] = {};
    UFieldPosition position{UDAT_YEAR_FIELD, 0, 0};
    Require(udat_format_nid_postfix(format, 1.7e12, buffer, 64, &position, &status) == 29 && std::u16string(buffer) == u"Tue, Nov 14, 2023 5:13 PM EST" && position.beginIndex == 13 && position.endIndex == 17, "a known date");
    status = Ok;
    Require(udat_format_nid_postfix(format, 1.7e12, nullptr, 0, nullptr, &status) == 29 && status == BufferOverflow, "format preflights");
    for (bool fields : {false, true}) {
        auto call = [fields](const void* f, char16_t* b, std::int32_t c, std::int32_t* s) { return fields ? udat_formatForFields_nid_postfix(f, 0, b, c, nullptr, s) : udat_format_nid_postfix(f, 0, b, c, nullptr, s); };
        status = Ok;
        Require(call(nullptr, nullptr, 4, &status) == -1 && status == IllegalArgument, "a NULL buffer with a size is illegal before the format");
        status = Ok;
        Require(call(nullptr, buffer, -1, &status) == -1 && status == IllegalArgument, "a negative size is illegal before the format");
        status = IllegalArgument;
        Require(call(nullptr, buffer, 64, &status) == -1, "a failed status returns -1");
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; call(nullptr, buffer, 64, &ok); }), "a null format throws");
        Require(Throws<std::invalid_argument>([&] { call(format, buffer, 64, nullptr); }), "a null status throws");
    }
    status = Ok;
    Require(udat_formatForFields_nid_postfix(format, 1.7e12, buffer, 64, nullptr, &status) == 29, "formatForFields takes a null iterator");
    status = Ok;
    Require(udat_toPattern_nid_postfix(nullptr, 0, buffer, 64, &status) == -1 && status == IllegalArgument, "toPattern rejects a NULL format, as in ICU");
    Require(Throws<std::invalid_argument>([&] { udat_toPattern_nid_postfix(format, 0, buffer, 64, nullptr); }), "a null status throws in toPattern");
    Require(udat_getSymbols_nid_postfix(nullptr, UDAT_MONTHS, 0, buffer, 64, nullptr) == -1, "getSymbols returns -1 for a NULL format without the status");
    Require(udat_countSymbols_nid_postfix(nullptr, UDAT_MONTHS) == 0, "countSymbols returns 0 for a NULL format");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udat_getSymbols_nid_postfix(format, UDAT_MONTHS, -1, buffer, 64, &ok); }), "a negative symbol index ICU would read before its array throws");
    Require(Throws<std::invalid_argument>([&] { udat_getSymbols_nid_postfix(format, UDAT_MONTHS, 0, buffer, 64, nullptr); }), "a null status throws in getSymbols");
    status = Ok;
    Require(udat_getSymbols_nid_postfix(format, 99, 0, buffer, 64, &status) == 0 && status == Ok, "an unknown symbol type is empty");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udat_getSymbols_nid_postfix(format, 99, -1, buffer, 64, &ok); }), "a negative index throws for an unknown type too, where ICU reads before a NULL array");
    status = Ok;
    Require(udat_getSymbols_nid_postfix(format, UDAT_LOCALIZED_CHARS, -7, buffer, 64, &status) > 0, "the localized pattern characters ignore the index");
    Require(udat_getSymbols_nid_postfix(format, UDAT_MONTHS, 10, buffer, 64, &status) == 8 && std::u16string(buffer) == u"November", "a known month");
    Require(Throws<std::invalid_argument>([] { udat_getCalendar_nid_postfix(nullptr); }), "a null format throws in getCalendar");
    udat_close_nid_postfix(format);
    udat_close_nid_postfix(nullptr);
    SetDefaults("ar_EG");
    status = Ok;
    void* fallen = udat_open_nid_postfix(UDAT_PATTERN, UDAT_PATTERN, "zz", u"Europe/Paris", -1, u"d MMMM y zzzz", -1, &status);
    void* own = udat_open_nid_postfix(UDAT_PATTERN, UDAT_PATTERN, "fr", u"Europe/Paris", -1, u"d MMMM y", -1, &status);
    Require(fallen && own && status <= Ok, "date formats open under a guest default");
    status = Ok;
    Require(udat_format_nid_postfix(fallen, 1.7e12, buffer, 64, nullptr, &status) > 0 && buffer[0] == 0x0661, "zz falls back to the guest default ar_EG");
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; udat_format_nid_postfix(fallen, 0, buffer, 64, nullptr, &ok); }), "a date format that fell back to a cleared default throws");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; udat_formatForFields_nid_postfix(fallen, 0, buffer, 64, nullptr, &ok); }), "formatForFields throws too");
    status = Ok;
    Require(udat_format_nid_postfix(own, 1.7e12, buffer, 64, nullptr, &status) == 16 && std::u16string(buffer) == u"14 novembre 2023", "a date format with its own data still works");
    udat_close_nid_postfix(fallen);
    udat_close_nid_postfix(own);
    SetDefaults(nullptr);
}

const char16_t* const Skeletons[] = {u"yMMMd", u"jmm", u"Hm", u"hm", u"yQQQ", u"EEEEd", u"MMMMy", u"jjmmss", u"Bh", u"vvvv", u"", u"xyz", u"GyMMMEd", u"Jmm"};

void CheckPatternGenerators(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        void* guest = udatpg_open_nid_postfix(locale, &status);
        UDateTimePatternGenerator* host = udatpg_open(locale, &hostStatus);
        const std::string what = label + " generator " + (locale ? locale : "NULL");
        Require(status == hostStatus && !guest == !host, what + " opens like ICU");
        for (const char16_t* skeleton : Skeletons) {
            for (std::int32_t options : {std::int32_t{UDATPG_MATCH_NO_OPTIONS}, std::int32_t{UDATPG_MATCH_HOUR_FIELD_LENGTH}, std::int32_t{UDATPG_MATCH_ALL_FIELDS_LENGTH}}) {
                Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udatpg_getBestPatternWithOptions_nid_postfix(guest, skeleton, -1, options, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udatpg_getBestPatternWithOptions(host, skeleton, -1, static_cast<UDateTimePatternMatchOptions>(options), b, c, s); }, what + " best pattern");
            }
            Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udatpg_getBestPattern_nid_postfix(guest, skeleton, -1, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udatpg_getBestPattern(host, skeleton, -1, b, c, s); }, what + " best pattern without options");
        }
        for (std::int32_t field = 0; field < UDATPG_FIELD_COUNT; ++field) {
            for (std::int32_t width = 0; width <= UDATPG_NARROW; ++width) {
                Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udatpg_getFieldDisplayName_nid_postfix(guest, field, width, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udatpg_getFieldDisplayName(host, static_cast<UDateTimePatternField>(field), static_cast<UDateTimePGDisplayWidth>(width), b, c, s); }, what + " field name");
            }
        }
        udatpg_close_nid_postfix(guest);
        udatpg_close(host);
    }
}

void CheckPatternGeneratorErrors() {
    SetDefaults(nullptr);
    for (const char16_t* pattern : Patterns) {
        Compare([&](char16_t* b, std::int32_t c, std::int32_t* s) { return udatpg_getSkeleton_nid_postfix(nullptr, pattern, -1, b, c, s); }, [&](char16_t* b, std::int32_t c, UErrorCode* s) { return udatpg_getSkeleton(nullptr, pattern, -1, b, c, s); }, "a skeleton");
    }
    std::int32_t status = Ok;
    char16_t buffer[64] = {};
    Require(udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 3, buffer, 64, &status) == 0 && status == IllegalArgument, "a NULL pattern with a length is illegal");
    Require(Throws<std::invalid_argument>([&] { udatpg_getSkeleton_nid_postfix(nullptr, u"y", -1, buffer, 64, nullptr); }), "a null status throws in getSkeleton");
    status = IllegalArgument;
    Require(!udatpg_open_nid_postfix(nullptr, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::invalid_argument>([] { udatpg_open_nid_postfix("en", nullptr); }), "a null status throws in udatpg_open");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udatpg_open_nid_postfix(nullptr, &ok); }), "a NULL locale without a guest default throws");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udatpg_open_nid_postfix("xx", &ok); }), "a generator locale without data throws");
    status = Ok;
    void* generator = udatpg_open_nid_postfix("en_US", &status);
    Require(udatpg_getBestPattern_nid_postfix(generator, u"yMMMd", 5, buffer, 64, &status) == 8 && std::u16string(buffer) == u"MMM d, y", "a known best pattern");
    status = Ok;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 2, buffer, 64, &status) == 0 && status == IllegalArgument, "a NULL skeleton with a length is illegal before the generator");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udatpg_getBestPattern_nid_postfix(nullptr, u"y", -1, buffer, 64, &ok); }), "a null generator throws");
    Require(Throws<std::invalid_argument>([&] { udatpg_getBestPatternWithOptions_nid_postfix(generator, u"y", -1, 0, buffer, 64, nullptr); }), "a null status throws in getBestPattern");
    status = IllegalArgument;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 2, buffer, 64, &status) == 0, "getBestPattern returns on a failed status");
    status = Ok;
    Require(udatpg_getFieldDisplayName_nid_postfix(generator, UDATPG_YEAR_FIELD, UDATPG_WIDE, buffer, 64, &status) == 4 && std::u16string(buffer) == u"year", "a known field name");
    status = Ok;
    Require(udatpg_getFieldDisplayName_nid_postfix(generator, UDATPG_YEAR_FIELD, UDATPG_WIDE, nullptr, 0, &status) == 4 && status == Ok, "a NULL field name buffer returns the length, as in ICU");
    status = Ok;
    Require(udatpg_getFieldDisplayName_nid_postfix(nullptr, 99, 99, nullptr, 3, &status) == -1 && status == IllegalArgument, "a NULL buffer with a capacity is illegal first");
    for (const auto& [field, width] : {std::pair{-1, 0}, std::pair{std::int32_t{UDATPG_FIELD_COUNT}, 0}, std::pair{0, -1}, std::pair{0, std::int32_t{UDATPG_NARROW} + 1}}) {
        Require(Throws<std::invalid_argument>([&, field = field, width = width] { std::int32_t ok = Ok; udatpg_getFieldDisplayName_nid_postfix(generator, field, width, buffer, 64, &ok); }), "a field or width ICU would index with throws");
    }
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, buffer, 64, &ok); }), "a null generator throws in getFieldDisplayName");
    Require(Throws<std::invalid_argument>([&] { udatpg_getFieldDisplayName_nid_postfix(generator, 0, 0, buffer, 64, nullptr); }), "a null status throws in getFieldDisplayName");
    udatpg_close_nid_postfix(generator);
    udatpg_close_nid_postfix(nullptr);
}

const double Ranges[][2] = {{1.7e12, 1.7e12 + 3600000}, {1.7e12, 1.7e12 + 86400000.0 * 40}, {1.6e12, 1.7e12}, {0, 0}, {1.7e12 + 5, 1.7e12}};

void CheckIntervals(const std::vector<const char*>& locales, const std::string& label) {
    for (const char* locale : locales) {
        for (const char16_t* skeleton : {u"yMMMd", u"jm", u"yMMMMEEEEd", u"hm", u"Hm", u"", u"GyMMMd"}) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            UDateIntervalFormat* guest = udtitvfmt_open_nid_postfix(locale, skeleton, -1, u"America/Sao_Paulo", -1, &status);
            UDateIntervalFormat* host = udtitvfmt_open(locale, skeleton, -1, u"America/Sao_Paulo", -1, &hostStatus);
            const std::string what = label + " interval " + (locale ? locale : "NULL") + " " + std::to_string(std::char_traits<char16_t>::length(skeleton));
            Require(status == hostStatus && !guest == !host, what + " opens like ICU");
            if (!guest) continue;
            UFormattedDateInterval* result = udtitvfmt_openResult_nid_postfix(&status);
            UFormattedDateInterval* hostResult = udtitvfmt_openResult(&hostStatus);
            for (const auto& range : Ranges) {
                for (bool calendars : {false, true}) {
                    status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    if (calendars) {
                        void* from = ucal_open_nid_postfix(u"Asia/Tokyo", -1, "en", UCAL_GREGORIAN, &status);
                        void* to = ucal_open_nid_postfix(u"Asia/Tokyo", -1, "en", UCAL_GREGORIAN, &status);
                        UCalendar* hostFrom = ucal_open(u"Asia/Tokyo", -1, "en", UCAL_GREGORIAN, &hostStatus);
                        UCalendar* hostTo = ucal_open(u"Asia/Tokyo", -1, "en", UCAL_GREGORIAN, &hostStatus);
                        ucal_setMillis_nid_postfix(from, range[0], &status);
                        ucal_setMillis_nid_postfix(to, range[1], &status);
                        ucal_setMillis(hostFrom, range[0], &hostStatus);
                        ucal_setMillis(hostTo, range[1], &hostStatus);
                        udtitvfmt_formatCalendarToResult_nid_postfix(guest, from, to, result, &status);
                        udtitvfmt_formatCalendarToResult(host, hostFrom, hostTo, hostResult, &hostStatus);
                        ucal_close_nid_postfix(from);
                        ucal_close_nid_postfix(to);
                        ucal_close(hostFrom);
                        ucal_close(hostTo);
                    } else {
                        udtitvfmt_formatToResult_nid_postfix(guest, range[0], range[1], result, &status);
                        udtitvfmt_formatToResult(host, range[0], range[1], hostResult, &hostStatus);
                    }
                    Require(status == hostStatus, what + " formats like ICU");
                    const UFormattedValue* value = udtitvfmt_resultAsValue_nid_postfix(result, &status);
                    const UFormattedValue* hostValue = udtitvfmt_resultAsValue(hostResult, &hostStatus);
                    std::int32_t length = -5;
                    std::int32_t hostLength = -5;
                    const char16_t* string = ufmtval_getString_nid_postfix(value, &length, &status);
                    const char16_t* hostString = ufmtval_getString(hostValue, &hostLength, &hostStatus);
                    Require(status == hostStatus && length == hostLength && !string == !hostString && (!string || std::u16string(string, length) == std::u16string(hostString, hostLength)), what + " interval text matches ICU");
                }
            }
            udtitvfmt_closeResult_nid_postfix(result);
            udtitvfmt_closeResult(hostResult);
            udtitvfmt_close_nid_postfix(guest);
            udtitvfmt_close(host);
        }
    }
}

void CheckIntervalErrors() {
    SetDefaults(nullptr);
    std::int32_t status = IllegalArgument;
    Require(!udtitvfmt_open_nid_postfix(nullptr, u"yMd", -1, nullptr, -1, &status) && status == IllegalArgument, "a failed status returns before the defaults");
    Require(Throws<std::invalid_argument>([] { udtitvfmt_open_nid_postfix("en", u"yMd", -1, u"UTC", -1, nullptr); }), "a null status throws in udtitvfmt_open");
    for (const auto& [skeleton, skeletonLength, zoneLength] : {std::tuple{static_cast<const char16_t*>(nullptr), 3, -1}, std::tuple{u"yMd", -2, -1}, std::tuple{u"yMd", -1, -2}}) {
        status = Ok;
        Require(!udtitvfmt_open_nid_postfix(nullptr, skeleton, skeletonLength, nullptr, zoneLength == -2 ? -2 : 0, &status) && status == IllegalArgument, "a bad skeleton or zone length is illegal before the defaults");
    }
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udtitvfmt_open_nid_postfix("en", u"yMd", -1, nullptr, 0, &ok); }), "a NULL zone throws");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udtitvfmt_open_nid_postfix(nullptr, u"yMd", -1, u"UTC", -1, &ok); }), "a NULL locale without a guest default throws");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; udtitvfmt_open_nid_postfix("xx", u"yMd", -1, u"UTC", -1, &ok); }), "an interval locale without data throws");
    status = Ok;
    UDateIntervalFormat* formatter = udtitvfmt_open_nid_postfix("en_US", u"yMMMd", -1, u"UTC", 3, &status);
    status = Ok;
    UFormattedDateInterval* result = udtitvfmt_openResult_nid_postfix(&status);
    udtitvfmt_formatToResult_nid_postfix(formatter, 1.7e12, 1.7e12 + 86400000.0 * 3, result, &status);
    std::int32_t length = 0;
    const char16_t* text = ufmtval_getString_nid_postfix(udtitvfmt_resultAsValue_nid_postfix(result, &status), &length, &status);
    Require(status == Ok && std::u16string(text, length) == u"Nov 14 – 17, 2023", "a known interval");
    alignas(16) unsigned char zeros[512] = {};
    status = Ok;
    udtitvfmt_formatToResult_nid_postfix(nullptr, 0, 1, nullptr, &status);
    Require(status == IllegalArgument, "a NULL result is illegal before the formatter");
    status = Ok;
    udtitvfmt_formatToResult_nid_postfix(nullptr, 0, 1, reinterpret_cast<UFormattedDateInterval*>(zeros), &status);
    Require(status == InvalidFormat, "a foreign result is an invalid format before the formatter");
    status = Ok;
    udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status);
    Require(status == IllegalArgument, "a NULL result is illegal before the calendars");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udtitvfmt_formatToResult_nid_postfix(nullptr, 0, 1, result, &ok); }), "a null formatter throws in formatToResult");
    Require(Throws<std::invalid_argument>([&] { udtitvfmt_formatToResult_nid_postfix(formatter, 0, 1, result, nullptr); }), "a null status throws in formatToResult");
    status = Ok;
    void* calendar = ucal_open_nid_postfix(u"UTC", -1, "en", UCAL_GREGORIAN, &status);
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udtitvfmt_formatCalendarToResult_nid_postfix(formatter, nullptr, calendar, result, &ok); }), "a null from calendar throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udtitvfmt_formatCalendarToResult_nid_postfix(formatter, calendar, nullptr, result, &ok); }), "a null to calendar throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, calendar, calendar, result, &ok); }), "a null formatter throws in formatCalendarToResult");
    status = IllegalArgument;
    udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, result, &status);
    Require(status == IllegalArgument, "formatCalendarToResult returns on a failed status");
    status = Ok;
    Require(!udtitvfmt_resultAsValue_nid_postfix(nullptr, &status) && status == IllegalArgument, "resultAsValue rejects NULL");
    Require(Throws<std::invalid_argument>([] { udtitvfmt_openResult_nid_postfix(nullptr); }), "a null status throws in openResult");
    ucal_close_nid_postfix(calendar);
    udtitvfmt_closeResult_nid_postfix(result);
    udtitvfmt_closeResult_nid_postfix(nullptr);
    udtitvfmt_close_nid_postfix(formatter);
    udtitvfmt_close_nid_postfix(nullptr);
    SetDefaults("ar_EG");
    status = Ok;
    UDateIntervalFormat* fallen = udtitvfmt_open_nid_postfix("zq", u"yMMMd", -1, u"UTC", -1, &status);
    result = udtitvfmt_openResult_nid_postfix(&status);
    Require(fallen && result && status <= Ok, "an interval format opens under a guest default");
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; udtitvfmt_formatToResult_nid_postfix(fallen, 0, 1e9, result, &ok); }), "an interval format that fell back to a cleared default throws");
    udtitvfmt_closeResult_nid_postfix(result);
    udtitvfmt_close_nid_postfix(fallen);
    SetDefaults(nullptr);
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckCalendarErrors();
    CheckDateFormatErrors();
    CheckPatternGeneratorErrors();
    CheckIntervalErrors();
    CheckCalendars();
    SetDefaults("en_US");
    CheckDateFormats({"en_US", "de", "ja_JP@calendar=japanese", "ar_EG", "fa", "zh_Hant", "th_TH", "", "en_US@numbers=arab", "en_GB@hours=h11"}, "sweep");
    CheckPatternGenerators({"en_US", "de", "ja", "en_GB", "fr_CA", "ar", "", "zh_Hant_TW", "ko", "en_US@hours=h23"}, "sweep");
    CheckIntervals({"en_US", "de", "ja", "ar_EG", "", "pt_BR", "zh"}, "sweep");
    for (const char* fallback : {"de", "ar_EG", "fa"}) {
        SetDefaults(fallback);
        CheckDateFormats({"xx", nullptr}, fallback);
        CheckPatternGenerators({"xx", nullptr}, fallback);
        CheckIntervals({"xx", nullptr}, fallback);
    }
    SetDefaults(nullptr);
    Require(opened > 1200, "the sweeps open calendars and formats");
    std::puts("guest_icu_date_format passed");
    return 0;
}

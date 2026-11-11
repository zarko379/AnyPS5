#include "prx/libSceIcu/EnumerationSources.hpp"
#include "prx/libSceIcu/Collation.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <unicode/ucal.h>
#include <unicode/ucnv.h>
#include <unicode/ucol.h>
#include <unicode/ucurr.h>
#include <unicode/udata.h>
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/unumsys.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

U_CAPI UEnumeration* U_EXPORT2 uloc_openKeywordList(const char* keywordList, int32_t keywordListSize, UErrorCode* status);

bool GuestEnumerationLayout(void* enumeration);
std::int32_t GuestEnumerationCount(void* enumeration, std::int32_t* status);
const char* GuestEnumerationNext(void* enumeration, std::int32_t* length, std::int32_t* status);
const char16_t* GuestEnumerationUnicodeNext(void* enumeration, std::int32_t* length, std::int32_t* status);
void GuestEnumerationReset(void* enumeration, std::int32_t* status);
void GuestEnumerationClose(void* enumeration);
std::int32_t GuestEnumerationCallbackCount(void* enumeration, std::int32_t* status);

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t UsingDefault = -127;
constexpr std::int32_t MemoryAllocation = 7;

std::vector<void*> freed;
std::int32_t live = 0;
bool failAllocation = false;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocation) return nullptr;
    ++live;
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) {
        freed.push_back(pointer);
        --live;
    }
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::logic_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::logic_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected posix_memalign"); }

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU ENUMERATION SOURCES: %s\n", message);
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

std::vector<std::string> HostNames(UEnumeration* host) {
    std::vector<std::string> names;
    UErrorCode status = U_ZERO_ERROR;
    uenum_reset(host, &status);
    std::int32_t length = 0;
    while (const char* name = uenum_next(host, &length, &status)) names.emplace_back(name, static_cast<std::size_t>(length));
    Require(U_SUCCESS(status), "the host enumeration lists its names");
    return names;
}

std::vector<std::u16string> HostUnicodeNames(UEnumeration* host) {
    std::vector<std::u16string> names;
    UErrorCode status = U_ZERO_ERROR;
    uenum_reset(host, &status);
    std::int32_t length = 0;
    while (const char16_t* name = uenum_unext(host, &length, &status)) names.emplace_back(name, static_cast<std::size_t>(length));
    Require(U_SUCCESS(status), "the host enumeration lists its Unicode names");
    return names;
}

void CheckSame(UEnumeration* guest, UEnumeration* host, const char* what) {
    if (!guest && !host) return;
    Require(guest && host, what);
    Require(GuestEnumerationLayout(guest), "the guest enumeration has upstream's layout with every callback");
    const std::vector<std::string> expected = HostNames(host);
    const std::vector<std::u16string> expectedUnicode = HostUnicodeNames(host);
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t hostCount = uenum_count(host, &hostStatus);
    std::int32_t status = Ok;
    Require(GuestEnumerationCount(guest, &status) == hostCount && status == hostStatus, "uenum_count matches ICU");
    status = Ok;
    Require(GuestEnumerationCallbackCount(guest, &status) == hostCount && status == hostStatus, "the count callback matches ICU");
    std::vector<std::string> names;
    std::int32_t length = -1;
    while (const char* name = GuestEnumerationNext(guest, &length, &status)) {
        Require(length == static_cast<std::int32_t>(std::strlen(name)), "uenum_next reports the name length");
        names.emplace_back(name, static_cast<std::size_t>(length));
    }
    Require(status == Ok && names == expected, what);
    Require(!GuestEnumerationNext(guest, nullptr, &status) && status == Ok, "the enumeration stays at its end without a length");
    GuestEnumerationReset(guest, &status);
    Require(status == Ok, "uenum_reset succeeds");
    std::vector<std::u16string> unicodeNames;
    while (const char16_t* name = GuestEnumerationUnicodeNext(guest, &length, &status)) unicodeNames.emplace_back(name, static_cast<std::size_t>(length));
    Require(status == Ok && unicodeNames == expectedUnicode, "uenum_unext lists the names after a reset");
    GuestEnumerationReset(guest, &status);
    if (expected.size() >= 2) {
        const char* first = GuestEnumerationNext(guest, nullptr, &status);
        Require(first && expected[0] == first, "the first name after a second reset");
        const char16_t* second = GuestEnumerationUnicodeNext(guest, nullptr, &status);
        Require(second && expectedUnicode[1] == second && status == Ok, "unext continues where next stopped");
    }
    std::int32_t failed = BufferOverflow;
    Require(!GuestEnumerationNext(guest, &length, &failed) && failed == BufferOverflow, "a failed status returns NULL");
    const std::int32_t before = live;
    GuestEnumerationClose(guest);
    Require(live == before - 1 && !freed.empty() && freed.back() == guest, "uenum_close frees the guest enumeration with the guest free");
    uenum_close(host);
}

void CheckNull(UEnumeration* guest, std::int32_t status, UEnumeration* host, UErrorCode hostStatus, const char* what) {
    Require(!guest && !host && status == hostStatus, what);
}

void CheckBridge() {
    const char* narrow[] = {"alpha", "be", ""};
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    CheckSame(uenum_openCharStringsEnumeration_nid_postfix(narrow, 3, &status), uenum_openCharStringsEnumeration(narrow, 3, &hostStatus), "a char string enumeration matches ICU");
    const char16_t* wide[] = {u"alpha", u"be", u"", u"gamma"};
    CheckSame(uenum_openUCharStringsEnumeration_nid_postfix(wide, 4, &status), uenum_openUCharStringsEnumeration(wide, 4, &hostStatus), "a UChar string enumeration matches ICU");
    CheckSame(uenum_openCharStringsEnumeration_nid_postfix(nullptr, 0, &status), uenum_openCharStringsEnumeration(nullptr, 0, &hostStatus), "an empty enumeration matches ICU");
    UEnumeration* guest = uenum_openCharStringsEnumeration_nid_postfix(narrow, -1, &status);
    Require(!guest && status == Ok, "a negative count returns NULL without an error, as in ICU");
    guest = uenum_openUCharStringsEnumeration_nid_postfix(nullptr, 2, &status);
    Require(!guest && status == Ok, "a NULL array with a count returns NULL without an error, as in ICU");
    status = IllegalArgument;
    Require(!uenum_openCharStringsEnumeration_nid_postfix(narrow, 3, &status) && status == IllegalArgument, "a failed status opens nothing");
    Require(Throws<std::invalid_argument>([&] { uenum_openCharStringsEnumeration_nid_postfix(narrow, 3, nullptr); }), "a null status throws in uenum_openCharStringsEnumeration");
    Require(Throws<std::invalid_argument>([&] { uenum_openUCharStringsEnumeration_nid_postfix(wide, 3, nullptr); }), "a null status throws in uenum_openUCharStringsEnumeration");
    status = Ok;
    guest = uenum_openCharStringsEnumeration_nid_postfix(narrow, 3, &status);
    Require(Throws<std::invalid_argument>([&] { GuestEnumerationCallbackCount(guest, nullptr); }), "a null status passed to a callback throws");
    GuestEnumerationClose(guest);
    failAllocation = true;
    status = Ok;
    Require(!uenum_openCharStringsEnumeration_nid_postfix(narrow, 3, &status) && status == MemoryAllocation, "a failed guest allocation is a memory allocation error");
    Require(Throws<std::invalid_argument>([&] { ucurr_openISOCurrencies_nid_postfix(UCURR_ALL, nullptr); }), "a failed guest allocation with a null status throws");
    failAllocation = false;
}

void CheckLocaleKeywords() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    for (const char* locale : {"de@collation=phonebook;calendar=gregorian", "en-u-ca-japanese-cu-eur", "th_TH@numbers=thai", "@a=b;c=d", "zh_Hant_TW@collation=stroke;currency=TWD;calendar=chinese"}) {
        CheckSame(uloc_openKeywords_nid_postfix(locale, &status), uloc_openKeywords(locale, &hostStatus), locale);
    }
    for (const char* locale : {"en_US", "", "root", "x"}) {
        CheckNull(uloc_openKeywords_nid_postfix(locale, &status), status, uloc_openKeywords(locale, &hostStatus), hostStatus, "a locale without keywords returns NULL without an error");
    }
    Require(!uloc_openKeywords_nid_postfix("de@a=b", nullptr), "a null status returns NULL in uloc_openKeywords, as in ICU");
    status = IllegalArgument;
    Require(!uloc_openKeywords_nid_postfix(nullptr, &status) && status == IllegalArgument, "a failed status returns before the default locale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; uloc_openKeywords_nid_postfix(nullptr, &ok); }), "a NULL locale without a guest default throws");
    status = Ok;
    uloc_setDefault_nid_postfix("ja_JP@calendar=japanese;numbers=jpanfin", &status);
    uloc_setDefault("ja_JP@calendar=japanese;numbers=jpanfin", &hostStatus);
    Require(status == Ok && U_SUCCESS(hostStatus), "the defaults are set");
    CheckSame(uloc_openKeywords_nid_postfix(nullptr, &status), uloc_openKeywords(nullptr, &hostStatus), "a NULL locale lists the guest default's keywords");
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; uloc_openKeywords_nid_postfix(nullptr, &ok); }), "a cleared default throws again");

    const char list[] = {'b', 'b', 0, 'a', 0, 'c', 'c', 'c', 0};
    CheckSame(uloc_openKeywordList_nid_postfix(list, 9, &status), uloc_openKeywordList(list, 9, &hostStatus), "a keyword list matches ICU");
    CheckSame(uloc_openKeywordList_nid_postfix(list, 5, &status), uloc_openKeywordList(list, 5, &hostStatus), "a keyword list prefix matches ICU");
    const char doubled[] = {'a', 0, 0, 'b', 0};
    CheckSame(uloc_openKeywordList_nid_postfix(doubled, 5, &status), uloc_openKeywordList(doubled, 5, &hostStatus), "a list stops at an empty keyword");
    CheckSame(uloc_openKeywordList_nid_postfix(nullptr, 0, &status), uloc_openKeywordList("", 0, &hostStatus), "an empty keyword list");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; uloc_openKeywordList_nid_postfix(list, 4, &ok); }), "a list whose last keyword is not terminated throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; uloc_openKeywordList_nid_postfix(list, -1, &ok); }), "a negative size throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; uloc_openKeywordList_nid_postfix(nullptr, 3, &ok); }), "a NULL list with a size throws");
    Require(Throws<std::invalid_argument>([&] { uloc_openKeywordList_nid_postfix(list, 9, nullptr); }), "a null status throws in uloc_openKeywordList");
    status = IllegalArgument;
    Require(!uloc_openKeywordList_nid_postfix(list, -1, &status) && status == IllegalArgument, "a failed status returns before the size check");

    for (std::int32_t type = 0; type < 3; ++type) {
        status = Ok;
        CheckSame(uloc_openAvailableByType_nid_postfix(type, &status), uloc_openAvailableByType(static_cast<ULocAvailableType>(type), &hostStatus), "an available locale type matches ICU");
    }
    for (std::int32_t type : {3, -1, 100}) {
        status = Ok;
        Require(!uloc_openAvailableByType_nid_postfix(type, &status) && status == IllegalArgument, "an unknown available type is an illegal argument");
    }
    Require(Throws<std::invalid_argument>([] { uloc_openAvailableByType_nid_postfix(0, nullptr); }), "a null status throws in uloc_openAvailableByType");
}

void CheckCurrencies() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    for (std::uint32_t type : {static_cast<std::uint32_t>(UCURR_ALL), static_cast<std::uint32_t>(UCURR_COMMON | UCURR_NON_DEPRECATED), static_cast<std::uint32_t>(UCURR_UNCOMMON), static_cast<std::uint32_t>(UCURR_DEPRECATED), 0u}) {
        CheckSame(ucurr_openISOCurrencies_nid_postfix(type, &status), ucurr_openISOCurrencies(type, &hostStatus), "an ISO currency list matches ICU");
    }
    UErrorCode ignored = U_ZERO_ERROR;
    CheckSame(ucurr_openISOCurrencies_nid_postfix(UCURR_COMMON, nullptr), ucurr_openISOCurrencies(UCURR_COMMON, &ignored), "a null status still lists the currencies, as in ICU");
    status = IllegalArgument;
    UEnumeration* failed = ucurr_openISOCurrencies_nid_postfix(UCURR_ALL, &status);
    Require(failed && status == IllegalArgument, "a failed status still lists the currencies, as in ICU");
    GuestEnumerationClose(failed);

    status = Ok;
    for (const char* locale : {"en_US", "ja", "und", "de_CH", "en@rg=chzzzz", "xx"}) {
        for (std::int8_t commonlyUsed : {std::int8_t{1}, std::int8_t{0}}) {
            CheckSame(ucurr_getKeywordValuesForLocale_nid_postfix("currency", locale, commonlyUsed, &status), ucurr_getKeywordValuesForLocale("currency", locale, commonlyUsed, &hostStatus), locale);
        }
    }
    status = IllegalArgument;
    Require(!ucurr_getKeywordValuesForLocale_nid_postfix("currency", nullptr, 1, &status) && status == IllegalArgument, "a failed status returns before the currency default locale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucurr_getKeywordValuesForLocale_nid_postfix("currency", nullptr, 1, &ok); }), "a NULL currency locale without a guest default throws");
    Require(Throws<std::invalid_argument>([] { ucurr_getKeywordValuesForLocale_nid_postfix("currency", "en", 1, nullptr); }), "a null status throws in ucurr_getKeywordValuesForLocale");
    status = Ok;
    uloc_setDefault_nid_postfix("fr_CA", &status);
    uloc_setDefault("fr_CA", &hostStatus);
    CheckSame(ucurr_getKeywordValuesForLocale_nid_postfix("currency", nullptr, 1, &status), ucurr_getKeywordValuesForLocale("currency", nullptr, 1, &hostStatus), "a NULL currency locale uses the guest default");
    uloc_setDefault_nid_postfix(nullptr, &status);
}

void CheckConverters() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    CheckSame(ucnv_openAllNames_nid_postfix(&status), ucnv_openAllNames(&hostStatus), "all converter names match ICU");
    for (const char* standard : {"IANA", "MIME", "WINDOWS", "", "nonexistent"}) {
        for (const char* name : {"UTF-8", "ibm-1208", "Shift_JIS", "latin1"}) {
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            UEnumeration* guest = ucnv_openStandardNames_nid_postfix(name, standard, &status);
            UEnumeration* host = ucnv_openStandardNames(name, standard, &hostStatus);
            Require(status == hostStatus, "a standard name status matches ICU");
            if (host) CheckSame(guest, host, "standard names match ICU");
            else Require(!guest, "no standard names, as in ICU");
        }
    }
    status = Ok;
    Require(!ucnv_openStandardNames_nid_postfix(nullptr, "IANA", &status) && status == IllegalArgument, "a NULL converter name is an illegal argument, not the default converter");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    CheckNull(ucnv_openStandardNames_nid_postfix("no-such-converter", "IANA", &status), status, ucnv_openStandardNames("no-such-converter", "IANA", &hostStatus), hostStatus, "an unknown converter matches ICU");
    status = IllegalArgument;
    Require(!ucnv_openAllNames_nid_postfix(&status) && status == IllegalArgument, "a failed status opens no converter names");
    Require(Throws<std::invalid_argument>([] { ucnv_openAllNames_nid_postfix(nullptr); }), "a null status throws in ucnv_openAllNames");
    Require(Throws<std::invalid_argument>([] { ucnv_openStandardNames_nid_postfix("UTF-8", "IANA", nullptr); }), "a null status throws in ucnv_openStandardNames");
}

void CheckCalendars() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    CheckSame(ucal_openTimeZones_nid_postfix(&status), ucal_openTimeZones(&hostStatus), "all time zones match ICU");
    const std::int32_t eastern = -5 * 60 * 60 * 1000;
    const std::int32_t india = 19800000;
    for (std::int32_t type : {0, 1, 2}) {
        for (const char* region : {static_cast<const char*>(nullptr), "US", "IN", "001", "ZZ"}) {
            for (const std::int32_t* offset : {static_cast<const std::int32_t*>(nullptr), &eastern, &india}) {
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                CheckSame(ucal_openTimeZoneIDEnumeration_nid_postfix(type, region, offset, &status), ucal_openTimeZoneIDEnumeration(static_cast<USystemTimeZoneType>(type), region, offset, &hostStatus), "a time zone ID enumeration matches ICU");
            }
        }
    }
    status = IllegalArgument;
    Require(!ucal_openTimeZones_nid_postfix(&status) && status == IllegalArgument, "a failed status opens no time zones");
    Require(!ucal_openTimeZoneIDEnumeration_nid_postfix(0, nullptr, nullptr, &status) && status == IllegalArgument, "a failed status opens no time zone IDs");
    Require(Throws<std::invalid_argument>([] { ucal_openTimeZones_nid_postfix(nullptr); }), "a null status throws in ucal_openTimeZones");
    Require(Throws<std::invalid_argument>([] { ucal_openTimeZoneIDEnumeration_nid_postfix(0, nullptr, nullptr, nullptr); }), "a null status throws in ucal_openTimeZoneIDEnumeration");

    status = Ok;
    for (const char* locale : {"th_TH", "ja", "ar_SA", "en_US", "und", "xx", "en@rg=thzzzz"}) {
        for (std::int8_t commonlyUsed : {std::int8_t{1}, std::int8_t{0}}) {
            CheckSame(ucal_getKeywordValuesForLocale_nid_postfix("calendar", locale, commonlyUsed, &status), ucal_getKeywordValuesForLocale("calendar", locale, commonlyUsed, &hostStatus), locale);
        }
    }
    status = IllegalArgument;
    Require(!ucal_getKeywordValuesForLocale_nid_postfix("calendar", nullptr, 1, &status) && status == IllegalArgument, "a failed status returns before the calendar default locale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucal_getKeywordValuesForLocale_nid_postfix("calendar", nullptr, 1, &ok); }), "a NULL calendar locale without a guest default throws");
    Require(Throws<std::invalid_argument>([] { ucal_getKeywordValuesForLocale_nid_postfix("calendar", "en", 1, nullptr); }), "a null status throws in ucal_getKeywordValuesForLocale");
    status = Ok;
    uloc_setDefault_nid_postfix("th_TH", &status);
    uloc_setDefault("th_TH", &hostStatus);
    CheckSame(ucal_getKeywordValuesForLocale_nid_postfix("calendar", nullptr, 1, &status), ucal_getKeywordValuesForLocale("calendar", nullptr, 1, &hostStatus), "a NULL calendar locale uses the guest default");
    uloc_setDefault_nid_postfix(nullptr, &status);

    CheckSame(unumsys_openAvailableNames_nid_postfix(&status), unumsys_openAvailableNames(&hostStatus), "numbering system names match ICU");
    status = IllegalArgument;
    Require(!unumsys_openAvailableNames_nid_postfix(&status) && status == IllegalArgument, "a failed status opens no numbering systems");
    Require(Throws<std::invalid_argument>([] { unumsys_openAvailableNames_nid_postfix(nullptr); }), "a null status throws in unumsys_openAvailableNames");
}

void CheckCollationKeywords() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    CheckSame(ucol_getKeywordValues_nid_postfix("collation", &status), ucol_getKeywordValues("collation", &hostStatus), "collation keyword values match ICU");
    for (const char* keyword : {"calendar", "", static_cast<const char*>(nullptr)}) {
        status = Ok;
        Require(!ucol_getKeywordValues_nid_postfix(keyword, &status) && status == IllegalArgument, "only the collation keyword has values");
    }
    status = BufferOverflow;
    Require(!ucol_getKeywordValues_nid_postfix(nullptr, &status) && status == BufferOverflow, "a failed status returns before the keyword check");
    Require(Throws<std::invalid_argument>([] { ucol_getKeywordValues_nid_postfix("collation", nullptr); }), "a null status throws in ucol_getKeywordValues");

    for (const char* locale : {"de", "zh", "es", "sv_SE", "ja@collation=unihan", "root", ""}) {
        for (std::int8_t commonlyUsed : {std::int8_t{1}, std::int8_t{0}}) {
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            CheckSame(ucol_getKeywordValuesForLocale_nid_postfix("collation", locale, commonlyUsed, &status), ucol_getKeywordValuesForLocale("collation", locale, commonlyUsed, &hostStatus), locale);
            Require(status == hostStatus, "the status of a collation keyword lookup matches ICU");
        }
    }
    status = IllegalArgument;
    Require(!ucol_getKeywordValuesForLocale_nid_postfix("collation", nullptr, 0, &status) && status == IllegalArgument, "a failed status returns before the collation default locale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucol_getKeywordValuesForLocale_nid_postfix("collation", nullptr, 0, &ok); }), "a NULL collation locale without a guest default throws");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucol_getKeywordValuesForLocale_nid_postfix("collation", "xx", 0, &ok); }), "a fallback through the default locale without a guest default throws");
    Require(Throws<std::invalid_argument>([] { ucol_getKeywordValuesForLocale_nid_postfix("collation", "de", 0, nullptr); }), "a null status throws in ucol_getKeywordValuesForLocale");
    status = Ok;
    uloc_setDefault_nid_postfix("de", &status);
    uloc_setDefault("de", &hostStatus);
    for (const char* locale : {static_cast<const char*>(nullptr), "xx"}) {
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        CheckSame(ucol_getKeywordValuesForLocale_nid_postfix("collation", locale, 0, &status), ucol_getKeywordValuesForLocale("collation", locale, 0, &hostStatus), "a collation lookup through the guest default matches ICU");
        Require(status == hostStatus, "the default lookup status matches ICU");
    }
    status = Ok;
    UEnumeration* fallback = ucol_getKeywordValuesForLocale_nid_postfix("collation", "xx", 0, &status);
    Require(fallback && status == UsingDefault, "an unknown locale falls back to the guest default");
    GuestEnumerationClose(fallback);
    uloc_setDefault_nid_postfix(nullptr, &status);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckBridge();
    CheckLocaleKeywords();
    CheckCurrencies();
    CheckConverters();
    CheckCalendars();
    CheckCollationKeywords();
    Require(live == 0, "every guest enumeration is freed");
    std::puts("guest_icu_enumeration_sources passed");
    return 0;
}

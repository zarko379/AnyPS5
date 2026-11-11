#include "prx/libSceIcu/ResourceBundle.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

U_CAPI UResourceBundle* U_EXPORT2 ures_openNoDefault(const char* path, const char* localeID, UErrorCode* status);
U_CAPI void U_EXPORT2 ures_openDirectFillIn(UResourceBundle* r, const char* path, const char* localeID, UErrorCode* status);
U_CAPI UResourceBundle* U_EXPORT2 ures_findResource(const char* pathToResource, UResourceBundle* fillIn, UErrorCode* status);
U_CAPI UResourceBundle* U_EXPORT2 ures_findSubResource(const UResourceBundle* resB, char* pathToResource, UResourceBundle* fillIn, UErrorCode* status);
U_CAPI UResourceBundle* U_EXPORT2 ures_getByKeyWithFallback(const UResourceBundle* resB, const char* inKey, UResourceBundle* fillIn, UErrorCode* status);
U_CAPI const UChar* U_EXPORT2 ures_getStringByKeyWithFallback(const UResourceBundle* resB, const char* inKey, int32_t* len, UErrorCode* status);
U_CAPI UEnumeration* U_EXPORT2 ures_getKeywordValues(const char* path, const char* keyword, UErrorCode* status);
U_CAPI int32_t U_EXPORT2 ures_getFunctionalEquivalent(char* result, int32_t resultCapacity, const char* path, const char* resName, const char* keyword, const char* locid, UBool* isAvailable, UBool omitDefault, UErrorCode* status);
U_CAPI const char* U_EXPORT2 ures_getVersionNumberInternal(const UResourceBundle* resourceBundle);
U_CAPI const char* U_EXPORT2 ures_getLocaleInternal(const UResourceBundle* resourceBundle, UErrorCode* status);
U_CAPI void U_EXPORT2 ures_getVersionByKey(const UResourceBundle* resB, const char* key, UVersionInfo ver, UErrorCode* status);

bool GuestEnumerationLayout(void* enumeration);
std::int32_t GuestEnumerationCount(void* enumeration, std::int32_t* status);
const char* GuestEnumerationNext(void* enumeration, std::int32_t* length, std::int32_t* status);
void GuestEnumerationClose(void* enumeration);

namespace {

constexpr std::int32_t Ok = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::logic_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::logic_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected posix_memalign"); }

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU RESOURCE BUNDLE: %s\n", message.c_str());
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

template<typename TAction>
bool NotImplemented(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void SetDefaults(const char* locale) {
    std::int32_t status = Ok;
    u_cleanup_nid_postfix();
    u_cleanup();
    UErrorCode accessStatus = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &accessStatus);
    if (locale) uloc_setDefault_nid_postfix(locale, &status);
    UErrorCode hostStatus = U_ZERO_ERROR;
    uloc_setDefault(locale ? locale : "en_US_POSIX", &hostStatus);
    Require(status == Ok && U_SUCCESS(hostStatus), std::string("the defaults are set to ") + (locale ? locale : "none"));
}

std::string Name(const char* text) {
    return text ? text : "(null)";
}

std::u16string Text(const char16_t* text, std::int32_t length) {
    return text ? std::u16string(text, static_cast<std::size_t>(length)) : std::u16string(u"(null)");
}

void Compare(UResourceBundle* guest, UResourceBundle* host, const std::string& what, std::int32_t depth) {
    Require(ures_getType_nid_postfix(guest) == ures_getType(host), what + ": type");
    Require(ures_getSize_nid_postfix(guest) == ures_getSize(host), what + ": size");
    Require(Name(ures_getKey_nid_postfix(guest)) == Name(ures_getKey(host)), what + ": key");
    Require(ures_hasNext_nid_postfix(guest) == ures_hasNext(host), what + ": hasNext");
    for (std::int32_t type : {ULOC_ACTUAL_LOCALE, ULOC_VALID_LOCALE, ULOC_REQUESTED_LOCALE}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::string guestName = Name(ures_getLocaleByType_nid_postfix(guest, type, &status));
        const std::string hostName = Name(ures_getLocaleByType(host, static_cast<ULocDataLocaleType>(type), &hostStatus));
        Require(guestName == hostName && status == hostStatus, what + ": locale by type " + std::to_string(type));
    }
    {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(Name(ures_getLocale_nid_postfix(guest, &status)) == Name(ures_getLocale(host, &hostStatus)) && status == hostStatus, what + ": getLocale");
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(Name(ures_getLocaleInternal_nid_postfix(guest, &status)) == Name(ures_getLocaleInternal(host, &hostStatus)) && status == hostStatus, what + ": getLocaleInternal");
    }
    {
        std::int32_t length = -5;
        std::int32_t hostLength = -5;
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const char16_t* text = ures_getString_nid_postfix(guest, &length, &status);
        const UChar* hostText = ures_getString(host, &hostLength, &hostStatus);
        Require(status == hostStatus && length == hostLength && Text(text, length) == Text(hostText, hostLength), what + ": string");
        for (std::int32_t capacity : {0, 1, 4, 300}) {
            for (std::int8_t forceCopy : {0, 1}) {
                std::vector<char> buffer(static_cast<std::size_t>(capacity) + 1, 'x');
                std::vector<char> hostBuffer(static_cast<std::size_t>(capacity) + 1, 'x');
                std::int32_t utf8Length = capacity;
                std::int32_t hostUtf8Length = capacity;
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                const char* utf8 = ures_getUTF8String_nid_postfix(guest, capacity ? buffer.data() : nullptr, &utf8Length, forceCopy, &status);
                const char* hostUtf8 = ures_getUTF8String(host, capacity ? hostBuffer.data() : nullptr, &hostUtf8Length, forceCopy, &hostStatus);
                Require(status == hostStatus && utf8Length == hostUtf8Length && (utf8 == nullptr) == (hostUtf8 == nullptr), what + ": UTF-8 string " + std::to_string(capacity));
                if (U_SUCCESS(hostStatus) && hostUtf8 && hostStatus != U_STRING_NOT_TERMINATED_WARNING) Require(std::string(utf8) == std::string(hostUtf8), what + ": UTF-8 text");
            }
        }
    }
    {
        std::int32_t length = -5;
        std::int32_t hostLength = -5;
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::uint8_t* binary = ures_getBinary_nid_postfix(guest, &length, &status);
        const std::uint8_t* hostBinary = ures_getBinary(host, &hostLength, &hostStatus);
        Require(status == hostStatus && length == hostLength && (binary == nullptr) == (hostBinary == nullptr), what + ": binary");
        if (binary) Require(std::memcmp(binary, hostBinary, static_cast<std::size_t>(length)) == 0, what + ": binary bytes");
        length = hostLength = -5;
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const std::int32_t* vector = ures_getIntVector_nid_postfix(guest, &length, &status);
        const std::int32_t* hostVector = ures_getIntVector(host, &hostLength, &hostStatus);
        Require(status == hostStatus && length == hostLength && (vector == nullptr) == (hostVector == nullptr), what + ": int vector");
        if (vector) Require(std::memcmp(vector, hostVector, static_cast<std::size_t>(length) * 4) == 0, what + ": int vector values");
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(ures_getInt_nid_postfix(guest, &status) == ures_getInt(host, &hostStatus) && status == hostStatus, what + ": int");
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        Require(ures_getUInt_nid_postfix(guest, &status) == ures_getUInt(host, &hostStatus) && status == hostStatus, what + ": uint");
    }
    const std::int32_t size = ures_getSize(host);
    for (std::int32_t index = -1; index <= size && index < 24; ++index) {
        std::int32_t length = -5;
        std::int32_t hostLength = -5;
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const char16_t* text = ures_getStringByIndex_nid_postfix(guest, index, &length, &status);
        const UChar* hostText = ures_getStringByIndex(host, index, &hostLength, &hostStatus);
        Require(status == hostStatus && length == hostLength && Text(text, length) == Text(hostText, hostLength), what + ": string by index " + std::to_string(index));
        char buffer[64];
        char hostBuffer[64];
        std::int32_t utf8Length = sizeof(buffer);
        std::int32_t hostUtf8Length = sizeof(hostBuffer);
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const char* utf8 = ures_getUTF8StringByIndex_nid_postfix(guest, index, buffer, &utf8Length, 1, &status);
        const char* hostUtf8 = ures_getUTF8StringByIndex(host, index, hostBuffer, &hostUtf8Length, 1, &hostStatus);
        Require(status == hostStatus && utf8Length == hostUtf8Length && (utf8 == nullptr) == (hostUtf8 == nullptr), what + ": UTF-8 by index " + std::to_string(index));
        if (utf8 && U_SUCCESS(hostStatus) && hostStatus != U_STRING_NOT_TERMINATED_WARNING) Require(std::string(utf8) == std::string(hostUtf8), what + ": UTF-8 by index text");
        if (depth <= 0) continue;
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        UResourceBundle* child = ures_getByIndex_nid_postfix(guest, index, nullptr, &status);
        UResourceBundle* hostChild = ures_getByIndex(host, index, nullptr, &hostStatus);
        Require(status == hostStatus && (child == nullptr) == (hostChild == nullptr), what + ": child " + std::to_string(index));
        if (child) {
            Compare(child, hostChild, what + "/" + std::to_string(index), depth - 1);
            const char* key = ures_getKey(hostChild);
            if (key && ures_getType(host) != URES_ARRAY) {
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                length = hostLength = -5;
                text = ures_getStringByKey_nid_postfix(guest, key, &length, &status);
                hostText = ures_getStringByKey(host, key, &hostLength, &hostStatus);
                Require(status == hostStatus && length == hostLength && Text(text, length) == Text(hostText, hostLength), what + ": string by key " + key);
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                Require(ures_countArrayItems_nid_postfix(guest, key, &status) == ures_countArrayItems(host, key, &hostStatus) && status == hostStatus, what + ": count " + key);
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                UResourceBundle* byKey = ures_getByKey_nid_postfix(guest, key, child, &status);
                UResourceBundle* hostByKey = ures_getByKey(host, key, hostChild, &hostStatus);
                Require(status == hostStatus && byKey == child && hostByKey == hostChild, what + ": by key into a fill-in " + key);
                Compare(byKey, hostByKey, what + "/" + key, 0);
            }
            ures_close_nid_postfix(child);
            ures_close(hostChild);
        }
    }
}

void CompareIteration(UResourceBundle* guest, UResourceBundle* host, const std::string& what) {
    ures_resetIterator_nid_postfix(guest);
    ures_resetIterator(host);
    UResourceBundle* fillIn = nullptr;
    UResourceBundle* hostFillIn = nullptr;
    for (std::int32_t step = 0; step < 40; ++step) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UResourceBundle* next = ures_getNextResource_nid_postfix(guest, fillIn, &status);
        UResourceBundle* hostNext = ures_getNextResource(host, hostFillIn, &hostStatus);
        Require(status == hostStatus && (next == nullptr) == (hostNext == nullptr), what + ": next resource " + std::to_string(step));
        if (fillIn) Require(next == fillIn, what + ": the fill-in is reused");
        fillIn = next;
        hostFillIn = hostNext;
        if (U_FAILURE(hostStatus)) break;
        Compare(next, hostNext, what + " next " + std::to_string(step), 0);
    }
    ures_close_nid_postfix(fillIn);
    ures_close(hostFillIn);
    ures_resetIterator_nid_postfix(guest);
    ures_resetIterator(host);
    for (std::int32_t step = 0; step < 40; ++step) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        std::int32_t length = -5;
        std::int32_t hostLength = -5;
        const char* key = nullptr;
        const char* hostKey = nullptr;
        const char16_t* text = ures_getNextString_nid_postfix(guest, &length, &key, &status);
        const UChar* hostText = ures_getNextString(host, &hostLength, &hostKey, &hostStatus);
        Require(status == hostStatus && length == hostLength && Text(text, length) == Text(hostText, hostLength) && Name(key) == Name(hostKey), what + ": next string " + std::to_string(step));
        if (U_FAILURE(hostStatus) || !ures_hasNext(host)) break;
    }
}

struct Opened {
    UResourceBundle* guest;
    UResourceBundle* host;
};

Opened OpenBoth(const char* path, const char* locale, bool direct, const std::string& what) {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    UResourceBundle* guest = direct ? ures_openDirect_nid_postfix(path, locale, &status) : ures_open_nid_postfix(path, locale, &status);
    UResourceBundle* host = direct ? ures_openDirect(path, locale, &hostStatus) : ures_open(path, locale, &hostStatus);
    Require(status == hostStatus && (guest == nullptr) == (host == nullptr), what + ": open status " + std::to_string(status) + " " + std::to_string(hostStatus));
    return {guest, host};
}

void CheckSweep() {
    struct Case {
        const char* path;
        const char* locale;
        bool direct;
        std::int32_t depth;
    };
    const Case cases[] = {
        {nullptr, "en", false, 2}, {nullptr, "de_CH", false, 2}, {"ICUDATA-lang", "fr", false, 1}, {"ICUDATA-curr", "ja", false, 2},
        {"ICUDATA-region", "sr_Latn", false, 1}, {"icudt68l-unit", "es", false, 1}, {nullptr, "supplementalData", true, 2},
        {nullptr, "currencyNumericCodes", true, 2}, {"ICUDATA-coll", "root", false, 3}, {nullptr, "zoneinfo64", true, 2},
        {"ICUDATA", "root", false, 1}, {nullptr, "", false, 1}, {nullptr, "en_US", true, 1}, {"ICUDATA-brkitr", "ja", false, 2},
    };
    for (const Case& item : cases) {
        const std::string what = Name(item.path) + ":" + item.locale;
        Opened opened = OpenBoth(item.path, item.locale, item.direct, what);
        Require(opened.guest != nullptr, what + ": opens");
        Compare(opened.guest, opened.host, what, item.depth);
        CompareIteration(opened.guest, opened.host, what);
        Require(std::string(ures_getVersionNumber_nid_postfix(opened.guest)) == ures_getVersionNumber(opened.host), what + ": version number");
        Require(std::string(ures_getVersionNumberInternal_nid_postfix(opened.guest)) == ures_getVersionNumberInternal(opened.host), what + ": internal version number");
        std::uint8_t version[4] = {9, 9, 9, 9};
        std::uint8_t hostVersion[4] = {9, 9, 9, 9};
        ures_getVersion_nid_postfix(opened.guest, version);
        ures_getVersion(opened.host, hostVersion);
        Require(std::memcmp(version, hostVersion, 4) == 0, what + ": version");
        ures_close_nid_postfix(opened.guest);
        ures_close(opened.host);
    }
    for (const char* locale : {"en", "de_AT", "zh_Hant_TW", "sr_Latn_RS", "pt", "ar_EG", "", "root", "en_US_POSIX", "i-default", "fr@calendar=buddhist"}) {
        for (const char* path : {static_cast<const char*>(nullptr), "ICUDATA-lang", "ICUDATA-region", "ICUDATA-curr", "ICUDATA-zone"}) {
            const std::string what = Name(path) + " sweep " + locale;
            Opened opened = OpenBoth(path, locale, false, what);
            Compare(opened.guest, opened.host, what, 0);
            for (const char* key : {"Version", "Languages", "Countries", "Currencies", "calendar", "layout", "NumberElements", "zoneStrings", "Keys", "%%ALIAS", "missing"}) {
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                UResourceBundle* guestKey = ures_getByKey_nid_postfix(opened.guest, key, nullptr, &status);
                UResourceBundle* hostKey = ures_getByKey(opened.host, key, nullptr, &hostStatus);
                Require(status == hostStatus && (guestKey == nullptr) == (hostKey == nullptr), what + ": by key " + key);
                if (guestKey) Compare(guestKey, hostKey, what + "/" + key, 0);
                ures_close_nid_postfix(guestKey);
                ures_close(hostKey);
                std::int32_t length = -5;
                std::int32_t hostLength = -5;
                status = Ok;
                hostStatus = U_ZERO_ERROR;
                const char16_t* text = ures_getStringByKeyWithFallback_nid_postfix(opened.guest, key, &length, &status);
                const UChar* hostText = ures_getStringByKeyWithFallback(opened.host, key, &hostLength, &hostStatus);
                Require(status == hostStatus && length == hostLength && Text(text, length) == Text(hostText, hostLength), what + ": string with fallback " + key);
            }
            ures_close_nid_postfix(opened.guest);
            ures_close(opened.host);
        }
    }
}

void CheckPaths() {
    for (const char* path : {"en/calendar/gregorian/monthNames", "de/NumberElements/latn/symbols", "/ICUDATA-lang/fr/Languages", "/ICUDATA-curr/ja/Currencies/USD", "en", "en/missing", "/ICUDATA", "/ICUDATA-lang/de", "zh_Hant/calendar/gregorian/DateTimePatterns"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UResourceBundle* found = ures_findResource_nid_postfix(path, nullptr, &status);
        UResourceBundle* hostFound = ures_findResource(path, nullptr, &hostStatus);
        Require(status == hostStatus && (found == nullptr) == (hostFound == nullptr), std::string("findResource ") + path);
        if (found) Compare(found, hostFound, std::string("findResource ") + path, 1);
        ures_close_nid_postfix(found);
        ures_close(hostFound);
    }
    Opened opened = OpenBoth(nullptr, "en", false, "sub resources");
    for (const char* sub : {"calendar/gregorian/dayNames/format/wide", "NumberElements/latn", "calendar/missing", "Version"}) {
        std::string path(sub);
        std::string hostPath(sub);
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UResourceBundle* found = ures_findSubResource_nid_postfix(opened.guest, path.data(), nullptr, &status);
        UResourceBundle* hostFound = ures_findSubResource(opened.host, hostPath.data(), nullptr, &hostStatus);
        Require(status == hostStatus && (found == nullptr) == (hostFound == nullptr), std::string("findSubResource ") + sub);
        if (found) Compare(found, hostFound, std::string("findSubResource ") + sub, 1);
        ures_close_nid_postfix(found);
        ures_close(hostFound);
    }
    for (const char* key : {"calendar/gregorian/monthNames/format/abbreviated", "NumberElements/latn/symbols/decimal", "Version", "calendar/gregorian/missing"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UResourceBundle* found = ures_getByKeyWithFallback_nid_postfix(opened.guest, key, nullptr, &status);
        UResourceBundle* hostFound = ures_getByKeyWithFallback(opened.host, key, nullptr, &hostStatus);
        Require(status == hostStatus && (found == nullptr) == (hostFound == nullptr), std::string("byKeyWithFallback ") + key);
        if (found) Compare(found, hostFound, std::string("byKeyWithFallback ") + key, 1);
        ures_close_nid_postfix(found);
        ures_close(hostFound);
    }
    std::uint8_t version[4] = {7, 7, 7, 7};
    std::uint8_t hostVersion[4] = {7, 7, 7, 7};
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    ures_getVersionByKey_nid_postfix(opened.guest, "Version", version, &status);
    ures_getVersionByKey(opened.host, "Version", hostVersion, &hostStatus);
    Require(status == hostStatus && std::memcmp(version, hostVersion, 4) == 0, "getVersionByKey reads the version");
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    ures_getVersionByKey_nid_postfix(opened.guest, "missing", version, &status);
    ures_getVersionByKey(opened.host, "missing", hostVersion, &hostStatus);
    Require(status == hostStatus && status == U_MISSING_RESOURCE_ERROR, "getVersionByKey reports a missing key");
    char buffer[8];
    char hostBuffer[8];
    std::int32_t length = sizeof(buffer);
    std::int32_t hostLength = sizeof(hostBuffer);
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    ures_getUTF8StringByKey_nid_postfix(opened.guest, "Version", buffer, &length, 1, &status);
    ures_getUTF8StringByKey(opened.host, "Version", hostBuffer, &hostLength, 1, &hostStatus);
    Require(status == hostStatus && length == hostLength && std::strncmp(buffer, hostBuffer, sizeof(buffer)) == 0, "getUTF8StringByKey copies the version");
    length = hostLength = -1;
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    ures_getUTF8StringByKey_nid_postfix(opened.guest, "Version", buffer, &length, 0, &status);
    ures_getUTF8StringByKey(opened.host, "Version", hostBuffer, &hostLength, 0, &hostStatus);
    Require(status == hostStatus && status == U_ILLEGAL_ARGUMENT_ERROR, "getUTF8StringByKey rejects a negative capacity");
    ures_close_nid_postfix(opened.guest);
    ures_close(opened.host);
}

void CheckFillIns() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    UResourceBundle* bundle = ures_open_nid_postfix(nullptr, "en", &status);
    UResourceBundle* fillIn = ures_open_nid_postfix("ICUDATA-lang", "de", &status);
    UResourceBundle* hostFillIn = ures_open("ICUDATA-lang", "de", &hostStatus);
    Require(status == Ok && bundle && fillIn, "bundles open for fill-ins");
    ures_openFillIn_nid_postfix(fillIn, "ICUDATA-region", "fr", &status);
    ures_openFillIn(hostFillIn, "ICUDATA-region", "fr", &hostStatus);
    Require(status == hostStatus, "openFillIn reopens an owned bundle");
    Compare(fillIn, hostFillIn, "openFillIn", 0);
    ures_openDirectFillIn_nid_postfix(fillIn, nullptr, "supplementalData", &status);
    ures_openDirectFillIn(hostFillIn, nullptr, "supplementalData", &hostStatus);
    Require(status == hostStatus, "openDirectFillIn reopens an owned bundle");
    Compare(fillIn, hostFillIn, "openDirectFillIn", 1);
    ures_openFillIn_nid_postfix(nullptr, nullptr, "en", &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "openFillIn rejects a NULL bundle");
    status = Ok;
    ures_openDirectFillIn_nid_postfix(nullptr, nullptr, "en", &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "openDirectFillIn rejects a NULL bundle");
    status = U_MISSING_RESOURCE_ERROR;
    ures_openFillIn_nid_postfix(nullptr, nullptr, "en", &status);
    Require(status == U_MISSING_RESOURCE_ERROR, "openFillIn keeps a failed status");
    status = Ok;
    UResourceBundle* reused = ures_getByKey_nid_postfix(bundle, "calendar", fillIn, &status);
    Require(status == Ok && reused == fillIn, "getByKey fills an owned bundle");
    reused = ures_getByIndex_nid_postfix(bundle, 0, fillIn, &status);
    Require(status == Ok && reused == fillIn, "getByIndex fills an owned bundle");
    reused = ures_getByKeyWithFallback_nid_postfix(bundle, "calendar/gregorian", fillIn, &status);
    Require(status == Ok && reused == fillIn, "getByKeyWithFallback fills an owned bundle");
    reused = ures_findResource_nid_postfix("en/calendar", fillIn, &status);
    Require(status == Ok && reused == fillIn, "findResource fills an owned bundle");
    char sub[] = "calendar/gregorian";
    reused = ures_findSubResource_nid_postfix(bundle, sub, fillIn, &status);
    Require(status == Ok && reused == fillIn, "findSubResource fills an owned bundle");
    alignas(16) unsigned char foreign[512] = {};
    auto* stranger = reinterpret_cast<UResourceBundle*>(foreign);
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getByKey_nid_postfix(bundle, "calendar", stranger, &ok); }), "a foreign fill-in throws in getByKey");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getByIndex_nid_postfix(bundle, 0, stranger, &ok); }), "a foreign fill-in throws in getByIndex");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getNextResource_nid_postfix(bundle, stranger, &ok); }), "a foreign fill-in throws in getNextResource");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getByKeyWithFallback_nid_postfix(bundle, "calendar", stranger, &ok); }), "a foreign fill-in throws in getByKeyWithFallback");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_findResource_nid_postfix("en/calendar", stranger, &ok); }), "a foreign fill-in throws in findResource");
    Require(NotImplemented([&] { std::int32_t ok = Ok; char path[] = "calendar"; ures_findSubResource_nid_postfix(bundle, path, stranger, &ok); }), "a foreign fill-in throws in findSubResource");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_openFillIn_nid_postfix(stranger, nullptr, "en", &ok); }), "a foreign fill-in throws in openFillIn");
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_openDirectFillIn_nid_postfix(stranger, nullptr, "en", &ok); }), "a foreign fill-in throws in openDirectFillIn");
    Require(Throws<std::invalid_argument>([&] { ures_getSize_nid_postfix(stranger); }), "a foreign bundle throws");
    Require(Throws<std::invalid_argument>([&] { ures_close_nid_postfix(stranger); }), "closing a foreign bundle throws");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ures_getStringByKey_nid_postfix(stranger, "x", nullptr, &ok); }), "a foreign bundle throws in getStringByKey");
    status = U_MISSING_RESOURCE_ERROR;
    Require(ures_getByKey_nid_postfix(bundle, "calendar", stranger, &status) == stranger && status == U_MISSING_RESOURCE_ERROR, "a failed status returns the fill-in untouched");
    status = Ok;
    Require(ures_getByKey_nid_postfix(nullptr, "calendar", stranger, &status) == stranger && status == U_ILLEGAL_ARGUMENT_ERROR, "a NULL bundle returns the fill-in");
    Require(ures_getByKey_nid_postfix(bundle, "calendar", nullptr, nullptr) == nullptr, "a NULL status returns the NULL fill-in");
    Require(ures_getString_nid_postfix(bundle, nullptr, nullptr) == nullptr, "getString accepts a NULL status");
    Require(ures_getSize_nid_postfix(nullptr) == 0 && ures_getType_nid_postfix(nullptr) == URES_NONE && !ures_getKey_nid_postfix(nullptr) && !ures_hasNext_nid_postfix(nullptr), "NULL bundles answer as in ICU");
    ures_resetIterator_nid_postfix(nullptr);
    ures_close_nid_postfix(nullptr);
    Require(!ures_getVersionNumber_nid_postfix(nullptr), "a NULL bundle has no version");
    ures_close_nid_postfix(fillIn);
    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getByKey_nid_postfix(bundle, "calendar", fillIn, &ok); }), "a closed fill-in throws");
    Require(Throws<std::invalid_argument>([&] { ures_close_nid_postfix(fillIn); }), "closing twice throws");
    ures_close_nid_postfix(bundle);
    ures_close(hostFillIn);
}

void CheckErrors() {
    Require(Throws<std::invalid_argument>([] { ures_open_nid_postfix(nullptr, "en", nullptr); }), "a NULL status throws in open");
    Require(Throws<std::invalid_argument>([] { ures_openDirect_nid_postfix(nullptr, "en", nullptr); }), "a NULL status throws in openDirect");
    Require(Throws<std::invalid_argument>([] { ures_openNoDefault_nid_postfix(nullptr, "en", nullptr); }), "a NULL status throws in openNoDefault");
    Require(Throws<std::invalid_argument>([] { ures_openFillIn_nid_postfix(nullptr, nullptr, "en", nullptr); }), "a NULL status throws in openFillIn");
    Require(Throws<std::invalid_argument>([] { ures_openAvailableLocales_nid_postfix(nullptr, nullptr); }), "a NULL status throws in openAvailableLocales");
    Require(Throws<std::invalid_argument>([] { ures_getKeywordValues_nid_postfix(nullptr, "calendar", nullptr); }), "a NULL status throws in getKeywordValues");
    Require(Throws<std::invalid_argument>([] { char result[8]; ures_getFunctionalEquivalent_nid_postfix(result, 8, nullptr, "x", "y", "en", nullptr, 0, nullptr); }), "a NULL status throws in getFunctionalEquivalent");
    Require(Throws<std::invalid_argument>([] { ures_getUTF8String_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr); }), "a NULL status throws in getUTF8String");
    Require(Throws<std::invalid_argument>([] { ures_getStringByKeyWithFallback_nid_postfix(nullptr, "x", nullptr, nullptr); }), "a NULL status throws in getStringByKeyWithFallback");
    Require(Throws<std::invalid_argument>([] { std::uint8_t version[4]; ures_getVersionByKey_nid_postfix(nullptr, "x", version, nullptr); }), "a NULL status throws in getVersionByKey");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ures_findResource_nid_postfix(nullptr, nullptr, &ok); }), "a NULL path throws in findResource");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; char path[] = "x"; ures_findSubResource_nid_postfix(nullptr, path, nullptr, &ok); }), "a NULL bundle throws in findSubResource");
    std::int32_t status = U_MISSING_RESOURCE_ERROR;
    Require(!ures_open_nid_postfix("foo", nullptr, &status) && status == U_MISSING_RESOURCE_ERROR, "a failed status returns before the package check");
    Require(!ures_findResource_nid_postfix(nullptr, nullptr, &status), "a failed status returns before the path check");
    for (const char* package : {"foo", "", "icudt68l", "ICUDATA_lang", "C:/data/pkg", "/tmp/pkg", "ICUDATA-lang\\x", "icudt67l-lang"}) {
        Require(NotImplemented([package] { std::int32_t ok = Ok; ures_open_nid_postfix(package, "en", &ok); }), std::string("a foreign package throws: ") + package);
        Require(NotImplemented([package] { std::int32_t ok = Ok; ures_openDirect_nid_postfix(package, "en", &ok); }), std::string("a foreign package throws in openDirect: ") + package);
        Require(NotImplemented([package] { std::int32_t ok = Ok; ures_openNoDefault_nid_postfix(package, "en", &ok); }), std::string("a foreign package throws in openNoDefault: ") + package);
        Require(NotImplemented([package] { std::int32_t ok = Ok; ures_openAvailableLocales_nid_postfix(package, &ok); }), std::string("a foreign package throws in openAvailableLocales: ") + package);
        Require(NotImplemented([package] { std::int32_t ok = Ok; ures_getKeywordValues_nid_postfix(package, "collations", &ok); }), std::string("a foreign package throws in getKeywordValues: ") + package);
        Require(NotImplemented([package] { std::int32_t ok = Ok; char result[32]; ures_getFunctionalEquivalent_nid_postfix(result, 32, package, "collations", "collation", "de", nullptr, 0, &ok); }), std::string("a foreign package throws in getFunctionalEquivalent: ") + package);
        const std::string path = std::string("/") + package + "/en/Version";
        Require(NotImplemented([&path] { std::int32_t ok = Ok; ures_findResource_nid_postfix(path.c_str(), nullptr, &ok); }), std::string("a foreign package throws in findResource: ") + package);
    }
    for (const char* tree : {"ICUDATA-missing", "icudt68l-lang", "ICUDATA-a/b"}) {
        Opened opened = OpenBoth(tree, "de", false, std::string("tree ") + tree);
        if (opened.guest) Compare(opened.guest, opened.host, std::string("tree ") + tree, 0);
        ures_close_nid_postfix(opened.guest);
        ures_close(opened.host);
    }
    status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    char path[] = "/ICUDATA";
    Require(ures_findResource_nid_postfix(path, nullptr, &status) == ures_findResource(path, nullptr, &hostStatus) && status == hostStatus && status == U_ILLEGAL_ARGUMENT_ERROR, "a package without a locale is an illegal argument");
}

void CheckDefaults() {
    SetDefaults(nullptr);
    for (const char* locale : {"xx", "xx_YY", "zz@collation=phonebook", "i-default@calendar=gregorian", "i-default_US@x=y", "i-default.utf8"}) {
        Require(NotImplemented([locale] { std::int32_t ok = Ok; ures_open_nid_postfix(nullptr, locale, &ok); }), std::string("a locale without data throws: ") + locale);
        Require(NotImplemented([locale] { std::int32_t ok = Ok; UResourceBundle* bundle = ures_open_nid_postfix(nullptr, "en", &ok); ures_openFillIn_nid_postfix(bundle, "ICUDATA-lang", locale, &ok); }), std::string("openFillIn throws for a locale without data: ") + locale);
        const std::string path = std::string(locale) + "/Version";
        Require(NotImplemented([&path] { std::int32_t ok = Ok; ures_findResource_nid_postfix(path.c_str(), nullptr, &ok); }), std::string("findResource throws for a locale without data: ") + locale);
    }
    Require(NotImplemented([] { std::int32_t ok = Ok; ures_open_nid_postfix(nullptr, nullptr, &ok); }), "a NULL locale throws in open");
    Require(NotImplemented([] { std::int32_t ok = Ok; ures_openNoDefault_nid_postfix(nullptr, nullptr, &ok); }), "a NULL locale throws in openNoDefault");
    Require(NotImplemented([] { std::int32_t ok = Ok; ures_openDirect_nid_postfix(nullptr, nullptr, &ok); }), "a NULL locale throws in openDirect");
    Require(NotImplemented([] { std::int32_t ok = Ok; ures_openNoDefault_nid_postfix(nullptr, "i-default@x=y", &ok); }), "i-default with keywords throws in openNoDefault");
    Require(NotImplemented([] { std::int32_t ok = Ok; char result[64]; ures_getFunctionalEquivalent_nid_postfix(result, 64, "ICUDATA-coll", "collations", "collation", "xx@collation=phonebook", nullptr, 0, &ok); }), "getFunctionalEquivalent throws for a locale without data");
    Require(NotImplemented([] { std::int32_t ok = Ok; char result[64]; ures_getFunctionalEquivalent_nid_postfix(result, 64, "ICUDATA-coll", "collations", "collation", nullptr, nullptr, 0, &ok); }), "getFunctionalEquivalent throws for a NULL locale");
    for (const char* locale : {"xx", "xx_YY", "i-default", "en"}) {
        Opened opened{};
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        opened.guest = ures_openNoDefault_nid_postfix(nullptr, locale, &status);
        opened.host = ures_openNoDefault(nullptr, locale, &hostStatus);
        Require(status == hostStatus && opened.guest, std::string("openNoDefault falls back to root deterministically: ") + locale);
        Compare(opened.guest, opened.host, std::string("openNoDefault ") + locale, 0);
        ures_close_nid_postfix(opened.guest);
        ures_close(opened.host);
    }
    Opened unlinked = OpenBoth(nullptr, "i-default", false, "i-default converts as a language tag");
    Compare(unlinked.guest, unlinked.host, "i-default", 0);
    ures_close_nid_postfix(unlinked.guest);
    ures_close(unlinked.host);
    for (const char* locale : {"en_US", "de_AT", "zh_Hant_TW", "sr_Latn_RS", "pt_PT", "en_GB", "es_MX", "fr_CA"}) {
        for (const char* path : {static_cast<const char*>(nullptr), "ICUDATA-lang", "ICUDATA-curr", "ICUDATA-region"}) {
            Opened opened = OpenBoth(path, locale, false, std::string("parents ") + locale);
            for (const char* key : {"Version", "Languages", "Countries", "Currencies", "calendar", "layout", "NumberElements", "Keys", "Types", "CurrencyPlurals", "localeDisplayPattern", "fields", "missing", "ExemplarCharacters", "AuxExemplarCharacters", "delimiters", "characterLabel", "measurementSystemNames", "Scripts", "Variants"}) {
                const std::string what = Name(path) + " " + locale + " " + key;
                UErrorCode hostStatus = U_ZERO_ERROR;
                UResourceBundle* hostKey = ures_getByKey(opened.host, key, nullptr, &hostStatus);
                UErrorCode actualStatus = U_ZERO_ERROR;
                const std::string actual = hostKey ? Name(ures_getLocaleByType(hostKey, ULOC_ACTUAL_LOCALE, &actualStatus)) : std::string();
                const bool decided = (hostStatus == U_USING_FALLBACK_WARNING || hostStatus == U_USING_DEFAULT_WARNING) && actual != "root";
                ures_close(hostKey);
                if (decided) {
                    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getByKey_nid_postfix(opened.guest, key, nullptr, &ok); }), what + ": a key from a parent locale throws");
                    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getStringByKey_nid_postfix(opened.guest, key, nullptr, &ok); }), what + ": a string from a parent locale throws");
                    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_countArrayItems_nid_postfix(opened.guest, key, &ok); }), what + ": a count from a parent locale throws");
                    Require(NotImplemented([&] { std::int32_t ok = Ok; char buffer[16]; std::int32_t length = 16; ures_getUTF8StringByKey_nid_postfix(opened.guest, key, buffer, &length, 1, &ok); }), what + ": a UTF-8 string from a parent locale throws");
                    Require(NotImplemented([&] { std::int32_t ok = Ok; std::uint8_t version[4]; ures_getVersionByKey_nid_postfix(opened.guest, key, version, &ok); }), what + ": a version from a parent locale throws");
                } else {
                    std::int32_t status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    UResourceBundle* guestKey = ures_getByKey_nid_postfix(opened.guest, key, nullptr, &status);
                    hostKey = ures_getByKey(opened.host, key, nullptr, &hostStatus);
                    Require(status == hostStatus, what + ": a key outside the parents matches");
                    if (guestKey) Compare(guestKey, hostKey, what, 0);
                    ures_close_nid_postfix(guestKey);
                    ures_close(hostKey);
                }
                UErrorCode fallbackStatus = U_ZERO_ERROR;
                UResourceBundle* fallback = ures_getByKeyWithFallback(opened.host, key, nullptr, &fallbackStatus);
                ures_close(fallback);
                const bool fellBack = fallbackStatus == U_USING_FALLBACK_WARNING || fallbackStatus == U_USING_DEFAULT_WARNING;
                if (fellBack) {
                    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getByKeyWithFallback_nid_postfix(opened.guest, key, nullptr, &ok); }), what + ": a fallback lookup throws");
                    Require(NotImplemented([&] { std::int32_t ok = Ok; ures_getStringByKeyWithFallback_nid_postfix(opened.guest, key, nullptr, &ok); }), what + ": a fallback string throws");
                } else {
                    std::int32_t status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    UResourceBundle* guestKey = ures_getByKeyWithFallback_nid_postfix(opened.guest, key, nullptr, &status);
                    hostKey = ures_getByKeyWithFallback(opened.host, key, nullptr, &hostStatus);
                    Require(status == hostStatus, what + ": a direct lookup matches");
                    ures_close_nid_postfix(guestKey);
                    ures_close(hostKey);
                }
            }
            ures_close_nid_postfix(opened.guest);
            ures_close(opened.host);
        }
    }
    for (const char* fallback : {"de", "en", "ar_EG", "fa", "xx"}) {
        SetDefaults(fallback);
        for (const char* locale : {"xx", "xx_YY", static_cast<const char*>(nullptr), "i-default@calendar=gregorian", "en_US", "de_AT", "fr_CA"}) {
            for (const char* path : {static_cast<const char*>(nullptr), "ICUDATA-lang", "ICUDATA-curr"}) {
                const std::string what = std::string(fallback) + " default " + Name(path) + " " + Name(locale);
                Opened opened = OpenBoth(path, locale, false, what);
                if (!opened.guest) continue;
                Compare(opened.guest, opened.host, what, 1);
                for (const char* key : {"Version", "Languages", "calendar", "Currencies", "Keys", "missing", "calendar/gregorian/monthNames", "NumberElements/latn/symbols"}) {
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    UResourceBundle* guestKey = ures_getByKey_nid_postfix(opened.guest, key, nullptr, &status);
                    UResourceBundle* hostKey = ures_getByKey(opened.host, key, nullptr, &hostStatus);
                    Require(status == hostStatus, what + ": by key " + key);
                    ures_close_nid_postfix(guestKey);
                    ures_close(hostKey);
                    status = Ok;
                    hostStatus = U_ZERO_ERROR;
                    guestKey = ures_getByKeyWithFallback_nid_postfix(opened.guest, key, nullptr, &status);
                    hostKey = ures_getByKeyWithFallback(opened.host, key, nullptr, &hostStatus);
                    Require(status == hostStatus && (guestKey == nullptr) == (hostKey == nullptr), what + ": with fallback " + key);
                    if (guestKey) Compare(guestKey, hostKey, what + " " + key, 0);
                    ures_close_nid_postfix(guestKey);
                    ures_close(hostKey);
                }
                ures_close_nid_postfix(opened.guest);
                ures_close(opened.host);
            }
            if (locale || std::strcmp(fallback, "xx") == 0) {
                Opened direct = OpenBoth(nullptr, locale, true, std::string("direct ") + fallback + " " + Name(locale));
                ures_close_nid_postfix(direct.guest);
                ures_close(direct.host);
            }
        }
        if (std::strcmp(fallback, "xx") != 0) {
            Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ures_openDirect_nid_postfix(nullptr, nullptr, &ok); }), std::string("openDirect with a NULL locale throws where ICU 68.2 would crash: ") + fallback);
            Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; UResourceBundle* bundle = ures_open_nid_postfix(nullptr, "en", &ok); ures_openDirectFillIn_nid_postfix(bundle, nullptr, nullptr, &ok); }), std::string("openDirectFillIn with a NULL locale throws: ") + fallback);
        }
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        char result[64] = "";
        char hostResult[64] = "";
        const std::int32_t length = ures_getFunctionalEquivalent_nid_postfix(result, 64, "ICUDATA-coll", "collations", "collation", "xx@collation=phonebook", nullptr, 0, &status);
        const std::int32_t hostLength = ures_getFunctionalEquivalent(hostResult, 64, "ICUDATA-coll", "collations", "collation", "xx@collation=phonebook", nullptr, 0, &hostStatus);
        Require(length == hostLength && status == hostStatus && std::strcmp(result, hostResult) == 0, std::string("getFunctionalEquivalent follows the guest default ") + fallback);
    }
    SetDefaults(nullptr);
}

void CheckEnumerations() {
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    for (const char* path : {static_cast<const char*>(nullptr), "ICUDATA-coll", "ICUDATA-lang", "ICUDATA-missing"}) {
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        void* guest = ures_openAvailableLocales_nid_postfix(path, &status);
        UEnumeration* host = ures_openAvailableLocales(path, &hostStatus);
        Require(status == hostStatus && (guest == nullptr) == (host == nullptr), std::string("openAvailableLocales ") + Name(path));
        if (!guest) continue;
        Require(GuestEnumerationLayout(guest), "the available locales are a guest enumeration");
        Require(GuestEnumerationCount(guest, &status) == uenum_count(host, &hostStatus) && status == hostStatus, "the available locale counts match");
        for (;;) {
            std::int32_t length = 0;
            std::int32_t hostLength = 0;
            const char* name = GuestEnumerationNext(guest, &length, &status);
            const char* hostName = uenum_next(host, &hostLength, &hostStatus);
            Require(Name(name) == Name(hostName) && length == hostLength && status == hostStatus, "the available locales match");
            if (!name) break;
        }
        GuestEnumerationClose(guest);
        uenum_close(host);
    }
    for (const char* keyword : {"collations", "calendar", "missing", static_cast<const char*>(nullptr)}) {
        for (const char* path : {static_cast<const char*>(nullptr), "ICUDATA-coll"}) {
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            void* guest = ures_getKeywordValues_nid_postfix(path, keyword, &status);
            UEnumeration* host = ures_getKeywordValues(path, keyword, &hostStatus);
            Require(status == hostStatus && (guest == nullptr) == (host == nullptr), std::string("getKeywordValues ") + Name(keyword));
            if (!guest) continue;
            Require(GuestEnumerationCount(guest, &status) == uenum_count(host, &hostStatus), "the keyword value counts match");
            for (;;) {
                const char* name = GuestEnumerationNext(guest, nullptr, &status);
                const char* hostName = uenum_next(host, nullptr, &hostStatus);
                Require(Name(name) == Name(hostName), "the keyword values match");
                if (!name) break;
            }
            GuestEnumerationClose(guest);
            uenum_close(host);
        }
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    Require(!ures_openAvailableLocales_nid_postfix("foo", &status) && !ures_getKeywordValues_nid_postfix("foo", "x", &status), "a failed status returns before the package check in enumerations");
}

void CheckFunctionalEquivalent() {
    SetDefaults("en_US");
    struct Case {
        const char* path;
        const char* resName;
        const char* keyword;
        const char* locid;
    };
    const Case cases[] = {
        {"ICUDATA-coll", "collations", "collation", "de@collation=phonebook"}, {"ICUDATA-coll", "collations", "collation", "de"},
        {"ICUDATA-coll", "collations", "collation", "zh_Hant"}, {"ICUDATA-coll", "collations", "collation", "sv@collation=standard"},
        {"ICUDATA-coll", "collations", "collation", "ja@collation=default"}, {"ICUDATA-coll", "collations", "collation", "en_US"},
        {"ICUDATA-coll", "collations", "collation", "zh@collation=pinyin"}, {"ICUDATA-coll", "collations", "collation", "de@collation=bogus"},
        {"ICUDATA-coll", "collations", nullptr, "de"}, {"ICUDATA-coll", nullptr, "collation", "de"}, {nullptr, "calendar", "calendar", "th@calendar=buddhist"},
        {nullptr, "calendar", "calendar", "ja_JP"}, {"ICUDATA-coll", "collations", "collation", ""},
    };
    for (const Case& item : cases) {
        for (std::int32_t capacity : {0, 1, 5, 64}) {
            for (int available = 0; available < 2; ++available) {
                for (std::int8_t omitDefault : {0, 1}) {
                    const std::string what = std::string("functional equivalent ") + Name(item.path) + " " + Name(item.resName) + " " + Name(item.keyword) + " " + item.locid + " " + std::to_string(capacity) + " " + std::to_string(available) + " " + std::to_string(omitDefault);
                    char result[72];
                    char hostResult[72];
                    std::memset(result, 'x', sizeof(result));
                    std::memset(hostResult, 'x', sizeof(hostResult));
                    std::int8_t isAvailable = 7;
                    UBool hostAvailable = 7;
                    std::int32_t status = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    const std::int32_t length = ures_getFunctionalEquivalent_nid_postfix(result, capacity, item.path, item.resName, item.keyword, item.locid, available ? &isAvailable : nullptr, omitDefault, &status);
                    const std::int32_t hostLength = ures_getFunctionalEquivalent(hostResult, capacity, item.path, item.resName, item.keyword, item.locid, available ? &hostAvailable : nullptr, omitDefault, &hostStatus);
                    Require(length == hostLength && status == hostStatus && isAvailable == hostAvailable && std::memcmp(result, hostResult, sizeof(result)) == 0, what);
                }
            }
        }
        for (std::int32_t capacity : {-1, 0, 8}) {
            std::int32_t status = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            char hostScratch[8];
            std::memset(hostScratch, 'x', sizeof(hostScratch));
            const std::int32_t hostLength = ures_getFunctionalEquivalent(hostScratch, capacity, item.path, item.resName, item.keyword, item.locid, nullptr, 0, &hostStatus);
            const bool written = std::any_of(std::begin(hostScratch), std::end(hostScratch), [](char value) { return value != 'x'; });
            const std::string what = std::string("a NULL result with capacity ") + std::to_string(capacity) + ": " + item.locid + " " + Name(item.keyword) + " " + Name(item.resName);
            if (written) {
                Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ures_getFunctionalEquivalent_nid_postfix(nullptr, capacity, item.path, item.resName, item.keyword, item.locid, nullptr, 0, &ok); }), what + " throws where ICU writes through it");
            } else {
                Require(ures_getFunctionalEquivalent_nid_postfix(nullptr, capacity, item.path, item.resName, item.keyword, item.locid, nullptr, 0, &status) == hostLength && status == hostStatus, what + " matches ICU");
            }
        }
    }
    std::int32_t status = U_MISSING_RESOURCE_ERROR;
    Require(ures_getFunctionalEquivalent_nid_postfix(nullptr, 8, "ICUDATA-coll", "collations", "collation", "de", nullptr, 0, &status) == 0 && status == U_MISSING_RESOURCE_ERROR, "a failed status returns before the buffer check");
    SetDefaults(nullptr);
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
    CheckErrors();
    CheckFillIns();
    CheckDefaults();
    SetDefaults("en_US");
    CheckSweep();
    CheckPaths();
    CheckEnumerations();
    CheckFunctionalEquivalent();
    std::puts("guest_icu_resource_bundle passed");
    return 0;
}

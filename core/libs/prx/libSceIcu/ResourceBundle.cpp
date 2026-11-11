#include "prx/libSceIcu/ResourceBundle.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include "uresimp.h"
#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr char Untouched = 0x7f;

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

std::mutex& BundleMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_set<const UResourceBundle*>& Bundles() {
    static std::unordered_set<const UResourceBundle*> bundles;
    return bundles;
}

UResourceBundle* Own(UResourceBundle* bundle) {
    if (!bundle) return bundle;
    const std::lock_guard lock(BundleMutex());
    Bundles().insert(bundle);
    return bundle;
}

bool Owned(const UResourceBundle* bundle) {
    const std::lock_guard lock(BundleMutex());
    return Bundles().count(bundle) != 0;
}

template<typename TBundle>
TBundle* Bundle(TBundle* bundle) {
    IcuBackend::Ensure();
    if (bundle && !Owned(bundle)) APS5_INVALID_ARG_EX;
    return bundle;
}

UResourceBundle* FillIn(UResourceBundle* fillIn) {
    IcuBackend::Ensure();
    if (fillIn && !Owned(fillIn)) NotImplemented_nid_no_patch("libSceIcu: a UResourceBundle fill-in the library did not return (a guest-allocated bundle would have ICU's internal layout)");
    return fillIn;
}

void CheckOpen(const char* path, const char* locale) {
    UErrorCode probe = U_ZERO_ERROR;
    ures_close(ures_open(path, locale, &probe));
    IcuBackend::CheckFallback(reinterpret_cast<const std::int32_t*>(&probe));
}

void Open(const char* path, const char* locale) {
    IcuBackend::Package(path);
    IcuBackend::CanonicalLocale(locale);
    CheckOpen(path, locale);
}

void OpenDirect(const char* path, const char* locale) {
    IcuBackend::Package(path);
    if (locale) return;
    IcuBackend::Locale(locale);
    UErrorCode probe = U_ZERO_ERROR;
    ures_close(ures_openDirect(path, uloc_getDefault(), &probe));
    if (U_SUCCESS(probe)) APS5_INVALID_ARG_EX;
}

void CheckFind(const char* pathToResource) {
    std::string path(pathToResource);
    std::string package;
    std::string::size_type localeStart = 0;
    bool hasPackage = false;
    if (!path.empty() && path[0] == '/') {
        const std::string::size_type end = path.find('/', 1);
        if (end == std::string::npos) return;
        package = path.substr(1, end - 1);
        hasPackage = true;
        localeStart = end + 1;
    }
    const std::string::size_type localeEnd = path.find('/', localeStart);
    const std::string locale = path.substr(localeStart, localeEnd == std::string::npos ? std::string::npos : localeEnd - localeStart);
    Open(hasPackage ? package.c_str() : nullptr, locale.c_str());
}

UEnumeration* Guest(UEnumeration* host, std::int32_t* status) {
    return IcuBackend::GuestEnumeration(host, status);
}

}

extern "C" {

UResourceBundle* APS5_VABI ures_open_nid_postfix(const char* packageName, const char* locale, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    Open(packageName, locale);
    return Own(ures_open(packageName, locale, error));
}

UResourceBundle* APS5_VABI ures_openNoDefault_nid_postfix(const char* path, const char* localeID, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Package(path);
    IcuBackend::CanonicalLocale(localeID);
    return Own(ures_openNoDefault(path, localeID, error));
}

UResourceBundle* APS5_VABI ures_openDirect_nid_postfix(const char* packageName, const char* locale, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    OpenDirect(packageName, locale);
    return Own(ures_openDirect(packageName, locale, error));
}

void APS5_VABI ures_openFillIn_nid_postfix(UResourceBundle* r, const char* packageName, const char* localeID, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && r) {
        FillIn(r);
        Open(packageName, localeID);
    }
    ures_openFillIn(r, packageName, localeID, error);
}

void APS5_VABI ures_openDirectFillIn_nid_postfix(UResourceBundle* r, const char* packageName, const char* locale, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && r) {
        FillIn(r);
        OpenDirect(packageName, locale);
    }
    ures_openDirectFillIn(r, packageName, locale, error);
}

void APS5_VABI ures_close_nid_postfix(UResourceBundle* resourceBundle) {
    Bundle(resourceBundle);
    if (!resourceBundle) return;
    {
        const std::lock_guard lock(BundleMutex());
        Bundles().erase(resourceBundle);
    }
    ures_close(resourceBundle);
}

UEnumeration* APS5_VABI ures_openAvailableLocales_nid_postfix(const char* packageName, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Package(packageName);
    return Guest(ures_openAvailableLocales(packageName, error), status);
}

UEnumeration* APS5_VABI ures_getKeywordValues_nid_postfix(const char* path, const char* keyword, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::Package(path);
    return Guest(ures_getKeywordValues(path, keyword, error), status);
}

std::int32_t APS5_VABI ures_getFunctionalEquivalent_nid_postfix(char* result, std::int32_t resultCapacity, const char* path, const char* resName, const char* keyword, const char* locid, std::int8_t* isAvailable, std::int8_t omitDefault, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return 0;
    IcuBackend::Package(path);
    IcuBackend::CanonicalLocale(locid);
    UErrorCode baseStatus = U_ZERO_ERROR;
    char kwVal[1024] = "";
    uloc_getKeywordValue(locid, keyword, kwVal, sizeof(kwVal) - 1, &baseStatus);
    char base[1024] = "";
    uloc_getBaseName(locid, base, sizeof(base) - 1, &baseStatus);
    if (U_SUCCESS(baseStatus)) CheckOpen(path, base);
    if (result) return ures_getFunctionalEquivalent(result, resultCapacity, path, resName, keyword, locid, reinterpret_cast<UBool*>(isAvailable), omitDefault, error);
    std::vector<char> scratch(resultCapacity > 0 ? std::min<std::size_t>(static_cast<std::size_t>(resultCapacity), sizeof(kwVal)) : 1, Untouched);
    const std::int32_t length = ures_getFunctionalEquivalent(scratch.data(), resultCapacity, path, resName, keyword, locid, reinterpret_cast<UBool*>(isAvailable), omitDefault, error);
    if (std::any_of(scratch.begin(), scratch.end(), [](char value) { return value != Untouched; })) APS5_INVALID_ARG_EX;
    return length;
}

const char* APS5_VABI ures_getVersionNumber_nid_postfix(const UResourceBundle* resourceBundle) {
    return ures_getVersionNumber(Bundle(resourceBundle));
}

const char* APS5_VABI ures_getVersionNumberInternal_nid_postfix(const UResourceBundle* resourceBundle) {
    return ures_getVersionNumberInternal(Bundle(resourceBundle));
}

void APS5_VABI ures_getVersion_nid_postfix(const UResourceBundle* resB, std::uint8_t* versionInfo) {
    ures_getVersion(Bundle(resB), versionInfo);
}

void APS5_VABI ures_getVersionByKey_nid_postfix(const UResourceBundle* res, const char* key, std::uint8_t* ver, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (res) IcuBackend::CheckKey(Bundle(res), key, status);
    ures_getVersionByKey(res, key, ver, error);
}

const char* APS5_VABI ures_getLocale_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status) {
    return ures_getLocale(Bundle(resourceBundle), Optional(status));
}

const char* APS5_VABI ures_getLocaleInternal_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status) {
    return ures_getLocaleInternal(Bundle(resourceBundle), Optional(status));
}

const char* APS5_VABI ures_getLocaleByType_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t type, std::int32_t* status) {
    return ures_getLocaleByType(Bundle(resourceBundle), static_cast<ULocDataLocaleType>(type), Optional(status));
}

const char16_t* APS5_VABI ures_getString_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* len, std::int32_t* status) {
    return ures_getString(Bundle(resourceBundle), len, Optional(status));
}

const char* APS5_VABI ures_getUTF8String_nid_postfix(const UResourceBundle* resB, char* dest, std::int32_t* length, std::int8_t forceCopy, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return ures_getUTF8String(Bundle(resB), dest, length, forceCopy, error);
}

const std::uint8_t* APS5_VABI ures_getBinary_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* len, std::int32_t* status) {
    return ures_getBinary(Bundle(resourceBundle), len, Optional(status));
}

const std::int32_t* APS5_VABI ures_getIntVector_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* len, std::int32_t* status) {
    return ures_getIntVector(Bundle(resourceBundle), len, Optional(status));
}

std::uint32_t APS5_VABI ures_getUInt_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status) {
    return ures_getUInt(Bundle(resourceBundle), Optional(status));
}

std::int32_t APS5_VABI ures_getInt_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t* status) {
    return ures_getInt(Bundle(resourceBundle), Optional(status));
}

std::int32_t APS5_VABI ures_getSize_nid_postfix(const UResourceBundle* resourceBundle) {
    return ures_getSize(Bundle(resourceBundle));
}

std::int32_t APS5_VABI ures_getType_nid_postfix(const UResourceBundle* resourceBundle) {
    return ures_getType(Bundle(resourceBundle));
}

const char* APS5_VABI ures_getKey_nid_postfix(const UResourceBundle* resourceBundle) {
    return ures_getKey(Bundle(resourceBundle));
}

void APS5_VABI ures_resetIterator_nid_postfix(UResourceBundle* resourceBundle) {
    ures_resetIterator(Bundle(resourceBundle));
}

std::int8_t APS5_VABI ures_hasNext_nid_postfix(const UResourceBundle* resourceBundle) {
    return ures_hasNext(Bundle(resourceBundle));
}

UResourceBundle* APS5_VABI ures_getNextResource_nid_postfix(UResourceBundle* resourceBundle, UResourceBundle* fillIn, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    Bundle(resourceBundle);
    if (!Proceeds(status) || !resourceBundle) return ures_getNextResource(resourceBundle, fillIn, error);
    return Own(ures_getNextResource(resourceBundle, FillIn(fillIn), error));
}

const char16_t* APS5_VABI ures_getNextString_nid_postfix(UResourceBundle* resourceBundle, std::int32_t* len, const char** key, std::int32_t* status) {
    return ures_getNextString(Bundle(resourceBundle), len, key, Optional(status));
}

UResourceBundle* APS5_VABI ures_getByIndex_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t indexR, UResourceBundle* fillIn, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    Bundle(resourceBundle);
    if (!Proceeds(status) || !resourceBundle) return ures_getByIndex(resourceBundle, indexR, fillIn, error);
    return Own(ures_getByIndex(resourceBundle, indexR, FillIn(fillIn), error));
}

const char16_t* APS5_VABI ures_getStringByIndex_nid_postfix(const UResourceBundle* resourceBundle, std::int32_t indexS, std::int32_t* len, std::int32_t* status) {
    return ures_getStringByIndex(Bundle(resourceBundle), indexS, len, Optional(status));
}

const char* APS5_VABI ures_getUTF8StringByIndex_nid_postfix(const UResourceBundle* resB, std::int32_t stringIndex, char* dest, std::int32_t* pLength, std::int8_t forceCopy, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return ures_getUTF8StringByIndex(Bundle(resB), stringIndex, dest, pLength, forceCopy, error);
}

UResourceBundle* APS5_VABI ures_getByKey_nid_postfix(const UResourceBundle* resourceBundle, const char* key, UResourceBundle* fillIn, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    Bundle(resourceBundle);
    if (!Proceeds(status) || !resourceBundle) return ures_getByKey(resourceBundle, key, fillIn, error);
    FillIn(fillIn);
    IcuBackend::CheckKey(resourceBundle, key, status);
    return Own(ures_getByKey(resourceBundle, key, fillIn, error));
}

const char16_t* APS5_VABI ures_getStringByKey_nid_postfix(const UResourceBundle* resB, const char* key, std::int32_t* len, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    IcuBackend::CheckKey(Bundle(resB), key, status);
    return ures_getStringByKey(resB, key, len, error);
}

const char* APS5_VABI ures_getUTF8StringByKey_nid_postfix(const UResourceBundle* resB, const char* key, char* dest, std::int32_t* pLength, std::int8_t forceCopy, std::int32_t* status) {
    UErrorCode* error = Status(status);
    IcuBackend::CheckKey(Bundle(resB), key, status);
    return ures_getUTF8StringByKey(resB, key, dest, pLength, forceCopy, error);
}

std::int32_t APS5_VABI ures_countArrayItems_nid_postfix(const UResourceBundle* resourceBundle, const char* resourceKey, std::int32_t* err) {
    UErrorCode* error = Optional(err);
    IcuBackend::CheckKey(Bundle(resourceBundle), resourceKey, err);
    return ures_countArrayItems(resourceBundle, resourceKey, error);
}

UResourceBundle* APS5_VABI ures_findResource_nid_postfix(const char* pathToResource, UResourceBundle* fillIn, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return fillIn;
    IcuBackend::RequireObject(pathToResource);
    FillIn(fillIn);
    CheckFind(pathToResource);
    return Own(ures_findResource(pathToResource, fillIn, error));
}

UResourceBundle* APS5_VABI ures_findSubResource_nid_postfix(const UResourceBundle* resB, char* pathToResource, UResourceBundle* fillIn, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return fillIn;
    IcuBackend::RequireObject(Bundle(resB));
    IcuBackend::RequireObject(pathToResource);
    FillIn(fillIn);
    return Own(ures_findSubResource(resB, pathToResource, fillIn, error));
}

UResourceBundle* APS5_VABI ures_getByKeyWithFallback_nid_postfix(const UResourceBundle* resB, const char* inKey, UResourceBundle* fillIn, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    Bundle(resB);
    if (!Proceeds(status) || !resB) return ures_getByKeyWithFallback(resB, inKey, fillIn, error);
    FillIn(fillIn);
    IcuBackend::CheckWithFallback(resB, inKey, status);
    return Own(ures_getByKeyWithFallback(resB, inKey, fillIn, error));
}

const char16_t* APS5_VABI ures_getStringByKeyWithFallback_nid_postfix(const UResourceBundle* resB, const char* inKey, std::int32_t* len, std::int32_t* status) {
    UErrorCode* error = Status(status);
    IcuBackend::CheckWithFallback(Bundle(resB), inKey, status);
    return ures_getStringByKeyWithFallback(resB, inKey, len, error);
}

}

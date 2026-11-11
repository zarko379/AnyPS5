#include "prx/libSceIcu/CaseMap.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ustring.h>
#include "ucasemap_imp.h"

namespace {

UErrorCode* Status(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

UErrorCode* RequiredStatus(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

const char* CaseLocale(const char* locale, const UErrorCode* status, const char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength) {
    const bool mapped = U_SUCCESS(*status) && destCapacity >= 0 && (dest || destCapacity == 0) && src && srcLength >= -1;
    if (!mapped) return locale ? locale : "";
    return IcuBackend::Locale(locale);
}

void RequireText(const char16_t* text, std::int32_t length) {
    if (!text && length != 0 && length != -1) APS5_INVALID_ARG_EX;
}

}

extern "C" {

std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, const char* locale, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    return u_strToUpper(dest, destCapacity, src, srcLength, CaseLocale(locale, error, dest, destCapacity, src, srcLength), error);
}

std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, const char* locale, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    return u_strToLower(dest, destCapacity, src, srcLength, CaseLocale(locale, error, dest, destCapacity, src, srcLength), error);
}

std::int32_t APS5_VABI u_strToTitle_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, UBreakIterator* titleIter, const char* locale, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    if (U_FAILURE(*error)) return 0;
    if (titleIter) NotImplemented_nid_no_patch("u_strToTitle with a guest break iterator");
    return u_strToTitle(dest, destCapacity, src, srcLength, nullptr, CaseLocale(locale, error, dest, destCapacity, src, srcLength), error);
}

std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, std::uint32_t options, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    return u_strFoldCase(dest, destCapacity, src, srcLength, options, error);
}

std::int32_t APS5_VABI u_strCaseCompare_nid_postfix(const char16_t* s1, std::int32_t length1, const char16_t* s2, std::int32_t length2, std::uint32_t options, std::int32_t* status) {
    return u_strCaseCompare(s1, length1, s2, length2, options, Status(status));
}

std::int32_t APS5_VABI u_strcasecmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::uint32_t options) {
    IcuBackend::Ensure();
    return u_strcasecmp(s1, s2, options);
}

std::int32_t APS5_VABI u_strncasecmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::int32_t n, std::uint32_t options) {
    RequireText(s1, n);
    RequireText(s2, n);
    IcuBackend::Ensure();
    return u_strncasecmp(s1, s2, n, options);
}

std::int32_t APS5_VABI u_memcasecmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::int32_t length, std::uint32_t options) {
    RequireText(s1, length);
    RequireText(s2, length);
    IcuBackend::Ensure();
    return u_memcasecmp(s1, s2, length, options);
}

void APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix(const char16_t* s1, std::int32_t length1, const char16_t* s2, std::int32_t length2, std::uint32_t options, std::int32_t* matchLen1, std::int32_t* matchLen2, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    if (U_FAILURE(*error)) return;
    if (matchLen1) IcuBackend::RequireObject(matchLen2);
    RequireText(s1, length1);
    RequireText(s2, length2);
    u_caseInsensitivePrefixMatch(s1, length1, s2, length2, options, matchLen1, matchLen2, error);
}

}

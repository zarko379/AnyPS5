#include "prx/libSceIcu/CaseMap.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include <unicode/ustring.h>

namespace {

UErrorCode* RequiredStatus(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

const char* CaseLocale(const char* locale, const UErrorCode* status, const char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, bool wordBreaks) {
    const bool mapped = U_SUCCESS(*status) && destCapacity >= 0 && (dest || destCapacity == 0) && src && srcLength >= -1;
    if (!mapped) return locale ? locale : "";
    return wordBreaks ? IcuBackend::LocaleObject(locale) : IcuBackend::Locale(locale);
}

}

extern "C" {

std::int32_t APS5_VABI u_strToTitle_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, UBreakIterator* titleIter, const char* locale, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    return u_strToTitle(dest, destCapacity, src, srcLength, titleIter, CaseLocale(locale, error, dest, destCapacity, src, srcLength, titleIter == nullptr), error);
}

}

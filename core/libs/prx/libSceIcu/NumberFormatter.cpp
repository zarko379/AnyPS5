#include "prx/libSceIcu/NumberFormatter.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/unumberformatter.h>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

void Formatter(const UNumberFormatter* formatter, const UErrorCode* status) {
    if (U_SUCCESS(*status)) IcuBackend::CheckTracked(formatter);
}

}

extern "C" {

UNumberFormatter* APS5_VABI unumf_openForSkeletonAndLocale_nid_postfix(const char16_t* skeleton, std::int32_t skeletonLen, const char* locale, std::int32_t* ec) {
    UErrorCode* error = Status(ec);
    IcuBackend::Locale(locale);
    const bool needsDefault = IcuBackend::LocaleData(nullptr, locale);
    UNumberFormatter* formatter = unumf_openForSkeletonAndLocale(skeleton, skeletonLen, locale, error);
    IcuBackend::Track(formatter, needsDefault);
    return formatter;
}

UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* ec) {
    return unumf_openResult(Status(ec));
}

void APS5_VABI unumf_formatDouble_nid_postfix(const UNumberFormatter* uformatter, double value, UFormattedNumber* uresult, std::int32_t* ec) {
    UErrorCode* error = Status(ec);
    Formatter(uformatter, error);
    unumf_formatDouble(uformatter, value, uresult, error);
}

void APS5_VABI unumf_formatDecimal_nid_postfix(const UNumberFormatter* uformatter, const char* value, std::int32_t valueLen, UFormattedNumber* uresult, std::int32_t* ec) {
    UErrorCode* error = Status(ec);
    Formatter(uformatter, error);
    unumf_formatDecimal(uformatter, value, valueLen, uresult, error);
}

std::int32_t APS5_VABI unumf_resultToString_nid_postfix(const UFormattedNumber* uresult, char16_t* buffer, std::int32_t bufferCapacity, std::int32_t* ec) {
    return unumf_resultToString(uresult, buffer, bufferCapacity, Status(ec));
}

void APS5_VABI unumf_resultGetAllFieldPositions_nid_postfix(const UFormattedNumber* uresult, UFieldPositionIterator* ufpositer, std::int32_t* ec) {
    unumf_resultGetAllFieldPositions(uresult, ufpositer, Status(ec));
}

void APS5_VABI unumf_close_nid_postfix(UNumberFormatter* uformatter) {
    IcuBackend::Ensure();
    IcuBackend::Untrack(uformatter);
    unumf_close(uformatter);
}

void APS5_VABI unumf_closeResult_nid_postfix(UFormattedNumber* uresult) {
    IcuBackend::Ensure();
    unumf_closeResult(uresult);
}

}

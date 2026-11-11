#include "prx/libSceIcu/StringSearch.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/usearch.h>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

}

extern "C" {

UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength, const char* locale, UBreakIterator* breakIterator, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if (breakIterator) NotImplemented_nid_no_patch("usearch_open with a guest UBreakIterator");
    return usearch_open(pattern, patternLength, text, textLength, locale, nullptr, error);
}

UCollator* APS5_VABI usearch_getCollator_nid_postfix(const UStringSearch* search) {
    IcuBackend::Ensure();
    return usearch_getCollator(search);
}

std::int32_t APS5_VABI usearch_getMatchedLength_nid_postfix(const UStringSearch* search) {
    IcuBackend::Ensure();
    return usearch_getMatchedLength(search);
}

std::int32_t APS5_VABI usearch_next_nid_postfix(UStringSearch* search, std::int32_t* status) {
    return usearch_next(search, Status(status));
}

void APS5_VABI usearch_reset_nid_postfix(UStringSearch* search) {
    IcuBackend::Ensure();
    usearch_reset(search);
}

void APS5_VABI usearch_setAttribute_nid_postfix(UStringSearch* search, std::int32_t attribute, std::int32_t value, std::int32_t* status) {
    usearch_setAttribute(search, static_cast<USearchAttribute>(attribute), static_cast<USearchAttributeValue>(value), Status(status));
}

void APS5_VABI usearch_setOffset_nid_postfix(UStringSearch* search, std::int32_t position, std::int32_t* status) {
    usearch_setOffset(search, position, Status(status));
}

void APS5_VABI usearch_setPattern_nid_postfix(UStringSearch* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && search && pattern && patternLength < -1) APS5_INVALID_ARG_EX;
    usearch_setPattern(search, pattern, patternLength, error);
}

void APS5_VABI usearch_setText_nid_postfix(UStringSearch* search, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    usearch_setText(search, text, textLength, Status(status));
}

}

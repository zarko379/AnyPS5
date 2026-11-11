#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ubrk.h>
#include <unicode/parseerr.h>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

template<typename TIterator>
TIterator* Iterator(TIterator* iterator) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(iterator);
    return iterator;
}

bool CannedType(std::int32_t type) {
    return type >= UBRK_CHARACTER && type <= UBRK_TITLE;
}

}

extern "C" {

UBreakIterator* APS5_VABI ubrk_open_nid_postfix(std::int32_t type, const char* locale, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return ubrk_open(static_cast<UBreakIteratorType>(type), CannedType(type) ? IcuBackend::LocaleObject(locale) : locale, text, textLength, error);
}

UBreakIterator* APS5_VABI ubrk_openRules_nid_postfix(const char16_t* rules, std::int32_t rulesLength, const char16_t* text, std::int32_t textLength, UParseError* parseErr, std::int32_t* status) {
    IcuBackend::Ensure();
    return ubrk_openRules(rules, rulesLength, text, textLength, parseErr, reinterpret_cast<UErrorCode*>(status));
}

UBreakIterator* APS5_VABI ubrk_openBinaryRules_nid_postfix(const std::uint8_t* binaryRules, std::int32_t rulesLength, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    return ubrk_openBinaryRules(binaryRules, rulesLength, text, textLength, Status(status));
}

std::int32_t APS5_VABI ubrk_getBinaryRules_nid_postfix(UBreakIterator* bi, std::uint8_t* binaryRules, std::int32_t rulesCapacity, std::int32_t* status) {
    return ubrk_getBinaryRules(bi, binaryRules, rulesCapacity, Status(status));
}

UBreakIterator* APS5_VABI ubrk_safeClone_nid_postfix(const UBreakIterator* bi, void* stackBuffer, std::int32_t* pBufferSize, std::int32_t* status) {
    IcuBackend::Ensure();
    return ubrk_safeClone(bi, stackBuffer, pBufferSize, reinterpret_cast<UErrorCode*>(status));
}

void APS5_VABI ubrk_close_nid_postfix(UBreakIterator* bi) {
    IcuBackend::Ensure();
    ubrk_close(bi);
}

void APS5_VABI ubrk_setText_nid_postfix(UBreakIterator* bi, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    ubrk_setText(Iterator(bi), text, textLength, error);
}

std::int32_t APS5_VABI ubrk_current_nid_postfix(const UBreakIterator* bi) {
    return ubrk_current(Iterator(bi));
}

std::int32_t APS5_VABI ubrk_first_nid_postfix(UBreakIterator* bi) {
    return ubrk_first(Iterator(bi));
}

std::int32_t APS5_VABI ubrk_last_nid_postfix(UBreakIterator* bi) {
    return ubrk_last(Iterator(bi));
}

std::int32_t APS5_VABI ubrk_next_nid_postfix(UBreakIterator* bi) {
    return ubrk_next(Iterator(bi));
}

std::int32_t APS5_VABI ubrk_previous_nid_postfix(UBreakIterator* bi) {
    return ubrk_previous(Iterator(bi));
}

std::int32_t APS5_VABI ubrk_following_nid_postfix(UBreakIterator* bi, std::int32_t offset) {
    return ubrk_following(Iterator(bi), offset);
}

std::int32_t APS5_VABI ubrk_preceding_nid_postfix(UBreakIterator* bi, std::int32_t offset) {
    return ubrk_preceding(Iterator(bi), offset);
}

std::int8_t APS5_VABI ubrk_isBoundary_nid_postfix(UBreakIterator* bi, std::int32_t offset) {
    return ubrk_isBoundary(Iterator(bi), offset);
}

std::int32_t APS5_VABI ubrk_getRuleStatus_nid_postfix(UBreakIterator* bi) {
    return ubrk_getRuleStatus(Iterator(bi));
}

std::int32_t APS5_VABI ubrk_getRuleStatusVec_nid_postfix(UBreakIterator* bi, std::int32_t* fillInVec, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    Iterator(bi);
    if (U_SUCCESS(*error) && !fillInVec && capacity > 0) {
        UErrorCode probe = U_ZERO_ERROR;
        if (ubrk_getRuleStatusVec(bi, nullptr, 0, &probe) > 0) APS5_INVALID_ARG_EX;
    }
    return ubrk_getRuleStatusVec(bi, fillInVec, capacity, error);
}

const char* APS5_VABI ubrk_getLocaleByType_nid_postfix(const UBreakIterator* bi, std::int32_t type, std::int32_t* status) {
    return ubrk_getLocaleByType(bi, static_cast<ULocDataLocaleType>(type), Status(status));
}

std::int32_t APS5_VABI ubrk_countAvailable_nid_postfix() {
    IcuBackend::Ensure();
    return ubrk_countAvailable();
}

const char* APS5_VABI ubrk_getAvailable_nid_postfix(std::int32_t index) {
    IcuBackend::Ensure();
    if (index < 0 || index == ubrk_countAvailable()) APS5_INVALID_ARG_EX;
    return ubrk_getAvailable(index);
}

}

#include "prx/libSceIcu/CaseProperties.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include "ucase.h"

namespace {

UChar32 Ready(std::int32_t c) {
    IcuBackend::Ensure();
    return c;
}

struct IteratorForward {
    GuestCaseContextIterator iterator;
    void* context;
};

UChar32 U_CALLCONV ForwardIterator(void* context, int8_t dir) {
    const auto* forward = static_cast<const IteratorForward*>(context);
    return forward->iterator(forward->context, dir);
}

template<typename TCall>
std::int32_t Full(std::int32_t c, const char16_t** pString, TCall call) {
    if (c < 0) APS5_INVALID_ARG_EX;
    IcuBackend::Ensure();
    static const UChar unwritten = 0;
    const UChar* string = &unwritten;
    const std::int32_t result = call(&string);
    if (string != &unwritten) {
        if (!pString) APS5_INVALID_ARG_EX;
        *pString = string;
    }
    return result;
}

using FullMapping = int32_t (*)(UChar32, UCaseContextIterator*, void*, const UChar**, int32_t);

std::int32_t Contextual(FullMapping mapping, std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale) {
    IteratorForward forward{iter, context};
    return Full(c, pString, [&](const UChar** string) {
        return mapping(c, iter ? ForwardIterator : nullptr, &forward, string, caseLocale);
    });
}

}

extern "C" {

std::int32_t APS5_VABI ucase_tolower_nid_postfix(std::int32_t c) {
    return ucase_tolower(Ready(c));
}

std::int32_t APS5_VABI ucase_toupper_nid_postfix(std::int32_t c) {
    return ucase_toupper(Ready(c));
}

std::int32_t APS5_VABI ucase_totitle_nid_postfix(std::int32_t c) {
    return ucase_totitle(Ready(c));
}

std::int32_t APS5_VABI ucase_fold_nid_postfix(std::int32_t c, std::uint32_t options) {
    return ucase_fold(Ready(c), options);
}

std::int32_t APS5_VABI ucase_getType_nid_postfix(std::int32_t c) {
    return ucase_getType(Ready(c));
}

std::int32_t APS5_VABI ucase_getTypeOrIgnorable_nid_postfix(std::int32_t c) {
    return ucase_getTypeOrIgnorable(Ready(c));
}

std::int8_t APS5_VABI ucase_isSoftDotted_nid_postfix(std::int32_t c) {
    return ucase_isSoftDotted(Ready(c));
}

std::int8_t APS5_VABI ucase_isCaseSensitive_nid_postfix(std::int32_t c) {
    return ucase_isCaseSensitive(Ready(c));
}

std::int32_t APS5_VABI ucase_toFullLower_nid_postfix(std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale) {
    return Contextual(ucase_toFullLower, c, iter, context, pString, caseLocale);
}

std::int32_t APS5_VABI ucase_toFullUpper_nid_postfix(std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale) {
    return Contextual(ucase_toFullUpper, c, iter, context, pString, caseLocale);
}

std::int32_t APS5_VABI ucase_toFullTitle_nid_postfix(std::int32_t c, GuestCaseContextIterator iter, void* context, const char16_t** pString, std::int32_t caseLocale) {
    return Contextual(ucase_toFullTitle, c, iter, context, pString, caseLocale);
}

std::int32_t APS5_VABI ucase_toFullFolding_nid_postfix(std::int32_t c, const char16_t** pString, std::uint32_t options) {
    return Full(c, pString, [&](const UChar** string) {
        return ucase_toFullFolding(c, string, options);
    });
}

}

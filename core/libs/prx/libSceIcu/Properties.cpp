#include "prx/libSceIcu/Properties.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include <unicode/uchar.h>

namespace {

UChar32 Ready(std::int32_t c) {
    IcuBackend::Ensure();
    return c;
}

UErrorCode* Status(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

}

extern "C" {

std::int8_t APS5_VABI u_isalnum_nid_postfix(std::int32_t c) {
    return u_isalnum(Ready(c));
}

std::int8_t APS5_VABI u_isalpha_nid_postfix(std::int32_t c) {
    return u_isalpha(Ready(c));
}

std::int8_t APS5_VABI u_isbase_nid_postfix(std::int32_t c) {
    return u_isbase(Ready(c));
}

std::int8_t APS5_VABI u_iscntrl_nid_postfix(std::int32_t c) {
    return u_iscntrl(Ready(c));
}

std::int8_t APS5_VABI u_isdefined_nid_postfix(std::int32_t c) {
    return u_isdefined(Ready(c));
}

std::int8_t APS5_VABI u_isdigit_nid_postfix(std::int32_t c) {
    return u_isdigit(Ready(c));
}

std::int8_t APS5_VABI u_isgraph_nid_postfix(std::int32_t c) {
    return u_isgraph(Ready(c));
}

std::int8_t APS5_VABI u_islower_nid_postfix(std::int32_t c) {
    return u_islower(Ready(c));
}

std::int8_t APS5_VABI u_isprint_nid_postfix(std::int32_t c) {
    return u_isprint(Ready(c));
}

std::int8_t APS5_VABI u_ispunct_nid_postfix(std::int32_t c) {
    return u_ispunct(Ready(c));
}

std::int8_t APS5_VABI u_istitle_nid_postfix(std::int32_t c) {
    return u_istitle(Ready(c));
}

std::int8_t APS5_VABI u_isupper_nid_postfix(std::int32_t c) {
    return u_isupper(Ready(c));
}

std::int8_t APS5_VABI u_isxdigit_nid_postfix(std::int32_t c) {
    return u_isxdigit(Ready(c));
}

std::int8_t APS5_VABI u_isIDIgnorable_nid_postfix(std::int32_t c) {
    return u_isIDIgnorable(Ready(c));
}

std::int8_t APS5_VABI u_isIDPart_nid_postfix(std::int32_t c) {
    return u_isIDPart(Ready(c));
}

std::int8_t APS5_VABI u_isIDStart_nid_postfix(std::int32_t c) {
    return u_isIDStart(Ready(c));
}

std::int8_t APS5_VABI u_isJavaIDPart_nid_postfix(std::int32_t c) {
    return u_isJavaIDPart(Ready(c));
}

std::int8_t APS5_VABI u_isJavaIDStart_nid_postfix(std::int32_t c) {
    return u_isJavaIDStart(Ready(c));
}

std::int8_t APS5_VABI u_isMirrored_nid_postfix(std::int32_t c) {
    return u_isMirrored(Ready(c));
}

std::int8_t APS5_VABI u_isUAlphabetic_nid_postfix(std::int32_t c) {
    return u_isUAlphabetic(Ready(c));
}

std::int8_t APS5_VABI u_isULowercase_nid_postfix(std::int32_t c) {
    return u_isULowercase(Ready(c));
}

std::int8_t APS5_VABI u_isUUppercase_nid_postfix(std::int32_t c) {
    return u_isUUppercase(Ready(c));
}

std::int8_t APS5_VABI u_charType_nid_postfix(std::int32_t c) {
    return u_charType(Ready(c));
}

std::int32_t APS5_VABI u_charDirection_nid_postfix(std::int32_t c) {
    return u_charDirection(Ready(c));
}

std::int32_t APS5_VABI u_charMirror_nid_postfix(std::int32_t c) {
    return u_charMirror(Ready(c));
}

std::int32_t APS5_VABI u_getBidiPairedBracket_nid_postfix(std::int32_t c) {
    return u_getBidiPairedBracket(Ready(c));
}

void APS5_VABI u_charAge_nid_postfix(std::int32_t c, std::uint8_t* versionArray) {
    u_charAge(Ready(c), versionArray);
}

std::uint8_t APS5_VABI u_getCombiningClass_nid_postfix(std::int32_t c) {
    return u_getCombiningClass(Ready(c));
}

std::int32_t APS5_VABI u_getIntPropertyValue_nid_postfix(std::int32_t c, std::int32_t which) {
    return u_getIntPropertyValue(Ready(c), static_cast<UProperty>(which));
}

std::int32_t APS5_VABI u_getIntPropertyMinValue_nid_postfix(std::int32_t which) {
    IcuBackend::Ensure();
    return u_getIntPropertyMinValue(static_cast<UProperty>(which));
}

std::int32_t APS5_VABI u_getIntPropertyMaxValue_nid_postfix(std::int32_t which) {
    IcuBackend::Ensure();
    return u_getIntPropertyMaxValue(static_cast<UProperty>(which));
}

std::int8_t APS5_VABI u_hasBinaryProperty_nid_postfix(std::int32_t c, std::int32_t which) {
    return u_hasBinaryProperty(Ready(c), static_cast<UProperty>(which));
}

std::int32_t APS5_VABI u_getPropertyEnum_nid_postfix(const char* alias) {
    IcuBackend::Ensure();
    return u_getPropertyEnum(alias);
}

const char* APS5_VABI u_getPropertyName_nid_postfix(std::int32_t property, std::int32_t nameChoice) {
    IcuBackend::Ensure();
    return u_getPropertyName(static_cast<UProperty>(property), static_cast<UPropertyNameChoice>(nameChoice));
}

std::int32_t APS5_VABI u_getPropertyValueEnum_nid_postfix(std::int32_t property, const char* alias) {
    IcuBackend::Ensure();
    return u_getPropertyValueEnum(static_cast<UProperty>(property), alias);
}

const char* APS5_VABI u_getPropertyValueName_nid_postfix(std::int32_t property, std::int32_t value, std::int32_t nameChoice) {
    IcuBackend::Ensure();
    return u_getPropertyValueName(static_cast<UProperty>(property), value, static_cast<UPropertyNameChoice>(nameChoice));
}

double APS5_VABI u_getNumericValue_nid_postfix(std::int32_t c) {
    return u_getNumericValue(Ready(c));
}

std::int32_t APS5_VABI u_foldCase_nid_postfix(std::int32_t c, std::uint32_t options) {
    return u_foldCase(Ready(c), options);
}

std::int32_t APS5_VABI u_tolower_nid_postfix(std::int32_t c) {
    return u_tolower(Ready(c));
}

std::int32_t APS5_VABI u_toupper_nid_postfix(std::int32_t c) {
    return u_toupper(Ready(c));
}

std::int32_t APS5_VABI u_totitle_nid_postfix(std::int32_t c) {
    return u_totitle(Ready(c));
}

std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t code, std::int32_t nameChoice, char* buffer, std::int32_t bufferLength, std::int32_t* status) {
    return u_charName(code, static_cast<UCharNameChoice>(nameChoice), buffer, bufferLength, Status(status));
}

std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t nameChoice, const char* name, std::int32_t* status) {
    return u_charFromName(static_cast<UCharNameChoice>(nameChoice), name, Status(status));
}

std::int32_t APS5_VABI u_getFC_NFKC_Closure_nid_postfix(std::int32_t c, char16_t* dest, std::int32_t destCapacity, std::int32_t* status) {
    return u_getFC_NFKC_Closure(c, dest, destCapacity, Status(status));
}

std::int32_t APS5_VABI ublock_getCode_nid_postfix(std::int32_t c) {
    return ublock_getCode(Ready(c));
}

}

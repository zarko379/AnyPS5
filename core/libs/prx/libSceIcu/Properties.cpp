#include "prx/libSceIcu/Properties.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Set.hpp"
#include <unicode/uchar.h>
#include <unicode/ucpmap.h>
#include <unicode/uniset.h>
#include <unicode/uset.h>
#include <array>
#include <mutex>

namespace {

UChar32 Ready(std::int32_t c) {
    IcuBackend::Ensure();
    return c;
}

UErrorCode* Status(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

UErrorCode* RequiredStatus(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

struct CharNamesForward {
    GuestCharNamesCallback fn;
    void* context;
};

UBool U_CALLCONV ForwardCharName(void* context, UChar32 code, UCharNameChoice nameChoice, const char* name, int32_t length) {
    const auto* forward = static_cast<const CharNamesForward*>(context);
    return forward->fn(forward->context, code, nameChoice, name, length);
}

struct CharTypesForward {
    GuestCharTypesCallback fn;
    const void* context;
};

UBool U_CALLCONV ForwardCharTypes(const void* context, UChar32 start, UChar32 limit, UCharCategory type) {
    const auto* forward = static_cast<const CharTypesForward*>(context);
    return forward->fn(forward->context, start, limit, type);
}

struct ValueFilterForward {
    GuestMapValueFilter fn;
    const void* context;
};

uint32_t U_CALLCONV ForwardValueFilter(const void* context, uint32_t value) {
    const auto* forward = static_cast<const ValueFilterForward*>(context);
    return forward->fn(forward->context, value);
}

std::mutex& PropertySetMutex() {
    static std::mutex mutex;
    return mutex;
}

std::array<USet*, UCHAR_BINARY_LIMIT>& PropertySets() {
    static std::array<USet*, UCHAR_BINARY_LIMIT> sets{};
    return sets;
}

USet* GuestSet(const USet* host) {
    const icu::UnicodeSet* set = icu::UnicodeSet::fromUSet(host);
    USet* guest = uset_openEmpty_nid_postfix();
    const std::int32_t count = set->getRangeCount();
    for (std::int32_t index = 0; index < count; ++index) uset_addRange_nid_postfix(guest, set->getRangeStart(index), set->getRangeEnd(index));
    return guest;
}

const UCPMap* Map(const UCPMap* map) {
    IcuBackend::RequireObject(map);
    IcuBackend::Ensure();
    return map;
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

void APS5_VABI u_enumCharNames_nid_postfix(std::int32_t start, std::int32_t limit, GuestCharNamesCallback fn, void* context, std::int32_t nameChoice, std::int32_t* status) {
    CharNamesForward forward{fn, context};
    u_enumCharNames(start, limit, fn ? ForwardCharName : nullptr, &forward, static_cast<UCharNameChoice>(nameChoice), Status(status));
}

void APS5_VABI u_enumCharTypes_nid_postfix(GuestCharTypesCallback enumRange, const void* context) {
    IcuBackend::Ensure();
    const CharTypesForward forward{enumRange, context};
    u_enumCharTypes(enumRange ? ForwardCharTypes : nullptr, &forward);
}

const USet* APS5_VABI u_getBinaryPropertySet_nid_postfix(std::int32_t property, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    const USet* host = u_getBinaryPropertySet(static_cast<UProperty>(property), error);
    if (!host) return nullptr;
    std::lock_guard lock(PropertySetMutex());
    USet*& guest = PropertySets()[property];
    if (!guest) guest = GuestSet(host);
    return guest;
}

const UCPMap* APS5_VABI u_getIntPropertyMap_nid_postfix(std::int32_t property, std::int32_t* status) {
    return u_getIntPropertyMap(static_cast<UProperty>(property), RequiredStatus(status));
}

std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const UCPMap* map, std::int32_t c) {
    return ucpmap_get(Map(map), c);
}

std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const UCPMap* map, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, GuestMapValueFilter filter, const void* context, std::uint32_t* value) {
    const ValueFilterForward forward{filter, context};
    return ucpmap_getRange(Map(map), start, static_cast<UCPMapRangeOption>(option), surrogateValue, filter ? ForwardValueFilter : nullptr, &forward, value);
}

}

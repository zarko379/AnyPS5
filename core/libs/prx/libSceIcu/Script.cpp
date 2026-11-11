#include "prx/libSceIcu/Script.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include <unicode/uscript.h>

namespace {

UScriptCode Code(std::int32_t script) {
    IcuBackend::Ensure();
    return static_cast<UScriptCode>(script);
}

UErrorCode* Status(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

UErrorCode* RequiredStatus(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

}

extern "C" {

std::int32_t APS5_VABI uscript_getCode_nid_postfix(const char* nameOrAbbrOrLocale, std::int32_t* fillIn, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    return uscript_getCode(nameOrAbbrOrLocale, reinterpret_cast<UScriptCode*>(fillIn), capacity, error);
}

const char* APS5_VABI uscript_getName_nid_postfix(std::int32_t scriptCode) {
    return uscript_getName(Code(scriptCode));
}

const char* APS5_VABI uscript_getShortName_nid_postfix(std::int32_t scriptCode) {
    return uscript_getShortName(Code(scriptCode));
}

std::int32_t APS5_VABI uscript_getScript_nid_postfix(std::int32_t codepoint, std::int32_t* status) {
    return uscript_getScript(codepoint, Status(status));
}

std::int8_t APS5_VABI uscript_hasScript_nid_postfix(std::int32_t c, std::int32_t sc) {
    return uscript_hasScript(c, Code(sc));
}

std::int32_t APS5_VABI uscript_getScriptExtensions_nid_postfix(std::int32_t c, std::int32_t* scripts, std::int32_t capacity, std::int32_t* status) {
    return uscript_getScriptExtensions(c, reinterpret_cast<UScriptCode*>(scripts), capacity, Status(status));
}

std::int32_t APS5_VABI uscript_getSampleString_nid_postfix(std::int32_t script, char16_t* dest, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = RequiredStatus(status);
    return uscript_getSampleString(static_cast<UScriptCode>(script), dest, capacity, error);
}

std::int32_t APS5_VABI uscript_getUsage_nid_postfix(std::int32_t script) {
    return uscript_getUsage(Code(script));
}

std::int8_t APS5_VABI uscript_isRightToLeft_nid_postfix(std::int32_t script) {
    return uscript_isRightToLeft(Code(script));
}

std::int8_t APS5_VABI uscript_breaksBetweenLetters_nid_postfix(std::int32_t script) {
    return uscript_breaksBetweenLetters(Code(script));
}

std::int8_t APS5_VABI uscript_isCased_nid_postfix(std::int32_t script) {
    return uscript_isCased(Code(script));
}

}

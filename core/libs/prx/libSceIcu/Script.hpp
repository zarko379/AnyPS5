#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI uscript_getCode_nid_postfix(const char* nameOrAbbrOrLocale, std::int32_t* fillIn, std::int32_t capacity, std::int32_t* status);
const char* APS5_VABI uscript_getName_nid_postfix(std::int32_t scriptCode);
const char* APS5_VABI uscript_getShortName_nid_postfix(std::int32_t scriptCode);
std::int32_t APS5_VABI uscript_getScript_nid_postfix(std::int32_t codepoint, std::int32_t* status);
std::int8_t APS5_VABI uscript_hasScript_nid_postfix(std::int32_t c, std::int32_t sc);
std::int32_t APS5_VABI uscript_getScriptExtensions_nid_postfix(std::int32_t c, std::int32_t* scripts, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uscript_getSampleString_nid_postfix(std::int32_t script, char16_t* dest, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uscript_getUsage_nid_postfix(std::int32_t script);
std::int8_t APS5_VABI uscript_isRightToLeft_nid_postfix(std::int32_t script);
std::int8_t APS5_VABI uscript_breaksBetweenLetters_nid_postfix(std::int32_t script);
std::int8_t APS5_VABI uscript_isCased_nid_postfix(std::int32_t script);
}

#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char* APS5_VABI u_austrcpy_nid_postfix(char*, const char16_t*);
char* APS5_VABI u_austrncpy_nid_postfix(char*, const char16_t*, std::int32_t);
void APS5_VABI u_charsToUChars_nid_postfix(const char*, char16_t*, std::int32_t);
void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t*, char*, std::int32_t);
std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char*, std::int32_t);
std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t*, std::int32_t);
std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char*, const char*);
char APS5_VABI uprv_ebcdicToAscii_nid_postfix(char);
char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(char);
std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t*, const std::uint8_t*, std::int32_t);
std::uint8_t* APS5_VABI uprv_eastrncpy_nid_postfix(std::uint8_t*, const std::uint8_t*, std::int32_t);
std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char*, const char*);
std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char*, const char*);
}

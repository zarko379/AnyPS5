#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UIDNA;
struct UIDNAInfo;

extern "C" {
UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t options, std::int32_t* status);
void APS5_VABI uidna_close_nid_postfix(UIDNA* idna);
std::int32_t APS5_VABI uidna_nameToASCII_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest, std::int32_t capacity, UIDNAInfo* info, std::int32_t* status);
std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest, std::int32_t capacity, UIDNAInfo* info, std::int32_t* status);
}

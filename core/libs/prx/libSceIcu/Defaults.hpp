#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void APS5_VABI uloc_setDefault_nid_postfix(const char* locale, std::int32_t* status);
const char* APS5_VABI uloc_getDefault_nid_postfix();
void APS5_VABI ucnv_setDefaultName_nid_postfix(const char* name);
const char* APS5_VABI ucnv_getDefaultName_nid_postfix();
}

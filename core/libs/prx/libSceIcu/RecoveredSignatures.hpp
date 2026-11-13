#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConstrainedFieldPosition;
struct UFormattedValue;

extern "C" {

std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t*);

std::int8_t APS5_VABI ufmtval_nextPosition_nid_postfix(const UFormattedValue* ufmtval, UConstrainedFieldPosition* ucfpos, std::int32_t* ec);

}

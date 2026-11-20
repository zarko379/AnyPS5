#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConstrainedFieldPosition;
struct UFormattedValue;
struct UResourceBundle;

extern "C" {

std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t*);

std::int8_t APS5_VABI ufmtval_nextPosition_nid_postfix(const UFormattedValue* ufmtval, UConstrainedFieldPosition* ucfpos, std::int32_t* ec);

void APS5_VABI ures_initStackObject_nid_postfix(UResourceBundle* resB);
std::uint32_t APS5_VABI ures_getUInt_nid_postfix(const UResourceBundle* resB, std::int32_t* status);

}

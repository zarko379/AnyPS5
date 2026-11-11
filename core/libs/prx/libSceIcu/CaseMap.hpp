#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UBreakIterator;

extern "C" {
std::int32_t APS5_VABI u_strToTitle_nid_postfix(char16_t* dest, std::int32_t destCapacity, const char16_t* src, std::int32_t srcLength, UBreakIterator* titleIter, const char* locale, std::int32_t* status);
}

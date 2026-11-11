#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;
struct UCPMap;

using GuestCharNamesCallback = std::int8_t (APS5_VABI*)(void* context, std::int32_t code, std::int32_t nameChoice, const char* name, std::int32_t length);
using GuestMapValueFilter = std::uint32_t (APS5_VABI*)(const void* context, std::uint32_t value);

extern "C" {
std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t code, std::int32_t nameChoice, char* buffer, std::int32_t bufferLength, std::int32_t* status);
std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t nameChoice, const char* name, std::int32_t* status);
std::int32_t APS5_VABI u_getFC_NFKC_Closure_nid_postfix(std::int32_t c, char16_t* dest, std::int32_t destCapacity, std::int32_t* status);
void APS5_VABI u_enumCharNames_nid_postfix(std::int32_t start, std::int32_t limit, GuestCharNamesCallback fn, void* context, std::int32_t nameChoice, std::int32_t* status);
const USet* APS5_VABI u_getBinaryPropertySet_nid_postfix(std::int32_t property, std::int32_t* status);
const UCPMap* APS5_VABI u_getIntPropertyMap_nid_postfix(std::int32_t property, std::int32_t* status);
std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const UCPMap* map, std::int32_t c);
std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const UCPMap* map, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, GuestMapValueFilter filter, const void* context, std::uint32_t* value);
}

#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;

using GuestCharNamesCallback = std::int8_t (APS5_VABI*)(void* context, std::int32_t code, std::int32_t nameChoice, const char* name, std::int32_t length);
using GuestSetAdd = void (APS5_VABI*)(USet* set, std::int32_t c);

struct GuestSetAdder {
    USet* set;
    GuestSetAdd add;
    void* addRange;
    void* addString;
    void* remove;
    void* removeRange;
};

extern "C" {
std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t code, std::int32_t nameChoice, char* buffer, std::int32_t bufferLength, std::int32_t* status);
std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t nameChoice, const char* name, std::int32_t* status);
void APS5_VABI u_enumCharNames_nid_postfix(std::int32_t start, std::int32_t limit, GuestCharNamesCallback fn, void* context, std::int32_t nameChoice, std::int32_t* status);
std::int32_t APS5_VABI uprv_getMaxCharNameLength_nid_postfix();
void APS5_VABI uprv_getCharNameCharacters_nid_postfix(const GuestSetAdder* sa);
}

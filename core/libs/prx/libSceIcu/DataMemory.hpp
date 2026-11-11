#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDataMemory;

struct GuestDataInfo {
    std::uint16_t size;
    std::uint16_t reservedWord;
    std::uint8_t isBigEndian;
    std::uint8_t charsetFamily;
    std::uint8_t sizeofUChar;
    std::uint8_t reservedByte;
    std::uint8_t dataFormat[4];
    std::uint8_t formatVersion[4];
    std::uint8_t dataVersion[4];
};

using GuestDataAcceptable = std::int8_t (APS5_VABI*)(void* context, const char* type, const char* name, const GuestDataInfo* info);

extern "C" {
UDataMemory* APS5_VABI udata_open_nid_postfix(const char* path, const char* type, const char* name, std::int32_t* status);
UDataMemory* APS5_VABI udata_openChoice_nid_postfix(const char* path, const char* type, const char* name, GuestDataAcceptable isAcceptable, void* context, std::int32_t* status);
void APS5_VABI udata_close_nid_postfix(UDataMemory* data);
const void* APS5_VABI udata_getMemory_nid_postfix(UDataMemory* data);
const void* APS5_VABI udata_getRawMemory_nid_postfix(const UDataMemory* data);
std::int32_t APS5_VABI udata_getLength_nid_postfix(const UDataMemory* data);
void APS5_VABI udata_getInfo_nid_postfix(UDataMemory* data, GuestDataInfo* info);
void APS5_VABI udata_setAppData_nid_postfix(const char* path, const void* data, std::int32_t* status);
}

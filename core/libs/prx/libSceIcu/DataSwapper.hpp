#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct VaList;
struct GuestDataSwapper;

using GuestReadUInt16 = std::uint16_t (APS5_VABI*)(std::uint16_t x);
using GuestReadUInt32 = std::uint32_t (APS5_VABI*)(std::uint32_t x);
using GuestCompareInvChars = std::int32_t (APS5_VABI*)(const GuestDataSwapper* ds, const char* outString, std::int32_t outLength, const char16_t* localString, std::int32_t localLength);
using GuestWriteUInt16 = void (APS5_VABI*)(std::uint16_t* p, std::uint16_t x);
using GuestWriteUInt32 = void (APS5_VABI*)(std::uint32_t* p, std::uint32_t x);
using GuestSwapFn = std::int32_t (APS5_VABI*)(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
using GuestPrintError = void (APS5_VABI*)(void* context, const char* fmt, VaList* args);

struct GuestDataSwapper {
    std::int8_t inIsBigEndian;
    std::uint8_t inCharset;
    std::int8_t outIsBigEndian;
    std::uint8_t outCharset;
    GuestReadUInt16 readUInt16;
    GuestReadUInt32 readUInt32;
    GuestCompareInvChars compareInvChars;
    GuestWriteUInt16 writeUInt16;
    GuestWriteUInt32 writeUInt32;
    GuestSwapFn swapArray16;
    GuestSwapFn swapArray32;
    GuestSwapFn swapArray64;
    GuestSwapFn swapInvChars;
    GuestPrintError printError;
    void* printErrorContext;
};

extern "C" {
GuestDataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status);
GuestDataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status);
void APS5_VABI udata_closeSwapper_nid_postfix(GuestDataSwapper* ds);
std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
std::int16_t APS5_VABI udata_readInt16_nid_postfix(const GuestDataSwapper* ds, std::int16_t x);
std::int32_t APS5_VABI udata_readInt32_nid_postfix(const GuestDataSwapper* ds, std::int32_t x);
std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
void APS5_VABI udata_printError_nid_postfix(const GuestDataSwapper* ds, const char* fmt, ...);
std::int32_t APS5_VABI utrie2_swap_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
std::int32_t APS5_VABI ucptrie_swap_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
std::int32_t APS5_VABI utrie_swapAnyVersion_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
std::int32_t APS5_VABI uchar_swapNames_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
std::int32_t APS5_VABI udict_swap_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status);
}

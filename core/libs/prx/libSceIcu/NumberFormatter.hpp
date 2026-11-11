#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFieldPositionIterator;
struct UFormattedNumber;
struct UNumberFormatter;

extern "C" {
UNumberFormatter* APS5_VABI unumf_openForSkeletonAndLocale_nid_postfix(const char16_t* skeleton, std::int32_t skeletonLen, const char* locale, std::int32_t* ec);
UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* ec);
void APS5_VABI unumf_formatDouble_nid_postfix(const UNumberFormatter* uformatter, double value, UFormattedNumber* uresult, std::int32_t* ec);
void APS5_VABI unumf_formatDecimal_nid_postfix(const UNumberFormatter* uformatter, const char* value, std::int32_t valueLen, UFormattedNumber* uresult, std::int32_t* ec);
std::int32_t APS5_VABI unumf_resultToString_nid_postfix(const UFormattedNumber* uresult, char16_t* buffer, std::int32_t bufferCapacity, std::int32_t* ec);
void APS5_VABI unumf_resultGetAllFieldPositions_nid_postfix(const UFormattedNumber* uresult, UFieldPositionIterator* ufpositer, std::int32_t* ec);
void APS5_VABI unumf_close_nid_postfix(UNumberFormatter* uformatter);
void APS5_VABI unumf_closeResult_nid_postfix(UFormattedNumber* uresult);
}

#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UBiDi;

extern "C" {
UBiDi* APS5_VABI ubidi_open_nid_postfix();
void APS5_VABI ubidi_close_nid_postfix(UBiDi* bidi);
void APS5_VABI ubidi_setPara_nid_postfix(UBiDi* bidi, const char16_t* text, std::int32_t length, std::uint8_t paraLevel, std::uint8_t* embeddingLevels, std::int32_t* status);
std::uint8_t APS5_VABI ubidi_getParaLevel_nid_postfix(const UBiDi* bidi);
std::int32_t APS5_VABI ubidi_countRuns_nid_postfix(UBiDi* bidi, std::int32_t* status);
std::int32_t APS5_VABI ubidi_getVisualRun_nid_postfix(UBiDi* bidi, std::int32_t runIndex, std::int32_t* logicalStart, std::int32_t* length);
void APS5_VABI ubidi_getLogicalRun_nid_postfix(const UBiDi* bidi, std::int32_t logicalPosition, std::int32_t* logicalLimit, std::uint8_t* level);
std::int32_t APS5_VABI ubidi_getBaseDirection_nid_postfix(const char16_t* text, std::int32_t length);
std::int32_t APS5_VABI ubidi_getClass_nid_postfix(std::int32_t c);
void APS5_VABI ubidi_reorderVisual_nid_postfix(const std::uint8_t* levels, std::int32_t length, std::int32_t* indexMap);
}

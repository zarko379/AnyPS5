#include "prx/libSceIcu/Bidi.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include <unicode/ubidi.h>
#include "ubidi_props.h"

namespace {

UErrorCode* Status(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

}

extern "C" {

UBiDi* APS5_VABI ubidi_open_nid_postfix() {
    IcuBackend::Ensure();
    return ubidi_open();
}

void APS5_VABI ubidi_close_nid_postfix(UBiDi* bidi) {
    ubidi_close(bidi);
}

void APS5_VABI ubidi_setPara_nid_postfix(UBiDi* bidi, const char16_t* text, std::int32_t length, std::uint8_t paraLevel, std::uint8_t* embeddingLevels, std::int32_t* status) {
    ubidi_setPara(bidi, text, length, paraLevel, embeddingLevels, Status(status));
}

std::uint8_t APS5_VABI ubidi_getParaLevel_nid_postfix(const UBiDi* bidi) {
    return ubidi_getParaLevel(bidi);
}

std::int32_t APS5_VABI ubidi_countRuns_nid_postfix(UBiDi* bidi, std::int32_t* status) {
    return ubidi_countRuns(bidi, Status(status));
}

std::int32_t APS5_VABI ubidi_getVisualRun_nid_postfix(UBiDi* bidi, std::int32_t runIndex, std::int32_t* logicalStart, std::int32_t* length) {
    return ubidi_getVisualRun(bidi, runIndex, logicalStart, length);
}

void APS5_VABI ubidi_getLogicalRun_nid_postfix(const UBiDi* bidi, std::int32_t logicalPosition, std::int32_t* logicalLimit, std::uint8_t* level) {
    IcuBackend::RequireObject(bidi);
    ubidi_getLogicalRun(bidi, logicalPosition, logicalLimit, level);
}

std::int32_t APS5_VABI ubidi_getBaseDirection_nid_postfix(const char16_t* text, std::int32_t length) {
    IcuBackend::Ensure();
    return ubidi_getBaseDirection(text, length);
}

std::int32_t APS5_VABI ubidi_getClass_nid_postfix(std::int32_t c) {
    IcuBackend::Ensure();
    return ubidi_getClass(c);
}

void APS5_VABI ubidi_reorderVisual_nid_postfix(const std::uint8_t* levels, std::int32_t length, std::int32_t* indexMap) {
    ubidi_reorderVisual(levels, length, indexMap);
}

}

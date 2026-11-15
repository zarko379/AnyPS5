#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct StringPiece {
    const char* data;
    std::int32_t length;
};

struct InitOnce {
    std::int32_t state;
    std::int32_t errorCode;
};

static_assert(sizeof(StringPiece) == 16 && sizeof(InitOnce) == 8);

} // namespace Icu

extern "C" {
void* APS5_VABI _ZN6icu_687UMemorynwEm_nid_postfix(std::size_t);
void* APS5_VABI _ZN6icu_687UMemorynaEm_nid_postfix(std::size_t);
void APS5_VABI _ZN6icu_687UMemorydlEPv_nid_postfix(void*);
void APS5_VABI _ZN6icu_687UMemorydaEPv_nid_postfix(void*);
void APS5_VABI _ZN6icu_687UObjectD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_687UObjectD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_687UObjectD2Ev_nid_postfix(void*);
const void* APS5_VABI _ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix(const void*);
void APS5_VABI _ZN6icu_6811StringPieceC1EPKc_nid_postfix(Icu::StringPiece*, const char*);
void APS5_VABI _ZN6icu_6811StringPieceC2EPKc_nid_postfix(Icu::StringPiece*, const char*);
void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(Icu::StringPiece*, const Icu::StringPiece*, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_i_nid_postfix(Icu::StringPiece*, const Icu::StringPiece*, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix(Icu::StringPiece*, const Icu::StringPiece*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(Icu::StringPiece*, const Icu::StringPiece*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPiece3setEPKc_nid_postfix(Icu::StringPiece*, const char*);
std::int32_t APS5_VABI _ZN6icu_6811StringPiece4findES0_i_nid_postfix(Icu::StringPiece*, Icu::StringPiece, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_6811StringPiece7compareES0__nid_postfix(Icu::StringPiece*, Icu::StringPiece);
std::int8_t APS5_VABI _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(const Icu::StringPiece*, const Icu::StringPiece*);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps20isSyntaxOrWhiteSpaceEi_nid_postfix(std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(const char16_t*, std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(const char16_t*, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(const char16_t*, std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(const char16_t*, std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce*);
void APS5_VABI _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce*);
std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char*);
}

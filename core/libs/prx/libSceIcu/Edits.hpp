#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct Edits {
    std::uint16_t* array;
    std::int32_t capacity;
    std::int32_t length;
    std::int32_t delta;
    std::int32_t numChanges;
    std::int32_t errorCode;
    std::uint16_t stackArray[100];
};

struct EditsIterator {
    const std::uint16_t* array;
    std::int32_t index;
    std::int32_t length;
    std::int32_t remaining;
    std::int8_t onlyChanges;
    std::int8_t coarse;
    std::int8_t dir;
    std::int8_t changed;
    std::int32_t oldLength;
    std::int32_t newLength;
    std::int32_t srcIndex;
    std::int32_t replIndex;
    std::int32_t destIndex;
};

static_assert(offsetof(Edits, capacity) == 8 && offsetof(Edits, errorCode) == 24 && offsetof(Edits, stackArray) == 28 && sizeof(Edits) == 232);
static_assert(offsetof(EditsIterator, index) == 8 && offsetof(EditsIterator, remaining) == 16 && offsetof(EditsIterator, onlyChanges) == 20);
static_assert(offsetof(EditsIterator, changed) == 23 && offsetof(EditsIterator, oldLength) == 24 && offsetof(EditsIterator, destIndex) == 40);
static_assert(sizeof(EditsIterator) == 48);

} // namespace Icu

extern "C" {
void APS5_VABI _ZN6icu_685Edits12releaseArrayEv_nid_postfix(Icu::Edits*);
Icu::Edits* APS5_VABI _ZN6icu_685Edits9copyArrayERKS0__nid_postfix(Icu::Edits*, const Icu::Edits*);
Icu::Edits* APS5_VABI _ZN6icu_685Edits9moveArrayERS0__nid_postfix(Icu::Edits*, Icu::Edits*);
Icu::Edits* APS5_VABI _ZN6icu_685EditsaSERKS0__nid_postfix(Icu::Edits*, const Icu::Edits*);
Icu::Edits* APS5_VABI _ZN6icu_685EditsaSEOS0__nid_postfix(Icu::Edits*, Icu::Edits*);
void APS5_VABI _ZN6icu_685EditsD1Ev_nid_postfix(Icu::Edits*);
void APS5_VABI _ZN6icu_685EditsD2Ev_nid_postfix(Icu::Edits*);
void APS5_VABI _ZN6icu_685Edits5resetEv_nid_postfix(Icu::Edits*);
void APS5_VABI _ZN6icu_685Edits12addUnchangedEi_nid_postfix(Icu::Edits*, std::int32_t);
void APS5_VABI _ZN6icu_685Edits10addReplaceEii_nid_postfix(Icu::Edits*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_685Edits6appendEi_nid_postfix(Icu::Edits*, std::int32_t);
std::int8_t APS5_VABI _ZN6icu_685Edits9growArrayEv_nid_postfix(Icu::Edits*);
std::int8_t APS5_VABI _ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(const Icu::Edits*, std::int32_t*);
Icu::Edits* APS5_VABI _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(Icu::Edits*, const Icu::Edits*, const Icu::Edits*,
                                                                                       std::int32_t*);
void APS5_VABI _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(Icu::EditsIterator*, const std::uint16_t*, std::int32_t, std::int8_t, std::int8_t);
void APS5_VABI _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(Icu::EditsIterator*, const std::uint16_t*, std::int32_t, std::int8_t, std::int8_t);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(Icu::EditsIterator*, std::int32_t);
void APS5_VABI _ZN6icu_685Edits8Iterator17updateNextIndexesEv_nid_postfix(Icu::EditsIterator*);
void APS5_VABI _ZN6icu_685Edits8Iterator21updatePreviousIndexesEv_nid_postfix(Icu::EditsIterator*);
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator6noNextEv_nid_postfix(Icu::EditsIterator*);
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(Icu::EditsIterator*, std::int8_t, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(Icu::EditsIterator*, std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(Icu::EditsIterator*, std::int32_t, std::int8_t, std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(Icu::EditsIterator*, std::int32_t,
                                                                                                           std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31sourceIndexFromDestinationIndexEiR10UErrorCode_nid_postfix(Icu::EditsIterator*, std::int32_t,
                                                                                                           std::int32_t*);
}

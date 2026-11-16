#pragma once
#include "CString.hpp"
#include "Collation.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

using ElementDeleter = void (APS5_VABI*)(void*);
using ElementsAreEqual = std::int8_t (APS5_VABI*)(UHashTok, UHashTok);
using ElementComparator = std::int8_t (APS5_VABI*)(UHashTok, UHashTok);
using ElementAssigner = void (APS5_VABI*)(UHashTok*, UHashTok*);

struct Vector {
    const void* vtable;
    std::int32_t count;
    std::int32_t capacity;
    UHashTok* elements;
    ElementDeleter deleter;
    ElementsAreEqual comparer;
};

struct Vector32 {
    const void* vtable;
    std::int32_t count;
    std::int32_t capacity;
    std::int32_t maxCapacity;
    std::int32_t* elements;
};

static_assert(offsetof(Vector, count) == 8 && offsetof(Vector, capacity) == 12 && offsetof(Vector, elements) == 16);
static_assert(offsetof(Vector, deleter) == 24 && offsetof(Vector, comparer) == 32 && sizeof(Vector) == 40);
static_assert(offsetof(Vector32, count) == 8 && offsetof(Vector32, capacity) == 12 && offsetof(Vector32, maxCapacity) == 16);
static_assert(offsetof(Vector32, elements) == 24 && sizeof(Vector32) == 32);

} // namespace Icu

extern "C" {
void APS5_VABI _ZN6icu_6810UStringSetD0Ev_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_6810UStringSetD1Ev_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_6810UStringSetD2Ev_nid_postfix(Icu::Vector*);
void* APS5_VABI _ZN6icu_686UStack3popEv_nid_postfix(Icu::Vector*);
std::int32_t APS5_VABI _ZN6icu_686UStack4popiEv_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_686UStackD0Ev_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_686UStackD1Ev_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_686UStackD2Ev_nid_postfix(Icu::Vector*);
std::int32_t APS5_VABI _ZNK6icu_686UStack6searchEPv_nid_postfix(const Icu::Vector*, void*);
void APS5_VABI _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(Icu::Vector*, void*, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t, std::int32_t*);
Icu::ElementDeleter APS5_VABI _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(Icu::Vector*, Icu::ElementDeleter);
Icu::ElementsAreEqual APS5_VABI _ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(Icu::Vector*, Icu::ElementsAreEqual);
void APS5_VABI _ZN6icu_687UVector12setElementAtEPvi_nid_postfix(Icu::Vector*, void*, std::int32_t);
void APS5_VABI _ZN6icu_687UVector12setElementAtEii_nid_postfix(Icu::Vector*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_687UVector12sortedInsertE8UElementPFaS1_S1_ER10UErrorCode_nid_postfix(Icu::Vector*, UHashTok, Icu::ElementComparator, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(Icu::Vector*, void*, Icu::ElementComparator, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t, Icu::ElementComparator, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_687UVector13removeElementEPv_nid_postfix(Icu::Vector*, void*);
std::int8_t APS5_VABI _ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(Icu::Vector*, void*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector15insertElementAtEiiR10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t, std::int32_t, std::int32_t*);
void* APS5_VABI _ZN6icu_687UVector15orphanElementAtEi_nid_postfix(Icu::Vector*, std::int32_t);
void APS5_VABI _ZN6icu_687UVector15removeElementAtEi_nid_postfix(Icu::Vector*, std::int32_t);
void APS5_VABI _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(Icu::Vector*, GuestComparator, const void*, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(Icu::Vector*, Icu::ElementComparator, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(Icu::Vector*, const Icu::Vector*, Icu::ElementAssigner, std::int32_t*);
void APS5_VABI _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(Icu::Vector*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_687UVector9removeAllERKS0__nid_postfix(Icu::Vector*, const Icu::Vector*);
std::int8_t APS5_VABI _ZN6icu_687UVector9retainAllERKS0__nid_postfix(Icu::Vector*, const Icu::Vector*);
void APS5_VABI _ZN6icu_687UVectorD0Ev_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_687UVectorD1Ev_nid_postfix(Icu::Vector*);
void APS5_VABI _ZN6icu_687UVectorD2Ev_nid_postfix(Icu::Vector*);
std::int8_t APS5_VABI _ZN6icu_687UVectoreqERKS0__nid_postfix(Icu::Vector*, const Icu::Vector*);
std::int32_t APS5_VABI _ZNK6icu_687UVector10elementAtiEi_nid_postfix(const Icu::Vector*, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_687UVector11containsAllERKS0__nid_postfix(const Icu::Vector*, const Icu::Vector*);
std::int8_t APS5_VABI _ZNK6icu_687UVector12containsNoneERKS0__nid_postfix(const Icu::Vector*, const Icu::Vector*);
std::int8_t APS5_VABI _ZNK6icu_687UVector6equalsERKS0__nid_postfix(const Icu::Vector*, const Icu::Vector*);
std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(const Icu::Vector*, UHashTok, std::int32_t, std::int8_t);
std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEPvi_nid_postfix(const Icu::Vector*, void*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEii_nid_postfix(const Icu::Vector*, std::int32_t, std::int32_t);
void** APS5_VABI _ZNK6icu_687UVector7toArrayEPPv_nid_postfix(const Icu::Vector*, void**);
void* APS5_VABI _ZNK6icu_687UVector9elementAtEi_nid_postfix(const Icu::Vector*, std::int32_t);
void APS5_VABI _ZN6icu_689UVector3212setElementAtEii_nid_postfix(Icu::Vector32*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(Icu::Vector32*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(Icu::Vector32*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(Icu::Vector32*, std::int32_t);
void APS5_VABI _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(Icu::Vector32*, std::int32_t, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(Icu::Vector32*, std::int32_t);
void APS5_VABI _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(Icu::Vector32*);
void APS5_VABI _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(Icu::Vector32*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(Icu::Vector32*, const Icu::Vector32*, std::int32_t*);
void APS5_VABI _ZN6icu_689UVector327setSizeEi_nid_postfix(Icu::Vector32*, std::int32_t);
std::int8_t APS5_VABI _ZN6icu_689UVector329removeAllERKS0__nid_postfix(Icu::Vector32*, const Icu::Vector32*);
std::int8_t APS5_VABI _ZN6icu_689UVector329retainAllERKS0__nid_postfix(Icu::Vector32*, const Icu::Vector32*);
void APS5_VABI _ZN6icu_689UVector32D0Ev_nid_postfix(Icu::Vector32*);
void APS5_VABI _ZN6icu_689UVector32D1Ev_nid_postfix(Icu::Vector32*);
void APS5_VABI _ZN6icu_689UVector32D2Ev_nid_postfix(Icu::Vector32*);
std::int8_t APS5_VABI _ZN6icu_689UVector32eqERKS0__nid_postfix(Icu::Vector32*, const Icu::Vector32*);
std::int8_t APS5_VABI _ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(const Icu::Vector32*, const Icu::Vector32*);
std::int8_t APS5_VABI _ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(const Icu::Vector32*, const Icu::Vector32*);
std::int8_t APS5_VABI _ZNK6icu_689UVector326equalsERKS0__nid_postfix(const Icu::Vector32*, const Icu::Vector32*);
std::int32_t APS5_VABI _ZNK6icu_689UVector327indexOfEii_nid_postfix(const Icu::Vector32*, std::int32_t, std::int32_t);
}

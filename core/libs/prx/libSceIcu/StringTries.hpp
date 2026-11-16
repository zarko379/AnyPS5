#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct BytesTrie {
    std::uint8_t* ownedArray;
    const std::uint8_t* bytes;
    const std::uint8_t* pos;
    std::int32_t remainingMatchLength;
};

struct UCharsTrie {
    char16_t* ownedArray;
    const char16_t* uchars;
    const char16_t* pos;
    std::int32_t remainingMatchLength;
};

static_assert(offsetof(BytesTrie, pos) == 16 && offsetof(BytesTrie, remainingMatchLength) == 24 && sizeof(BytesTrie) == 32);
static_assert(offsetof(UCharsTrie, pos) == 16 && offsetof(UCharsTrie, remainingMatchLength) == 24 && sizeof(UCharsTrie) == 32);

} // namespace Icu

extern "C" {
void APS5_VABI _ZN6icu_689BytesTrieD1Ev_nid_postfix(Icu::BytesTrie*);
void APS5_VABI _ZN6icu_689BytesTrieD2Ev_nid_postfix(Icu::BytesTrie*);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(const std::uint8_t*, std::int32_t);
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(const std::uint8_t*);
std::int32_t APS5_VABI _ZNK6icu_689BytesTrie7currentEv_nid_postfix(const Icu::BytesTrie*);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(Icu::BytesTrie*, const std::uint8_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(Icu::BytesTrie*, const std::uint8_t*, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEi_nid_postfix(Icu::BytesTrie*, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(Icu::BytesTrie*, const char*, std::int32_t);
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(const std::uint8_t*, std::int32_t, std::int8_t,
                                                                                                std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(const std::uint8_t*, std::int8_t, std::int32_t*);
void APS5_VABI _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(Icu::UCharsTrie*);
void APS5_VABI _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(Icu::UCharsTrie*);
std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(const Icu::UCharsTrie*);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(Icu::UCharsTrie*, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(Icu::UCharsTrie*, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(Icu::UCharsTrie*, const char16_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(Icu::UCharsTrie*, const char16_t*, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(Icu::UCharsTrie*, std::int32_t);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(Icu::UCharsTrie*, const char16_t* const*, std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(const char16_t*, std::int32_t, std::int8_t, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(const char16_t*, std::int8_t, std::int32_t*);
}

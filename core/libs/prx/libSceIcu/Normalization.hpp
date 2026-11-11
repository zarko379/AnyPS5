#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UNormalizer2;
struct USet;

extern "C" {
const UNormalizer2* APS5_VABI unorm2_getNFCInstance_nid_postfix(std::int32_t* status);
const UNormalizer2* APS5_VABI unorm2_getNFDInstance_nid_postfix(std::int32_t* status);
const UNormalizer2* APS5_VABI unorm2_getNFKCInstance_nid_postfix(std::int32_t* status);
const UNormalizer2* APS5_VABI unorm2_getNFKDInstance_nid_postfix(std::int32_t* status);
const UNormalizer2* APS5_VABI unorm2_getNFKCCasefoldInstance_nid_postfix(std::int32_t* status);
const UNormalizer2* APS5_VABI unorm2_getInstance_nid_postfix(const char* packageName, const char* name, std::int32_t mode, std::int32_t* status);
UNormalizer2* APS5_VABI unorm2_openFiltered_nid_postfix(const UNormalizer2* normalizer, const USet* filterSet, std::int32_t* status);
void APS5_VABI unorm2_close_nid_postfix(UNormalizer2* normalizer);
std::int32_t APS5_VABI unorm2_normalize_nid_postfix(const UNormalizer2* normalizer, const char16_t* source, std::int32_t length, char16_t* destination, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI unorm2_normalizeSecondAndAppend_nid_postfix(const UNormalizer2* normalizer, char16_t* first, std::int32_t firstLength, std::int32_t firstCapacity, const char16_t* second, std::int32_t secondLength, std::int32_t* status);
std::int32_t APS5_VABI unorm2_append_nid_postfix(const UNormalizer2* normalizer, char16_t* first, std::int32_t firstLength, std::int32_t firstCapacity, const char16_t* second, std::int32_t secondLength, std::int32_t* status);
std::int32_t APS5_VABI unorm2_getDecomposition_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint, char16_t* decomposition, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI unorm2_getRawDecomposition_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint, char16_t* decomposition, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI unorm2_composePair_nid_postfix(const UNormalizer2* normalizer, std::int32_t first, std::int32_t second);
std::uint8_t APS5_VABI unorm2_getCombiningClass_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint);
std::int8_t APS5_VABI unorm2_isNormalized_nid_postfix(const UNormalizer2* normalizer, const char16_t* text, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI unorm2_quickCheck_nid_postfix(const UNormalizer2* normalizer, const char16_t* text, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI unorm2_spanQuickCheckYes_nid_postfix(const UNormalizer2* normalizer, const char16_t* text, std::int32_t length, std::int32_t* status);
std::int8_t APS5_VABI unorm2_hasBoundaryBefore_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint);
std::int8_t APS5_VABI unorm2_hasBoundaryAfter_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint);
std::int8_t APS5_VABI unorm2_isInert_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint);
std::int32_t APS5_VABI unorm_compare_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second, std::int32_t secondLength, std::uint32_t options, std::int32_t* status);
}

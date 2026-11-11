#include "prx/libSceIcu/Normalization.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Set.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/unorm.h>
#include <unicode/unorm2.h>
#include <unicode/uset.h>
#include <mutex>
#include <unordered_map>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

const UNormalizer2* Normalizer(const UNormalizer2* normalizer) {
    IcuBackend::RequireObject(normalizer);
    IcuBackend::Ensure();
    return normalizer;
}

std::mutex& FilterMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<const UNormalizer2*, USet*>& FilterSets() {
    static std::unordered_map<const UNormalizer2*, USet*> sets;
    return sets;
}

USet* HostSet(const USet* guestSet, UErrorCode* status) {
    USet* host = uset_openEmpty();
    const std::int32_t count = uset_getItemCount_nid_postfix(guestSet);
    for (std::int32_t index = 0; index < count; ++index) {
        std::int32_t start = 0;
        std::int32_t end = 0;
        std::int32_t error = 0;
        uset_getItem_nid_postfix(guestSet, index, &start, &end, nullptr, 0, &error);
        if (error > 0) {
            uset_close(host);
            *status = static_cast<UErrorCode>(error);
            return nullptr;
        }
        uset_addRange(host, start, end);
    }
    uset_freeze(host);
    return host;
}

const UNormalizer2* Instance(const UNormalizer2* (*get)(UErrorCode*), std::int32_t* status) {
    return get(Status(status));
}

}

extern "C" {

const UNormalizer2* APS5_VABI unorm2_getNFCInstance_nid_postfix(std::int32_t* status) {
    return Instance(unorm2_getNFCInstance, status);
}

const UNormalizer2* APS5_VABI unorm2_getNFDInstance_nid_postfix(std::int32_t* status) {
    return Instance(unorm2_getNFDInstance, status);
}

const UNormalizer2* APS5_VABI unorm2_getNFKCInstance_nid_postfix(std::int32_t* status) {
    return Instance(unorm2_getNFKCInstance, status);
}

const UNormalizer2* APS5_VABI unorm2_getNFKDInstance_nid_postfix(std::int32_t* status) {
    return Instance(unorm2_getNFKDInstance, status);
}

const UNormalizer2* APS5_VABI unorm2_getNFKCCasefoldInstance_nid_postfix(std::int32_t* status) {
    return Instance(unorm2_getNFKCCasefoldInstance, status);
}

const UNormalizer2* APS5_VABI unorm2_getInstance_nid_postfix(const char* packageName, const char* name, std::int32_t mode, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if (packageName) NotImplemented_nid_no_patch("unorm2_getInstance with a package name");
    return unorm2_getInstance(nullptr, name, static_cast<UNormalization2Mode>(mode), error);
}

UNormalizer2* APS5_VABI unorm2_openFiltered_nid_postfix(const UNormalizer2* normalizer, const USet* filterSet, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if (!filterSet) {
        *error = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    Normalizer(normalizer);
    USet* host = HostSet(filterSet, error);
    if (!host) return nullptr;
    UNormalizer2* filtered = unorm2_openFiltered(normalizer, host, error);
    if (!filtered) {
        uset_close(host);
        return nullptr;
    }
    std::lock_guard lock(FilterMutex());
    FilterSets()[filtered] = host;
    return filtered;
}

void APS5_VABI unorm2_close_nid_postfix(UNormalizer2* normalizer) {
    if (!normalizer) return;
    IcuBackend::Ensure();
    USet* host = nullptr;
    {
        std::lock_guard lock(FilterMutex());
        auto found = FilterSets().find(normalizer);
        if (found != FilterSets().end()) {
            host = found->second;
            FilterSets().erase(found);
        }
    }
    unorm2_close(normalizer);
    if (host) uset_close(host);
}

std::int32_t APS5_VABI unorm2_normalize_nid_postfix(const UNormalizer2* normalizer, const char16_t* source, std::int32_t length, char16_t* destination, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_normalize(Normalizer(normalizer), source, length, destination, capacity, error);
}

std::int32_t APS5_VABI unorm2_normalizeSecondAndAppend_nid_postfix(const UNormalizer2* normalizer, char16_t* first, std::int32_t firstLength, std::int32_t firstCapacity, const char16_t* second, std::int32_t secondLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_normalizeSecondAndAppend(Normalizer(normalizer), first, firstLength, firstCapacity, second, secondLength, error);
}

std::int32_t APS5_VABI unorm2_append_nid_postfix(const UNormalizer2* normalizer, char16_t* first, std::int32_t firstLength, std::int32_t firstCapacity, const char16_t* second, std::int32_t secondLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_append(Normalizer(normalizer), first, firstLength, firstCapacity, second, secondLength, error);
}

std::int32_t APS5_VABI unorm2_getDecomposition_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint, char16_t* decomposition, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_getDecomposition(Normalizer(normalizer), codePoint, decomposition, capacity, error);
}

std::int32_t APS5_VABI unorm2_getRawDecomposition_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint, char16_t* decomposition, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_getRawDecomposition(Normalizer(normalizer), codePoint, decomposition, capacity, error);
}

std::int32_t APS5_VABI unorm2_composePair_nid_postfix(const UNormalizer2* normalizer, std::int32_t first, std::int32_t second) {
    return unorm2_composePair(Normalizer(normalizer), first, second);
}

std::uint8_t APS5_VABI unorm2_getCombiningClass_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint) {
    return unorm2_getCombiningClass(Normalizer(normalizer), codePoint);
}

std::int8_t APS5_VABI unorm2_isNormalized_nid_postfix(const UNormalizer2* normalizer, const char16_t* text, std::int32_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_isNormalized(Normalizer(normalizer), text, length, error);
}

std::int32_t APS5_VABI unorm2_quickCheck_nid_postfix(const UNormalizer2* normalizer, const char16_t* text, std::int32_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_quickCheck(Normalizer(normalizer), text, length, error);
}

std::int32_t APS5_VABI unorm2_spanQuickCheckYes_nid_postfix(const UNormalizer2* normalizer, const char16_t* text, std::int32_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm2_spanQuickCheckYes(Normalizer(normalizer), text, length, error);
}

std::int8_t APS5_VABI unorm2_hasBoundaryBefore_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint) {
    return unorm2_hasBoundaryBefore(Normalizer(normalizer), codePoint);
}

std::int8_t APS5_VABI unorm2_hasBoundaryAfter_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint) {
    return unorm2_hasBoundaryAfter(Normalizer(normalizer), codePoint);
}

std::int8_t APS5_VABI unorm2_isInert_nid_postfix(const UNormalizer2* normalizer, std::int32_t codePoint) {
    return unorm2_isInert(Normalizer(normalizer), codePoint);
}

std::int32_t APS5_VABI unorm_compare_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second, std::int32_t secondLength, std::uint32_t options, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return unorm_compare(first, firstLength, second, secondLength, options, error);
}

}

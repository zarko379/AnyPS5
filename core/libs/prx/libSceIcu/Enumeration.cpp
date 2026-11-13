#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t UnsupportedError = 16;

constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t Pad = 8;

std::int32_t Add(std::int32_t left, std::int32_t right) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

std::size_t BufferSize(std::int32_t capacity) {
    return sizeof(std::int32_t) + static_cast<std::size_t>(static_cast<std::int64_t>(capacity));
}

void* Buffer(UEnumeration* enumeration, std::int32_t capacity) {
    if (enumeration->baseContext) {
        if (*static_cast<std::int32_t*>(enumeration->baseContext) < capacity) {
            capacity = Add(capacity, Pad);
            enumeration->baseContext = realloc_nid_postfix(enumeration->baseContext, BufferSize(capacity));
            if (!enumeration->baseContext) return nullptr;
            *static_cast<std::int32_t*>(enumeration->baseContext) = capacity;
        }
    } else {
        capacity = Add(capacity, Pad);
        enumeration->baseContext = malloc_nid_postfix(BufferSize(capacity));
        if (!enumeration->baseContext) return nullptr;
        *static_cast<std::int32_t*>(enumeration->baseContext) = capacity;
    }
    return static_cast<char*>(enumeration->baseContext) + sizeof(std::int32_t);
}

void MemoryError(std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    *status = MemoryAllocationError;
}

bool Failed(const UEnumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return true;
    if (!status) APS5_INVALID_ARG_EX;
    return *status > 0;
}

}

extern "C" {

void APS5_VABI uenum_close_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    if (!enumeration->close) {
        free_nid_postfix(enumeration);
        return;
    }
    if (enumeration->baseContext) free_nid_postfix(enumeration->baseContext);
    enumeration->close(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = UnsupportedError;
    return -1;
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (enumeration->uNext) return enumeration->uNext(enumeration, resultLength, status);
    *status = UnsupportedError;
    return nullptr;
}

const char* APS5_VABI uenum_next_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (!enumeration->next) {
        *status = UnsupportedError;
        return nullptr;
    }
    std::int32_t ignoredLength = 0;
    return enumeration->next(enumeration, resultLength ? resultLength : &ignoredLength, status);
}

void APS5_VABI uenum_reset_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return;
    if (enumeration->reset) {
        enumeration->reset(enumeration, status);
        return;
    }
    *status = UnsupportedError;
}

const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (!enumeration) APS5_INVALID_ARG_EX;
    char16_t* units = nullptr;
    std::int32_t length = 0;
    if (enumeration->next) {
        const char* chars = enumeration->next(enumeration, &length, status);
        if (chars) {
            units = static_cast<char16_t*>(Buffer(enumeration, static_cast<std::int32_t>(static_cast<std::size_t>(Add(length, 1)) * sizeof(char16_t))));
            if (!units) MemoryError(status);
            else u_charsToUChars_nid_postfix(chars, units, Add(length, 1));
        }
    } else {
        if (!status) APS5_INVALID_ARG_EX;
        *status = UnsupportedError;
    }
    if (resultLength) *resultLength = length;
    return units;
}

const char* APS5_VABI uenum_nextDefault_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (!enumeration) APS5_INVALID_ARG_EX;
    if (!enumeration->uNext) {
        if (!status) APS5_INVALID_ARG_EX;
        *status = UnsupportedError;
        return nullptr;
    }
    const char16_t* units = enumeration->uNext(enumeration, resultLength, status);
    if (!units) return nullptr;
    if (!resultLength) APS5_INVALID_ARG_EX;
    char* chars = static_cast<char*>(Buffer(enumeration, Add(*resultLength, 1)));
    if (!chars) {
        MemoryError(status);
        return nullptr;
    }
    u_UCharsToChars_nid_postfix(units, chars, Add(*resultLength, 1));
    return chars;
}

}

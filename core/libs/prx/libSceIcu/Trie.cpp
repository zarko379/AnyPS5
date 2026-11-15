#include "prx/libSceIcu/Trie.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include "utrie2.h"
#include "utrie2_impl.h"
#include <cstddef>
#include <cstdint>

namespace {

#ifdef UTRIE2_DEBUG
#error "the guest reads UTrie2 fields inline and expects ICU's release layout"
#endif

static_assert(sizeof(UBool) == 1 && sizeof(UChar32) == 4);
static_assert(offsetof(UTrie2, index) == 0 && offsetof(UTrie2, data16) == 8 && offsetof(UTrie2, data32) == 16);
static_assert(offsetof(UTrie2, indexLength) == 24 && offsetof(UTrie2, dataLength) == 28);
static_assert(offsetof(UTrie2, index2NullOffset) == 32 && offsetof(UTrie2, dataNullOffset) == 34);
static_assert(offsetof(UTrie2, initialValue) == 36 && offsetof(UTrie2, errorValue) == 40);
static_assert(offsetof(UTrie2, highStart) == 44 && offsetof(UTrie2, highValueIndex) == 48);
static_assert(offsetof(UTrie2, memory) == 56 && offsetof(UTrie2, length) == 64 && offsetof(UTrie2, isMemoryOwned) == 68);
static_assert(offsetof(UTrie2, padding1) == 69 && offsetof(UTrie2, padding2) == 70 && offsetof(UTrie2, newTrie) == 72);
static_assert(sizeof(UTrie2) == 80);
static_assert(sizeof(UTrie2Header) == 16);

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

template<typename T>
const T* Object(const T* object) {
    IcuBackend::RequireObject(object);
    IcuBackend::Ensure();
    return object;
}

template<typename T>
T* Object(T* object) {
    IcuBackend::RequireObject(object);
    IcuBackend::Ensure();
    return object;
}

void CheckSerializedTrie2(std::int32_t valueBits, const void* data, std::int32_t length) {
    if (length <= 0 || (reinterpret_cast<std::uintptr_t>(data) & 3) != 0 || valueBits < 0 || valueBits >= UTRIE2_COUNT_VALUE_BITS) return;
    if (length < static_cast<std::int32_t>(sizeof(UTrie2Header))) return;
    IcuBackend::RequireObject(data);
    const auto* header = static_cast<const UTrie2Header*>(data);
    if (header->signature != UTRIE2_SIG || valueBits != (header->options & UTRIE2_OPTIONS_VALUE_BITS_MASK)) return;
    const std::int32_t indexLength = header->indexLength;
    const std::int32_t dataLength = header->shiftedDataLength << UTRIE2_INDEX_SHIFT;
    const std::int32_t actualLength = static_cast<std::int32_t>(sizeof(UTrie2Header)) + indexLength * 2 + dataLength * (valueBits == UTRIE2_16_VALUE_BITS ? 2 : 4);
    if (length < actualLength) return;
    const std::int32_t nullLimit = valueBits == UTRIE2_16_VALUE_BITS ? indexLength + dataLength : dataLength;
    if (indexLength < UTRIE2_INDEX_1_OFFSET || dataLength < UTRIE2_DATA_START_OFFSET || header->dataNullOffset >= nullLimit) APS5_INVALID_ARG_EX;
}

struct EnumForward {
    GuestTrie2EnumValue value;
    GuestTrie2EnumRange range;
    const void* context;
};

uint32_t U_CALLCONV ForwardEnumValue(const void* context, uint32_t value) {
    const auto* forward = static_cast<const EnumForward*>(context);
    return forward->value(forward->context, value);
}

UBool U_CALLCONV ForwardEnumRange(const void* context, UChar32 start, UChar32 end, uint32_t value) {
    const auto* forward = static_cast<const EnumForward*>(context);
    return forward->range(forward->context, start, end, value);
}

}

extern "C" {

UTrie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(std::int32_t valueBits, const void* data, std::int32_t length, std::int32_t* actualLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) CheckSerializedTrie2(valueBits, data, length);
    return utrie2_openFromSerialized(static_cast<UTrie2ValueBits>(valueBits), data, length, actualLength, error);
}

UTrie2* APS5_VABI utrie2_openDummy_nid_postfix(std::int32_t valueBits, std::uint32_t initialValue, std::uint32_t errorValue, std::int32_t* status) {
    return utrie2_openDummy(static_cast<UTrie2ValueBits>(valueBits), initialValue, errorValue, Status(status));
}

std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const UTrie2* trie, std::int32_t c) {
    return utrie2_get32(Object(trie), c);
}

std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const UTrie2* trie, std::int32_t c) {
    return utrie2_get32FromLeadSurrogateCodeUnit(Object(trie), c);
}

std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const UTrie2* trie, std::int32_t c, const std::uint8_t* src, const std::uint8_t* limit) {
    IcuBackend::RequireObject(Object(trie)->index);
    if (limit < src || (!src && limit)) APS5_INVALID_ARG_EX;
    return utrie2_internalU8NextIndex(trie, c, src, limit);
}

std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const UTrie2* trie, std::int32_t c, const std::uint8_t* start, const std::uint8_t* src) {
    IcuBackend::RequireObject(Object(trie)->index);
    return utrie2_internalU8PrevIndex(trie, c, start, src);
}

void APS5_VABI utrie2_close_nid_postfix(UTrie2* trie) {
    IcuBackend::Ensure();
    utrie2_close(trie);
}

std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const UTrie2* trie) {
    return utrie2_isFrozen(Object(trie));
}

std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const UTrie2* trie, void* data, std::int32_t capacity, std::int32_t* status) {
    return utrie2_serialize(trie, data, capacity, Status(status));
}

void APS5_VABI utrie2_enum_nid_postfix(const UTrie2* trie, GuestTrie2EnumValue enumValue, GuestTrie2EnumRange enumRange, const void* context) {
    IcuBackend::Ensure();
    if (enumRange) IcuBackend::RequireObject(trie);
    const EnumForward forward{enumValue, enumRange, context};
    utrie2_enum(trie, enumValue ? ForwardEnumValue : nullptr, enumRange ? ForwardEnumRange : nullptr, &forward);
}

void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const UTrie2* trie, std::int32_t lead, GuestTrie2EnumValue enumValue, GuestTrie2EnumRange enumRange, const void* context) {
    IcuBackend::Ensure();
    if (enumRange && U16_IS_LEAD(lead)) IcuBackend::RequireObject(trie);
    const EnumForward forward{enumValue, enumRange, context};
    utrie2_enumForLeadSurrogate(trie, lead, enumValue ? ForwardEnumValue : nullptr, enumRange ? ForwardEnumRange : nullptr, &forward);
}

}

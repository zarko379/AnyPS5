#include "prx/libSceIcu/Trie.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ucpmap.h>
#include <unicode/ucptrie.h>
#include <unicode/umutablecptrie.h>
#include "ucptrie_impl.h"
#include "utrie2.h"
#include "utrie2_impl.h"
#include <cstddef>
#include <cstdint>

namespace {

#ifdef UTRIE2_DEBUG
#error "the guest reads UTrie2 fields inline and expects ICU's release layout"
#endif
#ifdef UCPTRIE_DEBUG
#error "the guest reads UCPTrie fields inline and expects ICU's release layout"
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
static_assert(offsetof(UCPTrie, index) == 0 && offsetof(UCPTrie, data) == 8 && sizeof(UCPTrieData) == 8);
static_assert(offsetof(UCPTrie, indexLength) == 16 && offsetof(UCPTrie, dataLength) == 20 && offsetof(UCPTrie, highStart) == 24);
static_assert(offsetof(UCPTrie, shifted12HighStart) == 28 && offsetof(UCPTrie, type) == 30 && offsetof(UCPTrie, valueWidth) == 31);
static_assert(offsetof(UCPTrie, reserved32) == 32 && offsetof(UCPTrie, reserved16) == 36 && offsetof(UCPTrie, index3NullOffset) == 38);
static_assert(offsetof(UCPTrie, dataNullOffset) == 40 && offsetof(UCPTrie, nullValue) == 44);
static_assert(sizeof(UCPTrie) == 48);
static_assert(sizeof(UTrie2Header) == 16 && sizeof(UCPTrieHeader) == 16);

constexpr std::int32_t AsciiLimit = 0x80;

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

void CheckBinaryCodePointTrie(std::int32_t type, std::int32_t valueWidth, const void* data, std::int32_t length) {
    if (length <= 0 || (reinterpret_cast<std::uintptr_t>(data) & 3) != 0 || type < UCPTRIE_TYPE_ANY || type > UCPTRIE_TYPE_SMALL ||
        valueWidth < UCPTRIE_VALUE_BITS_ANY || valueWidth > UCPTRIE_VALUE_BITS_8) return;
    if (length < static_cast<std::int32_t>(sizeof(UCPTrieHeader))) return;
    IcuBackend::RequireObject(data);
    const auto* header = static_cast<const UCPTrieHeader*>(data);
    if (header->signature != UCPTRIE_SIG) return;
    const std::int32_t options = header->options;
    const std::int32_t actualType = (options >> 6) & 3;
    const std::int32_t actualWidth = options & UCPTRIE_OPTIONS_VALUE_BITS_MASK;
    if (actualType > UCPTRIE_TYPE_SMALL || actualWidth > UCPTRIE_VALUE_BITS_8 || (options & UCPTRIE_OPTIONS_RESERVED_MASK) != 0) return;
    if ((type >= 0 && type != actualType) || (valueWidth >= 0 && valueWidth != actualWidth)) return;
    const std::int32_t indexLength = header->indexLength;
    const std::int32_t dataLength = ((options & UCPTRIE_OPTIONS_DATA_LENGTH_MASK) << 4) | header->dataLength;
    const std::int32_t unit = actualWidth == UCPTRIE_VALUE_BITS_16 ? 2 : actualWidth == UCPTRIE_VALUE_BITS_32 ? 4 : 1;
    if (length < static_cast<std::int32_t>(sizeof(UCPTrieHeader)) + indexLength * 2 + dataLength * unit) return;
    const std::int32_t minIndexLength = actualType == UCPTRIE_TYPE_FAST ? UCPTRIE_BMP_INDEX_LENGTH : UCPTRIE_SMALL_INDEX_LENGTH;
    if (indexLength < minIndexLength || dataLength < AsciiLimit) APS5_INVALID_ARG_EX;
}

void CheckSmallIndex(const UCPTrie* trie, UChar32 c) {
    const bool valid = trie->type == UCPTRIE_TYPE_FAST ? 0xffff < c && c < trie->highStart
                                                        : static_cast<std::uint32_t>(c) < static_cast<std::uint32_t>(trie->highStart) && trie->highStart > UCPTRIE_SMALL_LIMIT;
    if (!valid) APS5_INVALID_ARG_EX;
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

struct FilterForward {
    GuestMapValueFilter filter;
    const void* context;
};

uint32_t U_CALLCONV ForwardFilter(const void* context, uint32_t value) {
    const auto* forward = static_cast<const FilterForward*>(context);
    return forward->filter(forward->context, value);
}

const void* RangeTrie(const void* trie, UChar32 start) {
    IcuBackend::Ensure();
    if (static_cast<std::uint32_t>(start) <= 0x10ffff) IcuBackend::RequireObject(trie);
    return trie;
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

UCPTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t type, std::int32_t valueWidth, const void* data, std::int32_t length, std::int32_t* actualLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) CheckBinaryCodePointTrie(type, valueWidth, data, length);
    return ucptrie_openFromBinary(static_cast<UCPTrieType>(type), static_cast<UCPTrieValueWidth>(valueWidth), data, length, actualLength, error);
}

void APS5_VABI ucptrie_close_nid_postfix(UCPTrie* trie) {
    IcuBackend::Ensure();
    ucptrie_close(trie);
}

std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const UCPTrie* trie) {
    return ucptrie_getType(Object(trie));
}

std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const UCPTrie* trie) {
    return ucptrie_getValueWidth(Object(trie));
}

std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const UCPTrie* trie, std::int32_t c) {
    return ucptrie_get(Object(trie), c);
}

std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const UCPTrie* trie, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, GuestMapValueFilter filter, const void* context, std::uint32_t* value) {
    RangeTrie(trie, start);
    const FilterForward forward{filter, context};
    return ucptrie_getRange(trie, start, static_cast<UCPMapRangeOption>(option), surrogateValue, filter ? ForwardFilter : nullptr, &forward, value);
}

std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const UCPTrie* trie, void* data, std::int32_t capacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) Object(trie);
    return ucptrie_toBinary(trie, data, capacity, error);
}

std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const UCPTrie* trie, std::int32_t c) {
    CheckSmallIndex(Object(trie), c);
    return ucptrie_internalSmallIndex(trie, c);
}

std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const UCPTrie* trie, std::int32_t lt1, std::uint8_t t2, std::uint8_t t3) {
    const auto c = static_cast<UChar32>((static_cast<std::uint32_t>(lt1) << 12) | (static_cast<std::uint32_t>(t2) << 6) | t3);
    if (c < Object(trie)->highStart) CheckSmallIndex(trie, c);
    return ucptrie_internalSmallU8Index(trie, lt1, t2, t3);
}

std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const UCPTrie* trie, std::int32_t c, const std::uint8_t* start, const std::uint8_t* src) {
    if (Object(trie)->type != UCPTRIE_TYPE_FAST) APS5_INVALID_ARG_EX;
    return ucptrie_internalU8PrevIndex(trie, c, start, src);
}

UMutableCPTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t initialValue, std::uint32_t errorValue, std::int32_t* status) {
    return umutablecptrie_open(initialValue, errorValue, Status(status));
}

UMutableCPTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const UMutableCPTrie* other, std::int32_t* status) {
    return umutablecptrie_clone(other, Status(status));
}

void APS5_VABI umutablecptrie_close_nid_postfix(UMutableCPTrie* trie) {
    IcuBackend::Ensure();
    umutablecptrie_close(trie);
}

UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const UCPMap* map, std::int32_t* status) {
    return umutablecptrie_fromUCPMap(map, Status(status));
}

UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const UCPTrie* trie, std::int32_t* status) {
    return umutablecptrie_fromUCPTrie(trie, Status(status));
}

std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const UMutableCPTrie* trie, std::int32_t c) {
    return umutablecptrie_get(Object(trie), c);
}

std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const UMutableCPTrie* trie, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, GuestMapValueFilter filter, const void* context, std::uint32_t* value) {
    RangeTrie(trie, start);
    const FilterForward forward{filter, context};
    return umutablecptrie_getRange(trie, start, static_cast<UCPMapRangeOption>(option), surrogateValue, filter ? ForwardFilter : nullptr, &forward, value);
}

void APS5_VABI umutablecptrie_set_nid_postfix(UMutableCPTrie* trie, std::int32_t c, std::uint32_t value, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) Object(trie);
    umutablecptrie_set(trie, c, value, error);
}

void APS5_VABI umutablecptrie_setRange_nid_postfix(UMutableCPTrie* trie, std::int32_t start, std::int32_t end, std::uint32_t value, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) Object(trie);
    umutablecptrie_setRange(trie, start, end, value, error);
}

UCPTrie* APS5_VABI umutablecptrie_buildImmutable_nid_postfix(UMutableCPTrie* trie, std::int32_t type, std::int32_t valueWidth, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error)) Object(trie);
    return umutablecptrie_buildImmutable(trie, static_cast<UCPTrieType>(type), static_cast<UCPTrieValueWidth>(valueWidth), error);
}

}

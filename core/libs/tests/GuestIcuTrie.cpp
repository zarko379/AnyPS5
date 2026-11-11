#include "prx/libSceIcu/Trie.hpp"
#include <unicode/uchar.h>
#include <unicode/ucpmap.h>
#include <unicode/ucptrie.h>
#include <unicode/udata.h>
#include <unicode/umutablecptrie.h>
#include "ucptrie_impl.h"
#include "utrie2.h"
#include "utrie2_impl.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t Untouched = 0x5a5a5a5a;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU TRIE: %s\n", message.c_str());
        std::abort();
    }
}

template<typename TAction>
bool Invalid(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct SetRange {
    UChar32 start, limit;
    uint32_t value;
    UBool overwrite;
};

struct CheckRange {
    UChar32 limit;
    uint32_t value;
};

const SetRange trie2SetRanges2[] = {
    { 0x21,     0x7f,     0x5555, true },
    { 0x2f800,  0x2fedc,  0x7a,   true },
    { 0x72,     0xdd,     3,      true },
    { 0xdd,     0xde,     4,      false },
    { 0x201,    0x240,    6,      true },
    { 0x241,    0x280,    6,      true },
    { 0x281,    0x2c0,    6,      true },
    { 0x2f987,  0x2fa98,  5,      true },
    { 0x2f777,  0x2f883,  0,      true },
    { 0x2f900,  0x2ffaa,  1,      false },
    { 0x2ffaa,  0x2ffab,  2,      true },
    { 0x2ffbb,  0x2ffc0,  7,      true }
};

const CheckRange trie2CheckRanges2[] = {
    { 0,        0 },
    { 0x21,     0 },
    { 0x72,     0x5555 },
    { 0xdd,     3 },
    { 0xde,     4 },
    { 0x201,    0 },
    { 0x240,    6 },
    { 0x241,    0 },
    { 0x280,    6 },
    { 0x281,    0 },
    { 0x2c0,    6 },
    { 0x2f883,  0 },
    { 0x2f987,  0x7a },
    { 0x2fa98,  5 },
    { 0x2fedc,  0x7a },
    { 0x2ffaa,  1 },
    { 0x2ffab,  2 },
    { 0x2ffbb,  0 },
    { 0x2ffc0,  7 },
    { 0x110000, 0 }
};

const CheckRange trie2CheckRanges2D87e[] = {
    { 0x2f800,  6 },
    { 0x2f883,  0 },
    { 0x2f987,  0x7a },
    { 0x2fa98,  5 },
    { 0x2fc00,  0x7a }
};

const SetRange cpSetRanges2[] = {
    { 0x21,     0x7f,     0x5555, false },
    { 0x2f800,  0x2fedc,  0x7a,   false },
    { 0x72,     0xdd,     3,      false },
    { 0xdd,     0xde,     4,      false },
    { 0x201,    0x240,    6,      false },
    { 0x241,    0x280,    6,      false },
    { 0x281,    0x2c0,    6,      false },
    { 0x2f987,  0x2fa98,  5,      false },
    { 0x2f777,  0x2f883,  0,      false },
    { 0x2fedc,  0x2ffaa,  1,      false },
    { 0x2ffaa,  0x2ffab,  2,      false },
    { 0x2ffbb,  0x2ffc0,  7,      false }
};

const CheckRange cpCheckRanges2[] = {
    { 0,        0 },
    { 0x21,     0 },
    { 0x72,     0x5555 },
    { 0xdd,     3 },
    { 0xde,     4 },
    { 0x201,    0 },
    { 0x240,    6 },
    { 0x241,    0 },
    { 0x280,    6 },
    { 0x281,    0 },
    { 0x2c0,    6 },
    { 0x2f883,  0 },
    { 0x2f987,  0x7a },
    { 0x2fa98,  5 },
    { 0x2fedc,  0x7a },
    { 0x2ffaa,  1 },
    { 0x2ffab,  2 },
    { 0x2ffbb,  0 },
    { 0x2ffc0,  7 },
    { 0x110000, 0 }
};

template<std::size_t N>
std::uint32_t Expected(const CheckRange (&ranges)[N], UChar32 c) {
    for (std::size_t index = 1; index < N; ++index)
        if (c < ranges[index].limit) return ranges[index].value;
    return 0xffffffffu;
}

struct Serialized {
    std::vector<std::uint32_t> words;
    std::int32_t length = 0;

    const void* data() const {
        return words.data();
    }
};

UTrie2* HostTrie2(UTrie2ValueBits valueBits, bool freeze) {
    UErrorCode status = U_ZERO_ERROR;
    UTrie2* trie = utrie2_open(0, 0xbad, &status);
    for (const SetRange& range : trie2SetRanges2) utrie2_setRange32(trie, range.start, range.limit - 1, range.value, range.overwrite, &status);
    utrie2_set32ForLeadSurrogateCodeUnit(trie, 0xd801, 0x99, &status);
    if (freeze) utrie2_freeze(trie, valueBits, &status);
    Require(U_SUCCESS(status), "host UTrie2 build");
    return trie;
}

Serialized Serialize(const UTrie2* trie) {
    UErrorCode status = U_ZERO_ERROR;
    Serialized result;
    result.length = utrie2_serialize(trie, nullptr, 0, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR, "host UTrie2 preflight");
    status = U_ZERO_ERROR;
    result.words.assign(static_cast<std::size_t>(result.length + 3) / 4, 0);
    utrie2_serialize(trie, result.words.data(), result.length, &status);
    Require(U_SUCCESS(status), "host UTrie2 serialize");
    return result;
}

std::uint32_t APS5_VABI GuestEnumValue(const void*, std::uint32_t value) {
    return value ^ 0x5555;
}

struct EnumLog {
    std::vector<std::pair<UChar32, UChar32>> ranges;
    std::vector<std::uint32_t> values;
    const void* context = nullptr;
    std::size_t stopAfter = 0;
};

std::int8_t APS5_VABI GuestEnumRange(const void* context, std::int32_t start, std::int32_t end, std::uint32_t value) {
    auto* log = static_cast<EnumLog*>(const_cast<void*>(context));
    log->ranges.emplace_back(start, end);
    log->values.push_back(value);
    return log->stopAfter == 0 || log->ranges.size() < log->stopAfter;
}

UBool U_CALLCONV HostEnumRange(const void* context, UChar32 start, UChar32 end, uint32_t value) {
    auto* log = static_cast<EnumLog*>(const_cast<void*>(context));
    log->ranges.emplace_back(start, end);
    log->values.push_back(value);
    return true;
}

uint32_t U_CALLCONV HostEnumValue(const void*, uint32_t value) {
    return value ^ 0x5555;
}

std::uint32_t APS5_VABI GuestFilter(const void* context, std::uint32_t value) {
    if (context) ++*static_cast<int*>(const_cast<void*>(context));
    return value ^ 0x5555;
}

void TestTrie2Serialized(UTrie2ValueBits valueBits) {
    UTrie2* host = HostTrie2(valueBits, true);
    const Serialized serialized = Serialize(host);
    std::int32_t status = Ok;
    std::int32_t actual = 0;
    UTrie2* trie = utrie2_openFromSerialized_nid_postfix(valueBits, serialized.data(), serialized.length + 8, &actual, &status);
    Require(trie && status == Ok && actual == serialized.length, "openFromSerialized");
    Require(utrie2_isFrozen_nid_postfix(trie) == 1, "isFrozen");
    Require(trie->index == static_cast<const std::uint16_t*>(serialized.data()) + 8, "UTrie2 index field");
    Require((valueBits == UTRIE2_16_VALUE_BITS) == (trie->data16 != nullptr) && (valueBits == UTRIE2_32_VALUE_BITS) == (trie->data32 != nullptr), "UTrie2 data fields");
    Require(trie->errorValue == 0xbad && trie->initialValue == 0, "UTrie2 special values");
    for (UChar32 c = 0; c < 0x110000; ++c) {
        const std::uint32_t expected = Expected(trie2CheckRanges2, c);
        if (utrie2_get32_nid_postfix(trie, c) != expected) Require(false, "get32 " + std::to_string(c));
        const std::uint32_t inline32 = valueBits == UTRIE2_16_VALUE_BITS ? UTRIE2_GET16(trie, c) : UTRIE2_GET32(trie, c);
        if (inline32 != expected) Require(false, "inline get " + std::to_string(c));
    }
    Require(utrie2_get32_nid_postfix(trie, -1) == 0xbad && utrie2_get32_nid_postfix(trie, 0x110000) == 0xbad, "get32 out of range");
    Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0xd801) == 0x99 && utrie2_get32_nid_postfix(trie, 0xd801) == 0, "lead code unit value");
    Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0xd800) == 0 && utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0x41) == 0xbad, "lead code unit errors");

    EnumLog log;
    utrie2_enum_nid_postfix(trie, GuestEnumValue, GuestEnumRange, &log);
    Require(log.ranges.size() == std::size(trie2CheckRanges2) - 1, "enum count");
    for (std::size_t index = 1; index < std::size(trie2CheckRanges2); ++index) {
        Require(log.ranges[index - 1].first == trie2CheckRanges2[index - 1].limit && log.ranges[index - 1].second == trie2CheckRanges2[index].limit - 1, "enum range");
        Require((log.values[index - 1] ^ 0x5555) == trie2CheckRanges2[index].value, "enum value");
    }
    EnumLog raw;
    utrie2_enum_nid_postfix(trie, nullptr, GuestEnumRange, &raw);
    Require(raw.ranges == log.ranges && raw.values[1] == 0x5555 && raw.values[2] == 3, "enum without value function");
    EnumLog stopped;
    stopped.stopAfter = 2;
    utrie2_enum_nid_postfix(trie, GuestEnumValue, GuestEnumRange, &stopped);
    Require(stopped.ranges.size() == 2, "enum stops");
    EnumLog lead;
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xd87e, GuestEnumValue, GuestEnumRange, &lead);
    Require(lead.ranges.size() == std::size(trie2CheckRanges2D87e) - 1, "enumForLeadSurrogate count");
    for (std::size_t index = 1; index < std::size(trie2CheckRanges2D87e); ++index) {
        Require(lead.ranges[index - 1].first == trie2CheckRanges2D87e[index - 1].limit && lead.ranges[index - 1].second == trie2CheckRanges2D87e[index].limit - 1, "enumForLeadSurrogate range");
        Require((lead.values[index - 1] ^ 0x5555) == trie2CheckRanges2D87e[index].value, "enumForLeadSurrogate value");
    }
    EnumLog hostLead;
    EnumLog guestLead;
    utrie2_enumForLeadSurrogate(host, 0xd800, HostEnumValue, HostEnumRange, &hostLead);
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xd800, GuestEnumValue, GuestEnumRange, &guestLead);
    Require(hostLead.ranges == guestLead.ranges && hostLead.values == guestLead.values, "enumForLeadSurrogate against ICU");
    EnumLog none;
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xdc00, GuestEnumValue, GuestEnumRange, &none);
    utrie2_enumForLeadSurrogate_nid_postfix(nullptr, 0x41, GuestEnumValue, GuestEnumRange, &none);
    utrie2_enumForLeadSurrogate_nid_postfix(nullptr, 0xd800, GuestEnumValue, nullptr, &none);
    utrie2_enum_nid_postfix(nullptr, GuestEnumValue, nullptr, &none);
    Require(none.ranges.empty(), "enum calls without ranges");
    Require(Invalid([&] { utrie2_enum_nid_postfix(nullptr, nullptr, GuestEnumRange, &none); }), "enum NULL trie");
    Require(Invalid([&] { utrie2_enumForLeadSurrogate_nid_postfix(nullptr, 0xd800, nullptr, GuestEnumRange, &none); }), "enumForLeadSurrogate NULL trie");

    const std::uint8_t sequences[][4] = {{0xc3, 0xa4, 0, 0}, {0xe4, 0xb8, 0x80, 0}, {0xf0, 0xaf, 0xa0, 0x80}, {0xc0, 0x80, 0, 0}, {0xe4, 0x41, 0, 0}, {0xf4, 0x90, 0x80, 0x80}, {0xed, 0xa0, 0x80, 0}};
    for (const auto& sequence : sequences) {
        for (std::int32_t length = 1; length <= 4; ++length) {
            Require(utrie2_internalU8NextIndex_nid_postfix(trie, sequence[0], sequence + 1, sequence + length) == utrie2_internalU8NextIndex(host, sequence[0], sequence + 1, sequence + length), "U8 next index");
            Require(utrie2_internalU8PrevIndex_nid_postfix(trie, sequence[length - 1], sequence, sequence + length - 1) == utrie2_internalU8PrevIndex(host, sequence[length - 1], sequence, sequence + length - 1), "U8 prev index");
        }
    }
    std::uint8_t longSequence[12] = {0x41, 0x41, 0x41, 0x41, 0x41, 0xf0, 0xaf, 0xa0, 0x80, 0x80, 0x80, 0x80};
    Require(utrie2_internalU8PrevIndex_nid_postfix(trie, 0x80, longSequence, longSequence + 11) == utrie2_internalU8PrevIndex(host, 0x80, longSequence, longSequence + 11), "U8 prev index over 7 bytes");
    Require(utrie2_internalU8NextIndex_nid_postfix(trie, 0xf0, longSequence + 6, longSequence + 12) == utrie2_internalU8NextIndex(host, 0xf0, longSequence + 6, longSequence + 12), "U8 next index over 7 bytes");
    Require(utrie2_internalU8NextIndex_nid_postfix(trie, 0xc3, nullptr, nullptr) == utrie2_internalU8NextIndex(host, 0xc3, nullptr, nullptr), "U8 next index empty");
    Require(utrie2_internalU8PrevIndex_nid_postfix(trie, 0x80, sequences[0] + 2, sequences[0]) == utrie2_internalU8PrevIndex(host, 0x80, sequences[0] + 2, sequences[0]), "U8 prev index reversed reads nothing");
    Require(Invalid([&] { utrie2_internalU8NextIndex_nid_postfix(trie, 0xc3, sequences[0] + 2, sequences[0]); }), "U8 next index reversed");
    Require(Invalid([&] { utrie2_internalU8NextIndex_nid_postfix(trie, 0xc3, nullptr, sequences[0]); }), "U8 next index NULL source");
    Require(Invalid([&] { utrie2_internalU8NextIndex_nid_postfix(nullptr, 0xc3, sequences[0], sequences[0] + 1); }), "U8 next index NULL trie");
    Require(Invalid([&] { utrie2_internalU8PrevIndex_nid_postfix(nullptr, 0x80, sequences[0], sequences[0] + 1); }), "U8 prev index NULL trie");

    std::vector<std::uint32_t> copy(serialized.words.size() + 1, 0);
    status = Ok;
    Require(utrie2_serialize_nid_postfix(trie, nullptr, 0, &status) == serialized.length && status == U_BUFFER_OVERFLOW_ERROR, "serialize preflight");
    status = Ok;
    Require(utrie2_serialize_nid_postfix(trie, copy.data(), serialized.length - 4, &status) == serialized.length && status == U_BUFFER_OVERFLOW_ERROR, "serialize overflow");
    status = Ok;
    Require(utrie2_serialize_nid_postfix(trie, copy.data(), serialized.length, &status) == serialized.length && status == Ok, "serialize");
    Require(std::memcmp(copy.data(), serialized.data(), static_cast<std::size_t>(serialized.length)) == 0, "serialize bytes");
    status = Ok;
    Require(utrie2_serialize_nid_postfix(trie, reinterpret_cast<char*>(copy.data()) + 2, serialized.length, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "serialize misaligned");
    status = Ok;
    Require(utrie2_serialize_nid_postfix(trie, copy.data(), -1, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "serialize negative capacity");
    status = Ok;
    Require(utrie2_serialize_nid_postfix(nullptr, copy.data(), serialized.length, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "serialize NULL trie");
    status = U_ILLEGAL_ARGUMENT_ERROR;
    Require(utrie2_serialize_nid_postfix(nullptr, nullptr, -5, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "serialize failed status");
    Require(Invalid([&] { utrie2_serialize_nid_postfix(trie, copy.data(), serialized.length, nullptr); }), "serialize NULL status");
    utrie2_close_nid_postfix(trie);
    utrie2_close(host);
}

void TestTrie2Errors() {
    UTrie2* host = HostTrie2(UTRIE2_16_VALUE_BITS, true);
    Serialized serialized = Serialize(host);
    std::int32_t status = Untouched;
    std::int32_t actual = 77;
    Require(utrie2_openFromSerialized_nid_postfix(0, nullptr, 0, &actual, &status) == nullptr && status == Untouched && actual == 77, "openFromSerialized failed status");
    const auto open = [&](std::int32_t valueBits, const void* data, std::int32_t length) {
        status = Ok;
        actual = 77;
        UTrie2* trie = utrie2_openFromSerialized_nid_postfix(valueBits, data, length, &actual, &status);
        Require(trie == nullptr && actual == 77, "openFromSerialized failure result");
        return status;
    };
    Require(open(0, serialized.data(), 0) == U_ILLEGAL_ARGUMENT_ERROR, "openFromSerialized length 0");
    Require(open(0, reinterpret_cast<const char*>(serialized.data()) + 2, serialized.length) == U_ILLEGAL_ARGUMENT_ERROR, "openFromSerialized misaligned");
    Require(open(2, serialized.data(), serialized.length) == U_ILLEGAL_ARGUMENT_ERROR && open(-1, serialized.data(), serialized.length) == U_ILLEGAL_ARGUMENT_ERROR, "openFromSerialized value bits");
    Require(open(0, serialized.data(), 15) == U_INVALID_FORMAT_ERROR, "openFromSerialized short header");
    Require(open(1, serialized.data(), serialized.length) == U_INVALID_FORMAT_ERROR, "openFromSerialized value bits mismatch");
    Require(open(0, serialized.data(), serialized.length - 2) == U_INVALID_FORMAT_ERROR, "openFromSerialized truncated");
    Require(open(0, nullptr, 15) == U_INVALID_FORMAT_ERROR, "openFromSerialized NULL short data");
    Require(Invalid([&] { open(0, nullptr, 16); }), "openFromSerialized NULL data");
    Require(Invalid([&] { utrie2_openFromSerialized_nid_postfix(0, serialized.data(), serialized.length, nullptr, nullptr); }), "openFromSerialized NULL status");
    status = Ok;
    UTrie2* trie = utrie2_openFromSerialized_nid_postfix(0, serialized.data(), serialized.length, nullptr, &status);
    Require(trie && status == Ok, "openFromSerialized without actual length");
    utrie2_close_nid_postfix(trie);

    serialized.words[0] = 0x54726933;
    Require(open(0, serialized.data(), serialized.length) == U_INVALID_FORMAT_ERROR, "openFromSerialized signature");
    serialized.words[0] = UTRIE2_SIG;
    auto* header = reinterpret_cast<UTrie2Header*>(serialized.words.data());
    const UTrie2Header saved = *header;
    header->indexLength = UTRIE2_INDEX_1_OFFSET - 1;
    Require(Invalid([&] { open(0, serialized.data(), serialized.length); }), "openFromSerialized short index");
    *header = saved;
    header->shiftedDataLength = static_cast<std::uint16_t>((UTRIE2_DATA_START_OFFSET >> UTRIE2_INDEX_SHIFT) - 1);
    Require(Invalid([&] { open(0, serialized.data(), serialized.length); }), "openFromSerialized short data");
    *header = saved;
    header->dataNullOffset = static_cast<std::uint16_t>(header->indexLength + (header->shiftedDataLength << UTRIE2_INDEX_SHIFT));
    Require(Invalid([&] { open(0, serialized.data(), serialized.length); }), "openFromSerialized null offset");
    header->dataNullOffset = static_cast<std::uint16_t>(header->dataNullOffset - 1);
    status = Ok;
    trie = utrie2_openFromSerialized_nid_postfix(0, serialized.data(), serialized.length, nullptr, &status);
    Require(trie && status == Ok, "openFromSerialized last null offset");
    utrie2_close_nid_postfix(trie);
    *header = saved;
    header->indexLength = 0;
    header->shiftedDataLength = 0;
    Require(Invalid([&] { open(0, serialized.data(), 16); }), "openFromSerialized empty arrays");

    UTrie2* unfrozen = HostTrie2(UTRIE2_16_VALUE_BITS, false);
    const std::uint8_t bytes[2] = {0xc3, 0xa4};
    Require(utrie2_isFrozen_nid_postfix(unfrozen) == 0 && utrie2_get32_nid_postfix(unfrozen, 0x30) == 0x5555 && utrie2_get32_nid_postfix(unfrozen, 0x110000) == 0xbad, "unfrozen trie");
    Require(Invalid([&] { utrie2_internalU8NextIndex_nid_postfix(unfrozen, 0xc3, bytes + 1, bytes + 2); }), "U8 next index unfrozen");
    Require(Invalid([&] { utrie2_internalU8PrevIndex_nid_postfix(unfrozen, 0xa4, bytes, bytes + 1); }), "U8 prev index unfrozen");
    status = Ok;
    Require(utrie2_serialize_nid_postfix(unfrozen, nullptr, 0, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "serialize unfrozen");
    utrie2_close(unfrozen);
    utrie2_close(host);

    Require(Invalid([] { utrie2_get32_nid_postfix(nullptr, 0x41); }), "get32 NULL");
    Require(Invalid([] { utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(nullptr, 0xd800); }), "get32FromLeadSurrogateCodeUnit NULL");
    Require(Invalid([] { utrie2_isFrozen_nid_postfix(nullptr); }), "isFrozen NULL");
    utrie2_close_nid_postfix(nullptr);
}

void TestTrie2Dummy() {
    for (std::int32_t valueBits = 0; valueBits < 2; ++valueBits) {
        std::int32_t status = Ok;
        UTrie2* trie = utrie2_openDummy_nid_postfix(valueBits, 0x1234, 0x5678, &status);
        Require(trie && status == Ok && utrie2_isFrozen_nid_postfix(trie) == 1, "openDummy");
        Require(utrie2_get32_nid_postfix(trie, 0) == 0x1234 && utrie2_get32_nid_postfix(trie, 0x10ffff) == 0x1234 && utrie2_get32_nid_postfix(trie, 0xd800) == 0x1234, "dummy values");
        Require(utrie2_get32_nid_postfix(trie, -5) == 0x5678 && utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0xdbff) == 0x1234, "dummy error and lead values");
        UErrorCode hostStatus = U_ZERO_ERROR;
        UTrie2* host = utrie2_openDummy(static_cast<UTrie2ValueBits>(valueBits), 0x1234, 0x5678, &hostStatus);
        const Serialized expected = Serialize(host);
        std::vector<std::uint32_t> bytes(expected.words.size(), 0);
        status = Ok;
        Require(utrie2_serialize_nid_postfix(trie, bytes.data(), expected.length, &status) == expected.length && status == Ok && bytes == expected.words, "dummy serialize against ICU");
        EnumLog log;
        utrie2_enum_nid_postfix(trie, nullptr, GuestEnumRange, &log);
        Require(log.ranges.size() == 1 && log.ranges[0].first == 0 && log.ranges[0].second == 0x10ffff && log.values[0] == 0x1234, "dummy enum");
        utrie2_close(host);
        utrie2_close_nid_postfix(trie);
    }
    std::int32_t status = Ok;
    Require(utrie2_openDummy_nid_postfix(2, 0, 0, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openDummy value bits");
    status = U_INVALID_FORMAT_ERROR;
    Require(utrie2_openDummy_nid_postfix(0, 0, 0, &status) == nullptr && status == U_INVALID_FORMAT_ERROR, "openDummy failed status");
    Require(Invalid([] { utrie2_openDummy_nid_postfix(0, 0, 0, nullptr); }), "openDummy NULL status");
}

UMutableCPTrie* GuestMutable() {
    std::int32_t status = Ok;
    UMutableCPTrie* trie = umutablecptrie_open_nid_postfix(0, 0xad, &status);
    Require(trie && status == Ok, "umutablecptrie_open");
    for (const SetRange& range : cpSetRanges2) {
        if (range.limit - range.start == 1) umutablecptrie_set_nid_postfix(trie, range.start, range.value, &status);
        else umutablecptrie_setRange_nid_postfix(trie, range.start, range.limit - 1, range.value, &status);
    }
    Require(status == Ok, "umutablecptrie set ranges");
    return trie;
}

template<typename TGet>
void CheckRanges(TGet get, std::uint32_t mask, const std::string& name) {
    for (UChar32 c = 0; c < 0x110000; ++c)
        if (get(c) != (Expected(cpCheckRanges2, c) & mask)) Require(false, name + " " + std::to_string(c));
}

template<typename TRange>
void CheckGetRanges(TRange range, std::uint32_t mask, const std::string& name) {
    int calls = 0;
    std::size_t index = 1;
    UChar32 start = 0;
    std::uint32_t value = 0;
    UChar32 end;
    while ((end = range(start, &calls, &value)) >= 0) {
        while (index < std::size(cpCheckRanges2) - 1 && ((cpCheckRanges2[index].value ^ 0x5555) & mask) == ((cpCheckRanges2[index + 1].value ^ 0x5555) & mask)) ++index;
        Require(end == cpCheckRanges2[index].limit - 1, name + " range end");
        Require(value == ((cpCheckRanges2[index].value & mask) ^ 0x5555), name + " range value");
        start = end + 1;
        ++index;
    }
    Require(start == 0x110000 && calls > 0, name + " range coverage");
}

void TestCodePointTries() {
    UMutableCPTrie* mutableTrie = GuestMutable();
    CheckRanges([&](UChar32 c) { return umutablecptrie_get_nid_postfix(mutableTrie, c); }, 0xffffffffu, "mutable get");
    Require(umutablecptrie_get_nid_postfix(mutableTrie, -1) == 0xad && umutablecptrie_get_nid_postfix(mutableTrie, 0x110000) == 0xad, "mutable error value");
    CheckGetRanges([&](UChar32 start, int* calls, std::uint32_t* value) {
        return umutablecptrie_getRange_nid_postfix(mutableTrie, start, UCPMAP_RANGE_NORMAL, 0, GuestFilter, calls, value);
    }, 0xffffffffu, "mutable getRange");

    const struct {
        std::int32_t type;
        std::int32_t width;
        std::uint32_t mask;
    } builds[] = {{UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16, 0xffff}, {UCPTRIE_TYPE_SMALL, UCPTRIE_VALUE_BITS_32, 0xffffffffu}, {UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_8, 0xff}, {UCPTRIE_TYPE_SMALL, UCPTRIE_VALUE_BITS_16, 0xffff}};
    for (const auto& build : builds) {
        std::int32_t status = Ok;
        UMutableCPTrie* clone = umutablecptrie_clone_nid_postfix(mutableTrie, &status);
        Require(clone && status == Ok, "umutablecptrie_clone");
        UCPTrie* trie = umutablecptrie_buildImmutable_nid_postfix(clone, build.type, build.width, &status);
        Require(trie && status == Ok, "buildImmutable");
        Require(umutablecptrie_get_nid_postfix(clone, 0x30) == 0, "buildImmutable clears the mutable trie");
        umutablecptrie_close_nid_postfix(clone);
        Require(ucptrie_getType_nid_postfix(trie) == build.type && ucptrie_getValueWidth_nid_postfix(trie) == build.width, "type and width");
        Require(trie->type == build.type && trie->valueWidth == build.width && trie->nullValue == 0, "UCPTrie fields");
        CheckRanges([&](UChar32 c) { return ucptrie_get_nid_postfix(trie, c); }, build.mask, "ucptrie_get");
        if (build.width == UCPTRIE_VALUE_BITS_16) CheckRanges([&](UChar32 c) { return static_cast<std::uint32_t>(build.type == UCPTRIE_TYPE_FAST ? UCPTRIE_FAST_GET(trie, UCPTRIE_16, c) : UCPTRIE_SMALL_GET(trie, UCPTRIE_16, c)); }, build.mask, "inline get");
        Require(ucptrie_get_nid_postfix(trie, -1) == (0xad & build.mask) && ucptrie_get_nid_postfix(trie, 0x110000) == (0xad & build.mask), "ucptrie error value");
        CheckGetRanges([&](UChar32 start, int* calls, std::uint32_t* value) {
            return ucptrie_getRange_nid_postfix(trie, start, UCPMAP_RANGE_NORMAL, 0, GuestFilter, calls, value);
        }, build.mask, "ucptrie_getRange");
        for (std::int32_t option = 0; option < 3; ++option) {
            for (UChar32 start : {0, 0x7f, 0xd7ff, 0xd800, 0xd888, 0xdddd, 0xdfff, 0xe000, 0x2f800, 0x10ffff, 0x110000, -1}) {
                std::uint32_t guestValue = 1;
                std::uint32_t hostValue = 1;
                int calls = 0;
                const UChar32 guestEnd = ucptrie_getRange_nid_postfix(trie, start, option, 5, GuestFilter, &calls, &guestValue);
                const UChar32 hostEnd = ucptrie_getRange(trie, start, static_cast<UCPMapRangeOption>(option), 5, HostEnumValue, nullptr, &hostValue);
                Require(guestEnd == hostEnd && guestValue == hostValue, "getRange options against ICU");
                Require(ucptrie_getRange_nid_postfix(trie, start, option, 5, nullptr, nullptr, nullptr) == ucptrie_getRange(trie, start, static_cast<UCPMapRangeOption>(option), 5, nullptr, nullptr, nullptr), "getRange without filter");
            }
        }

        status = Ok;
        const std::int32_t length = ucptrie_toBinary_nid_postfix(trie, nullptr, 0, &status);
        Require(length > 16 && status == U_BUFFER_OVERFLOW_ERROR, "toBinary preflight");
        std::vector<std::uint32_t> bytes(static_cast<std::size_t>(length + 7) / 4, 0);
        status = Ok;
        Require(ucptrie_toBinary_nid_postfix(trie, bytes.data(), length - 1, &status) == length && status == U_BUFFER_OVERFLOW_ERROR, "toBinary overflow");
        status = Ok;
        Require(ucptrie_toBinary_nid_postfix(trie, bytes.data(), length, &status) == length && status == Ok, "toBinary");
        std::vector<std::uint32_t> expected(bytes.size(), 0);
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(ucptrie_toBinary(trie, expected.data(), length, &hostStatus) == length && expected == bytes, "toBinary against ICU");
        status = Ok;
        Require(ucptrie_toBinary_nid_postfix(trie, reinterpret_cast<char*>(bytes.data()) + 1, length, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "toBinary misaligned");
        status = Ok;
        Require(ucptrie_toBinary_nid_postfix(trie, nullptr, 4, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "toBinary NULL data");
        status = U_INVALID_FORMAT_ERROR;
        Require(ucptrie_toBinary_nid_postfix(nullptr, nullptr, -1, &status) == 0 && status == U_INVALID_FORMAT_ERROR, "toBinary failed status");
        Require(Invalid([&] { std::int32_t error = Ok; ucptrie_toBinary_nid_postfix(nullptr, nullptr, 0, &error); }), "toBinary NULL trie");
        Require(Invalid([&] { ucptrie_toBinary_nid_postfix(trie, nullptr, 0, nullptr); }), "toBinary NULL status");

        std::int32_t actual = 0;
        status = Ok;
        UCPTrie* reopened = ucptrie_openFromBinary_nid_postfix(UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY, bytes.data(), length + 4, &actual, &status);
        Require(reopened && status == Ok && actual == length && reopened->index == reinterpret_cast<const std::uint16_t*>(bytes.data()) + 8, "openFromBinary");
        CheckRanges([&](UChar32 c) { return ucptrie_get_nid_postfix(reopened, c); }, build.mask, "reopened get");
        for (UChar32 c : {0x10000, 0x2f800, 0x2fedb, 0x2ffbf, trie->highStart - 1}) {
            if (c <= 0xffff || c >= trie->highStart) continue;
            Require(ucptrie_internalSmallIndex_nid_postfix(reopened, c) == ucptrie_internalSmallIndex(trie, c), "internalSmallIndex against ICU");
            const std::int32_t lt1 = c >> 12;
            const auto t2 = static_cast<std::uint8_t>((c >> 6) & 0x3f);
            const auto t3 = static_cast<std::uint8_t>(c & 0x3f);
            Require(ucptrie_internalSmallU8Index_nid_postfix(reopened, lt1, t2, t3) == ucptrie_internalSmallU8Index(trie, lt1, t2, t3), "internalSmallU8Index against ICU");
        }
        if (build.type == UCPTRIE_TYPE_SMALL) {
            Require(ucptrie_internalSmallIndex_nid_postfix(reopened, 0x1234) == ucptrie_internalSmallIndex(trie, 0x1234), "internalSmallIndex small BMP");
            Require(ucptrie_internalSmallIndex_nid_postfix(reopened, 0x41) == ucptrie_internalSmallIndex(trie, 0x41), "internalSmallIndex small ASCII");
        } else {
            Require(Invalid([&] { ucptrie_internalSmallIndex_nid_postfix(reopened, 0xffff); }), "internalSmallIndex fast BMP");
            Require(Invalid([&] { ucptrie_internalSmallU8Index_nid_postfix(reopened, 0, 1, 1); }), "internalSmallU8Index fast BMP");
        }
        Require(Invalid([&] { ucptrie_internalSmallIndex_nid_postfix(reopened, trie->highStart); }), "internalSmallIndex highStart");
        Require(Invalid([&] { ucptrie_internalSmallIndex_nid_postfix(reopened, -1); }), "internalSmallIndex negative");
        Require(Invalid([&] { ucptrie_internalSmallU8Index_nid_postfix(reopened, -1, 0, 0); }), "internalSmallU8Index negative");
        Require(ucptrie_internalSmallU8Index_nid_postfix(reopened, 0x110, 0, 0) == reopened->dataLength - 2, "internalSmallU8Index high");
        const std::uint8_t sequences[][4] = {{0x41, 0, 0, 0}, {0xc3, 0xa4, 0, 0}, {0xe4, 0xb8, 0x80, 0}, {0xf0, 0xaf, 0xa0, 0x80}, {0xf0, 0xaf, 0xbf, 0xa0}, {0xe4, 0x41, 0, 0}, {0xf4, 0x90, 0x80, 0x80}};
        for (const auto& sequence : sequences) {
            for (std::int32_t last = 0; last < 4; ++last) {
                if (build.type == UCPTRIE_TYPE_FAST) Require(ucptrie_internalU8PrevIndex_nid_postfix(reopened, sequence[last], sequence, sequence + last) == ucptrie_internalU8PrevIndex(trie, sequence[last], sequence, sequence + last), "internalU8PrevIndex against ICU");
                else Require(Invalid([&] { ucptrie_internalU8PrevIndex_nid_postfix(reopened, sequence[last], sequence, sequence + last); }), "internalU8PrevIndex small trie");
            }
        }

        status = Ok;
        UMutableCPTrie* fromTrie = umutablecptrie_fromUCPTrie_nid_postfix(reopened, &status);
        Require(fromTrie && status == Ok, "fromUCPTrie");
        CheckRanges([&](UChar32 c) { return umutablecptrie_get_nid_postfix(fromTrie, c); }, build.mask, "fromUCPTrie get");
        umutablecptrie_close_nid_postfix(fromTrie);
        status = Ok;
        UMutableCPTrie* fromMap = umutablecptrie_fromUCPMap_nid_postfix(reinterpret_cast<const UCPMap*>(reopened), &status);
        Require(fromMap && status == Ok && umutablecptrie_get_nid_postfix(fromMap, 0x2ffaa) == (2 & build.mask) && umutablecptrie_get_nid_postfix(fromMap, -1) == (0xad & build.mask), "fromUCPMap");
        umutablecptrie_close_nid_postfix(fromMap);
        ucptrie_close_nid_postfix(reopened);
        ucptrie_close_nid_postfix(trie);
    }
    umutablecptrie_close_nid_postfix(mutableTrie);
}

void TestCodePointTrieErrors() {
    std::int32_t status = Ok;
    UMutableCPTrie* mutableTrie = GuestMutable();
    UCPTrie* trie = umutablecptrie_buildImmutable_nid_postfix(mutableTrie, UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16, &status);
    std::vector<std::uint32_t> bytes(0x4000, 0);
    const std::int32_t length = ucptrie_toBinary_nid_postfix(trie, bytes.data(), 0x10000, &status);
    Require(status == Ok, "error setup");
    std::int32_t actual = 77;
    const auto open = [&](std::int32_t type, std::int32_t width, const void* data, std::int32_t size) {
        status = Ok;
        actual = 77;
        Require(ucptrie_openFromBinary_nid_postfix(type, width, data, size, &actual, &status) == nullptr && actual == 77, "openFromBinary failure result");
        return status;
    };
    status = Untouched;
    Require(ucptrie_openFromBinary_nid_postfix(9, 9, nullptr, 0, &actual, &status) == nullptr && status == Untouched, "openFromBinary failed status");
    Require(open(-1, -1, bytes.data(), 0) == U_ILLEGAL_ARGUMENT_ERROR, "openFromBinary length");
    Require(open(-1, -1, reinterpret_cast<const char*>(bytes.data()) + 2, length) == U_ILLEGAL_ARGUMENT_ERROR, "openFromBinary misaligned");
    Require(open(2, -1, bytes.data(), length) == U_ILLEGAL_ARGUMENT_ERROR && open(-2, -1, bytes.data(), length) == U_ILLEGAL_ARGUMENT_ERROR, "openFromBinary type");
    Require(open(-1, 3, bytes.data(), length) == U_ILLEGAL_ARGUMENT_ERROR && open(-1, -2, bytes.data(), length) == U_ILLEGAL_ARGUMENT_ERROR, "openFromBinary width");
    Require(open(-1, -1, bytes.data(), 15) == U_INVALID_FORMAT_ERROR && open(-1, -1, nullptr, 15) == U_INVALID_FORMAT_ERROR, "openFromBinary short");
    Require(open(UCPTRIE_TYPE_SMALL, -1, bytes.data(), length) == U_INVALID_FORMAT_ERROR && open(-1, UCPTRIE_VALUE_BITS_32, bytes.data(), length) == U_INVALID_FORMAT_ERROR, "openFromBinary mismatch");
    Require(open(-1, -1, bytes.data(), length - 1) == U_INVALID_FORMAT_ERROR, "openFromBinary truncated");
    Require(Invalid([&] { open(-1, -1, nullptr, 16); }), "openFromBinary NULL data");
    Require(Invalid([&] { ucptrie_openFromBinary_nid_postfix(-1, -1, bytes.data(), length, nullptr, nullptr); }), "openFromBinary NULL status");
    auto* header = reinterpret_cast<UCPTrieHeader*>(bytes.data());
    const UCPTrieHeader saved = *header;
    header->signature = UTRIE2_SIG;
    Require(open(-1, -1, bytes.data(), length) == U_INVALID_FORMAT_ERROR, "openFromBinary signature");
    *header = saved;
    header->options = static_cast<std::uint16_t>(header->options | 0x08);
    Require(open(-1, -1, bytes.data(), length) == U_INVALID_FORMAT_ERROR, "openFromBinary reserved bits");
    *header = saved;
    header->options = static_cast<std::uint16_t>((header->options & ~7) | 3);
    Require(open(-1, -1, bytes.data(), length) == U_INVALID_FORMAT_ERROR, "openFromBinary stored width");
    *header = saved;
    header->indexLength = UCPTRIE_BMP_INDEX_LENGTH - 1;
    Require(Invalid([&] { open(-1, -1, bytes.data(), length); }), "openFromBinary short fast index");
    *header = saved;
    header->options = static_cast<std::uint16_t>(header->options & ~UCPTRIE_OPTIONS_DATA_LENGTH_MASK);
    header->dataLength = 0x7f;
    Require(Invalid([&] { open(-1, -1, bytes.data(), length); }), "openFromBinary short data");
    header->dataLength = 0x80;
    header->dataNullOffset = 0xfff;
    header->options = static_cast<std::uint16_t>(header->options & ~UCPTRIE_OPTIONS_DATA_NULL_OFFSET_MASK);
    status = Ok;
    UCPTrie* minimal = ucptrie_openFromBinary_nid_postfix(-1, -1, bytes.data(), length, nullptr, &status);
    Require(minimal && status == Ok && ucptrie_get_nid_postfix(minimal, 0x30) == 0x5555, "openFromBinary minimal data");
    ucptrie_close_nid_postfix(minimal);
    *header = saved;
    header->options = static_cast<std::uint16_t>(header->options | (UCPTRIE_TYPE_SMALL << 6));
    header->indexLength = UCPTRIE_SMALL_INDEX_LENGTH - 1;
    Require(Invalid([&] { open(-1, -1, bytes.data(), length); }), "openFromBinary short small index");
    header->indexLength = 0;
    header->dataLength = 0;
    header->options = static_cast<std::uint16_t>(UCPTRIE_TYPE_SMALL << 6);
    Require(Invalid([&] { open(-1, -1, bytes.data(), 16); }), "openFromBinary empty arrays");
    *header = saved;

    std::int32_t smallStatus = Ok;
    UMutableCPTrie* lowValues = umutablecptrie_open_nid_postfix(0, 0xad, &smallStatus);
    umutablecptrie_setRange_nid_postfix(lowValues, 0x41, 0x7a, 5, &smallStatus);
    UCPTrie* small = umutablecptrie_buildImmutable_nid_postfix(lowValues, UCPTRIE_TYPE_SMALL, UCPTRIE_VALUE_BITS_8, &smallStatus);
    Require(small && smallStatus == Ok && small->highStart > 0x41 && small->highStart <= UCPTRIE_SMALL_LIMIT && ucptrie_get_nid_postfix(small, 0x41) == 5, "small trie below the small limit");
    Require(Invalid([&] { ucptrie_internalSmallIndex_nid_postfix(small, 0x41); }), "internalSmallIndex below the small limit");
    ucptrie_close_nid_postfix(small);
    umutablecptrie_close_nid_postfix(lowValues);
    Require(Invalid([] { ucptrie_getType_nid_postfix(nullptr); }) && Invalid([] { ucptrie_getValueWidth_nid_postfix(nullptr); }), "type and width NULL");
    Require(Invalid([] { ucptrie_get_nid_postfix(nullptr, 0x41); }), "ucptrie_get NULL");
    Require(ucptrie_getRange_nid_postfix(nullptr, -1, 0, 0, GuestFilter, nullptr, nullptr) == -1 && ucptrie_getRange_nid_postfix(nullptr, 0x110000, 2, 0, nullptr, nullptr, nullptr) == -1, "getRange invalid start reads nothing");
    Require(Invalid([] { ucptrie_getRange_nid_postfix(nullptr, 0x10ffff, 0, 0, nullptr, nullptr, nullptr); }), "getRange NULL trie");
    Require(umutablecptrie_getRange_nid_postfix(nullptr, -1, 1, 0, nullptr, nullptr, nullptr) == -1, "mutable getRange invalid start reads nothing");
    Require(Invalid([] { umutablecptrie_getRange_nid_postfix(nullptr, 0, 0, 0, nullptr, nullptr, nullptr); }), "mutable getRange NULL trie");
    Require(Invalid([] { ucptrie_internalSmallIndex_nid_postfix(nullptr, 0x10000); }) && Invalid([] { ucptrie_internalSmallU8Index_nid_postfix(nullptr, 0x10, 0, 0); }) && Invalid([] { ucptrie_internalU8PrevIndex_nid_postfix(nullptr, 0x41, nullptr, nullptr); }), "internal index NULL trie");
    ucptrie_close_nid_postfix(nullptr);
    ucptrie_close_nid_postfix(trie);

    status = Ok;
    umutablecptrie_set_nid_postfix(mutableTrie, 0x110000, 1, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "set out of range");
    status = Ok;
    umutablecptrie_set_nid_postfix(mutableTrie, -1, 1, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "set negative");
    status = Ok;
    umutablecptrie_setRange_nid_postfix(mutableTrie, 0x20, 0x10, 1, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "setRange reversed");
    status = Ok;
    umutablecptrie_setRange_nid_postfix(mutableTrie, 0x20, 0x110000, 1, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "setRange end");
    status = Ok;
    umutablecptrie_setRange_nid_postfix(mutableTrie, 0x30, 0x10ffff, 9, &status);
    umutablecptrie_set_nid_postfix(mutableTrie, 0x10ffff, 8, &status);
    Require(status == Ok && umutablecptrie_get_nid_postfix(mutableTrie, 0x50000) == 9 && umutablecptrie_get_nid_postfix(mutableTrie, 0x10ffff) == 8 && umutablecptrie_get_nid_postfix(mutableTrie, 0x2f) == 0, "set and setRange");
    status = U_INVALID_FORMAT_ERROR;
    umutablecptrie_set_nid_postfix(nullptr, 0, 1, &status);
    umutablecptrie_setRange_nid_postfix(nullptr, 0, 1, 1, &status);
    Require(umutablecptrie_buildImmutable_nid_postfix(nullptr, 0, 0, &status) == nullptr && status == U_INVALID_FORMAT_ERROR, "failed status reads no trie");
    Require(umutablecptrie_open_nid_postfix(0, 0, &status) == nullptr && umutablecptrie_clone_nid_postfix(mutableTrie, &status) == nullptr && status == U_INVALID_FORMAT_ERROR, "open and clone failed status");
    Require(umutablecptrie_fromUCPTrie_nid_postfix(nullptr, &status) == nullptr && umutablecptrie_fromUCPMap_nid_postfix(nullptr, &status) == nullptr && status == U_INVALID_FORMAT_ERROR, "from failed status");
    status = Ok;
    Require(Invalid([&] { umutablecptrie_set_nid_postfix(nullptr, 0, 1, &status); }) && Invalid([&] { umutablecptrie_setRange_nid_postfix(nullptr, 0, 1, 1, &status); }), "set NULL trie");
    Require(Invalid([&] { umutablecptrie_buildImmutable_nid_postfix(nullptr, 0, 0, &status); }) && Invalid([] { umutablecptrie_get_nid_postfix(nullptr, 0); }), "build and get NULL trie");
    Require(Invalid([&] { umutablecptrie_set_nid_postfix(mutableTrie, 0, 1, nullptr); }) && Invalid([&] { umutablecptrie_setRange_nid_postfix(mutableTrie, 0, 1, 1, nullptr); }), "set NULL status");
    Require(Invalid([] { umutablecptrie_open_nid_postfix(0, 0, nullptr); }) && Invalid([&] { umutablecptrie_clone_nid_postfix(mutableTrie, nullptr); }), "open and clone NULL status");
    Require(Invalid([] { umutablecptrie_fromUCPTrie_nid_postfix(nullptr, nullptr); }) && Invalid([] { umutablecptrie_fromUCPMap_nid_postfix(nullptr, nullptr); }), "from NULL status");
    Require(Invalid([&] { umutablecptrie_buildImmutable_nid_postfix(mutableTrie, 0, 0, nullptr); }), "build NULL status");
    Require(umutablecptrie_clone_nid_postfix(nullptr, &status) == nullptr && status == Ok, "clone NULL");
    Require(umutablecptrie_fromUCPTrie_nid_postfix(nullptr, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "fromUCPTrie NULL");
    status = Ok;
    Require(umutablecptrie_fromUCPMap_nid_postfix(nullptr, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "fromUCPMap NULL");
    status = Ok;
    Require(umutablecptrie_buildImmutable_nid_postfix(mutableTrie, 2, 0, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "buildImmutable type");
    status = Ok;
    Require(umutablecptrie_buildImmutable_nid_postfix(mutableTrie, 0, UCPTRIE_VALUE_BITS_ANY, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "buildImmutable width");
    umutablecptrie_close_nid_postfix(mutableTrie);
    umutablecptrie_close_nid_postfix(nullptr);
}

void TestPropertyMap() {
    UErrorCode hostStatus = U_ZERO_ERROR;
    const UCPMap* map = u_getIntPropertyMap(UCHAR_GENERAL_CATEGORY, &hostStatus);
    Require(U_SUCCESS(hostStatus) && map, "property map");
    const auto* trie = reinterpret_cast<const UCPTrie*>(map);
    std::int32_t status = Ok;
    UMutableCPTrie* mutableTrie = umutablecptrie_fromUCPMap_nid_postfix(map, &status);
    Require(mutableTrie && status == Ok, "fromUCPMap property map");
    for (UChar32 c = -1; c <= 0x110000; c += 7) {
        const std::uint32_t expected = ucpmap_get(map, c);
        if (ucptrie_get_nid_postfix(trie, c) != expected || umutablecptrie_get_nid_postfix(mutableTrie, c) != expected) Require(false, "property map value " + std::to_string(c));
    }
    Require(ucptrie_getType_nid_postfix(trie) == ucptrie_getType(trie) && ucptrie_getValueWidth_nid_postfix(trie) == ucptrie_getValueWidth(trie), "property map type");
    umutablecptrie_close_nid_postfix(mutableTrie);
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    TestTrie2Serialized(UTRIE2_16_VALUE_BITS);
    TestTrie2Serialized(UTRIE2_32_VALUE_BITS);
    TestTrie2Errors();
    TestTrie2Dummy();
    TestCodePointTries();
    TestCodePointTrieErrors();
    TestPropertyMap();
    std::puts("ICU TRIE TESTS PASSED");
    return 0;
}

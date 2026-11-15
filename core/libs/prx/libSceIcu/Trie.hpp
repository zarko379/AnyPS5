#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UTrie2;

using GuestTrie2EnumValue = std::uint32_t (APS5_VABI*)(const void* context, std::uint32_t value);
using GuestTrie2EnumRange = std::int8_t (APS5_VABI*)(const void* context, std::int32_t start, std::int32_t end, std::uint32_t value);

extern "C" {
UTrie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(std::int32_t valueBits, const void* data, std::int32_t length, std::int32_t* actualLength, std::int32_t* status);
UTrie2* APS5_VABI utrie2_openDummy_nid_postfix(std::int32_t valueBits, std::uint32_t initialValue, std::uint32_t errorValue, std::int32_t* status);
std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const UTrie2* trie, std::int32_t c);
std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const UTrie2* trie, std::int32_t c);
std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const UTrie2* trie, std::int32_t c, const std::uint8_t* src, const std::uint8_t* limit);
std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const UTrie2* trie, std::int32_t c, const std::uint8_t* start, const std::uint8_t* src);
void APS5_VABI utrie2_close_nid_postfix(UTrie2* trie);
std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const UTrie2* trie);
std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const UTrie2* trie, void* data, std::int32_t capacity, std::int32_t* status);
void APS5_VABI utrie2_enum_nid_postfix(const UTrie2* trie, GuestTrie2EnumValue enumValue, GuestTrie2EnumRange enumRange, const void* context);
void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const UTrie2* trie, std::int32_t lead, GuestTrie2EnumValue enumValue, GuestTrie2EnumRange enumRange, const void* context);
}

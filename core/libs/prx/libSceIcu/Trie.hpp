#pragma once
#include "prx/libSceIcu/CharacterProperties.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UTrie2;
struct UCPTrie;
struct UCPMap;
struct UMutableCPTrie;

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
UCPTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t type, std::int32_t valueWidth, const void* data, std::int32_t length, std::int32_t* actualLength, std::int32_t* status);
void APS5_VABI ucptrie_close_nid_postfix(UCPTrie* trie);
std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const UCPTrie* trie);
std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const UCPTrie* trie);
std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const UCPTrie* trie, std::int32_t c);
std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const UCPTrie* trie, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, GuestMapValueFilter filter, const void* context, std::uint32_t* value);
std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const UCPTrie* trie, void* data, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const UCPTrie* trie, std::int32_t c);
std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const UCPTrie* trie, std::int32_t lt1, std::uint8_t t2, std::uint8_t t3);
std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const UCPTrie* trie, std::int32_t c, const std::uint8_t* start, const std::uint8_t* src);
UMutableCPTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t initialValue, std::uint32_t errorValue, std::int32_t* status);
UMutableCPTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const UMutableCPTrie* other, std::int32_t* status);
void APS5_VABI umutablecptrie_close_nid_postfix(UMutableCPTrie* trie);
UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const UCPMap* map, std::int32_t* status);
UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const UCPTrie* trie, std::int32_t* status);
std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const UMutableCPTrie* trie, std::int32_t c);
std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const UMutableCPTrie* trie, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, GuestMapValueFilter filter, const void* context, std::uint32_t* value);
void APS5_VABI umutablecptrie_set_nid_postfix(UMutableCPTrie* trie, std::int32_t c, std::uint32_t value, std::int32_t* status);
void APS5_VABI umutablecptrie_setRange_nid_postfix(UMutableCPTrie* trie, std::int32_t start, std::int32_t end, std::uint32_t value, std::int32_t* status);
UCPTrie* APS5_VABI umutablecptrie_buildImmutable_nid_postfix(UMutableCPTrie* trie, std::int32_t type, std::int32_t valueWidth, std::int32_t* status);
}

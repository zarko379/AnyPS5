#pragma once
#include "CString.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

using HashFunction = std::int32_t (APS5_VABI*)(UHashTok);
using KeyComparator = std::int8_t (APS5_VABI*)(UHashTok, UHashTok);
using ObjectDeleter = void (APS5_VABI*)(void*);

struct HashElement {
    std::int32_t hashcode;
    UHashTok value;
    UHashTok key;
};

struct Hashtable {
    HashElement* elements;
    HashFunction keyHasher;
    KeyComparator keyComparator;
    KeyComparator valueComparator;
    ObjectDeleter keyDeleter;
    ObjectDeleter valueDeleter;
    std::int32_t count;
    std::int32_t length;
    std::int32_t highWaterMark;
    std::int32_t lowWaterMark;
    float highWaterRatio;
    float lowWaterRatio;
    std::int8_t primeIndex;
    std::int8_t allocated;
};

static_assert(offsetof(HashElement, hashcode) == 0 && offsetof(HashElement, value) == 8 && offsetof(HashElement, key) == 16);
static_assert(sizeof(HashElement) == 24);
static_assert(offsetof(Hashtable, keyHasher) == 8 && offsetof(Hashtable, keyComparator) == 16 && offsetof(Hashtable, valueComparator) == 24);
static_assert(offsetof(Hashtable, keyDeleter) == 32 && offsetof(Hashtable, valueDeleter) == 40 && offsetof(Hashtable, count) == 48);
static_assert(offsetof(Hashtable, length) == 52 && offsetof(Hashtable, highWaterMark) == 56 && offsetof(Hashtable, lowWaterMark) == 60);
static_assert(offsetof(Hashtable, highWaterRatio) == 64 && offsetof(Hashtable, lowWaterRatio) == 68);
static_assert(offsetof(Hashtable, primeIndex) == 72 && offsetof(Hashtable, allocated) == 73 && sizeof(Hashtable) == 80);

} // namespace Icu

extern "C" {
Icu::Hashtable* APS5_VABI uhash_open_nid_postfix(Icu::HashFunction, Icu::KeyComparator, Icu::KeyComparator, std::int32_t*);
Icu::Hashtable* APS5_VABI uhash_openSize_nid_postfix(Icu::HashFunction, Icu::KeyComparator, Icu::KeyComparator, std::int32_t, std::int32_t*);
Icu::Hashtable* APS5_VABI uhash_init_nid_postfix(Icu::Hashtable*, Icu::HashFunction, Icu::KeyComparator, Icu::KeyComparator, std::int32_t*);
Icu::Hashtable* APS5_VABI uhash_initSize_nid_postfix(Icu::Hashtable*, Icu::HashFunction, Icu::KeyComparator, Icu::KeyComparator, std::int32_t,
                                                     std::int32_t*);
void APS5_VABI uhash_close_nid_postfix(Icu::Hashtable*);
Icu::HashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(Icu::Hashtable*, Icu::HashFunction);
Icu::KeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(Icu::Hashtable*, Icu::KeyComparator);
Icu::KeyComparator APS5_VABI uhash_setValueComparator_nid_postfix(Icu::Hashtable*, Icu::KeyComparator);
Icu::ObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(Icu::Hashtable*, Icu::ObjectDeleter);
Icu::ObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(Icu::Hashtable*, Icu::ObjectDeleter);
void APS5_VABI uhash_setResizePolicy_nid_postfix(Icu::Hashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_count_nid_postfix(const Icu::Hashtable*);
void* APS5_VABI uhash_get_nid_postfix(const Icu::Hashtable*, const void*);
void* APS5_VABI uhash_iget_nid_postfix(const Icu::Hashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_geti_nid_postfix(const Icu::Hashtable*, const void*);
std::int32_t APS5_VABI uhash_igeti_nid_postfix(const Icu::Hashtable*, std::int32_t);
void* APS5_VABI uhash_put_nid_postfix(Icu::Hashtable*, void*, void*, std::int32_t*);
void* APS5_VABI uhash_iput_nid_postfix(Icu::Hashtable*, std::int32_t, void*, std::int32_t*);
std::int32_t APS5_VABI uhash_puti_nid_postfix(Icu::Hashtable*, void*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI uhash_iputi_nid_postfix(Icu::Hashtable*, std::int32_t, std::int32_t, std::int32_t*);
void* APS5_VABI uhash_remove_nid_postfix(Icu::Hashtable*, const void*);
void* APS5_VABI uhash_iremove_nid_postfix(Icu::Hashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_removei_nid_postfix(Icu::Hashtable*, const void*);
std::int32_t APS5_VABI uhash_iremovei_nid_postfix(Icu::Hashtable*, std::int32_t);
void APS5_VABI uhash_removeAll_nid_postfix(Icu::Hashtable*);
const Icu::HashElement* APS5_VABI uhash_find_nid_postfix(const Icu::Hashtable*, const void*);
const Icu::HashElement* APS5_VABI uhash_nextElement_nid_postfix(const Icu::Hashtable*, std::int32_t*);
void* APS5_VABI uhash_removeElement_nid_postfix(Icu::Hashtable*, const Icu::HashElement*);
std::int8_t APS5_VABI uhash_equals_nid_postfix(const Icu::Hashtable*, const Icu::Hashtable*);
void APS5_VABI uhash_deleteHashtable_nid_postfix(void*);
}

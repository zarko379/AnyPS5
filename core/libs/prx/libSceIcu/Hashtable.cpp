// Derived from icu4c/source/common/uhash.cpp and uhash_us.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "Hashtable.hpp"
#include "Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <stdexcept>
#include <string>

namespace {

using Icu::HashElement;
using Icu::Hashtable;

constexpr std::array<std::int32_t, 29> Primes{7,        13,       31,       61,        127,       251,       509,       1021,
                                              2039,     4093,     8191,     16381,     32749,     65521,     131071,    262139,
                                              524287,   1048573,  2097143,  4194301,   8388593,   16777213,  33554393,  67108859,
                                              134217689, 268435399, 536870909, 1073741789, 2147483647};
constexpr std::int32_t DefaultPrimeIndex = 4;
constexpr std::array<float, 6> ResizePolicyRatios{0.0F, 0.5F, 0.1F, 0.5F, 0.0F, 1.0F};
constexpr std::int32_t HashDeleted = static_cast<std::int32_t>(0x80000000u);
constexpr std::int32_t HashEmpty = HashDeleted + 1;
constexpr std::int8_t HintKeyPointer = 1;
constexpr std::int8_t HintValuePointer = 2;
constexpr std::int32_t MemoryAllocationError = 7;

bool EmptyOrDeleted(std::int32_t hashcode) {
    return hashcode < 0;
}

bool Failure(const std::int32_t* status) {
    return *status > 0;
}

std::int32_t* Status(std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    return status;
}

template<typename TTable>
TTable* Table(TTable* hash) {
    if (!hash) APS5_INVALID_ARG_EX;
    return hash;
}

UHashTok Pointer(const void* pointer) {
    UHashTok token;
    token.pointer = const_cast<void*>(pointer);
    return token;
}

UHashTok Integer(std::int32_t integer) {
    UHashTok token;
    token.pointer = nullptr;
    token.integer = integer;
    return token;
}

UHashTok Empty() {
    UHashTok token;
    token.pointer = nullptr;
    token.integer = 0;
    return token;
}

std::int32_t HashKey(const Hashtable* hash, UHashTok key) {
    if (!hash->keyHasher) APS5_INVALID_ARG_EX;
    return hash->keyHasher(key);
}

bool SameKey(const Hashtable* hash, UHashTok left, UHashTok right) {
    if (!hash->keyComparator) APS5_INVALID_ARG_EX;
    return hash->keyComparator(left, right) != 0;
}

void DeleteKeyValue(const Hashtable* hash, void* key, void* value) {
    if (hash->keyDeleter && key) hash->keyDeleter(key);
    if (hash->valueDeleter && value) hash->valueDeleter(value);
}

UHashTok SetElement(Hashtable* hash, HashElement* element, std::int32_t hashcode, UHashTok key, UHashTok value, std::int8_t hint) {
    UHashTok oldValue = element->value;
    if (hash->keyDeleter && element->key.pointer && element->key.pointer != key.pointer) hash->keyDeleter(element->key.pointer);
    if (hash->valueDeleter) {
        if (oldValue.pointer && oldValue.pointer != value.pointer) hash->valueDeleter(oldValue.pointer);
        oldValue.pointer = nullptr;
    }
    if (hint & HintKeyPointer) element->key.pointer = key.pointer;
    else element->key = key;
    if (hint & HintValuePointer) element->value.pointer = value.pointer;
    else element->value = value;
    element->hashcode = hashcode;
    return oldValue;
}

UHashTok InternalRemoveElement(Hashtable* hash, HashElement* element) {
    --hash->count;
    return SetElement(hash, element, HashDeleted, Empty(), Empty(), 0);
}

void InternalSetResizePolicy(Hashtable* hash, std::int32_t policy) {
    if (policy < 0 || policy > 2) APS5_INVALID_ARG_EX;
    hash->lowWaterRatio = ResizePolicyRatios[static_cast<std::size_t>(policy) * 2];
    hash->highWaterRatio = ResizePolicyRatios[static_cast<std::size_t>(policy) * 2 + 1];
}

void Allocate(Hashtable* hash, std::int32_t primeIndex, std::int32_t* status) {
    if (Failure(status)) return;
    hash->primeIndex = static_cast<std::int8_t>(primeIndex);
    hash->length = Primes[static_cast<std::size_t>(primeIndex)];
    hash->elements = static_cast<HashElement*>(uprv_malloc_nid_postfix(sizeof(HashElement) * static_cast<std::size_t>(hash->length)));
    if (!hash->elements) {
        *status = MemoryAllocationError;
        return;
    }
    for (std::int32_t index = 0; index < hash->length; ++index) {
        hash->elements[index].key = Empty();
        hash->elements[index].value = Empty();
        hash->elements[index].hashcode = HashEmpty;
    }
    hash->count = 0;
    hash->lowWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->lowWaterRatio);
    hash->highWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->highWaterRatio);
}

Hashtable* Init(Hashtable* result, Icu::HashFunction keyHash, Icu::KeyComparator keyComp, Icu::KeyComparator valueComp, std::int32_t primeIndex,
                std::int32_t* status) {
    if (Failure(status)) return nullptr;
    Table(result);
    result->keyHasher = keyHash;
    result->keyComparator = keyComp;
    result->valueComparator = valueComp;
    result->keyDeleter = nullptr;
    result->valueDeleter = nullptr;
    result->allocated = 0;
    InternalSetResizePolicy(result, 0);
    Allocate(result, primeIndex, status);
    if (Failure(status)) return nullptr;
    return result;
}

Hashtable* Create(Icu::HashFunction keyHash, Icu::KeyComparator keyComp, Icu::KeyComparator valueComp, std::int32_t primeIndex, std::int32_t* status) {
    if (Failure(status)) return nullptr;
    auto* result = static_cast<Hashtable*>(uprv_malloc_nid_postfix(sizeof(Hashtable)));
    if (!result) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    Init(result, keyHash, keyComp, valueComp, primeIndex, status);
    result->allocated = 1;
    if (Failure(status)) {
        IcuReleaseMemory_nid_no_patch(result);
        return nullptr;
    }
    return result;
}

std::int32_t PrimeIndexForSize(std::int32_t size) {
    std::int32_t index = 0;
    while (index < static_cast<std::int32_t>(Primes.size()) - 1 && Primes[static_cast<std::size_t>(index)] < size) ++index;
    return index;
}

HashElement* Find(const Hashtable* hash, UHashTok key, std::int32_t hashcode) {
    std::int32_t firstDeleted = -1;
    std::int32_t jump = 0;
    std::int32_t tableHash = HashEmpty;
    HashElement* elements = hash->elements;
    if (!elements || hash->length <= 1) APS5_INVALID_ARG_EX;
    hashcode &= 0x7FFFFFFF;
    const std::int32_t startIndex = (hashcode ^ 0x4000000) % hash->length;
    std::int32_t index = startIndex;
    do {
        tableHash = elements[index].hashcode;
        if (tableHash == hashcode) {
            if (SameKey(hash, key, elements[index].key)) return &elements[index];
        } else if (!EmptyOrDeleted(tableHash)) {
        } else if (tableHash == HashEmpty) {
            break;
        } else if (firstDeleted < 0) {
            firstDeleted = index;
        }
        if (jump == 0) jump = (hashcode % (hash->length - 1)) + 1;
        index = (index + jump) % hash->length;
    } while (index != startIndex);
    if (firstDeleted >= 0) index = firstDeleted;
    else if (tableHash != HashEmpty) APS5_INVALID_ARG_EX;
    return &elements[index];
}

void Rehash(Hashtable* hash, std::int32_t* status) {
    HashElement* old = hash->elements;
    const std::int32_t oldLength = hash->length;
    std::int32_t newPrimeIndex = hash->primeIndex;
    if (hash->count > hash->highWaterMark) {
        if (++newPrimeIndex >= static_cast<std::int32_t>(Primes.size())) return;
    } else if (hash->count < hash->lowWaterMark) {
        if (--newPrimeIndex < 0) return;
    } else {
        return;
    }
    Allocate(hash, newPrimeIndex, status);
    if (Failure(status)) {
        hash->elements = old;
        hash->length = oldLength;
        return;
    }
    for (std::int32_t index = oldLength - 1; index >= 0; --index) {
        if (!EmptyOrDeleted(old[index].hashcode)) {
            HashElement* element = Find(hash, old[index].key, old[index].hashcode);
            element->key = old[index].key;
            element->value = old[index].value;
            element->hashcode = old[index].hashcode;
            ++hash->count;
        }
    }
    IcuReleaseMemory_nid_no_patch(old);
}

UHashTok Remove(Hashtable* hash, UHashTok key) {
    const std::int32_t hashcode = HashKey(hash, key);
    HashElement* element = Find(hash, key, hashcode);
    UHashTok result = Empty();
    if (!EmptyOrDeleted(element->hashcode)) {
        result = InternalRemoveElement(hash, element);
        if (hash->count < hash->lowWaterMark) {
            std::int32_t status = 0;
            Rehash(hash, &status);
        }
    }
    return result;
}

UHashTok Put(Hashtable* hash, UHashTok key, UHashTok value, std::int8_t hint, std::int32_t* status) {
    Status(status);
    Table(hash);
    if (!Failure(status)) {
        if ((hint & HintValuePointer) && !value.pointer) return Remove(hash, key);
        if (hash->count > hash->highWaterMark) Rehash(hash, status);
        if (!Failure(status)) {
            const std::int32_t hashcode = HashKey(hash, key);
            HashElement* element = Find(hash, key, hashcode);
            if (EmptyOrDeleted(element->hashcode)) {
                ++hash->count;
                if (hash->count == hash->length) {
                    --hash->count;
                    *status = MemoryAllocationError;
                }
            }
            if (!Failure(status)) return SetElement(hash, element, hashcode & 0x7FFFFFFF, key, value, hint);
        }
    }
    DeleteKeyValue(hash, key.pointer, value.pointer);
    return Empty();
}

UHashTok Lookup(const Hashtable* hash, UHashTok key) {
    const std::int32_t hashcode = HashKey(Table(hash), key);
    return Find(hash, key, hashcode)->value;
}

const HashElement* NextElement(const Hashtable* hash, std::int32_t* position) {
    Table(hash);
    if (!position) APS5_INVALID_ARG_EX;
    for (std::int32_t index = *position + 1; index < hash->length; ++index) {
        if (!EmptyOrDeleted(hash->elements[index].hashcode)) {
            *position = index;
            return &hash->elements[index];
        }
    }
    return nullptr;
}

void* RemoveElement(Hashtable* hash, const HashElement* element) {
    Table(hash);
    if (!element) APS5_INVALID_ARG_EX;
    if (!EmptyOrDeleted(element->hashcode)) return InternalRemoveElement(hash, const_cast<HashElement*>(element)).pointer;
    return nullptr;
}

} // namespace

extern "C" {

Icu::Hashtable* APS5_VABI uhash_open_nid_postfix(Icu::HashFunction keyHash, Icu::KeyComparator keyComp, Icu::KeyComparator valueComp, std::int32_t* status) {
    return Create(keyHash, keyComp, valueComp, DefaultPrimeIndex, Status(status));
}

Icu::Hashtable* APS5_VABI uhash_openSize_nid_postfix(Icu::HashFunction keyHash, Icu::KeyComparator keyComp, Icu::KeyComparator valueComp,
                                                     std::int32_t size, std::int32_t* status) {
    return Create(keyHash, keyComp, valueComp, PrimeIndexForSize(size), Status(status));
}

Icu::Hashtable* APS5_VABI uhash_init_nid_postfix(Icu::Hashtable* fillinResult, Icu::HashFunction keyHash, Icu::KeyComparator keyComp,
                                                 Icu::KeyComparator valueComp, std::int32_t* status) {
    return Init(fillinResult, keyHash, keyComp, valueComp, DefaultPrimeIndex, Status(status));
}

Icu::Hashtable* APS5_VABI uhash_initSize_nid_postfix(Icu::Hashtable* fillinResult, Icu::HashFunction keyHash, Icu::KeyComparator keyComp,
                                                     Icu::KeyComparator valueComp, std::int32_t size, std::int32_t* status) {
    return Init(fillinResult, keyHash, keyComp, valueComp, PrimeIndexForSize(size), Status(status));
}

void APS5_VABI uhash_close_nid_postfix(Icu::Hashtable* hash) {
    if (!hash) return;
    if (hash->elements) {
        if (hash->keyDeleter || hash->valueDeleter) {
            std::int32_t position = -1;
            const HashElement* element;
            while ((element = NextElement(hash, &position)) != nullptr) DeleteKeyValue(hash, element->key.pointer, element->value.pointer);
        }
        IcuReleaseMemory_nid_no_patch(hash->elements);
        hash->elements = nullptr;
    }
    if (hash->allocated) IcuReleaseMemory_nid_no_patch(hash);
}

Icu::HashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(Icu::Hashtable* hash, Icu::HashFunction function) {
    const Icu::HashFunction result = Table(hash)->keyHasher;
    hash->keyHasher = function;
    return result;
}

Icu::KeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(Icu::Hashtable* hash, Icu::KeyComparator function) {
    const Icu::KeyComparator result = Table(hash)->keyComparator;
    hash->keyComparator = function;
    return result;
}

Icu::KeyComparator APS5_VABI uhash_setValueComparator_nid_postfix(Icu::Hashtable* hash, Icu::KeyComparator function) {
    const Icu::KeyComparator result = Table(hash)->valueComparator;
    hash->valueComparator = function;
    return result;
}

Icu::ObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(Icu::Hashtable* hash, Icu::ObjectDeleter function) {
    const Icu::ObjectDeleter result = Table(hash)->keyDeleter;
    hash->keyDeleter = function;
    return result;
}

Icu::ObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(Icu::Hashtable* hash, Icu::ObjectDeleter function) {
    const Icu::ObjectDeleter result = Table(hash)->valueDeleter;
    hash->valueDeleter = function;
    return result;
}

void APS5_VABI uhash_setResizePolicy_nid_postfix(Icu::Hashtable* hash, std::int32_t policy) {
    std::int32_t status = 0;
    InternalSetResizePolicy(Table(hash), policy);
    hash->lowWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->lowWaterRatio);
    hash->highWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->highWaterRatio);
    Rehash(hash, &status);
}

std::int32_t APS5_VABI uhash_count_nid_postfix(const Icu::Hashtable* hash) {
    return Table(hash)->count;
}

void* APS5_VABI uhash_get_nid_postfix(const Icu::Hashtable* hash, const void* key) {
    return Lookup(hash, Pointer(key)).pointer;
}

void* APS5_VABI uhash_iget_nid_postfix(const Icu::Hashtable* hash, std::int32_t key) {
    return Lookup(hash, Integer(key)).pointer;
}

std::int32_t APS5_VABI uhash_geti_nid_postfix(const Icu::Hashtable* hash, const void* key) {
    return Lookup(hash, Pointer(key)).integer;
}

std::int32_t APS5_VABI uhash_igeti_nid_postfix(const Icu::Hashtable* hash, std::int32_t key) {
    return Lookup(hash, Integer(key)).integer;
}

void* APS5_VABI uhash_put_nid_postfix(Icu::Hashtable* hash, void* key, void* value, std::int32_t* status) {
    return Put(hash, Pointer(key), Pointer(value), HintKeyPointer | HintValuePointer, status).pointer;
}

void* APS5_VABI uhash_iput_nid_postfix(Icu::Hashtable* hash, std::int32_t key, void* value, std::int32_t* status) {
    return Put(hash, Integer(key), Pointer(value), HintValuePointer, status).pointer;
}

std::int32_t APS5_VABI uhash_puti_nid_postfix(Icu::Hashtable* hash, void* key, std::int32_t value, std::int32_t* status) {
    return Put(hash, Pointer(key), Integer(value), HintKeyPointer, status).integer;
}

std::int32_t APS5_VABI uhash_iputi_nid_postfix(Icu::Hashtable* hash, std::int32_t key, std::int32_t value, std::int32_t* status) {
    return Put(hash, Integer(key), Integer(value), 0, status).integer;
}

void* APS5_VABI uhash_remove_nid_postfix(Icu::Hashtable* hash, const void* key) {
    return Remove(Table(hash), Pointer(key)).pointer;
}

void* APS5_VABI uhash_iremove_nid_postfix(Icu::Hashtable* hash, std::int32_t key) {
    return Remove(Table(hash), Integer(key)).pointer;
}

std::int32_t APS5_VABI uhash_removei_nid_postfix(Icu::Hashtable* hash, const void* key) {
    return Remove(Table(hash), Pointer(key)).integer;
}

std::int32_t APS5_VABI uhash_iremovei_nid_postfix(Icu::Hashtable* hash, std::int32_t key) {
    return Remove(Table(hash), Integer(key)).integer;
}

void APS5_VABI uhash_removeAll_nid_postfix(Icu::Hashtable* hash) {
    std::int32_t position = -1;
    const HashElement* element;
    if (Table(hash)->count != 0)
        while ((element = NextElement(hash, &position)) != nullptr) RemoveElement(hash, element);
}

const Icu::HashElement* APS5_VABI uhash_find_nid_postfix(const Icu::Hashtable* hash, const void* key) {
    const UHashTok token = Pointer(key);
    const std::int32_t hashcode = HashKey(Table(hash), token);
    const HashElement* element = Find(hash, token, hashcode);
    return EmptyOrDeleted(element->hashcode) ? nullptr : element;
}

const Icu::HashElement* APS5_VABI uhash_nextElement_nid_postfix(const Icu::Hashtable* hash, std::int32_t* position) {
    return NextElement(hash, position);
}

void* APS5_VABI uhash_removeElement_nid_postfix(Icu::Hashtable* hash, const Icu::HashElement* element) {
    return RemoveElement(hash, element);
}

std::int8_t APS5_VABI uhash_equals_nid_postfix(const Icu::Hashtable* hash1, const Icu::Hashtable* hash2) {
    if (hash1 == hash2) return 1;
    if (!hash1 || !hash2 || hash1->keyComparator != hash2->keyComparator || hash1->valueComparator != hash2->valueComparator ||
        !hash1->valueComparator)
        return 0;
    const std::int32_t count = hash1->count;
    if (count != hash2->count) return 0;
    std::int32_t position = -1;
    for (std::int32_t index = 0; index < count; ++index) {
        const HashElement* element1 = NextElement(hash1, &position);
        if (!element1) APS5_INVALID_ARG_EX;
        const UHashTok key1 = element1->key;
        const UHashTok value1 = element1->value;
        const std::int32_t hashcode = HashKey(hash2, key1);
        const HashElement* element2 = Find(hash2, key1, hashcode);
        if (hash1->valueComparator(value1, element2->value) == 0) return 0;
    }
    return 1;
}

void APS5_VABI uhash_deleteHashtable_nid_postfix(void* object) {
    if (!object) return;
    auto* hash = *static_cast<Icu::Hashtable**>(object);
    if (hash) uhash_close_nid_postfix(hash);
    IcuReleaseMemory_nid_no_patch(object);
}

}

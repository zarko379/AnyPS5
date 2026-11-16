// Derived from icu4c/source/common/uvector.cpp, uvectr32.cpp, uvectr32.h and ustack.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uvector.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uvectr32.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ustack.cpp
//
// The upstream files' notices, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2013, International Business Machines Corporation and
//   others. All Rights Reserved.
//
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2015, International Business Machines Corporation and
//   others. All Rights Reserved.
//
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/Vectors.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/CoreClasses.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

using Icu::Vector;
using Icu::Vector32;

constexpr std::int32_t DefaultCapacity = 8;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t BufferOverflowError = 15;
constexpr std::int8_t HintKeyPointer = 1;

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

bool Failure(const std::int32_t* status) {
    return *status > 0;
}

void Release(void* memory) {
    if (memory) IcuReleaseMemory_nid_no_patch(memory);
}

bool EnsureCapacity(Vector* vector, std::int32_t minimumCapacity, std::int32_t* status) {
    if (minimumCapacity < 0) {
        *status = IllegalArgumentError;
        return false;
    }
    if (vector->capacity < minimumCapacity) {
        if (vector->capacity > (INT32_MAX - 1) / 2) {
            *status = IllegalArgumentError;
            return false;
        }
        std::int32_t newCapacity = vector->capacity * 2;
        if (newCapacity < minimumCapacity) newCapacity = minimumCapacity;
        if (newCapacity > static_cast<std::int32_t>(INT32_MAX / sizeof(UHashTok))) {
            *status = IllegalArgumentError;
            return false;
        }
        auto* elements = static_cast<UHashTok*>(uprv_realloc_nid_postfix(vector->elements, sizeof(UHashTok) * static_cast<std::size_t>(newCapacity)));
        if (!elements) {
            *status = MemoryAllocationError;
            return false;
        }
        vector->elements = elements;
        vector->capacity = newCapacity;
    }
    return true;
}

void DeleteElement(const Vector* vector, void* pointer) {
    if (pointer && vector->deleter) vector->deleter(pointer);
}

void* OrphanElementAt(Vector* vector, std::int32_t index) {
    void* element = nullptr;
    if (0 <= index && index < vector->count) {
        element = vector->elements[index].pointer;
        for (std::int32_t i = index; i < vector->count - 1; ++i) vector->elements[i] = vector->elements[i + 1];
        --vector->count;
    }
    return element;
}

void RemoveElementAt(Vector* vector, std::int32_t index) {
    DeleteElement(vector, OrphanElementAt(vector, index));
}

void RemoveAllElements(Vector* vector) {
    if (vector->deleter) {
        for (std::int32_t i = 0; i < vector->count; ++i) DeleteElement(vector, vector->elements[i].pointer);
    }
    vector->count = 0;
}

std::int32_t IndexOf(const Vector* vector, UHashTok key, std::int32_t startIndex, std::int8_t hint) {
    if (startIndex < 0) APS5_INVALID_ARG_EX;
    if (vector->comparer) {
        for (std::int32_t i = startIndex; i < vector->count; ++i) {
            if (vector->comparer(key, vector->elements[i])) return i;
        }
        return -1;
    }
    for (std::int32_t i = startIndex; i < vector->count; ++i) {
        if (hint & HintKeyPointer) {
            if (key.pointer == vector->elements[i].pointer) return i;
        } else if (key.integer == vector->elements[i].integer) {
            return i;
        }
    }
    return -1;
}

UHashTok PointerKey(void* pointer) {
    UHashTok key;
    key.pointer = pointer;
    return key;
}

UHashTok IntegerKey(std::int32_t integer) {
    UHashTok key;
    key.pointer = nullptr;
    key.integer = integer;
    return key;
}

void SetSize(Vector* vector, std::int32_t newSize, std::int32_t* status) {
    if (newSize < 0) return;
    if (newSize > vector->count) {
        if (!EnsureCapacity(vector, newSize, status)) return;
        for (std::int32_t i = vector->count; i < newSize; ++i) vector->elements[i] = IntegerKey(0);
    } else {
        for (std::int32_t i = vector->count - 1; i >= newSize; --i) RemoveElementAt(vector, i);
    }
    vector->count = newSize;
}

void InsertElementAt(Vector* vector, UHashTok element, std::int32_t index, std::int32_t* status) {
    if (0 <= index && index <= vector->count && EnsureCapacity(vector, vector->count + 1, status)) {
        for (std::int32_t i = vector->count; i > index; --i) vector->elements[i] = vector->elements[i - 1];
        vector->elements[index] = element;
        ++vector->count;
    }
}

void SetElementAt(Vector* vector, UHashTok element, std::int32_t index) {
    if (0 <= index && index < vector->count) {
        DeleteElement(vector, vector->elements[index].pointer);
        vector->elements[index] = element;
    }
}

void SortedInsert(Vector* vector, UHashTok element, Icu::ElementComparator compare, std::int32_t* status) {
    if (vector->count > 0) Object(compare);
    std::int32_t min = 0;
    std::int32_t max = vector->count;
    while (min != max) {
        const std::int32_t probe = (min + max) / 2;
        const std::int8_t order = compare(vector->elements[probe], element);
        if (order > 0) max = probe;
        else min = probe + 1;
    }
    if (EnsureCapacity(vector, vector->count + 1, status)) {
        for (std::int32_t i = vector->count; i > min; --i) vector->elements[i] = vector->elements[i - 1];
        vector->elements[min] = element;
        ++vector->count;
    }
}

void DestroyVector(Vector* vector) {
    RemoveAllElements(Object(vector));
    Release(vector->elements);
    vector->elements = nullptr;
}

std::int32_t APS5_VABI SortElements_nid_no_patch(const void* context, const void* left, const void* right) {
    const Icu::ElementComparator compare = *static_cast<const Icu::ElementComparator*>(context);
    return compare(*static_cast<const UHashTok*>(left), *static_cast<const UHashTok*>(right));
}

std::int32_t APS5_VABI SortIntegers_nid_no_patch(const void*, const void* left, const void* right) {
    const std::int32_t first = static_cast<const UHashTok*>(left)->integer;
    const std::int32_t second = static_cast<const UHashTok*>(right)->integer;
    return first < second ? -1 : first == second ? 0 : 1;
}

bool EnsureCapacity32(Vector32* vector, std::int32_t minimumCapacity, std::int32_t* status);

bool ExpandCapacity32(Vector32* vector, std::int32_t minimumCapacity, std::int32_t* status) {
    if (Failure(status)) return false;
    if (minimumCapacity < 0) {
        *status = IllegalArgumentError;
        return false;
    }
    if (vector->capacity >= minimumCapacity) return true;
    if (vector->maxCapacity > 0 && minimumCapacity > vector->maxCapacity) {
        *status = BufferOverflowError;
        return false;
    }
    if (vector->capacity > (INT32_MAX - 1) / 2) {
        *status = IllegalArgumentError;
        return false;
    }
    std::int32_t newCapacity = vector->capacity * 2;
    if (newCapacity < minimumCapacity) newCapacity = minimumCapacity;
    if (vector->maxCapacity > 0 && newCapacity > vector->maxCapacity) newCapacity = vector->maxCapacity;
    if (newCapacity > static_cast<std::int32_t>(INT32_MAX / sizeof(std::int32_t))) {
        *status = IllegalArgumentError;
        return false;
    }
    auto* elements = static_cast<std::int32_t*>(uprv_realloc_nid_postfix(vector->elements, sizeof(std::int32_t) * static_cast<std::size_t>(newCapacity)));
    if (!elements) {
        *status = MemoryAllocationError;
        return false;
    }
    vector->elements = elements;
    vector->capacity = newCapacity;
    return true;
}

bool EnsureCapacity32(Vector32* vector, std::int32_t minimumCapacity, std::int32_t* status) {
    if (minimumCapacity >= 0 && vector->capacity >= minimumCapacity) return true;
    return ExpandCapacity32(vector, minimumCapacity, status);
}

std::int32_t IndexOf32(const Vector32* vector, std::int32_t key, std::int32_t startIndex) {
    if (startIndex < 0) APS5_INVALID_ARG_EX;
    for (std::int32_t i = startIndex; i < vector->count; ++i) {
        if (key == vector->elements[i]) return i;
    }
    return -1;
}

void RemoveElementAt32(Vector32* vector, std::int32_t index) {
    if (index >= 0) {
        for (std::int32_t i = index; i < vector->count - 1; ++i) vector->elements[i] = vector->elements[i + 1];
        --vector->count;
    }
}

void SetSize32(Vector32* vector, std::int32_t newSize) {
    if (newSize < 0) return;
    if (newSize > vector->count) {
        std::int32_t status = 0;
        if (!EnsureCapacity32(vector, newSize, &status)) return;
        for (std::int32_t i = vector->count; i < newSize; ++i) vector->elements[i] = 0;
    }
    vector->count = newSize;
}

bool Equal32(const Vector32* vector, const Vector32* other) {
    if (vector->count != other->count) return false;
    for (std::int32_t i = 0; i < vector->count; ++i) {
        if (vector->elements[i] != other->elements[i]) return false;
    }
    return true;
}

void DestroyVector32(Vector32* vector) {
    Object(vector);
    Release(vector->elements);
    vector->elements = nullptr;
}

} // namespace

extern "C" {

void APS5_VABI _ZN6icu_6810UStringSetD0Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
    _ZN6icu_687UMemorydlEPv_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6810UStringSetD1Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
}

void APS5_VABI _ZN6icu_6810UStringSetD2Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
}

void* APS5_VABI _ZN6icu_686UStack3popEv_nid_postfix(Vector* self) {
    const std::int32_t last = Object(self)->count - 1;
    void* result = nullptr;
    if (last >= 0) {
        result = self->elements[last].pointer;
        RemoveElementAt(self, last);
    }
    return result;
}

std::int32_t APS5_VABI _ZN6icu_686UStack4popiEv_nid_postfix(Vector* self) {
    const std::int32_t last = Object(self)->count - 1;
    std::int32_t result = 0;
    if (last >= 0) {
        result = self->elements[last].integer;
        RemoveElementAt(self, last);
    }
    return result;
}

void APS5_VABI _ZN6icu_686UStackD0Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
    _ZN6icu_687UMemorydlEPv_nid_postfix(self);
}

void APS5_VABI _ZN6icu_686UStackD1Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
}

void APS5_VABI _ZN6icu_686UStackD2Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
}

std::int32_t APS5_VABI _ZNK6icu_686UStack6searchEPv_nid_postfix(const Vector* self, void* object) {
    const std::int32_t index = IndexOf(Object(self), PointerKey(object), 0, HintKeyPointer);
    return index >= 0 ? self->count - index : index;
}

void APS5_VABI _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(Vector* self, void* object, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (EnsureCapacity(self, self->count + 1, local)) self->elements[self->count++] = PointerKey(object);
    });
}

void APS5_VABI _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t element, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (EnsureCapacity(self, self->count + 1, local)) self->elements[self->count++] = IntegerKey(element);
    });
}

Icu::ElementDeleter APS5_VABI _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(Vector* self, Icu::ElementDeleter deleter) {
    const Icu::ElementDeleter old = Object(self)->deleter;
    self->deleter = deleter;
    return old;
}

Icu::ElementsAreEqual APS5_VABI _ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(Vector* self, Icu::ElementsAreEqual comparer) {
    const Icu::ElementsAreEqual old = Object(self)->comparer;
    self->comparer = comparer;
    return old;
}

void APS5_VABI _ZN6icu_687UVector12setElementAtEPvi_nid_postfix(Vector* self, void* object, std::int32_t index) {
    SetElementAt(Object(self), PointerKey(object), index);
}

void APS5_VABI _ZN6icu_687UVector12setElementAtEii_nid_postfix(Vector* self, std::int32_t element, std::int32_t index) {
    SetElementAt(Object(self), IntegerKey(element), index);
}

void APS5_VABI _ZN6icu_687UVector12sortedInsertE8UElementPFaS1_S1_ER10UErrorCode_nid_postfix(Vector* self, UHashTok element, Icu::ElementComparator compare,
                                                                                            std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        SortedInsert(Object(self), element, compare, local);
    });
}

void APS5_VABI _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(Vector* self, void* object, Icu::ElementComparator compare,
                                                                                           std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        SortedInsert(Object(self), PointerKey(object), compare, local);
    });
}

void APS5_VABI _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(Vector* self, std::int32_t element, Icu::ElementComparator compare,
                                                                                          std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        SortedInsert(Object(self), IntegerKey(element), compare, local);
    });
}

std::int8_t APS5_VABI _ZN6icu_687UVector13removeElementEPv_nid_postfix(Vector* self, void* object) {
    const std::int32_t index = IndexOf(Object(self), PointerKey(object), 0, HintKeyPointer);
    if (index < 0) return 0;
    RemoveElementAt(self, index);
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t minimumCapacity, std::int32_t* status) {
    return IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) -> std::int8_t {
        return EnsureCapacity(Object(self), minimumCapacity, local) ? 1 : 0;
    });
}

void APS5_VABI _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(Vector* self, void* object, std::int32_t index, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        InsertElementAt(Object(self), PointerKey(object), index, local);
    });
}

void APS5_VABI _ZN6icu_687UVector15insertElementAtEiiR10UErrorCode_nid_postfix(Vector* self, std::int32_t element, std::int32_t index, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        InsertElementAt(Object(self), IntegerKey(element), index, local);
    });
}

void* APS5_VABI _ZN6icu_687UVector15orphanElementAtEi_nid_postfix(Vector* self, std::int32_t index) {
    return OrphanElementAt(Object(self), index);
}

void APS5_VABI _ZN6icu_687UVector15removeElementAtEi_nid_postfix(Vector* self, std::int32_t index) {
    RemoveElementAt(Object(self), index);
}

void APS5_VABI _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(Vector* self) {
    RemoveAllElements(Object(self));
}

void APS5_VABI _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(Vector* self, GuestComparator compare, const void* context,
                                                                                                std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (Failure(local)) return;
        uprv_sortArray_nid_postfix(self->elements, self->count, sizeof(UHashTok), compare, context, 1, local);
    });
}

void APS5_VABI _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(Vector* self, Icu::ElementComparator compare, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (Failure(local)) return;
        if (self->count > 1) Object(compare);
        uprv_sortArray_nid_postfix(self->elements, self->count, sizeof(UHashTok), SortElements_nid_no_patch, &compare, 0, local);
    });
}

void APS5_VABI _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t initialCapacity, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (Failure(local)) return;
        if (initialCapacity < 1 || initialCapacity > static_cast<std::int32_t>(INT32_MAX / sizeof(UHashTok))) initialCapacity = DefaultCapacity;
        self->elements = static_cast<UHashTok*>(uprv_malloc_nid_postfix(sizeof(UHashTok) * static_cast<std::size_t>(initialCapacity)));
        if (!self->elements) *local = MemoryAllocationError;
        else self->capacity = initialCapacity;
    });
}

void APS5_VABI _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(Vector* self, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (Failure(local)) return;
        uprv_sortArray_nid_postfix(self->elements, self->count, sizeof(UHashTok), SortIntegers_nid_no_patch, nullptr, 0, local);
    });
}

void APS5_VABI _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(Vector* self, const Vector* other, Icu::ElementAssigner assign,
                                                                                        std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        Object(other);
        if (!EnsureCapacity(self, other->count, local)) return;
        SetSize(self, other->count, local);
        if (Failure(local)) return;
        if (other->count > 0) Object(assign);
        for (std::int32_t i = 0; i < other->count; ++i) {
            DeleteElement(self, self->elements[i].pointer);
            assign(&self->elements[i], &other->elements[i]);
        }
    });
}

void APS5_VABI _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t newSize, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        SetSize(Object(self), newSize, local);
    });
}

std::int8_t APS5_VABI _ZN6icu_687UVector9removeAllERKS0__nid_postfix(Vector* self, const Vector* other) {
    Object(self);
    Object(other);
    std::int8_t changed = 0;
    for (std::int32_t i = 0; i < other->count; ++i) {
        const std::int32_t index = IndexOf(self, other->elements[i], 0, 0);
        if (index >= 0) {
            RemoveElementAt(self, index);
            changed = 1;
        }
    }
    return changed;
}

std::int8_t APS5_VABI _ZN6icu_687UVector9retainAllERKS0__nid_postfix(Vector* self, const Vector* other) {
    Object(self);
    Object(other);
    std::int8_t changed = 0;
    for (std::int32_t j = self->count - 1; j >= 0; --j) {
        if (IndexOf(other, self->elements[j], 0, 0) < 0) {
            RemoveElementAt(self, j);
            changed = 1;
        }
    }
    return changed;
}

void APS5_VABI _ZN6icu_687UVectorD0Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
    _ZN6icu_687UMemorydlEPv_nid_postfix(self);
}

void APS5_VABI _ZN6icu_687UVectorD1Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
}

void APS5_VABI _ZN6icu_687UVectorD2Ev_nid_postfix(Vector* self) {
    DestroyVector(self);
}

std::int8_t APS5_VABI _ZN6icu_687UVectoreqERKS0__nid_postfix(Vector* self, const Vector* other) {
    Object(self);
    Object(other);
    if (self->count != other->count) return 0;
    if (self->comparer) {
        for (std::int32_t i = 0; i < self->count; ++i) {
            if (!self->comparer(self->elements[i], other->elements[i])) return 0;
        }
    }
    return 1;
}

std::int32_t APS5_VABI _ZNK6icu_687UVector10elementAtiEi_nid_postfix(const Vector* self, std::int32_t index) {
    Object(self);
    return 0 <= index && index < self->count ? self->elements[index].integer : 0;
}

std::int8_t APS5_VABI _ZNK6icu_687UVector11containsAllERKS0__nid_postfix(const Vector* self, const Vector* other) {
    Object(self);
    Object(other);
    for (std::int32_t i = 0; i < other->count; ++i) {
        if (IndexOf(self, other->elements[i], 0, 0) < 0) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_687UVector12containsNoneERKS0__nid_postfix(const Vector* self, const Vector* other) {
    Object(self);
    Object(other);
    for (std::int32_t i = 0; i < other->count; ++i) {
        if (IndexOf(self, other->elements[i], 0, 0) >= 0) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_687UVector6equalsERKS0__nid_postfix(const Vector* self, const Vector* other) {
    Object(self);
    Object(other);
    if (self->count != other->count) return 0;
    if (!self->comparer) {
        for (std::int32_t i = 0; i < self->count; ++i) {
            if (self->elements[i].pointer != other->elements[i].pointer) return 0;
        }
        return 1;
    }
    for (std::int32_t i = 0; i < self->count; ++i) {
        if (!self->comparer(PointerKey(&other->elements[i]), self->elements[i])) return 0;
    }
    return 1;
}

std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(const Vector* self, UHashTok key, std::int32_t startIndex, std::int8_t hint) {
    return IndexOf(Object(self), key, startIndex, hint);
}

std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEPvi_nid_postfix(const Vector* self, void* object, std::int32_t startIndex) {
    return IndexOf(Object(self), PointerKey(object), startIndex, HintKeyPointer);
}

std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEii_nid_postfix(const Vector* self, std::int32_t element, std::int32_t startIndex) {
    return IndexOf(Object(self), IntegerKey(element), startIndex, 0);
}

void** APS5_VABI _ZNK6icu_687UVector7toArrayEPPv_nid_postfix(const Vector* self, void** result) {
    Object(self);
    if (self->count > 0) Object(result);
    for (std::int32_t i = 0; i < self->count; ++i) result[i] = self->elements[i].pointer;
    return result;
}

void* APS5_VABI _ZNK6icu_687UVector9elementAtEi_nid_postfix(const Vector* self, std::int32_t index) {
    Object(self);
    return 0 <= index && index < self->count ? self->elements[index].pointer : nullptr;
}

void APS5_VABI _ZN6icu_689UVector3212setElementAtEii_nid_postfix(Vector32* self, std::int32_t element, std::int32_t index) {
    Object(self);
    if (0 <= index && index < self->count) self->elements[index] = element;
}

void APS5_VABI _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t element, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        std::int32_t min = 0;
        std::int32_t max = self->count;
        while (min != max) {
            const std::int32_t probe = (min + max) / 2;
            if (self->elements[probe] > element) max = probe;
            else min = probe + 1;
        }
        if (EnsureCapacity32(self, self->count + 1, local)) {
            for (std::int32_t i = self->count; i > min; --i) self->elements[i] = self->elements[i - 1];
            self->elements[min] = element;
            ++self->count;
        }
    });
}

std::int8_t APS5_VABI _ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t minimumCapacity, std::int32_t* status) {
    return IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) -> std::int8_t {
        return ExpandCapacity32(Object(self), minimumCapacity, local) ? 1 : 0;
    });
}

void APS5_VABI _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(Vector32* self, std::int32_t limit) {
    Object(self);
    if (limit < 0) limit = 0;
    if (limit > static_cast<std::int32_t>(INT32_MAX / sizeof(std::int32_t))) return;
    self->maxCapacity = limit;
    if (self->capacity <= self->maxCapacity || self->maxCapacity == 0) return;
    auto* elements = static_cast<std::int32_t*>(uprv_realloc_nid_postfix(self->elements, sizeof(std::int32_t) * static_cast<std::size_t>(self->maxCapacity)));
    if (!elements) return;
    self->elements = elements;
    self->capacity = self->maxCapacity;
    if (self->count > self->capacity) self->count = self->capacity;
}

void APS5_VABI _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t element, std::int32_t index, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (0 <= index && index <= self->count && EnsureCapacity32(self, self->count + 1, local)) {
            for (std::int32_t i = self->count; i > index; --i) self->elements[i] = self->elements[i - 1];
            self->elements[index] = element;
            ++self->count;
        }
    });
}

void APS5_VABI _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(Vector32* self, std::int32_t index) {
    RemoveElementAt32(Object(self), index);
}

void APS5_VABI _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(Vector32* self) {
    Object(self)->count = 0;
}

void APS5_VABI _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t initialCapacity, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        if (initialCapacity < 1) initialCapacity = DefaultCapacity;
        if (self->maxCapacity > 0 && self->maxCapacity < initialCapacity) initialCapacity = self->maxCapacity;
        if (initialCapacity > static_cast<std::int32_t>(INT32_MAX / sizeof(std::int32_t)))
            initialCapacity = DefaultCapacity < self->maxCapacity ? DefaultCapacity : self->maxCapacity;
        self->elements = static_cast<std::int32_t*>(uprv_malloc_nid_postfix(sizeof(std::int32_t) * static_cast<std::size_t>(initialCapacity)));
        if (!self->elements) *local = MemoryAllocationError;
        else self->capacity = initialCapacity;
    });
}

void APS5_VABI _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(Vector32* self, const Vector32* other, std::int32_t* status) {
    IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) {
        Object(self);
        Object(other);
        if (!EnsureCapacity32(self, other->count, local)) return;
        SetSize32(self, other->count);
        for (std::int32_t i = 0; i < other->count; ++i) self->elements[i] = other->elements[i];
    });
}

void APS5_VABI _ZN6icu_689UVector327setSizeEi_nid_postfix(Vector32* self, std::int32_t newSize) {
    SetSize32(Object(self), newSize);
}

std::int8_t APS5_VABI _ZN6icu_689UVector329removeAllERKS0__nid_postfix(Vector32* self, const Vector32* other) {
    Object(self);
    Object(other);
    std::int8_t changed = 0;
    for (std::int32_t i = 0; i < other->count; ++i) {
        const std::int32_t index = IndexOf32(self, other->elements[i], 0);
        if (index >= 0) {
            RemoveElementAt32(self, index);
            changed = 1;
        }
    }
    return changed;
}

std::int8_t APS5_VABI _ZN6icu_689UVector329retainAllERKS0__nid_postfix(Vector32* self, const Vector32* other) {
    Object(self);
    Object(other);
    std::int8_t changed = 0;
    for (std::int32_t j = self->count - 1; j >= 0; --j) {
        if (IndexOf32(other, self->elements[j], 0) < 0) {
            RemoveElementAt32(self, j);
            changed = 1;
        }
    }
    return changed;
}

void APS5_VABI _ZN6icu_689UVector32D0Ev_nid_postfix(Vector32* self) {
    DestroyVector32(self);
    _ZN6icu_687UMemorydlEPv_nid_postfix(self);
}

void APS5_VABI _ZN6icu_689UVector32D1Ev_nid_postfix(Vector32* self) {
    DestroyVector32(self);
}

void APS5_VABI _ZN6icu_689UVector32D2Ev_nid_postfix(Vector32* self) {
    DestroyVector32(self);
}

std::int8_t APS5_VABI _ZN6icu_689UVector32eqERKS0__nid_postfix(Vector32* self, const Vector32* other) {
    return Equal32(Object(self), Object(other)) ? 1 : 0;
}

std::int8_t APS5_VABI _ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(const Vector32* self, const Vector32* other) {
    Object(self);
    Object(other);
    for (std::int32_t i = 0; i < other->count; ++i) {
        if (IndexOf32(self, other->elements[i], 0) < 0) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(const Vector32* self, const Vector32* other) {
    Object(self);
    Object(other);
    for (std::int32_t i = 0; i < other->count; ++i) {
        if (IndexOf32(self, other->elements[i], 0) >= 0) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_689UVector326equalsERKS0__nid_postfix(const Vector32* self, const Vector32* other) {
    return Equal32(Object(self), Object(other)) ? 1 : 0;
}

std::int32_t APS5_VABI _ZNK6icu_689UVector327indexOfEii_nid_postfix(const Vector32* self, std::int32_t key, std::int32_t startIndex) {
    return IndexOf32(Object(self), key, startIndex);
}

}

// Derived from icu4c/source/common/charstr.cpp and cmemory.h@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/charstr.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2010-2015, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "CharString.hpp"
#include "Configuration.hpp"
#include "Invariant.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Icu::CharArray;
using Icu::CharString;

constexpr std::int32_t StackCapacity = 40;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t InternalProgramError = 5;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t BufferOverflowError = 15;
constexpr std::int32_t InvariantConversionError = 26;
constexpr std::int32_t StringNotTerminatedWarning = -124;
constexpr char FileSeparator = '/';

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

bool Failure(std::int32_t code) {
    return code > 0;
}

void Construct(CharArray* array) {
    array->ptr = array->stackArray;
    array->capacity = StackCapacity;
    array->needToRelease = 0;
}

void Release(CharArray* array) {
    if (array->needToRelease) IcuReleaseMemory_nid_no_patch(array->ptr);
}

void ResetToStack(CharArray* array) {
    array->ptr = array->stackArray;
    array->capacity = StackCapacity;
    array->needToRelease = 0;
}

void Copy(char* destination, const char* source, std::size_t count) {
    if (count == 0) return;
    if (!destination || !source) APS5_INVALID_ARG_EX;
    std::memmove(destination, source, count);
}

char* Resize(CharArray* array, std::int32_t newCapacity, std::int32_t length) {
    if (newCapacity <= 0) return nullptr;
    auto* p = static_cast<char*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(newCapacity)));
    if (p) {
        if (length > 0) {
            if (length > array->capacity) length = array->capacity;
            if (length > newCapacity) length = newCapacity;
            Copy(p, array->ptr, static_cast<std::size_t>(length));
        }
        Release(array);
        array->ptr = p;
        array->capacity = newCapacity;
        array->needToRelease = 1;
    }
    return p;
}

void MoveConstruct(CharArray* array, CharArray* source) {
    array->ptr = source->ptr;
    array->capacity = source->capacity;
    array->needToRelease = source->needToRelease;
    if (source->ptr == source->stackArray) {
        array->ptr = array->stackArray;
        Copy(array->stackArray, source->stackArray, static_cast<std::size_t>(source->capacity));
    } else {
        ResetToStack(source);
    }
}

CharArray* MoveAssign(CharArray* array, CharArray* source) {
    Release(array);
    array->capacity = source->capacity;
    array->needToRelease = source->needToRelease;
    if (source->ptr == source->stackArray) {
        array->ptr = array->stackArray;
        Copy(array->stackArray, source->stackArray, static_cast<std::size_t>(source->capacity));
    } else {
        array->ptr = source->ptr;
        ResetToStack(source);
    }
    return array;
}

void ConstructWithCapacity(CharArray* array, std::int32_t newCapacity, std::int32_t status) {
    Construct(array);
    if (Failure(status)) return;
    if (array->capacity < newCapacity) Resize(array, newCapacity, 0);
}

bool EnsureCapacity(CharString* string, std::int32_t capacity, std::int32_t desiredCapacityHint, std::int32_t* errorCode) {
    if (Failure(*errorCode)) return false;
    if (capacity > string->buffer.capacity) {
        if (desiredCapacityHint == 0) desiredCapacityHint = capacity + string->buffer.capacity;
        if ((desiredCapacityHint <= capacity || !Resize(&string->buffer, desiredCapacityHint, string->len + 1)) &&
            !Resize(&string->buffer, capacity, string->len + 1)) {
            *errorCode = MemoryAllocationError;
            return false;
        }
    }
    return true;
}

CharString* Append(CharString* string, const char* text, std::int32_t textLength, std::int32_t* errorCode);

CharString* AppendChar(CharString* string, char c, std::int32_t* errorCode) {
    if (EnsureCapacity(string, string->len + 2, 0, errorCode)) {
        string->buffer.ptr[string->len++] = c;
        string->buffer.ptr[string->len] = 0;
    }
    return string;
}

CharString* Append(CharString* string, const char* text, std::int32_t textLength, std::int32_t* errorCode) {
    if (Failure(*errorCode)) return string;
    if (textLength < -1 || (!text && textLength != 0)) {
        *errorCode = IllegalArgumentError;
        return string;
    }
    if (textLength < 0) textLength = static_cast<std::int32_t>(std::strlen(text));
    if (textLength > 0) {
        char* alias = string->buffer.ptr;
        if (text == alias + string->len) {
            if (textLength >= (string->buffer.capacity - string->len)) {
                *errorCode = InternalProgramError;
            } else {
                string->buffer.ptr[string->len += textLength] = 0;
            }
        } else if (alias <= text && text < (alias + string->len) && textLength >= (string->buffer.capacity - string->len)) {
            CharString copy;
            Construct(&copy.buffer);
            copy.len = 0;
            copy.buffer.ptr[0] = 0;
            Append(&copy, text, textLength, errorCode);
            Append(string, copy.buffer.ptr, copy.len, errorCode);
            Release(&copy.buffer);
            return string;
        } else if (EnsureCapacity(string, string->len + textLength + 1, 0, errorCode)) {
            Copy(string->buffer.ptr + string->len, text, static_cast<std::size_t>(textLength));
            string->buffer.ptr[string->len += textLength] = 0;
        }
    }
    return string;
}

char DirSepChar(const CharString*) {
    return FileSeparator;
}

bool EndsWithSeparator(const CharString* string) {
    const char c = string->buffer.ptr[string->len - 1];
    return c == FileSeparator;
}

} // namespace

extern "C" {

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(Icu::CharArray* self) {
    Construct(Object(self));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ev_nid_postfix(Icu::CharArray* self) {
    Construct(Object(self));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ei10UErrorCode_nid_postfix(Icu::CharArray* self, std::int32_t newCapacity, std::int32_t status) {
    ConstructWithCapacity(Object(self), newCapacity, status);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ei10UErrorCode_nid_postfix(Icu::CharArray* self, std::int32_t newCapacity, std::int32_t status) {
    ConstructWithCapacity(Object(self), newCapacity, status);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(Icu::CharArray* self, Icu::CharArray* source) {
    MoveConstruct(Object(self), Object(source));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2EOS1__nid_postfix(Icu::CharArray* self, Icu::CharArray* source) {
    MoveConstruct(Object(self), Object(source));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(Icu::CharArray* self) {
    Release(Object(self));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED2Ev_nid_postfix(Icu::CharArray* self) {
    Release(Object(self));
}

Icu::CharArray* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(Icu::CharArray* self, Icu::CharArray* source) {
    return MoveAssign(Object(self), Object(source));
}

std::int32_t APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE11getCapacityEv_nid_postfix(const Icu::CharArray* self) {
    return Object(self)->capacity;
}

char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(const Icu::CharArray* self) {
    return Object(self)->ptr;
}

char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(const Icu::CharArray* self) {
    return Object(self)->ptr + self->capacity;
}

const char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(const Icu::CharArray* self, std::ptrdiff_t index) {
    return Object(self)->ptr + index;
}

char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(Icu::CharArray* self, std::ptrdiff_t index) {
    return Object(self)->ptr + index;
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(Icu::CharArray* self, char* otherArray, std::int32_t otherCapacity) {
    Object(self);
    if (otherArray && otherCapacity > 0) {
        Release(self);
        self->ptr = otherArray;
        self->capacity = otherCapacity;
        self->needToRelease = 0;
    }
}

char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(Icu::CharArray* self, std::int32_t newCapacity, std::int32_t length) {
    return Resize(Object(self), newCapacity, length);
}

char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(Icu::CharArray* self, std::int32_t length, std::int32_t* resultCapacity) {
    Object(self);
    char* p;
    if (self->needToRelease) {
        Object(resultCapacity);
        p = self->ptr;
    } else if (length <= 0) {
        return nullptr;
    } else {
        Object(resultCapacity);
        if (length > self->capacity) length = self->capacity;
        p = static_cast<char*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(length)));
        if (!p) return nullptr;
        Copy(p, self->ptr, static_cast<std::size_t>(length));
    }
    *resultCapacity = length;
    ResetToStack(self);
    return p;
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(Icu::CharArray* self, const Icu::CharArray* source,
                                                                                         std::int32_t* status) {
    Object(self);
    Object(source);
    if (Failure(*Object(status))) return;
    if (!Resize(self, source->capacity, 0)) {
        *status = MemoryAllocationError;
        return;
    }
    Copy(self->ptr, source->ptr, static_cast<std::size_t>(self->capacity));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12releaseArrayEv_nid_postfix(Icu::CharArray* self) {
    Release(Object(self));
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE17resetToStackArrayEv_nid_postfix(Icu::CharArray* self) {
    ResetToStack(Object(self));
}

void APS5_VABI _ZN6icu_6810CharStringC1EOS0__nid_postfix(Icu::CharString* self, Icu::CharString* source) {
    MoveConstruct(&Object(self)->buffer, &Object(source)->buffer);
    self->len = source->len;
    source->len = 0;
}

void APS5_VABI _ZN6icu_6810CharStringC2EOS0__nid_postfix(Icu::CharString* self, Icu::CharString* source) {
    _ZN6icu_6810CharStringC1EOS0__nid_postfix(self, source);
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharStringaSEOS0__nid_postfix(Icu::CharString* self, Icu::CharString* source) {
    MoveAssign(&Object(self)->buffer, &Object(source)->buffer);
    self->len = source->len;
    source->len = 0;
    return self;
}

char* APS5_VABI _ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(const Icu::CharString* self, std::int32_t* errorCode) {
    Object(self);
    if (Failure(*Object(errorCode))) return nullptr;
    auto* p = static_cast<char*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(self->len) + 1));
    if (!p) {
        *errorCode = MemoryAllocationError;
        return nullptr;
    }
    Copy(p, self->buffer.ptr, static_cast<std::size_t>(self->len) + 1);
    return p;
}

std::int32_t APS5_VABI _ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(const Icu::CharString* self, char* dest, std::int32_t capacity,
                                                                                   std::int32_t* errorCode) {
    Object(self);
    if (Failure(*Object(errorCode))) return self->len;
    if (capacity < 0 || (capacity > 0 && !dest)) {
        *errorCode = IllegalArgumentError;
        return self->len;
    }
    const char* source = self->buffer.ptr;
    if (0 < self->len && self->len <= capacity && source != dest) Copy(dest, source, static_cast<std::size_t>(self->len));
    if (self->len < capacity) {
        dest[self->len] = 0;
        if (*errorCode == StringNotTerminatedWarning) *errorCode = 0;
    } else if (self->len == capacity) {
        *errorCode = StringNotTerminatedWarning;
    } else {
        *errorCode = BufferOverflowError;
    }
    return self->len;
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString8copyFromERKS0_R10UErrorCode_nid_postfix(Icu::CharString* self, const Icu::CharString* source,
                                                                                        std::int32_t* errorCode) {
    Object(self);
    Object(source);
    if (!Failure(*Object(errorCode)) && self != source && EnsureCapacity(self, source->len + 1, 0, errorCode)) {
        self->len = source->len;
        Copy(self->buffer.ptr, source->buffer.ptr, static_cast<std::size_t>(self->len) + 1);
    }
    return self;
}

std::int32_t APS5_VABI _ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(const Icu::CharString* self, char c) {
    for (std::int32_t i = Object(self)->len; i > 0;)
        if (self->buffer.ptr[--i] == c) return i;
    return -1;
}

std::int8_t APS5_VABI _ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(const Icu::CharString* self, Icu::StringPiece piece) {
    Object(self);
    if (piece.length == 0) return 0;
    const char* p = self->buffer.ptr;
    const std::int32_t lastStart = self->len - piece.length;
    for (std::int32_t i = 0; i <= lastStart; ++i) {
        if (!piece.data) APS5_INVALID_ARG_EX;
        if (std::memcmp(p + i, piece.data, static_cast<std::size_t>(piece.length)) == 0) return 1;
    }
    return 0;
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString8truncateEi_nid_postfix(Icu::CharString* self, std::int32_t newLength) {
    Object(self);
    if (newLength < 0) newLength = 0;
    if (newLength < self->len) self->buffer.ptr[self->len = newLength] = 0;
    return self;
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(Icu::CharString* self, char c, std::int32_t* errorCode) {
    Object(self);
    return AppendChar(self, c, Object(errorCode));
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(Icu::CharString* self, const char* text, std::int32_t textLength,
                                                                                      std::int32_t* errorCode) {
    Object(self);
    return Append(self, text, textLength, Object(errorCode));
}

char* APS5_VABI _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(Icu::CharString* self, std::int32_t minCapacity,
                                                                                    std::int32_t desiredCapacityHint, std::int32_t* resultCapacity,
                                                                                    std::int32_t* errorCode) {
    Object(self);
    Object(resultCapacity);
    if (Failure(*Object(errorCode))) {
        *resultCapacity = 0;
        return nullptr;
    }
    const std::int32_t appendCapacity = self->buffer.capacity - self->len - 1;
    if (appendCapacity >= minCapacity) {
        *resultCapacity = appendCapacity;
        return self->buffer.ptr + self->len;
    }
    if (EnsureCapacity(self, self->len + minCapacity + 1, self->len + desiredCapacityHint + 1, errorCode)) {
        *resultCapacity = self->buffer.capacity - self->len - 1;
        return self->buffer.ptr + self->len;
    }
    *resultCapacity = 0;
    return nullptr;
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(Icu::CharString* self, const char16_t* uchars,
                                                                                                     std::int32_t ucharsLength, std::int32_t* errorCode) {
    Object(self);
    if (Failure(*Object(errorCode))) return self;
    if (!uprv_isInvariantUString_nid_postfix(uchars, ucharsLength)) {
        *errorCode = InvariantConversionError;
        return self;
    }
    if (EnsureCapacity(self, self->len + ucharsLength + 1, 0, errorCode)) {
        u_UCharsToChars_nid_postfix(uchars, self->buffer.ptr + self->len, ucharsLength);
        self->len += ucharsLength;
        self->buffer.ptr[self->len] = 0;
    }
    return self;
}

std::int8_t APS5_VABI _ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(Icu::CharString* self, std::int32_t capacity,
                                                                                       std::int32_t desiredCapacityHint, std::int32_t* errorCode) {
    Object(self);
    return EnsureCapacity(self, capacity, desiredCapacityHint, Object(errorCode));
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(Icu::CharString* self, Icu::StringPiece piece,
                                                                                                           std::int32_t* errorCode) {
    Object(self);
    if (Failure(*Object(errorCode))) return self;
    if (piece.length == 0) return self;
    if (self->len > 0 && !EndsWithSeparator(self)) AppendChar(self, DirSepChar(self), errorCode);
    return Append(self, piece.data, piece.length, errorCode);
}

Icu::CharString* APS5_VABI _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(Icu::CharString* self, std::int32_t* errorCode) {
    Object(self);
    if (!Failure(*Object(errorCode)) && self->len > 0 && !EndsWithSeparator(self)) AppendChar(self, DirSepChar(self), errorCode);
    return self;
}

char APS5_VABI _ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(const Icu::CharString* self) {
    return DirSepChar(Object(self));
}

}

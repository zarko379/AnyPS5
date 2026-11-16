#include "prx/libSceIcu/BaseClasses.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libSceIcu/ClassIds.hpp"
#include "prx/libSceIcu/CoreClasses.hpp"
#include "prx/libc/include/General.hpp"
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t ActualLocale = 0;
constexpr std::int32_t ValidLocale = 1;
constexpr std::int32_t FullNameCapacity = 157;
constexpr std::int32_t TrieTypeBytes = 0;
constexpr std::int32_t TrieTypeUChars = 1;
constexpr std::int32_t TransformTypeOffset = 0x1000000;
constexpr std::int32_t TransformTypeMask = 0x7f000000;
constexpr std::int32_t TransformOffsetMask = 0x1fffff;
constexpr std::int32_t Sentinel = -1;
constexpr std::ptrdiff_t MatcherOffset = 8;

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

void Keep(void* self) {
    Object(self);
}

void Delete(void* self) {
    _ZN6icu_687UMemorydlEPv_nid_postfix(Object(self));
}

template<typename TChar>
TChar* ScratchBuffer(std::int32_t minCapacity, TChar* scratch, std::int32_t scratchCapacity, std::int32_t* resultCapacity) {
    Object(resultCapacity);
    if (minCapacity < 1 || scratchCapacity < minCapacity) {
        *resultCapacity = 0;
        return nullptr;
    }
    *resultCapacity = scratchCapacity;
    return scratch;
}

void CopyLocaleId(char* destination, const char* source) {
    if (!source) return;
    Object(destination);
    std::strncpy(destination, source, FullNameCapacity);
    destination[FullNameCapacity - 1] = 0;
}

const char* LocaleName(const Icu::LocaleFields* locale) {
    return Object(Object(locale)->fullName);
}

void Iterator(Icu::CharacterIterator* self, std::int32_t length, std::int32_t position, std::int32_t begin, std::int32_t end) {
    Object(self);
    self->textLength = length;
    self->pos = position;
    self->begin = begin;
    self->end = end;
}

} // namespace

extern "C" {

char16_t* APS5_VABI _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(void* self, std::int32_t minCapacity, std::int32_t, char16_t* scratch,
                                                                              std::int32_t scratchCapacity, std::int32_t* resultCapacity) {
    Object(self);
    return ScratchBuffer(minCapacity, scratch, scratchCapacity, resultCapacity);
}

std::int8_t APS5_VABI _ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix(void* self, std::int32_t) {
    Object(self);
    return 1;
}

void APS5_VABI _ZN6icu_6810AppendableD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6810AppendableD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6810AppendableD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(Icu::LocaleBased* self, const char* validId, const char* actualId) {
    Object(self);
    CopyLocaleId(self->valid, validId);
    CopyLocaleId(self->actual, actualId);
}

void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(Icu::LocaleBased* self, const Icu::LocaleFields* validId,
                                                                                const Icu::LocaleFields* actualId) {
    Object(self);
    const char* validName = LocaleName(validId);
    std::strcpy(Object(self->valid), validName);
    const char* actualName = LocaleName(actualId);
    std::strcpy(Object(self->actual), actualName);
}

const char* APS5_VABI _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(const Icu::LocaleBased* self, std::int32_t type,
                                                                                                         std::int32_t* status) {
    return IcuBackend::WithGuestStatus(status, __func__, [&](std::int32_t* local) -> const char* {
        if (*local > 0) return nullptr;
        Object(self);
        if (type == ValidLocale) return self->valid;
        if (type == ActualLocale) return self->actual;
        *local = IllegalArgumentError;
        return nullptr;
    });
}

void APS5_VABI _ZN6icu_6811ReplaceableD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6811ReplaceableD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6811ReplaceableD2Ev_nid_postfix(void* self) {
    Keep(self);
}

std::int8_t APS5_VABI _ZNK6icu_6811Replaceable11hasMetaDataEv_nid_postfix(const void* self) {
    Object(self);
    return 1;
}

void* APS5_VABI _ZNK6icu_6811Replaceable5cloneEv_nid_postfix(const void* self) {
    Object(self);
    return nullptr;
}

void APS5_VABI _ZN6icu_6811SymbolTableD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6811SymbolTableD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6812ResourceSinkD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6812ResourceSinkD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6812ResourceSinkD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813EventListenerD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6813EventListenerD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813EventListenerD2Ev_nid_postfix(void* self) {
    Keep(self);
}

const void* APS5_VABI _ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix();
}

void APS5_VABI _ZN6icu_6813ParsePositionD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6813ParsePositionD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813ParsePositionD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813ResourceValueD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6813ResourceValueD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813ResourceValueD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix(void* self, const void*) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813UnicodeFilterD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6813UnicodeFilterD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD0Ev_nid_postfix(void* self) {
    Delete(static_cast<char*>(Object(self)) - MatcherOffset);
}

void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void* APS5_VABI _ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(const void* self) {
    return const_cast<char*>(static_cast<const char*>(Object(self))) + MatcherOffset;
}

void APS5_VABI _ZN6icu_6814UnicodeFunctorD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6814UnicodeFunctorD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void* APS5_VABI _ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix(const void* self) {
    Object(self);
    return nullptr;
}

void* APS5_VABI _ZNK6icu_6814UnicodeFunctor9toMatcherEv_nid_postfix(const void* self) {
    Object(self);
    return nullptr;
}

void APS5_VABI _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6815ServiceListenerD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6815ServiceListenerD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6815ServiceListenerD2Ev_nid_postfix(void* self) {
    Keep(self);
}

const void* APS5_VABI _ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix();
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2ERKS0__nid_postfix(Icu::CharacterIterator* self, const Icu::CharacterIterator* that) {
    const Icu::CharacterIterator source = *Object(that);
    Iterator(self, source.textLength, source.pos, source.begin, source.end);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Ei_nid_postfix(Icu::CharacterIterator* self, std::int32_t length) {
    if (length < 0) length = 0;
    Iterator(self, length, 0, 0, length);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Eii_nid_postfix(Icu::CharacterIterator* self, std::int32_t length, std::int32_t position) {
    if (length < 0) length = 0;
    if (position < 0) position = 0;
    else if (position > length) position = length;
    Iterator(self, length, position, 0, length);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Eiiii_nid_postfix(Icu::CharacterIterator* self, std::int32_t length, std::int32_t textBegin, std::int32_t textEnd,
                                                               std::int32_t position) {
    if (length < 0) length = 0;
    if (textBegin < 0) textBegin = 0;
    else if (textBegin > length) textBegin = length;
    if (textEnd < textBegin) textEnd = textBegin;
    else if (textEnd > length) textEnd = length;
    if (position < textBegin) position = textBegin;
    else if (position > textEnd) position = textEnd;
    Iterator(self, length, position, textBegin, textEnd);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Ev_nid_postfix(Icu::CharacterIterator* self) {
    Iterator(self, 0, 0, 0, 0);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorD2Ev_nid_postfix(void* self) {
    Keep(self);
}

Icu::CharacterIterator* APS5_VABI _ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(Icu::CharacterIterator* self, const Icu::CharacterIterator* that) {
    const Icu::CharacterIterator source = *Object(that);
    Iterator(self, source.textLength, source.pos, source.begin, source.end);
    return self;
}

void APS5_VABI _ZN6icu_6817DictionaryMatcherD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6817DictionaryMatcherD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6817DictionaryMatcherD2Ev_nid_postfix(void* self) {
    Keep(self);
}

char* APS5_VABI _ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::CharStringByteSink* self, std::int32_t minCapacity,
                                                                                std::int32_t desiredCapacityHint, char* scratch,
                                                                                std::int32_t scratchCapacity, std::int32_t* resultCapacity) {
    Object(self);
    Object(resultCapacity);
    if (minCapacity < 1 || scratchCapacity < minCapacity) {
        *resultCapacity = 0;
        return nullptr;
    }
    std::int32_t status = 0;
    char* result = _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(Object(self->dest), minCapacity, desiredCapacityHint, resultCapacity,
                                                                                       &status);
    if (status <= 0) return result;
    *resultCapacity = scratchCapacity;
    return scratch;
}

void APS5_VABI _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(Icu::CharStringByteSink* self, const char* bytes, std::int32_t n) {
    std::int32_t status = 0;
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(Object(Object(self)->dest), bytes, n, &status);
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkD2Ev_nid_postfix(void* self) {
    Keep(self);
}

char* APS5_VABI _ZN6icu_6820CheckedArrayByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::CheckedArrayByteSink* self, std::int32_t minCapacity, std::int32_t,
                                                                                  char* scratch, std::int32_t scratchCapacity,
                                                                                  std::int32_t* resultCapacity) {
    Object(self);
    Object(resultCapacity);
    if (minCapacity < 1 || scratchCapacity < minCapacity) {
        *resultCapacity = 0;
        return nullptr;
    }
    const std::int32_t available = self->capacity - self->size;
    if (available >= minCapacity) {
        *resultCapacity = available;
        return self->outbuf + self->size;
    }
    *resultCapacity = scratchCapacity;
    return scratch;
}

Icu::CheckedArrayByteSink* APS5_VABI _ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix(Icu::CheckedArrayByteSink* self) {
    Object(self);
    self->size = 0;
    self->appended = 0;
    self->overflowed = 0;
    return self;
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(Icu::CheckedArrayByteSink* self, const char* bytes, std::int32_t n) {
    Object(self);
    if (n <= 0) return;
    if (n > INT32_MAX - self->appended) {
        self->appended = INT32_MAX;
        self->overflowed = 1;
        return;
    }
    const std::int32_t available = self->capacity - self->size;
    const std::int32_t copied = n > available ? available : n;
    const bool copies = copied > 0 && bytes != self->outbuf + self->size;
    if (copies) {
        Object(bytes);
        Object(self->outbuf);
    }
    self->appended += n;
    if (n > available) self->overflowed = 1;
    if (copies) std::memmove(self->outbuf + self->size, bytes, static_cast<std::size_t>(copied));
    self->size += copied;
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorC2ERKS0__nid_postfix(void* self, const void* other) {
    Object(other);
    Keep(self);
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorC2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderC2Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderD2Ev_nid_postfix(void* self) {
    Keep(self);
}

char* APS5_VABI _ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix(void* self, std::int32_t minCapacity, std::int32_t, char* scratch,
                                                                       std::int32_t scratchCapacity, std::int32_t* resultCapacity) {
    Object(self);
    return ScratchBuffer(minCapacity, scratch, scratchCapacity, resultCapacity);
}

void APS5_VABI _ZN6icu_688ByteSink5FlushEv_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_688ByteSinkD0Ev_nid_postfix(void* self) {
    Delete(self);
}

void APS5_VABI _ZN6icu_688ByteSinkD1Ev_nid_postfix(void* self) {
    Keep(self);
}

void APS5_VABI _ZN6icu_688ByteSinkD2Ev_nid_postfix(void* self) {
    Keep(self);
}

std::int32_t APS5_VABI _ZNK6icu_6822BytesDictionaryMatcher7getTypeEv_nid_postfix(const void* self) {
    Object(self);
    return TrieTypeBytes;
}

std::int32_t APS5_VABI _ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(const Icu::BytesDictionaryMatcher* self, std::int32_t c) {
    const std::int32_t transformConstant = Object(self)->transformConstant;
    if ((transformConstant & TransformTypeMask) != TransformTypeOffset) return c;
    if (c == 0x200D) return 0xFF;
    if (c == 0x200C) return 0xFE;
    const std::int32_t delta = c - (transformConstant & TransformOffsetMask);
    if (delta < 0 || 0xFD < delta) return Sentinel;
    return delta;
}

std::int32_t APS5_VABI _ZNK6icu_6823UCharsDictionaryMatcher7getTypeEv_nid_postfix(const void* self) {
    Object(self);
    return TrieTypeUChars;
}

const void* APS5_VABI _ZNK6icu_6813SimpleFactory17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_6813SimpleFactory16getStaticClassIDEv_nid_postfix();
}

const void* APS5_VABI _ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix();
}

const void* APS5_VABI _ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix();
}

const void* APS5_VABI _ZNK6icu_6818UStringEnumeration17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_6818UStringEnumeration16getStaticClassIDEv_nid_postfix();
}

const void* APS5_VABI _ZNK6icu_6818UnicodeSetIterator17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_6818UnicodeSetIterator16getStaticClassIDEv_nid_postfix();
}

const void* APS5_VABI _ZNK6icu_6824ICUResourceBundleFactory17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return _ZN6icu_6824ICUResourceBundleFactory16getStaticClassIDEv_nid_postfix();
}

}

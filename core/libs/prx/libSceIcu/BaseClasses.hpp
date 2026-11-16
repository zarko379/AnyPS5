#pragma once
#include "CharString.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct CheckedArrayByteSink {
    const void* vtable;
    char* outbuf;
    std::int32_t capacity;
    std::int32_t size;
    std::int32_t appended;
    std::int8_t overflowed;
};

struct CharStringByteSink {
    const void* vtable;
    CharString* dest;
};

struct CharacterIterator {
    const void* vtable;
    std::int32_t textLength;
    std::int32_t pos;
    std::int32_t begin;
    std::int32_t end;
};

struct LocaleBased {
    char* valid;
    char* actual;
};

struct LocaleFields {
    const void* vtable;
    char language[12];
    char script[6];
    char country[4];
    std::int32_t variantBegin;
    char* fullName;
    char fullNameBuffer[157];
    char* baseName;
    std::int8_t isBogus;
};

struct BytesDictionaryMatcher {
    const void* vtable;
    const char* characters;
    std::int32_t transformConstant;
    void* file;
};

static_assert(offsetof(CheckedArrayByteSink, outbuf) == 8 && offsetof(CheckedArrayByteSink, capacity) == 16 && offsetof(CheckedArrayByteSink, size) == 20);
static_assert(offsetof(CheckedArrayByteSink, appended) == 24 && offsetof(CheckedArrayByteSink, overflowed) == 28 && sizeof(CheckedArrayByteSink) == 32);
static_assert(offsetof(CharStringByteSink, dest) == 8 && sizeof(CharStringByteSink) == 16);
static_assert(offsetof(CharacterIterator, textLength) == 8 && offsetof(CharacterIterator, end) == 20 && sizeof(CharacterIterator) == 24);
static_assert(sizeof(LocaleBased) == 16);
static_assert(offsetof(LocaleFields, variantBegin) == 32 && offsetof(LocaleFields, fullName) == 40 && offsetof(LocaleFields, fullNameBuffer) == 48);
static_assert(offsetof(LocaleFields, baseName) == 208 && offsetof(LocaleFields, isBogus) == 216 && sizeof(LocaleFields) == 224);
static_assert(offsetof(BytesDictionaryMatcher, transformConstant) == 16 && offsetof(BytesDictionaryMatcher, file) == 24 && sizeof(BytesDictionaryMatcher) == 32);

} // namespace Icu

extern "C" {
char16_t* APS5_VABI _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(void*, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix(void*, std::int32_t);
void APS5_VABI _ZN6icu_6810AppendableD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6810AppendableD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6810AppendableD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(Icu::LocaleBased*, const char*, const char*);
void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(Icu::LocaleBased*, const Icu::LocaleFields*, const Icu::LocaleFields*);
const char* APS5_VABI _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(const Icu::LocaleBased*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_6811ReplaceableD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6811ReplaceableD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6811ReplaceableD2Ev_nid_postfix(void*);
std::int8_t APS5_VABI _ZNK6icu_6811Replaceable11hasMetaDataEv_nid_postfix(const void*);
void* APS5_VABI _ZNK6icu_6811Replaceable5cloneEv_nid_postfix(const void*);
void APS5_VABI _ZN6icu_6811SymbolTableD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6811SymbolTableD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6812ResourceSinkD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6812ResourceSinkD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6812ResourceSinkD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813EventListenerD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813EventListenerD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813EventListenerD2Ev_nid_postfix(void*);
const void* APS5_VABI _ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix(const void*);
void APS5_VABI _ZN6icu_6813ParsePositionD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813ParsePositionD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813ParsePositionD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813ResourceValueD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813ResourceValueD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813ResourceValueD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix(void*, const void*);
void APS5_VABI _ZN6icu_6813UnicodeFilterD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6813UnicodeFilterD2Ev_nid_postfix(void*);
void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD0Ev_nid_postfix(void*);
void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD1Ev_nid_postfix(void*);
void* APS5_VABI _ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(const void*);
void APS5_VABI _ZN6icu_6814UnicodeFunctorD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6814UnicodeFunctorD2Ev_nid_postfix(void*);
void* APS5_VABI _ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix(const void*);
void* APS5_VABI _ZNK6icu_6814UnicodeFunctor9toMatcherEv_nid_postfix(const void*);
void APS5_VABI _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6815ServiceListenerD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6815ServiceListenerD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6815ServiceListenerD2Ev_nid_postfix(void*);
const void* APS5_VABI _ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix(const void*);
void APS5_VABI _ZN6icu_6817CharacterIteratorC2ERKS0__nid_postfix(Icu::CharacterIterator*, const Icu::CharacterIterator*);
void APS5_VABI _ZN6icu_6817CharacterIteratorC2Ei_nid_postfix(Icu::CharacterIterator*, std::int32_t);
void APS5_VABI _ZN6icu_6817CharacterIteratorC2Eii_nid_postfix(Icu::CharacterIterator*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6817CharacterIteratorC2Eiiii_nid_postfix(Icu::CharacterIterator*, std::int32_t, std::int32_t, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6817CharacterIteratorC2Ev_nid_postfix(Icu::CharacterIterator*);
void APS5_VABI _ZN6icu_6817CharacterIteratorD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6817CharacterIteratorD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6817CharacterIteratorD2Ev_nid_postfix(void*);
Icu::CharacterIterator* APS5_VABI _ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(Icu::CharacterIterator*, const Icu::CharacterIterator*);
void APS5_VABI _ZN6icu_6817DictionaryMatcherD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6817DictionaryMatcherD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6817DictionaryMatcherD2Ev_nid_postfix(void*);
char* APS5_VABI _ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::CharStringByteSink*, std::int32_t, std::int32_t, char*, std::int32_t,
                                                                                std::int32_t*);
void APS5_VABI _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(Icu::CharStringByteSink*, const char*, std::int32_t);
void APS5_VABI _ZN6icu_6818CharStringByteSinkD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6818CharStringByteSinkD2Ev_nid_postfix(void*);
char* APS5_VABI _ZN6icu_6820CheckedArrayByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::CheckedArrayByteSink*, std::int32_t, std::int32_t, char*,
                                                                                  std::int32_t, std::int32_t*);
Icu::CheckedArrayByteSink* APS5_VABI _ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix(Icu::CheckedArrayByteSink*);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(Icu::CheckedArrayByteSink*, const char*, std::int32_t);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorC2ERKS0__nid_postfix(void*, const void*);
void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorC2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderC2Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_6828FilteredBreakIteratorBuilderD2Ev_nid_postfix(void*);
char* APS5_VABI _ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix(void*, std::int32_t, std::int32_t, char*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_688ByteSink5FlushEv_nid_postfix(void*);
void APS5_VABI _ZN6icu_688ByteSinkD0Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_688ByteSinkD1Ev_nid_postfix(void*);
void APS5_VABI _ZN6icu_688ByteSinkD2Ev_nid_postfix(void*);
std::int32_t APS5_VABI _ZNK6icu_6822BytesDictionaryMatcher7getTypeEv_nid_postfix(const void*);
std::int32_t APS5_VABI _ZNK6icu_6822BytesDictionaryMatcher9transformEi_nid_postfix(const Icu::BytesDictionaryMatcher*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6823UCharsDictionaryMatcher7getTypeEv_nid_postfix(const void*);
const void* APS5_VABI _ZNK6icu_6813SimpleFactory17getDynamicClassIDEv_nid_postfix(const void*);
const void* APS5_VABI _ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(const void*);
const void* APS5_VABI _ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix(const void*);
const void* APS5_VABI _ZNK6icu_6818UStringEnumeration17getDynamicClassIDEv_nid_postfix(const void*);
const void* APS5_VABI _ZNK6icu_6818UnicodeSetIterator17getDynamicClassIDEv_nid_postfix(const void*);
const void* APS5_VABI _ZNK6icu_6824ICUResourceBundleFactory17getDynamicClassIDEv_nid_postfix(const void*);
}

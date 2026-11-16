#pragma once
#include "CoreClasses.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct CharArray {
    char* ptr;
    std::int32_t capacity;
    std::int8_t needToRelease;
    char stackArray[40];
};

struct CharString {
    CharArray buffer;
    std::int32_t len;
};

static_assert(offsetof(CharArray, capacity) == 8 && offsetof(CharArray, needToRelease) == 12 && offsetof(CharArray, stackArray) == 13);
static_assert(sizeof(CharArray) == 56 && offsetof(CharString, len) == 56 && sizeof(CharString) == 64);

} // namespace Icu

extern "C" {
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(Icu::CharArray*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ev_nid_postfix(Icu::CharArray*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ei10UErrorCode_nid_postfix(Icu::CharArray*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ei10UErrorCode_nid_postfix(Icu::CharArray*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(Icu::CharArray*, Icu::CharArray*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2EOS1__nid_postfix(Icu::CharArray*, Icu::CharArray*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(Icu::CharArray*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED2Ev_nid_postfix(Icu::CharArray*);
Icu::CharArray* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(Icu::CharArray*, Icu::CharArray*);
std::int32_t APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE11getCapacityEv_nid_postfix(const Icu::CharArray*);
char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(const Icu::CharArray*);
char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(const Icu::CharArray*);
const char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(const Icu::CharArray*, std::ptrdiff_t);
char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(Icu::CharArray*, std::ptrdiff_t);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(Icu::CharArray*, char*, std::int32_t);
char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(Icu::CharArray*, std::int32_t, std::int32_t);
char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(Icu::CharArray*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(Icu::CharArray*, const Icu::CharArray*, std::int32_t*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12releaseArrayEv_nid_postfix(Icu::CharArray*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE17resetToStackArrayEv_nid_postfix(Icu::CharArray*);
void APS5_VABI _ZN6icu_6810CharStringC1EOS0__nid_postfix(Icu::CharString*, Icu::CharString*);
void APS5_VABI _ZN6icu_6810CharStringC2EOS0__nid_postfix(Icu::CharString*, Icu::CharString*);
Icu::CharString* APS5_VABI _ZN6icu_6810CharStringaSEOS0__nid_postfix(Icu::CharString*, Icu::CharString*);
char* APS5_VABI _ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(const Icu::CharString*, std::int32_t*);
std::int32_t APS5_VABI _ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(const Icu::CharString*, char*, std::int32_t, std::int32_t*);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString8copyFromERKS0_R10UErrorCode_nid_postfix(Icu::CharString*, const Icu::CharString*, std::int32_t*);
std::int32_t APS5_VABI _ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(const Icu::CharString*, char);
std::int8_t APS5_VABI _ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(const Icu::CharString*, Icu::StringPiece);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString8truncateEi_nid_postfix(Icu::CharString*, std::int32_t);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(Icu::CharString*, char, std::int32_t*);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(Icu::CharString*, const char*, std::int32_t, std::int32_t*);
char* APS5_VABI _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(Icu::CharString*, std::int32_t, std::int32_t, std::int32_t*,
                                                                                    std::int32_t*);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(Icu::CharString*, const char16_t*, std::int32_t,
                                                                                                     std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(Icu::CharString*, std::int32_t, std::int32_t, std::int32_t*);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(Icu::CharString*, Icu::StringPiece,
                                                                                                           std::int32_t*);
Icu::CharString* APS5_VABI _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(Icu::CharString*, std::int32_t*);
char APS5_VABI _ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(const Icu::CharString*);
}

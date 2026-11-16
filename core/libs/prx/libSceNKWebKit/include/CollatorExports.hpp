#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIcu/Iterator.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>

struct UCollator;

namespace NKWebKit::WtfAbi {

struct Collator {
    char* locale;
    bool shouldSortLowercaseFirst;
    UCollator* collator;
};

static_assert(sizeof(Collator) == 24);
static_assert(offsetof(Collator, shouldSortLowercaseFirst) == 8);
static_assert(offsetof(Collator, collator) == 16);

}

extern "C" {

void APS5_VABI _ZN3WTF8CollatorC1EPKcb(NKWebKit::WtfAbi::Collator* self, const char* locale, bool shouldSortLowercaseFirst);
void APS5_VABI _ZN3WTF8CollatorC2EPKcb(NKWebKit::WtfAbi::Collator* self, const char* locale, bool shouldSortLowercaseFirst);
void APS5_VABI _ZN3WTF8CollatorD1Ev(NKWebKit::WtfAbi::Collator* self);
void APS5_VABI _ZN3WTF8CollatorD2Ev(NKWebKit::WtfAbi::Collator* self);
int APS5_VABI _ZNK3WTF8Collator7collateENS_10StringViewES1_(const NKWebKit::WtfAbi::Collator* self, NKWebKit::WtfAbi::StringView a, NKWebKit::WtfAbi::StringView b);
int APS5_VABI _ZNK3WTF8Collator11collateUTF8EPKcS2_(const NKWebKit::WtfAbi::Collator* self, const char* a, const char* b);
UCharIterator APS5_VABI _ZN3WTF14createIteratorENS_10StringViewE(NKWebKit::WtfAbi::StringView string);

}

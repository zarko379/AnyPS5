#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

struct UBreakIterator;

extern "C" {

bool APS5_VABI _ZN3WTF15isWordTextBreakEP14UBreakIterator(UBreakIterator* iterator);
unsigned APS5_VABI _ZN3WTF19numGraphemeClustersENS_10StringViewE(NKWebKit::WtfAbi::StringView string);
unsigned APS5_VABI _ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj(NKWebKit::WtfAbi::StringView string, unsigned numGraphemeClusters);

}

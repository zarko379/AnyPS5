#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>

namespace Jsc::WtfAbi {

struct String {
    const StringImpl* impl;
};

enum ConversionMode : int {
    LenientConversion,
    StrictConversion,
    StrictConversionReplacingUnpairedSurrogatesWithFFFD,
};

enum class UTF8ConversionError : int {
    None,
    OutOfMemory,
    IllegalSource,
    SourceExhausted,
};

static_assert(sizeof(String) == 8);
static_assert(sizeof(ConversionMode) == 4);
static_assert(sizeof(UTF8ConversionError) == 4);

} // namespace Jsc::WtfAbi

extern "C" {

const Jsc::WtfAbi::String* APS5_VABI _ZN3WTF10nullStringEv();
unsigned APS5_VABI _ZNK3WTF6String12toUIntStrictEPbi(const Jsc::WtfAbi::String* self, bool* ok, int base);
Jsc::WtfAbi::UTF8ConversionError APS5_VABI _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(
    const char16_t* characters, unsigned length, char*& buffer, std::size_t bufferSize, Jsc::WtfAbi::ConversionMode mode);

}

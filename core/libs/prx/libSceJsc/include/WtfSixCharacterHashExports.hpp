#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <type_traits>

static_assert(sizeof(std::array<char, 7>) == 7);
static_assert(std::is_trivially_copyable_v<std::array<char, 7>>);

extern "C" {

std::array<char, 7> APS5_VABI _ZN3WTF31integerToSixCharacterHashStringEj(unsigned hash);

}

#pragma once

#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

void Sha1AddBytes(SHA1& sha1, const std::uint8_t* input, std::size_t length);

} // namespace Jsc::WtfAbi

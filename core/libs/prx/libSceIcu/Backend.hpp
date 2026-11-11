#pragma once

#include <cstdint>

namespace IcuBackend {

void Ensure();
std::int32_t* RequireStatus(std::int32_t* status);
const void* RequireObject(const void* object);

}

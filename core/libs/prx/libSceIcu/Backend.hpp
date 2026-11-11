#pragma once

#include <cstdint>

namespace IcuBackend {

void Ensure();
std::int32_t* RequireStatus(std::int32_t* status);
const void* RequireObject(const void* object);

void SetGuestDefaultLocale(bool set);
void SetGuestDefaultConverter(bool set);
const char* Locale(const char* locale);
const char* Converter(const char* name);
const char16_t* Zone(const char16_t* zone, std::int32_t length);
void CheckFallback(const std::int32_t* status);
void Cleanup();

}

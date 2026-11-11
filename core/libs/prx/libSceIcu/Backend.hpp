#pragma once

#include <cstdint>

struct USet;
struct UEnumeration;

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
bool LocaleData(const char* tree, const char* locale);
void Track(const void* handle, bool needsDefault);
void CheckTracked(const void* handle);
bool Tracked(const void* handle);
void Untrack(const void* handle);
void PatternGenerator();
void Cleanup();
USet* GuestSet(const USet* host);
UEnumeration* GuestEnumeration(UEnumeration* host, std::int32_t* status);

}

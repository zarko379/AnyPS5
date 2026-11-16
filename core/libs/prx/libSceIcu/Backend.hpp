#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

struct USet;
struct UEnumeration;
struct UResourceBundle;

namespace IcuBackend {

void Ensure();
std::int32_t* RequireStatus(std::int32_t* status);
const void* RequireObject(const void* object);

class GuestStatus {
public:
    GuestStatus(std::int32_t* status, const char* function);
    std::int32_t* Local() { return &local; }
    void Commit() const;

private:
    std::int32_t* guest;
    const char* caller;
    int savedErrno;
    std::int32_t incoming;
    std::int32_t local;
};

template<typename TCall>
auto WithGuestStatus(std::int32_t* status, const char* caller, TCall&& call) {
    GuestStatus guestStatus(status, caller);
    if constexpr (std::is_void_v<std::invoke_result_t<TCall, std::int32_t*>>) {
        std::forward<TCall>(call)(guestStatus.Local());
        guestStatus.Commit();
    } else {
        auto result = std::forward<TCall>(call)(guestStatus.Local());
        guestStatus.Commit();
        return result;
    }
}

void SetGuestDefaultLocale(bool set);
bool SetDefaultLocale(const char* locale, std::int32_t* status);
const char* GuestDefaultLocale();
void SetGuestDefaultConverter(bool set);
const char* Locale(const char* locale);
const char* Converter(const char* name);
const char16_t* Zone(const char16_t* zone, std::int32_t length);
void CheckFallback(const std::int32_t* status);
bool GuestDefault();
void RequireGuestDefault();
bool ReadsDefaultLocale(const char* locale);
bool CanonicalNameReadsDefault(const char* locale);
const char* CanonicalLocale(const char* locale);
const char* LocaleObject(const char* locale);
void NewDefaultLocale(const char* locale);
const char* Package(const char* path);
void CheckKey(const UResourceBundle* bundle, const char* key, const std::int32_t* status);
void CheckWithFallback(const UResourceBundle* bundle, const char* key, const std::int32_t* status);
bool LocaleData(const char* tree, const char* locale);
void Track(const void* handle, bool needsDefault);
void CheckTracked(const void* handle);
bool Tracked(const void* handle);
void Untrack(const void* handle);
void PatternGenerator();
void ConverterList();
void ConverterListBuilt();
void Cleanup();
USet* GuestSet(const USet* host);
UEnumeration* GuestEnumeration(UEnumeration* host, std::int32_t* status);

}

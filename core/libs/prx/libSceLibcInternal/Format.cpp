#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <string>

namespace {

template <typename TFloat>
TFloat ParseWideFloat(const char16_t* text, char16_t** end, std::int64_t pten, int* error, TFloat (APS5_VABI *parse)(const char*, char**, std::int64_t, int*)) {
    std::string prefix;
    for (const char16_t* current = text; *current != 0 && *current < 0x80; ++current) prefix.push_back(static_cast<char>(*current));
    char* parsedEnd = nullptr;
    const TFloat value = parse(prefix.c_str(), &parsedEnd, pten, error);
    if (end != nullptr) *end = const_cast<char16_t*>(text) + (parsedEnd - prefix.c_str());
    return value;
}

}

extern "C" {

double APS5_VABI _WStodx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error) {
    return ParseWideFloat<double>(text, end, pten, error, _Stodx_nid_postfix);
}

float APS5_VABI _WStofx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error) {
    return ParseWideFloat<float>(text, end, pten, error, _Stofx_nid_postfix);
}

long double APS5_VABI _WStoldx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error) {
    return ParseWideFloat<long double>(text, end, pten, error, _Stoldx_nid_postfix);
}

int APS5_VABI __vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    if (!stream) {
        *__error_nid_postfix() = 9;
        return -1;
    }
    return vfprintf_nid_postfix(stream, format, args);
}

}

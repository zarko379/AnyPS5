#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <cmath>
#include <cstdarg>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest libc internal format check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int APS5_VABI WriteFormatted(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = __vfprintf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

static void CheckWideScaledConversions() {
    const char16_t decimal[] = u"  12.5e1xyz";
    char16_t* end = nullptr;
    int error = 7;
    Require(_WStodx_nid_postfix(decimal, &end, 0, &error) == 125.0);
    Require(end == decimal + 8 && error == 0);
    char16_t* plainEnd = nullptr;
    Require(_WStod_nid_postfix(decimal, &plainEnd, 0) == 125.0 && plainEnd == end);
    const char16_t scaled[] = u"1.5";
    error = 7;
    Require(_WStodx_nid_postfix(scaled, &end, 2, &error) == 150.0 && end == scaled + 3 && error == 0);
    const char16_t accented[] = u"3.25\u00e9";
    Require(_WStodx_nid_postfix(accented, &end, 0, nullptr) == 3.25 && end == accented + 4);
    const char16_t huge[] = u"1e400";
    error = 0;
    Require(_WStodx_nid_postfix(huge, &end, 0, &error) == HUGE_VAL && end == huge + 5 && error == 1);
    const char16_t none[] = u"abc";
    error = 7;
    Require(_WStodx_nid_postfix(none, &end, 0, &error) == 0.0 && end == none && error == 0);
    const char16_t single[] = u"1e39";
    error = 0;
    Require(_WStofx_nid_postfix(single, &end, 0, &error) == HUGE_VALF && end == single + 4 && error == 1);
    const char16_t shortScaled[] = u"-0.25";
    error = 7;
    Require(_WStofx_nid_postfix(shortScaled, &end, 1, &error) == -2.5f && end == shortScaled + 5 && error == 0);
    const char16_t extended[] = u"2.5 ";
    error = 7;
    Require(_WStoldx_nid_postfix(extended, &end, -1, &error) == 0.25L && end == extended + 3 && error == 0);
}

int main() {
    const auto filePath = "guest_libc_internal_format_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    auto* stream = fopen_nid_postfix(filePath.c_str(), "w+b");
    Require(stream != nullptr);

    *__error_nid_postfix() = 77;
    Require(WriteFormatted(stream, "") == 0);
    Require(*__error_nid_postfix() == 77);
    const int written = WriteFormatted(stream, "value: %d, string: %s", 12345, "testing");
    Require(written == 29);
    const int extra = WriteFormatted(stream, " %04x %.*s %lld %% %c", 0x2au, 3, "abcdef", -9000000000LL, 'Z');
    Require(extra == 25);
    Require(*__error_nid_postfix() == 77);
    Require(fclose_nid_postfix(stream) == 0);

    std::ifstream in(filePath, std::ios::binary);
    Require(in.is_open());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    Require(contents == "value: 12345, string: testing 002a abc -9000000000 % Z");
    std::filesystem::remove(filePath);

    FileStream closedStream(TemporaryFile::Open());
    closedStream.Close();
    *__error_nid_postfix() = 0;
    Require(WriteFormatted(&closedStream, "test") == -1);
    Require(*__error_nid_postfix() == 9);

    *__error_nid_postfix() = 0;
    Require(WriteFormatted(nullptr, "test") == -1);
    Require(*__error_nid_postfix() == 9);

    CheckWideScaledConversions();
    return 0;
}

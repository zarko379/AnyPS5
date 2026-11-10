#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <string>

#if defined(__clang__) && !defined(_WIN32)
#define __builtin_sysv_va_list va_list
#define __builtin_sysv_va_start va_start
#define __builtin_sysv_va_end va_end
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "curl utility check failed at line %u\n", location.line());
        std::abort();
    }
}

static std::string Printed(int expectedResult, const char* format, auto... args) {
    std::array<char, 512> buffer;
    buffer.fill('Z');
    const int result = curl_msnprintf_nid_postfix(buffer.data(), buffer.size(), format, args...);
    Require(result == expectedResult);
    Require(std::memchr(buffer.data(), 0, buffer.size()) != nullptr);
    return buffer.data();
}

static void CheckPrinted(const char* expected, const char* format, auto... args) {
    Require(Printed(static_cast<int>(std::strlen(expected)), format, args...) == expected);
}

template<class Exception>
static void CheckThrows(auto action) {
    bool threw = false;
    try {
        action();
    } catch (const Exception&) {
        threw = true;
    }
    Require(threw);
}

static int APS5_VABI ListBounded(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = curl_mvsnprintf_nid_postfix(buffer, size, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ListUnbounded(char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = curl_mvsprintf_nid_postfix(buffer, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static char* APS5_VABI ListAllocated(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    char* result = curl_mvaprintf_nid_postfix(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ListStream(std::FILE* stream, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = curl_mvfprintf_nid_postfix(stream, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ListStandardOutput(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = curl_mvprintf_nid_postfix(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static void CheckIntegers() {
    CheckPrinted("-42|17|4294967295|10|ff|FF|A", "%d|%i|%u|%o|%x|%X|%c", -42, 17, 4294967295u, 8, 255, 255, 'A');
    CheckPrinted("70000|70000", "%hd|%hhd", 70000, 70000);
    CheckPrinted("-9000000000|18446744073709551615|123456789abc", "%ld|%lu|%zx", -9000000000LL, UINT64_MAX, std::uint64_t{0x123456789abc});
    CheckPrinted("-9223372036854775808|5|1099511627776", "%lld|%qd|%Od", INT64_MIN, 5LL, 1LL << 40);
    CheckPrinted("-1|4294967295", "%d|%u", -1LL, -1LL);
    CheckPrinted("+5| 5|-5|+0", "%+u|% d|%+d|%+x", 5, 5, -5, 0);
    CheckPrinted("010|0|0x0|0XFF|0xff", "%#o|%#o|%#x|%#X|%#x", 8, 0, 0, 255, 255);
    CheckPrinted("|  007|007", "|%5.3d|%.3d", 7, 7);
    CheckPrinted("[]", "[%.0d]", 0);
    CheckPrinted("-0042|42   |42   |   42|+0042|  -42", "%05d|%-5d|%0-5d|%5d|%+05d|% 5d", -42, 42, 42, 42, 42, -42);
    CheckPrinted("005|-5", "%. 3d|%.-3d", 5, -5);
    CheckPrinted("  x|x  |y", "%3c|%-3c|%c", 'x', 'x', 0x179);
    CheckPrinted("1 2 3 4 5 6 7 8 9", "%d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8, 9);
}

static void CheckParameters() {
    CheckPrinted("   42|42   |007|7", "%*d|%*d|%.*d|%.*d", 5, 42, -5, 42, 3, 7, -1, 7);
    CheckPrinted("b a c", "%2$s %1$s %s", "a", "b", "c");
    CheckPrinted("3 3", "%1$d %1$d", 3);
    CheckPrinted("  7|", "%1$*2$d|", 3, 7);
    CheckPrinted("64d", "%I64d", 5);
    CheckPrinted("ab", "a%yb", nullptr);
    CheckPrinted("ab", "ab%", nullptr);
    CheckPrinted("d", "%-%d", nullptr, 7);
    CheckPrinted("d", "%129$d", nullptr);
    CheckPrinted("100%|%", "100%%|%%");
    std::string many;
    for (int index = 0; index < 129; ++index) many += "%d";
    std::array<char, 8> buffer;
    buffer.fill('Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), buffer.size(), many.c_str()) == -1 && buffer[0] == 'Z');
    Require(curl_msprintf_nid_postfix(buffer.data(), many.c_str()) == -1 && buffer[0] == '\0');
    Require(curl_maprintf_nid_postfix(many.c_str()) == nullptr);
    buffer.fill('Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), buffer.size(), "%*129$d", 1, 2) == -1 && buffer[0] == 'Z');
    CheckPrinted("d", "%.*129$d", 1, 2);
    std::string reused;
    for (int index = 0; index < 128; ++index) reused += "%1$d";
    Require(Printed(128, reused.c_str(), 1) == std::string(128, '1'));
    reused += "%1$d";
    CheckThrows<std::length_error>([&] { curl_msnprintf_nid_postfix(buffer.data(), buffer.size(), reused.c_str(), 1); });
    CheckThrows<std::invalid_argument>([&] { curl_msnprintf_nid_postfix(buffer.data(), buffer.size(), "%2$d", 1, 2); });
}

static void CheckStrings() {
    const char* none = nullptr;
    CheckPrinted("(nil)||(nil)|(nil)|(nil)", "%s|%.4s|%.5s|%S|%#s", none, none, none, none, none);
    CheckPrinted("     abc|abc     |ab|ab", "%8s|%-8s|%.2s|%. 2s", "abc", "abc", "abc", "abc");
    CheckPrinted("\"abc\"|\"    ab\"|\"ab    \"", "%S|%#6s|%-#6s", "abc", "ab", "ab");
    CheckPrinted("abc|abc", "%.-1s|%.-3s", "abc", "abc");
    void* pointer = reinterpret_cast<void*>(0x1234);
    CheckPrinted("(nil)|(nil)     |     (nil)|0x1234|    0x1234|0x00001234|0x1234    ", "%p|%10p|%-10p|%p|%10p|%010p|%-10p",
        nullptr, nullptr, nullptr, pointer, pointer, pointer, pointer);
    CheckPrinted("0x0000001234", "%.10p", pointer);
}

static void CheckDoubles() {
    CheckPrinted("1.500000|3.14|1.234568e+04|1E-05|1.500000", "%f|%.2f|%e|%G|%Lf", 1.5, 3.14159, 12345.678, 0.00001, 1.5);
    CheckPrinted("  2.3|1.50    |+1.3| 2.0|2.|1.500000", "%05.1f|%-8.2f|%+.1f|% .1f|%#.0f|%*f", 2.3, 1.5, 1.3, 2.0, 2.0, -6, 1.5);
    CheckPrinted("1 2 3 4 5 6 7 8 9 10 11", "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f",
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0);
    CheckPrinted("1 2.5 3 4.5 5", "%d %.1f %d %.1f %d", 1, 2.5, 3, 4.5, 5);
    CheckPrinted("inf|-inf|nan", "%f|%e|%f", INFINITY, -INFINITY, NAN);
    Require(Printed(325, "%.400f", 1.0) == "1." + std::string(323, '0'));
    Require(Printed(325, "%400.1f", 1.0) == std::string(324, ' ') + "1");
    CheckPrinted("1.000000", "%.*f", -2, 1.0);
    CheckThrows<std::domain_error>([] { Printed(0, "%.2f", INFINITY); });
    CheckThrows<std::overflow_error>([] { Printed(0, "%+.304f", 9e19); });
}

static void CheckCounts() {
    std::array<unsigned char, 24> bytes;
    const auto check = [&](const char* format, size_t written) {
        bytes.fill(0xAA);
        CheckPrinted("abc", format, bytes.data() + 8);
        std::int64_t value = 0;
        std::memcpy(&value, bytes.data() + 8, written);
        Require(value == 3);
        for (size_t index = 0; index < bytes.size(); ++index)
            if (index < 8 || index >= 8 + written) Require(bytes[index] == 0xAA);
    };
    check("abc%n", 4);
    check("abc%ln", 8);
    check("abc%lln", 8);
    check("abc%zn", 8);
    check("abc%Ln", 4);
    check("abc%hn", 2);
    check("abc%hhn", 2);
    CheckThrows<std::invalid_argument>([] { Printed(0, "%n", nullptr); });
}

static void CheckOutputs() {
    std::array<char, 16> buffer;
    buffer.fill('Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), 5, "hello world") == 4 && std::strcmp(buffer.data(), "hell") == 0);
    buffer.fill('Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), 5, "hello") == 4 && std::strcmp(buffer.data(), "hell") == 0);
    buffer.fill('Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), 6, "hello") == 5 && std::strcmp(buffer.data(), "hello") == 0 && buffer[6] == 'Z');
    buffer.fill('Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), 0, "abc") == 0 && buffer[0] == 'Z');
    Require(curl_msnprintf_nid_postfix(buffer.data(), 8, "%100d", 1) == 7 && std::strcmp(buffer.data(), "       ") == 0);
    Require(curl_msnprintf_nid_postfix(buffer.data(), 1, "") == 0 && buffer[0] == '\0');
    buffer.fill('Z');
    Require(ListBounded(buffer.data(), buffer.size(), "%s=%ld,%.1f", "v", -7LL, 0.5) == 8 && std::strcmp(buffer.data(), "v=-7,0.5") == 0);
    buffer.fill('Z');
    Require(curl_msprintf_nid_postfix(buffer.data(), "%d-%s", 5, "ab") == 4 && std::strcmp(buffer.data(), "5-ab") == 0 && buffer[5] == 'Z');
    buffer.fill('Z');
    Require(ListUnbounded(buffer.data(), "%x%s", 0xbeef, "!") == 5 && std::strcmp(buffer.data(), "beef!") == 0 && buffer[6] == 'Z');
    char* allocated = curl_maprintf_nid_postfix("x%sy%d", "abc", 12);
    Require(allocated != nullptr && std::strcmp(allocated, "xabcy12") == 0);
    curl_free(allocated);
    allocated = ListAllocated("%5s|%c", "ab", 'q');
    Require(allocated != nullptr && std::strcmp(allocated, "   ab|q") == 0);
    curl_free(allocated);
    allocated = curl_maprintf_nid_postfix("%s", "");
    Require(allocated != nullptr && allocated[0] == '\0');
    curl_free(allocated);

    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-curl-printf-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto* stream = fopen_nid_postfix(path.string().c_str(), "wb");
    Require(stream != nullptr);
    auto* file = reinterpret_cast<std::FILE*>(stream);
    Require(curl_mfprintf_nid_postfix(file, "%s=%d\n", "key", 9) == 6);
    Require(ListStream(file, "%05.1f|%S\n", 2.3, "q") == 10);
    Require(curl_mfprintf_nid_postfix(file, "") == 0);
    Require(fclose_nid_postfix(stream) == 0);
    {
        std::ifstream input(path, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Require(text == "key=9\n  2.3|\"q\"\n");
    }
    stream = fopen_nid_postfix(path.string().c_str(), "rb");
    Require(stream != nullptr);
    Require(curl_mfprintf_nid_postfix(reinterpret_cast<std::FILE*>(stream), "ab%d", 1) == 0);
    Require(fclose_nid_postfix(stream) == 0);
    std::filesystem::remove(path);

    Require(curl_mprintf_nid_postfix("curl mprintf %s\n", "line") == 18);
    Require(ListStandardOutput("curl mvprintf %d\n", 2) == 16);
}

static void CheckPrintf() {
    CheckIntegers();
    CheckParameters();
    CheckStrings();
    CheckDoubles();
    CheckCounts();
    CheckOutputs();
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    const char* ver = curl_version_nid_postfix();
    Require(ver != nullptr);
    Require(std::strncmp(ver, "libcurl/", 8) == 0);

    const auto* info = curl_version_info_nid_postfix(CURLVERSION_NOW);
    Require(info != nullptr);
    Require(info->version != nullptr);
    Require(info->ssl_version != nullptr);
    Require(std::strstr(info->ssl_version, "mbedTLS") != nullptr);
    Require(info->protocols != nullptr);

    bool has_http = false;
    bool has_file = false;
    for (const char* const* p = info->protocols; *p != nullptr; ++p) {
        if (std::strcmp(*p, "http") == 0) has_http = true;
        if (std::strcmp(*p, "file") == 0) has_file = true;
    }
    Require(has_http);
    Require(has_file);

    const std::time_t date1 = curl_getdate_nid_postfix("Sun, 06 Nov 1994 08:49:37 GMT", nullptr);
    Require(date1 == 784111777);

    const std::time_t date2 = curl_getdate_nid_postfix("Sunday, 06-Nov-94 08:49:37 GMT", nullptr);
    Require(date2 == 784111777);

    const std::time_t date3 = curl_getdate_nid_postfix("Sun Nov  6 08:49:37 1994", nullptr);
    Require(date3 == 784111777);

    const std::time_t invalid_date = curl_getdate_nid_postfix("Not a valid date string", nullptr);
    Require(invalid_date == -1);

    Require(std::strcmp(curl_easy_strerror_nid_postfix(CURLE_OK), "No error") == 0);
    Require(curl_easy_strerror_nid_postfix(CURLE_UNSUPPORTED_PROTOCOL) != nullptr);

    Require(std::strcmp(curl_multi_strerror_nid_postfix(CURLM_OK), "No error") == 0);
    Require(curl_multi_strerror_nid_postfix(CURLM_BAD_HANDLE) != nullptr);

    Require(std::strcmp(curl_share_strerror_nid_postfix(CURLSHE_OK), "No error") == 0);
    Require(curl_share_strerror_nid_postfix(CURLSHE_BAD_OPTION) != nullptr);

    char* non_env = curl_getenv_nid_postfix("NONEXISTENT_VAR_ANYPS5_TEST_12345");
    Require(non_env == nullptr);

    Require(curl_version_info_nid_postfix(CURLVERSION_FIRST) == info);
    CheckPrintf();
    curl_global_cleanup_nid_postfix();
    return 0;
}

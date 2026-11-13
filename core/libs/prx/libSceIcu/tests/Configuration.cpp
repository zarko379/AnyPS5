#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdarg>
#include <cstddef>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Unsupported = 16;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU configuration check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

void Locking() {
    int marker = 0;
    for (std::int32_t incoming : {0, -128, -124, -1}) {
        auto status = incoming;
        u_setMutexFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, nullptr, &status);
        Require(status == Unsupported);
        status = incoming;
        u_setMutexFunctions_nid_postfix(&marker, &marker, &marker, &marker, &marker, &status);
        Require(status == Unsupported);
        status = incoming;
        u_setAtomicIncDecFunctions_nid_postfix(&marker, nullptr, nullptr, &status);
        Require(status == Unsupported);
        status = incoming;
        u_setAtomicIncDecFunctions_nid_postfix(nullptr, &marker, &marker, &status);
        Require(status == Unsupported);
    }
    for (std::int32_t incoming : {1, 7, 10, 15, 16, 27}) {
        auto status = incoming;
        u_setMutexFunctions_nid_postfix(&marker, &marker, &marker, &marker, &marker, &status);
        Require(status == incoming);
        u_setAtomicIncDecFunctions_nid_postfix(&marker, &marker, &marker, &status);
        Require(status == incoming);
    }
    RequireThrows([&] { u_setMutexFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { u_setAtomicIncDecFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr); });
}

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t) { return nullptr; }
void* APS5_VABI Reallocate_nid_no_patch(const void*, void*, std::size_t) { return nullptr; }
void APS5_VABI Release_nid_no_patch(const void*, void*) {}

void Memory() {
    int marker = 0;
    for (std::int32_t incoming : {0, -124, -1}) {
        auto status = incoming;
        u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
        Require(status == incoming);
        status = incoming;
        u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
        Require(status == incoming);
    }
    for (int missing = 0; missing < 8; ++missing) {
        std::int32_t status = 0;
        u_setMemoryFunctions_nid_postfix(&marker, missing & 1 ? nullptr : Allocate_nid_no_patch, missing & 2 ? nullptr : Reallocate_nid_no_patch,
                                         missing & 4 ? nullptr : Release_nid_no_patch, &status);
        Require(status == (missing ? 1 : 0));
    }
    std::int32_t status = -124;
    u_setMemoryFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, &status);
    Require(status == 1);
    for (std::int32_t incoming : {1, 7, 16}) {
        status = incoming;
        u_setMemoryFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, &status);
        Require(status == incoming);
    }
    RequireThrows([&] { u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, nullptr); });

    std::vector<std::thread> threads;
    std::vector<std::int32_t> results(8, 99);
    for (std::size_t index = 0; index < results.size(); ++index) {
        threads.emplace_back([&, index] {
            std::int32_t local = 0;
            for (int round = 0; round < 1000; ++round) {
                local = 0;
                u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &local);
                if (local != 0) break;
            }
            results[index] = local;
        });
    }
    for (auto& thread : threads) thread.join();
    for (auto value : results) Require(value == 0);
}

void DataDirectory() {
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    u_setDataDirectory_nid_postfix("/app0/icu");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "/app0/icu");
    u_setDataDirectory_nid_postfix(nullptr);
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    u_setDataDirectory_nid_postfix("/data");
    u_setDataDirectory_nid_postfix("");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");

    char buffer[] = "/first/path";
    u_setDataDirectory_nid_postfix(buffer);
    buffer[1] = 'X';
    Require(std::string(u_getDataDirectory_nid_postfix()) == "/first/path");
    Require(u_getDataDirectory_nid_postfix() == u_getDataDirectory_nid_postfix());
    u_setDataDirectory_nid_postfix("C:\\icu\\data/mixed");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "C:\\icu\\data/mixed");
    u_setDataDirectory_nid_postfix(std::string(5000, 'd').c_str());
    Require(std::string(u_getDataDirectory_nid_postfix()) == std::string(5000, 'd'));

    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([index] {
            const std::string value = "/thread/" + std::to_string(index);
            for (int round = 0; round < 500; ++round) u_setDataDirectory_nid_postfix(value.c_str());
        });
    }
    for (auto& thread : threads) thread.join();
    const std::string final = u_getDataDirectory_nid_postfix();
    Require(final.size() == 9 && final.starts_with("/thread/") && final[8] >= '0' && final[8] < '8');
    u_setDataDirectory_nid_postfix(nullptr);
}

void TimeZoneDirectory() {
    std::int32_t status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 0);
    u_setTimeZoneFilesDirectory_nid_postfix("/app0/tz", &status);
    Require(status == 0 && std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/app0/tz" && status == 0);

    status = 7;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 7);
    u_setTimeZoneFilesDirectory_nid_postfix("/other", &status);
    Require(status == 7);
    status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/app0/tz");

    status = -124;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/app0/tz" && status == -124);
    u_setTimeZoneFilesDirectory_nid_postfix("/warn", &status);
    Require(status == -124);
    status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/warn");

    u_setTimeZoneFilesDirectory_nid_postfix(nullptr, &status);
    Require(status == 1);
    status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "");

    u_setTimeZoneFilesDirectory_nid_postfix("/first", &status);
    u_setTimeZoneFilesDirectory_nid_postfix("", &status);
    Require(status == 0 && std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "");

    char buffer[] = "/copy/me";
    u_setTimeZoneFilesDirectory_nid_postfix(buffer, &status);
    buffer[1] = 'X';
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/copy/me");
    u_setTimeZoneFilesDirectory_nid_postfix("C:\\tz/mixed", &status);
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "C:\\tz/mixed");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    u_setDataDirectory_nid_postfix("/data");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "C:\\tz/mixed");
    u_setDataDirectory_nid_postfix(nullptr);

    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([index] {
            const std::string value = "/zone/" + std::to_string(index);
            std::int32_t local = 0;
            for (int round = 0; round < 500; ++round) u_setTimeZoneFilesDirectory_nid_postfix(value.c_str(), &local);
            Require(local == 0);
        });
    }
    for (auto& thread : threads) thread.join();
    const std::string final = u_getTimeZoneFilesDirectory_nid_postfix(&status);
    Require(final.size() == 7 && final.starts_with("/zone/") && final[6] >= '0' && final[6] < '8');
    status = 0;
    u_setTimeZoneFilesDirectory_nid_postfix(nullptr, &status);

    RequireThrows([&] { u_getTimeZoneFilesDirectory_nid_postfix(nullptr); });
    RequireThrows([&] { u_setTimeZoneFilesDirectory_nid_postfix("/x", nullptr); });
}

void Cleanup() {
    std::int32_t status = 0;
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 0);

    u_setDataDirectory_nid_postfix("/app0/icu");
    u_setTimeZoneFilesDirectory_nid_postfix("/app0/tz", &status);
    int marker = 0;
    u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0 && std::string(u_getDataDirectory_nid_postfix()) == "/app0/icu");
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 0);
    u_cleanup_nid_postfix();
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");

    u_setDataDirectory_nid_postfix("/again");
    u_setTimeZoneFilesDirectory_nid_postfix("/again/tz", &status);
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0);
    Require(std::string(u_getDataDirectory_nid_postfix()) == "/again");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/again/tz");
    u_cleanup_nid_postfix();

    std::vector<std::thread> threads;
    for (int index = 0; index < 4; ++index) {
        threads.emplace_back([index] {
            std::int32_t local = 0;
            for (int round = 0; round < 300; ++round) {
                if (index % 2) u_cleanup_nid_postfix();
                else u_setDataDirectory_nid_postfix("/busy");
            }
            u_getTimeZoneFilesDirectory_nid_postfix(&local);
        });
    }
    for (auto& thread : threads) thread.join();
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
}

struct TraceCall {
    const void* context = nullptr;
    std::int32_t function = -100;
    std::int32_t calls = 0;
};

TraceCall traceCall;

void APS5_VABI TraceEntered_nid_no_patch(const void* context, std::int32_t function) {
    traceCall.context = context;
    traceCall.function = function;
    ++traceCall.calls;
}

void APS5_VABI TraceExited_nid_no_patch(const void*, std::int32_t, const char*, VaList*) {}
void APS5_VABI TraceData_nid_no_patch(const void*, std::int32_t, std::int32_t, const char*, VaList*) {}

void TraceFunctions(const void* context, Icu::TraceEntry entry, Icu::TraceExit exit, Icu::TraceData data) {
    const void* gotContext = &traceCall;
    Icu::TraceEntry gotEntry = TraceEntered_nid_no_patch;
    Icu::TraceExit gotExit = TraceExited_nid_no_patch;
    Icu::TraceData gotData = TraceData_nid_no_patch;
    utrace_getFunctions_nid_postfix(&gotContext, &gotEntry, &gotExit, &gotData);
    Require(gotContext == context && gotEntry == entry && gotExit == exit && gotData == data);
}

void Tracing() {
    Require(utrace_getLevel_nid_postfix() == 0);
    TraceFunctions(nullptr, nullptr, nullptr, nullptr);
    utrace_entry_nid_postfix(1);
    Require(traceCall.calls == 0);
    for (const auto& [level, expected] : std::initializer_list<std::pair<std::int32_t, std::int32_t>>{
             {5, 5}, {-1, -1}, {9, 9}, {0, 0}, {-2, -1}, {10, 9}, {INT32_MIN, -1}, {INT32_MAX, 9}, {3, 3}}) {
        utrace_setLevel_nid_postfix(level);
        Require(utrace_getLevel_nid_postfix() == expected);
    }
    int marker = 0;
    utrace_setFunctions_nid_postfix(&marker, TraceEntered_nid_no_patch, TraceExited_nid_no_patch, TraceData_nid_no_patch);
    TraceFunctions(&marker, TraceEntered_nid_no_patch, TraceExited_nid_no_patch, TraceData_nid_no_patch);
    utrace_entry_nid_postfix(0x1003);
    Require(traceCall.calls == 1 && traceCall.context == &marker && traceCall.function == 0x1003);
    utrace_entry_nid_postfix(-7);
    Require(traceCall.calls == 2 && traceCall.function == -7);
    utrace_setFunctions_nid_postfix(nullptr, nullptr, TraceExited_nid_no_patch, nullptr);
    TraceFunctions(nullptr, nullptr, TraceExited_nid_no_patch, nullptr);
    utrace_entry_nid_postfix(0);
    Require(traceCall.calls == 2);
    const void* context = nullptr;
    Icu::TraceEntry entry = nullptr;
    Icu::TraceExit exit = nullptr;
    Icu::TraceData data = nullptr;
    RequireThrows([&] { utrace_getFunctions_nid_postfix(nullptr, &entry, &exit, &data); });
    RequireThrows([&] { utrace_getFunctions_nid_postfix(&context, nullptr, &exit, &data); });
    RequireThrows([&] { utrace_getFunctions_nid_postfix(&context, &entry, nullptr, &data); });
    RequireThrows([&] { utrace_getFunctions_nid_postfix(&context, &entry, &exit, nullptr); });
    utrace_setFunctions_nid_postfix(&marker, TraceEntered_nid_no_patch, TraceExited_nid_no_patch, TraceData_nid_no_patch);
    utrace_setLevel_nid_postfix(7);
    u_cleanup_nid_postfix();
    Require(utrace_getLevel_nid_postfix() == -1);
    TraceFunctions(nullptr, nullptr, nullptr, nullptr);
    utrace_entry_nid_postfix(1);
    Require(traceCall.calls == 2);
}

void TraceNames() {
    const std::pair<std::int32_t, const char*> names[] = {
        {0, "u_init"}, {1, "u_cleanup"}, {0x1000, "ucnv_open"}, {0x1001, "ucnv_openPackage"}, {0x1002, "ucnv_openAlgorithmic"},
        {0x1003, "ucnv_clone"}, {0x1004, "ucnv_close"}, {0x1005, "ucnv_flushCache"}, {0x1006, "ucnv_load"}, {0x1007, "ucnv_unload"},
        {0x2000, "ucol_open"}, {0x2001, "ucol_close"}, {0x2002, "ucol_strcoll"}, {0x2003, "ucol_getSortKey"}, {0x2004, "ucol_getLocale"},
        {0x2005, "ucol_nextSortKeyPart"}, {0x2006, "ucol_strcollIter"}, {0x2007, "ucol_openFromShortString"}, {0x3000, "resc"},
        {0x3001, "bundle-open"}, {0x3002, "file-open"}, {0x3003, "res-open"}};
    for (const auto& [number, name] : names) Require(std::string(utrace_functionName_nid_postfix(number)) == name);
    for (const std::int32_t number : {-1, 2, 0xfff, 0x1008, 0x1fff, 0x2008, 0x3004, 0x4000, 0x4001, INT32_MIN, INT32_MAX})
        Require(std::string(utrace_functionName_nid_postfix(number)) == "[BOGUS Trace Function Number]");
}

struct TraceRecord {
    const void* context = nullptr;
    std::int32_t function = 0;
    std::int32_t level = 0;
    std::string format;
    std::string text;
    std::int32_t calls = 0;
};

TraceRecord traceRecord;

void APS5_VABI TraceExitRecorded_nid_no_patch(const void* context, std::int32_t function, const char* fmt, VaList* args) {
    char buffer[128];
    traceRecord = {context, function, 0, fmt, "", traceRecord.calls + 1};
    utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, fmt, args);
    traceRecord.text = buffer;
}

void APS5_VABI TraceDataRecorded_nid_no_patch(const void* context, std::int32_t function, std::int32_t level, const char* fmt, VaList* args) {
    char buffer[128];
    traceRecord = {context, function, level, fmt, "", traceRecord.calls + 1};
    utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, fmt, args);
    traceRecord.text = buffer;
}

void TraceExitAndData() {
    u_cleanup_nid_postfix();
    utrace_exit_nid_postfix(1, 0x55);
    utrace_data_nid_postfix(1, 9, "%s", "unused");
    Require(traceRecord.calls == 0);
    int marker = 0;
    utrace_setFunctions_nid_postfix(&marker, nullptr, TraceExitRecorded_nid_no_patch, TraceDataRecorded_nid_no_patch);
    utrace_setLevel_nid_postfix(-1);
    utrace_exit_nid_postfix(0x2001, 0);
    Require(traceRecord.calls == 1 && traceRecord.context == &marker && traceRecord.function == 0x2001 && traceRecord.format == "Returns." && traceRecord.text == "Returns.");
    utrace_exit_nid_postfix(7, 1, -2);
    Require(traceRecord.format == "Returns %d." && traceRecord.text == "Returns fffffffe.");
    utrace_exit_nid_postfix(7, 0x10, 5);
    Require(traceRecord.format == "Returns.  Status = %d." && traceRecord.text == "Returns.  Status = 00000005.");
    utrace_exit_nid_postfix(7, 0x11, 0x1234, 1);
    Require(traceRecord.format == "Returns %d.  Status = %d." && traceRecord.text == "Returns 00001234.  Status = 00000001.");
    utrace_exit_nid_postfix(7, 0x12, 3, &marker);
    Require(traceRecord.format == "Returns %d.  Status = %p." && traceRecord.calls == 5);
    for (const std::int32_t returnType : {2, 3, 0x13, 0x20, -1}) RequireThrows([&] { utrace_exit_nid_postfix(7, returnType, 0, 0); });
    Require(traceRecord.calls == 5);
    utrace_data_nid_postfix(0x3001, 4, "%s:%d:%c", "bundle", 12, 'z');
    Require(traceRecord.calls == 6 && traceRecord.function == 0x3001 && traceRecord.level == 4 && traceRecord.format == "%s:%d:%c" && traceRecord.text == "bundle:0000000c:z");
    utrace_data_nid_postfix(1, 9, "%d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8);
    Require(traceRecord.text == "00000001 00000002 00000003 00000004 00000005 00000006 00000007 00000008");
    u_cleanup_nid_postfix();
}

extern "C" std::int32_t utrace_format_68(char* outBuf, std::int32_t capacity, std::int32_t indent, const char* fmt, ...);

template<typename... TArgs>
void SameAsIcu(std::int32_t capacity, std::int32_t indent, const char* fmt, TArgs... args) {
    char guest[512];
    char host[512];
    std::memset(guest, 0x5a, sizeof(guest));
    std::memset(host, 0x5a, sizeof(host));
    const std::int32_t guestLength = utrace_format_nid_postfix(capacity ? guest : nullptr, capacity, indent, fmt, args...);
    const std::int32_t hostLength = utrace_format_68(capacity ? host : nullptr, capacity, indent, fmt, args...);
    if (guestLength != hostLength || std::memcmp(guest, host, sizeof(guest)) != 0) {
        std::fprintf(stderr, "utrace_format \"%s\": %d \"%.*s\" against ICU %d \"%.*s\"\n", fmt, guestLength, capacity, guest, hostLength, capacity, host);
        Require(false);
    }
}

std::int32_t APS5_VABI GuestVformat_nid_no_patch(char* outBuf, std::int32_t capacity, const char* fmt, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, fmt);
    const std::int32_t length = utrace_vformat_nid_postfix(outBuf, capacity, 2, fmt, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, fmt);
    const std::int32_t length = utrace_vformat_nid_postfix(outBuf, capacity, 2, fmt, reinterpret_cast<VaList*>(args));
    va_end(args);
#endif
    return length;
}

void TraceFormat() {
    const char16_t text[] = u"été";
    const std::int8_t bytes[] = {1, -2, 3, 0};
    const std::int16_t halves[] = {0x1234, -1, 0};
    const std::int32_t words[] = {0x12345678, -5, 0};
    const std::int64_t longs[] = {0x0123456789abcdefLL, -1, 0};
    int marker = 0;
    const void* pointers[] = {&marker, nullptr};
    const char* strings[] = {"one", "two", nullptr};
    const char16_t* unicode[] = {u"ab", nullptr};
    const char chars[] = "chars";
    for (const std::int32_t capacity : {512, 40, 7, 1, 0}) {
        for (const std::int32_t indent : {0, 3}) {
            SameAsIcu(capacity, indent, "plain");
            SameAsIcu(capacity, indent, "%c|%s|%s|%%|%q|%", 'x', "str", static_cast<const char*>(nullptr));
            SameAsIcu(capacity, indent, "%S|%S|%S", text, 3, text, -1, static_cast<const char16_t*>(nullptr), 2);
            SameAsIcu(capacity, indent, "%b %h %d %l %p", 0x1ff, 0x12345, -7, static_cast<std::int64_t>(0x0123456789abcdefLL), static_cast<const void*>(&marker));
            SameAsIcu(capacity, indent, "%vb %vh %vd %vl", bytes, 4, halves, -1, words, 3, longs, -1);
            SameAsIcu(capacity, indent, "%vp %vc %vs %vS %vd", pointers, -1, chars, -1, strings, -1, unicode, 2, static_cast<const void*>(nullptr), 4);
            SameAsIcu(capacity, indent, "%vx %v", words, 2, words, 1);
            SameAsIcu(capacity, indent, "a\nb\n\nc%s\n", "d\ne");
            SameAsIcu(capacity, indent, "%d%d%d%d%d%d%d%d%d%d", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        }
    }
    char buffer[64];
    Require(GuestVformat_nid_no_patch(buffer, sizeof(buffer), "%s %d\n%c", "va", 10, 'q') == 18 && std::string(buffer) == "  va 0000000a\n  q");
    RequireThrows([&] { utrace_format_nid_postfix(buffer, sizeof(buffer), 0, nullptr); });
    RequireThrows([&] { utrace_format_nid_postfix(nullptr, 1, 0, "x"); });
    Require(utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, "no arguments", nullptr) == 13 && std::string(buffer) == "no arguments");
    RequireThrows([&] { utrace_vformat_nid_postfix(buffer, sizeof(buffer), 0, "%d", nullptr); });
    RequireThrows([&] { utrace_format_nid_postfix(buffer, sizeof(buffer), 0, "%l%vx", static_cast<std::int64_t>(1), words, -1); });
    SameAsIcu(64, 0, "%l%vx", static_cast<std::int64_t>(0), words, -1);
}

}

struct AllocatorLog {
    int allocations = 0;
    int reallocations = 0;
    int releases = 0;
    const void* context = nullptr;
    void* lastRelease = nullptr;
    std::size_t lastSize = 0;
};

AllocatorLog g_log;
alignas(16) unsigned char g_arena[256];

void* APS5_VABI LoggingAllocate(const void* context, std::size_t size) {
    ++g_log.allocations;
    g_log.context = context;
    g_log.lastSize = size;
    return g_arena;
}

void* APS5_VABI LoggingReallocate(const void* context, void* buffer, std::size_t size) {
    ++g_log.reallocations;
    g_log.context = context;
    g_log.lastSize = size;
    return buffer;
}

void APS5_VABI LoggingRelease(const void* context, void* buffer) {
    ++g_log.releases;
    g_log.context = context;
    g_log.lastRelease = buffer;
}

void UprvMemoryWithUserFunctions() {
    int marker = 0;
    std::int32_t status = 0;
    u_cleanup_nid_postfix();
    u_setMemoryFunctions_nid_postfix(&marker, LoggingAllocate, LoggingReallocate, LoggingRelease, &status);
    Require(status == 0);
    g_log = {};

    void* block = uprv_malloc_nid_postfix(40);
    Require(block == g_arena && g_log.allocations == 1 && g_log.context == &marker && g_log.lastSize == 40);

    void* zero = uprv_malloc_nid_postfix(0);
    Require(zero != nullptr && zero != g_arena && g_log.allocations == 1);

    Require(uprv_realloc_nid_postfix(block, 80) == block && g_log.reallocations == 1 && g_log.lastSize == 80);
    Require(g_log.releases == 0);

    Require(uprv_realloc_nid_postfix(block, 0) == zero && g_log.releases == 1 && g_log.lastRelease == block);

    Require(uprv_realloc_nid_postfix(zero, 24) == g_arena && g_log.allocations == 2 && g_log.lastSize == 24 && g_log.reallocations == 1);
    Require(uprv_realloc_nid_postfix(zero, 0) == zero && g_log.allocations == 2 && g_log.releases == 1);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* copy = uprv_strdup_nid_postfix("icu");
    Require(copy == reinterpret_cast<char*>(g_arena) && std::memcmp(copy, "icu", 4) == 0 && g_log.lastSize == 4);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* prefix = uprv_strndup_nid_postfix("abcdef", 3);
    Require(prefix == reinterpret_cast<char*>(g_arena) && std::memcmp(prefix, "abc", 4) == 0 && g_log.lastSize == 4);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* whole = uprv_strndup_nid_postfix("abcdef", -1);
    Require(std::memcmp(whole, "abcdef", 7) == 0 && g_log.lastSize == 7);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* longer = uprv_strndup_nid_postfix("ab", 2);
    Require(std::memcmp(longer, "ab", 3) == 0 && g_log.lastSize == 3);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* empty = uprv_strndup_nid_postfix("abc", 0);
    Require(empty[0] == 0 && g_log.lastSize == 1);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    auto* cleared = static_cast<unsigned char*>(uprv_calloc_nid_postfix(3, 8));
    Require(cleared == g_arena && g_log.lastSize == 24 && g_log.context == &marker && cleared[0] == 0 && cleared[23] == 0 && cleared[24] == 0x55);
    const int allocations = g_log.allocations;
    Require(uprv_calloc_nid_postfix(0, 8) == zero && uprv_calloc_nid_postfix(8, 0) == zero && uprv_calloc_nid_postfix((SIZE_MAX >> 1) + 1, 2) == zero);
    Require(g_log.allocations == allocations);
    u_cleanup_nid_postfix();
}

void UprvMemoryWithDefaultHeap() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    u_cleanup_nid_postfix();

    auto* block = static_cast<unsigned char*>(uprv_malloc_nid_postfix(16));
    Require(block != nullptr);
    std::memset(block, 0xA5, 16);
    block = static_cast<unsigned char*>(uprv_realloc_nid_postfix(block, 4096));
    Require(block != nullptr && block[0] == 0xA5 && block[15] == 0xA5);
    void* zero = uprv_realloc_nid_postfix(block, 0);
    Require(zero != nullptr && zero == uprv_malloc_nid_postfix(0));

    char* copy = uprv_strdup_nid_postfix("default heap");
    Require(copy != nullptr && std::string(copy) == "default heap");
    free_nid_postfix(copy);
    char* prefix = uprv_strndup_nid_postfix("default heap", 7);
    Require(prefix != nullptr && std::string(prefix) == "default");
    free_nid_postfix(prefix);
    char* whole = uprv_strndup_nid_postfix("heap", -5);
    Require(whole != nullptr && std::string(whole) == "heap");
    free_nid_postfix(whole);
    auto* cleared = static_cast<unsigned char*>(uprv_calloc_nid_postfix(4, 4));
    Require(cleared != nullptr && std::all_of(cleared, cleared + 16, [](unsigned char value) { return value == 0; }));
    free_nid_postfix(cleared);
}

int main() {
    Tracing();
    TraceNames();
    TraceExitAndData();
    TraceFormat();
    Locking();
    Memory();
    UprvMemoryWithUserFunctions();
    UprvMemoryWithDefaultHeap();
    DataDirectory();
    TimeZoneDirectory();
    Cleanup();
    std::puts("ICU configuration tests passed");
}

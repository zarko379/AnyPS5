#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace {

struct MemoryFunctions {
    const void* context = nullptr;
    Icu::MemoryAllocate allocate = nullptr;
    Icu::MemoryReallocate reallocate = nullptr;
    Icu::MemoryFree release = nullptr;
};

constexpr std::int32_t TraceOff = -1;
constexpr std::int32_t TraceError = 0;
constexpr std::int32_t TraceVerbose = 9;

struct Trace {
    Icu::TraceEntry entry = nullptr;
    Icu::TraceExit exit = nullptr;
    Icu::TraceData data = nullptr;
    const void* context = nullptr;
    std::int32_t level = TraceError;
};

constexpr std::array<const char*, 2> GeneralTraceNames{"u_init", "u_cleanup"};
constexpr std::array<const char*, 8> ConversionTraceNames{"ucnv_open", "ucnv_openPackage", "ucnv_openAlgorithmic", "ucnv_clone",
                                                          "ucnv_close", "ucnv_flushCache", "ucnv_load", "ucnv_unload"};
constexpr std::array<const char*, 8> CollationTraceNames{"ucol_open", "ucol_close", "ucol_strcoll", "ucol_getSortKey",
                                                         "ucol_getLocale", "ucol_nextSortKeyPart", "ucol_strcollIter", "ucol_openFromShortString"};
constexpr std::array<const char*, 4> ResourceTraceNames{"resc", "bundle-open", "file-open", "res-open"};

std::mutex TraceLock;
Trace Tracing;

std::mutex MemoryLock;
MemoryFunctions Memory;
const std::int32_t ZeroMemory[] = {0, 0, 0, 0, 0, 0};

MemoryFunctions CurrentMemory() {
    const std::lock_guard lock(MemoryLock);
    return Memory;
}

void* Allocate(std::size_t size) {
    if (size == 0) return const_cast<std::int32_t*>(ZeroMemory);
    const MemoryFunctions functions = CurrentMemory();
    return functions.allocate ? functions.allocate(functions.context, size) : malloc_nid_postfix(size);
}

void Release(void* memory) {
    const MemoryFunctions functions = CurrentMemory();
    if (functions.release) functions.release(functions.context, memory);
    else free_nid_postfix(memory);
}
std::mutex DirectoryLock;
std::string DataDirectory;
std::string TimeZoneDirectory;

}

extern "C" {

void APS5_VABI u_setMutexFunctions_nid_postfix(const void*, const void*, const void*, const void*, const void*, std::int32_t* status) {
    if (!Icu::Failure(status)) *status = Icu::UnsupportedError;
}

void APS5_VABI u_setAtomicIncDecFunctions_nid_postfix(const void*, const void*, const void*, std::int32_t* status) {
    if (!Icu::Failure(status)) *status = Icu::UnsupportedError;
}


void APS5_VABI u_setMemoryFunctions_nid_postfix(const void* context, Icu::MemoryAllocate allocate, Icu::MemoryReallocate reallocate,
                                                Icu::MemoryFree release, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!allocate || !reallocate || !release) {
        *status = Icu::IllegalArgument;
        return;
    }
    const std::lock_guard lock(MemoryLock);
    Memory = {context, allocate, reallocate, release};
}


const char* APS5_VABI u_getDataDirectory_nid_postfix() {
    const std::lock_guard lock(DirectoryLock);
    return DataDirectory.c_str();
}

void APS5_VABI u_setDataDirectory_nid_postfix(const char* directory) {
    std::string copy = directory ? directory : "";
    const std::lock_guard lock(DirectoryLock);
    DataDirectory = std::move(copy);
}


const char* APS5_VABI u_getTimeZoneFilesDirectory_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return "";
    const std::lock_guard lock(DirectoryLock);
    return TimeZoneDirectory.c_str();
}

void APS5_VABI u_setTimeZoneFilesDirectory_nid_postfix(const char* path, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    const std::lock_guard lock(DirectoryLock);
    TimeZoneDirectory.clear();
    if (!path) {
        *status = Icu::IllegalArgument;
        return;
    }
    TimeZoneDirectory = path;
}


void APS5_VABI utrace_entry_nid_postfix(std::int32_t function) {
    Trace trace;
    {
        const std::lock_guard lock(TraceLock);
        trace = Tracing;
    }
    if (trace.entry) trace.entry(trace.context, function);
}

void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, Icu::TraceEntry entry, Icu::TraceExit exit, Icu::TraceData data) {
    const std::lock_guard lock(TraceLock);
    Tracing.entry = entry;
    Tracing.exit = exit;
    Tracing.data = data;
    Tracing.context = context;
}

void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, Icu::TraceEntry* entry, Icu::TraceExit* exit, Icu::TraceData* data) {
    if (!context || !entry || !exit || !data) APS5_INVALID_ARG_EX;
    const std::lock_guard lock(TraceLock);
    *entry = Tracing.entry;
    *exit = Tracing.exit;
    *data = Tracing.data;
    *context = Tracing.context;
}

void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level) {
    const std::lock_guard lock(TraceLock);
    Tracing.level = std::clamp(level, TraceOff, TraceVerbose);
}

std::int32_t APS5_VABI utrace_getLevel_nid_postfix() {
    const std::lock_guard lock(TraceLock);
    return Tracing.level;
}

const char* APS5_VABI utrace_functionName_nid_postfix(std::int32_t function) {
    if (function >= 0 && function < static_cast<std::int32_t>(GeneralTraceNames.size())) return GeneralTraceNames[static_cast<std::size_t>(function)];
    if (function >= 0x1000 && function < 0x1000 + static_cast<std::int32_t>(ConversionTraceNames.size()))
        return ConversionTraceNames[static_cast<std::size_t>(function - 0x1000)];
    if (function >= 0x2000 && function < 0x2000 + static_cast<std::int32_t>(CollationTraceNames.size()))
        return CollationTraceNames[static_cast<std::size_t>(function - 0x2000)];
    if (function >= 0x3000 && function < 0x3000 + static_cast<std::int32_t>(ResourceTraceNames.size()))
        return ResourceTraceNames[static_cast<std::size_t>(function - 0x3000)];
    return "[BOGUS Trace Function Number]";
}


void APS5_VABI u_cleanup_nid_postfix() {
    IcuBackend::Cleanup();
    {
        const std::lock_guard lock(MemoryLock);
        Memory = {};
    }
    {
        const std::lock_guard lock(TraceLock);
        Tracing = Trace{};
        Tracing.level = TraceOff;
    }
    const std::lock_guard lock(DirectoryLock);
    DataDirectory.clear();
    TimeZoneDirectory.clear();
}

void* APS5_VABI uprv_malloc_nid_postfix(std::size_t size) {
    return Allocate(size);
}

void* APS5_VABI uprv_realloc_nid_postfix(void* buffer, std::size_t size) {
    if (buffer == ZeroMemory) return Allocate(size);
    if (size == 0) {
        Release(buffer);
        return const_cast<std::int32_t*>(ZeroMemory);
    }
    const MemoryFunctions functions = CurrentMemory();
    return functions.reallocate ? functions.reallocate(functions.context, buffer, size) : realloc_nid_postfix(buffer, size);
}

char* APS5_VABI uprv_strdup_nid_postfix(const char* source) {
    if (!source) APS5_INVALID_ARG_EX;
    const std::size_t length = std::strlen(source) + 1;
    char* duplicate = static_cast<char*>(Allocate(length));
    if (duplicate) std::memcpy(duplicate, source, length);
    return duplicate;
}

char* APS5_VABI uprv_strndup_nid_postfix(const char* source, std::int32_t count) {
    if (count < 0) return uprv_strdup_nid_postfix(source);
    const std::size_t length = static_cast<std::size_t>(count);
    char* duplicate = static_cast<char*>(Allocate(length + 1));
    if (duplicate) {
        if (length != 0) std::memcpy(duplicate, source, length);
        duplicate[length] = 0;
    }
    return duplicate;
}

}

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdarg>
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

constexpr std::int32_t ExitValue = 1;
constexpr std::int32_t ExitPointer = 2;
constexpr std::int32_t ExitStatus = 0x10;

Trace CurrentTrace() {
    const std::lock_guard lock(TraceLock);
    return Tracing;
}

const char* ExitFormat(std::int32_t returnType) {
    switch (returnType) {
    case 0:
        return "Returns.";
    case ExitValue:
        return "Returns %d.";
    case ExitStatus:
        return "Returns.  Status = %d.";
    case ExitValue | ExitStatus:
        return "Returns %d.  Status = %d.";
    case ExitPointer | ExitStatus:
        return "Returns %d.  Status = %p.";
    default:
        APS5_INVALID_ARG_EX;
    }
}

struct Arguments {
    VaList* list;

    std::uint64_t Next() {
        if (!list) APS5_INVALID_ARG_EX;
        std::uint64_t value;
        if (list->gp_offset < 48) {
            std::memcpy(&value, static_cast<const char*>(list->reg_save_area) + list->gp_offset, sizeof(value));
            list->gp_offset += 8;
        } else {
            std::memcpy(&value, list->overflow_arg_area, sizeof(value));
            list->overflow_arg_area = static_cast<char*>(list->overflow_arg_area) + 8;
        }
        return value;
    }

    std::int32_t Int() {
        return static_cast<std::int32_t>(Next());
    }

    std::int64_t Long() {
        return static_cast<std::int64_t>(Next());
    }

    const void* Pointer() {
        return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Next()));
    }
};

bool VectorType(char type) {
    return type == 'b' || type == 'h' || type == 'd' || type == 'l' || type == 'p' || type == 'c' || type == 's' || type == 'S';
}

struct TraceOutput {
    char* buffer;
    std::int32_t capacity;
    std::int32_t indent;
    std::int32_t index = 0;

    void Char(char c, std::int32_t indentBy) {
        if (index == 0 || (c != '\n' && c != 0 && index < capacity && buffer[index - 1] == '\n') || (c == '\n' && index >= capacity)) {
            for (std::int32_t i = 0; i < indentBy; ++i) {
                if (index < capacity) buffer[index] = ' ';
                ++index;
            }
        }
        if (index < capacity) buffer[index] = c;
        if (c != 0) ++index;
    }

    void Char(char c) {
        Char(c, indent);
    }

    void Hex(std::int64_t value, std::int32_t digits) {
        static constexpr char HexChars[] = "0123456789abcdef";
        for (std::int32_t shift = (digits - 1) * 4; shift >= 0; shift -= 4) Char(HexChars[(value >> shift) & 0xf], 0);
    }

    void Pointer(const void* value) {
        const auto bits = reinterpret_cast<std::uintptr_t>(value);
        for (std::int32_t byte = sizeof(void*) - 1; byte >= 0; --byte) Hex(static_cast<signed char>(bits >> (8 * byte)), 2);
    }

    void String(const char* text) {
        if (!text) text = "*NULL*";
        std::size_t i = 0;
        char c;
        do {
            c = text[i++];
            Char(c);
        } while (c != 0);
    }

    void UnicodeString(const char16_t* text, std::int32_t length) {
        if (!text) {
            String(nullptr);
            return;
        }
        for (std::int64_t i = 0; i < length || length == -1; ++i) {
            const char16_t c = text[i];
            Hex(c, 4);
            Char(' ');
            if (length == -1 && c == 0) break;
        }
    }

    void Vector(char type, const void* data, std::int32_t length, std::int64_t& last) {
        if (!data) {
            String("*NULL* ");
        } else {
            if (length == -1 && last != 0 && !VectorType(type)) APS5_INVALID_ARG_EX;
            const auto* bytes = static_cast<const signed char*>(data);
            const auto* halves = static_cast<const std::int16_t*>(data);
            const auto* words = static_cast<const std::int32_t*>(data);
            const auto* longs = static_cast<const std::int64_t*>(data);
            const auto* pointers = static_cast<const void* const*>(data);
            for (std::int64_t i = 0; i < length || length == -1; ++i) {
                std::int32_t digits = 0;
                switch (type) {
                case 'b':
                    digits = 2;
                    last = *bytes++;
                    break;
                case 'h':
                    digits = 4;
                    last = *halves++;
                    break;
                case 'd':
                    digits = 8;
                    last = *words++;
                    break;
                case 'l':
                    digits = 16;
                    last = *longs++;
                    break;
                case 'p':
                    Pointer(*pointers);
                    last = *pointers ? 1 : 0;
                    ++pointers;
                    break;
                case 'c':
                    Char(static_cast<char>(*bytes));
                    last = *bytes++;
                    break;
                case 's':
                    String(static_cast<const char*>(*pointers));
                    Char('\n');
                    last = *pointers ? 1 : 0;
                    ++pointers;
                    break;
                case 'S':
                    UnicodeString(static_cast<const char16_t*>(*pointers), -1);
                    Char('\n');
                    last = *pointers ? 1 : 0;
                    ++pointers;
                    break;
                default:
                    break;
                }
                if (digits > 0) {
                    Hex(last, digits);
                    Char(' ');
                }
                if (length == -1 && last == 0) break;
            }
        }
        Char('[');
        Hex(length, 8);
        Char(']');
    }
};

std::int32_t FormatTrace(char* outBuf, std::int32_t capacity, std::int32_t indent, const char* fmt, VaList* list) {
    if (!fmt || (!outBuf && capacity > 0)) APS5_INVALID_ARG_EX;
    Arguments args{list};
    TraceOutput out{outBuf, capacity, indent};
    std::int64_t last = 0;
    std::size_t at = 0;
    for (;;) {
        char c = fmt[at++];
        if (c != '%') {
            out.Char(c);
            if (c == 0) break;
            continue;
        }
        c = fmt[at++];
        switch (c) {
        case 'c':
            out.Char(static_cast<char>(args.Int()));
            break;
        case 's':
            out.String(static_cast<const char*>(args.Pointer()));
            break;
        case 'S': {
            const auto* text = static_cast<const char16_t*>(args.Pointer());
            out.UnicodeString(text, args.Int());
            break;
        }
        case 'b':
            out.Hex(args.Int(), 2);
            break;
        case 'h':
            out.Hex(args.Int(), 4);
            break;
        case 'd':
            out.Hex(args.Int(), 8);
            break;
        case 'l':
            last = args.Long();
            out.Hex(last, 16);
            break;
        case 'p':
            out.Pointer(args.Pointer());
            break;
        case 0:
            out.Char('%');
            --at;
            break;
        case 'v': {
            const char type = fmt[at];
            if (type != 0) ++at;
            const void* data = args.Pointer();
            out.Vector(type, data, args.Int(), last);
            break;
        }
        default:
            out.Char(c);
            break;
        }
    }
    out.Char(0);
    return out.index + 1;
}

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


void APS5_VABI utrace_exit_nid_postfix(std::int32_t function, std::int32_t returnType, ...) {
    const Trace trace = CurrentTrace();
    if (!trace.exit) return;
    const char* format = ExitFormat(returnType);
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, returnType);
    trace.exit(trace.context, function, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, returnType);
    trace.exit(trace.context, function, format, reinterpret_cast<VaList*>(args));
    va_end(args);
#endif
}

void APS5_VABI utrace_data_nid_postfix(std::int32_t function, std::int32_t level, const char* fmt, ...) {
    const Trace trace = CurrentTrace();
    if (!trace.data) return;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, fmt);
    trace.data(trace.context, function, level, fmt, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, fmt);
    trace.data(trace.context, function, level, fmt, reinterpret_cast<VaList*>(args));
    va_end(args);
#endif
}

std::int32_t APS5_VABI utrace_vformat_nid_postfix(char* outBuf, std::int32_t capacity, std::int32_t indent, const char* fmt, VaList* args) {
    return FormatTrace(outBuf, capacity, indent, fmt, args);
}

std::int32_t APS5_VABI utrace_format_nid_postfix(char* outBuf, std::int32_t capacity, std::int32_t indent, const char* fmt, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, fmt);
    const std::int32_t length = FormatTrace(outBuf, capacity, indent, fmt, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, fmt);
    const std::int32_t length = FormatTrace(outBuf, capacity, indent, fmt, reinterpret_cast<VaList*>(args));
    va_end(args);
#endif
    return length;
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

void* APS5_VABI uprv_calloc_nid_postfix(std::size_t count, std::size_t size) {
    size *= count;
    void* memory = Allocate(size);
    if (memory) std::memset(memory, 0, size);
    return memory;
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

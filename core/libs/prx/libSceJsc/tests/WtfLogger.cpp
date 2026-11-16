#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using Jsc::WtfAbi::LoggerObserverVector;
using ObserversFunction = LoggerObserverVector* (APS5_VABI*)();

static_assert(std::is_same_v<decltype(&_ZN3WTF6Logger9observersEv), ObserversFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF Logger check failed at line %d\n", line);
        std::abort();
    }
}

LoggerObserverVector* Observers() {
    return _ZN3WTF6Logger9observersEv();
}

void TestLayout() {
    Check(sizeof(LoggerObserverVector) == 16, __LINE__);
    Check(offsetof(LoggerObserverVector, buffer) == 0, __LINE__);
    Check(offsetof(LoggerObserverVector, capacity) == 8, __LINE__);
    Check(offsetof(LoggerObserverVector, size) == 12, __LINE__);
}

void TestStartsEmpty() {
    LoggerObserverVector* observers = Observers();
    Check(observers != nullptr, __LINE__);
    Check(reinterpret_cast<std::uintptr_t>(observers) % alignof(LoggerObserverVector) == 0, __LINE__);
    Check(observers->buffer == nullptr, __LINE__);
    Check(observers->capacity == 0, __LINE__);
    Check(observers->size == 0, __LINE__);
}

void TestSameVectorOnEveryCall() {
    LoggerObserverVector* first = Observers();
    Check(Observers() == first, __LINE__);
    LoggerObserverVector* fromThread = nullptr;
    std::thread thread([&] { fromThread = Observers(); });
    thread.join();
    Check(fromThread == first, __LINE__);
}

void TestKeepsRegisteredObservers() {
    int firstObserver = 1;
    int secondObserver = 2;
    void* storage[4] = {};
    LoggerObserverVector* observers = Observers();
    observers->buffer = storage;
    observers->capacity = 4;
    observers->buffer[observers->size++] = &firstObserver;
    std::thread thread([&] {
        LoggerObserverVector* shared = Observers();
        shared->buffer[shared->size++] = &secondObserver;
    });
    thread.join();
    LoggerObserverVector* again = Observers();
    Check(again->buffer == storage, __LINE__);
    Check(again->capacity == 4, __LINE__);
    Check(again->size == 2, __LINE__);
    Check(again->buffer[0] == &firstObserver, __LINE__);
    Check(again->buffer[1] == &secondObserver, __LINE__);
    *again = {};
    Check(Observers()->buffer == nullptr && Observers()->size == 0, __LINE__);
}


using LogAlwaysVFunction = void (APS5_VABI*)(const char*, const void*);

static_assert(std::is_same_v<decltype(&WTFLogAlwaysV), LogAlwaysVFunction>);

#ifdef _WIN32
void APS5_VABI LogAlways(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    WTFLogAlwaysV(format, args);
    __builtin_sysv_va_end(args);
}

int DuplicateDescriptor(int descriptor) {
    return _dup(descriptor);
}

int ReplaceDescriptor(int source, int target) {
    return _dup2(source, target);
}

int CloseDescriptor(int descriptor) {
    return _close(descriptor);
}

int DescriptorOf(std::FILE* file) {
    return _fileno(file);
}
#else
void LogAlways(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    WTFLogAlwaysV(format, args);
    va_end(args);
}

int DuplicateDescriptor(int descriptor) {
    return dup(descriptor);
}

int ReplaceDescriptor(int source, int target) {
    return dup2(source, target);
}

int CloseDescriptor(int descriptor) {
    return close(descriptor);
}

int DescriptorOf(std::FILE* file) {
    return fileno(file);
}
#endif

template <typename Function>
std::string CaptureStandardError(Function function) {
    const char* path = "jsc_wtf_log_always_capture.txt";
    std::FILE* file = std::fopen(path, "w+b");
    Check(file != nullptr, __LINE__);
    std::fflush(stderr);
    const int saved = DuplicateDescriptor(DescriptorOf(stderr));
    Check(saved >= 0, __LINE__);
    Check(ReplaceDescriptor(DescriptorOf(file), DescriptorOf(stderr)) >= 0, __LINE__);
    function();
    std::fflush(stderr);
    Check(ReplaceDescriptor(saved, DescriptorOf(stderr)) >= 0, __LINE__);
    CloseDescriptor(saved);
    std::rewind(file);
    std::string output;
    char buffer[256];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) output.append(buffer, read);
    std::fclose(file);
    std::remove(path);
    return output;
}

template <typename Function>
bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestLogAlwaysV() {
    Check(CaptureStandardError([] { LogAlways("value %d %s", 42, "x"); }) == "value 42 x\n", __LINE__);
    Check(CaptureStandardError([] { LogAlways("line\n"); }) == "line\n", __LINE__);
    Check(CaptureStandardError([] { LogAlways(""); }) == "\n", __LINE__);
    Check(CaptureStandardError([] { LogAlways("%s", "tail\n"); }) == "tail\n\n", __LINE__);
    Check(CaptureStandardError([] { LogAlways("%u-%x-%.2f", 7u, 255u, 1.5); }) == "7-ff-1.50\n", __LINE__);
    Check(ThrowsInvalidArgument([] { LogAlways(nullptr); }), __LINE__);
}
} // namespace

int main() {
    TestLayout();
    TestStartsEmpty();
    TestSameVectorOnEveryCall();
    TestKeepsRegisteredObservers();
    TestLogAlwaysV();
    return 0;
}

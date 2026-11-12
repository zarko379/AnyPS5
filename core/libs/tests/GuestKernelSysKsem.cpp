#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Semaphore/include/Ksem.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Enoent = 2;
constexpr int Efault = 14;
constexpr int Ebusy = 16;
constexpr int Eexist = 17;
constexpr int Einval = 22;
constexpr int Enfile = 23;
constexpr int Enospc = 28;
constexpr int Eagain = 35;
constexpr int Etimedout = 60;
constexpr int Eoverflow = 84;
constexpr int Enametoolong = 63;
constexpr int OCreat = 0x0200;
constexpr int OExcl = 0x0800;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

KernelTimespec FromNow(std::chrono::milliseconds delay) {
    const auto when = std::chrono::system_clock::now() + delay;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(when.time_since_epoch()).count();
    return {nanoseconds / 1000000000, nanoseconds % 1000000000};
}

int Value(std::intptr_t id) {
    int value = -1;
    Require(ksem_getvalue_nid_postfix(id, &value) == 0);
    return value;
}

template <typename TFinished>
void WaitForRegisteredWaiters(std::intptr_t id, unsigned expected, TFinished finished) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        Require(!finished());
        if (KsemWaiterCountForTests_nid_no_patch(id) == expected) {
            Require(!finished());
            return;
        }
        Require(std::chrono::steady_clock::now() < limit);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Counting() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0x80000000u) == -1 && Error() == Einval);
    Require(ksem_init_nid_postfix(nullptr, 1) == -1 && Error() == Efault);
    Require(ksem_init_nid_postfix(&id, 2) == 0 && id >= 0);
    Require(Value(id) == 2);
    Require(ksem_trywait_nid_postfix(id) == 0 && ksem_wait_nid_postfix(id) == 0 && Value(id) == 0);
    Require(ksem_trywait_nid_postfix(id) == -1 && Error() == Eagain);
    Require(ksem_post_nid_postfix(id) == 0 && Value(id) == 1);
    Require(ksem_getvalue_nid_postfix(id, nullptr) == -1 && Error() == Efault);
    Require(ksem_getvalue_nid_postfix(id, reinterpret_cast<int*>(0x10)) == -1 && Error() == Efault);
    Require(Value(id) == 1);
    Require(ksem_trywait_nid_postfix(id) == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    Require(ksem_init_nid_postfix(&id, 0x7fffffffu) == 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Eoverflow);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void Timed() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    const KernelTimespec invalid{0, 1000000000};
    Require(ksem_timedwait_nid_postfix(id, &invalid) == -1 && Error() == Einval);
    const auto deadline = FromNow(std::chrono::milliseconds(150));
    const auto start = std::chrono::steady_clock::now();
    Require(ksem_timedwait_nid_postfix(id, &deadline) == -1 && Error() == Etimedout);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(100));
    Require(ksem_post_nid_postfix(id) == 0);
    const auto later = FromNow(std::chrono::seconds(5));
    Require(ksem_timedwait_nid_postfix(id, &later) == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void Blocking() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    std::atomic<int> result{-2};
    std::thread waiter([&] { result = ksem_wait_nid_postfix(id); });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(result == -2);
    Require(ksem_destroy_nid_postfix(id) == -1 && Error() == Ebusy);
    Require(ksem_post_nid_postfix(id) == 0);
    waiter.join();
    Require(result == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void AbsoluteTimeouts() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    for (const auto seconds : {std::int64_t{0}, std::numeric_limits<std::int64_t>::min()}) {
        const KernelTimespec expired{seconds, 0};
        Require(ksem_timedwait_nid_postfix(id, &expired) == -1 && Error() == Etimedout);
        Require(ksem_post_nid_postfix(id) == 0);
        Require(ksem_timedwait_nid_postfix(id, &expired) == 0 && Value(id) == 0);
    }
    const KernelTimespec future{std::numeric_limits<std::int64_t>::max(), 999999999};
    std::atomic<bool> completed{false};
    int result = -2;
    std::thread waiter([&] {
        result = ksem_timedwait_nid_postfix(id, &future);
        completed.store(true, std::memory_order_release);
    });
    WaitForRegisteredWaiters(id, 1, [&] { return completed.load(std::memory_order_acquire); });
    Require(ksem_post_nid_postfix(id) == 0);
    waiter.join();
    Require(result == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void TimedWakeup() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = FromNow(std::chrono::seconds(5));
    std::atomic<unsigned> completed{0};
    std::array<int, 2> results{};
    std::array<int, 2> errors{};
    std::array<std::thread, 2> waiters;
    for (std::size_t index = 0; index < waiters.size(); ++index) {
        waiters[index] = std::thread([&, index] {
            results[index] = ksem_timedwait_nid_postfix(id, &deadline);
            errors[index] = Error();
            completed.fetch_add(1, std::memory_order_release);
        });
    }
    WaitForRegisteredWaiters(id, 2, [&] { return completed.load(std::memory_order_acquire) != 0; });
    Require(ksem_post_nid_postfix(id) == 0);
    for (auto& waiter : waiters) waiter.join();
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(4950));
    Require((results[0] == 0 && results[1] == -1 && errors[1] == Etimedout) ||
            (results[1] == 0 && results[0] == -1 && errors[0] == Etimedout));
    Require(Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void Identifiers() {
    Require(ksem_post_nid_postfix(-1) == -1 && Error() == Einval);
    const auto path = "anyps5-ksem-stale-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    { std::ofstream stream(path); stream << "x"; }
    const int plain = open_nid_postfix(path.c_str(), 0, 0);
    Require(plain >= 0);
    Require(ksem_post_nid_postfix(plain) == -1 && Error() == Einval);
    Require(close_nid_postfix(plain) == 0);
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 1) == 0);
    Require(close_nid_postfix(static_cast<int>(id)) == 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    const int reused = open_nid_postfix(path.c_str(), 0, 0);
    Require(reused >= 0);
    Require(ksem_post_nid_postfix(reused) == -1 && Error() == Einval);
    Require(ksem_getvalue_nid_postfix(reused, nullptr) == -1 && Error() == Einval);
    Require(close_nid_postfix(reused) == 0);
    Require(ksem_init_nid_postfix(&id, 1) == 0);
    const int other = open_nid_postfix(path.c_str(), 0, 0);
    Require(other >= 0 && other != id);
    Require(dup2_nid_postfix(other, static_cast<int>(id)) == static_cast<int>(id));
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    Require(ksem_trywait_nid_postfix(id) == -1 && Error() == Einval);
    Require(close_nid_postfix(static_cast<int>(id)) == 0 && close_nid_postfix(other) == 0);
    std::filesystem::remove(path);
}

bool Throws(void (*call)(int), int argument) {
    try {
        call(argument);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void Duplicates() {
    std::intptr_t id = -1;
    std::intptr_t target = -1;
    Require(ksem_init_nid_postfix(&id, 1) == 0 && ksem_init_nid_postfix(&target, 1) == 0);
    Require(Throws([](int descriptor) { dup_nid_postfix(descriptor); }, static_cast<int>(id)));
    static int destination = -1;
    destination = static_cast<int>(target);
    Require(Throws([](int descriptor) { dup2_nid_postfix(descriptor, destination); }, static_cast<int>(id)));
    Require(dup2_nid_postfix(static_cast<int>(id), static_cast<int>(id)) == static_cast<int>(id));
    Require(Value(id) == 1 && Value(target) == 1);
    Require(ksem_destroy_nid_postfix(id) == 0 && ksem_destroy_nid_postfix(target) == 0);
}

void Limit() {
    std::intptr_t ids[30] = {};
    for (auto& id : ids) Require(ksem_init_nid_postfix(&id, 0) == 0);
    std::intptr_t extra = -1;
    Require(ksem_init_nid_postfix(&extra, 0) == -1 && Error() == Enospc && extra >= 0);
    Require(ksem_post_nid_postfix(extra) == -1 && Error() == Einval);
    Require(ksem_destroy_nid_postfix(ids[0]) == 0);
    Require(ksem_init_nid_postfix(&ids[0], 0) == 0);
    for (const auto id : ids) Require(ksem_destroy_nid_postfix(id) == 0);
}

bool OpenThrows(const char* name) {
    std::intptr_t id = -1;
    try {
        ksem_open_nid_postfix(&id, name, 0, 0, 0);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool UnlinkThrows(const char* name) {
    try {
        ksem_unlink_nid_postfix(name);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void NamedErrors() {
    std::intptr_t id = -1;
    Require(ksem_open_nid_postfix(&id, "/anyps5-errors", OCreat | 0x0002, 0600, 0) == -1 && Error() == Einval);
    Require(ksem_open_nid_postfix(&id, "/anyps5-errors", OCreat, 0600, 0x80000000u) == -1 && Error() == Einval);
    Require(ksem_open_nid_postfix(nullptr, "/anyps5-errors", OCreat, 0600, 0) == -1 && Error() == Efault);
    Require(ksem_open_nid_postfix(reinterpret_cast<std::intptr_t*>(0x10), "/anyps5-errors", OCreat, 0600, 0) == -1 && Error() == Efault);
    Require(ksem_open_nid_postfix(&id, nullptr, OCreat, 0600, 0) == -1 && Error() == Efault);
    const std::string longName = "/" + std::string(1023, 'a');
    Require(ksem_open_nid_postfix(&id, longName.c_str(), OCreat, 0600, 0) == -1 && Error() == Enametoolong);
    Require(ksem_open_nid_postfix(&id, "anyps5-errors", OCreat, 0600, 0) == -1 && Error() == Einval);
    Require(ksem_open_nid_postfix(&id, "", OCreat, 0600, 0) == -1 && Error() == Einval);
    Require(ksem_open_nid_postfix(&id, "/anyps5-errors", 0, 0600, 0) == -1 && Error() == Enoent && id >= 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    Require(ksem_unlink_nid_postfix("/anyps5-errors") == -1 && Error() == Enoent);
    Require(ksem_unlink_nid_postfix(nullptr) == -1 && Error() == Efault);
    Require(ksem_unlink_nid_postfix(longName.c_str()) == -1 && Error() == Enametoolong);
    Require(ksem_close_nid_postfix(-1) == -1 && Error() == Einval);
    std::intptr_t anonymous = -1;
    Require(ksem_init_nid_postfix(&anonymous, 0) == 0);
    Require(ksem_close_nid_postfix(anonymous) == -1 && Error() == Einval);
    Require(Value(anonymous) == 0);
    Require(ksem_destroy_nid_postfix(anonymous) == 0);
}

void Named() {
    std::intptr_t first = -1;
    std::intptr_t second = -1;
    std::intptr_t third = -1;
    Require(ksem_open_nid_postfix(&first, "/anyps5-named", OCreat | OExcl, 0600, 1) == 0 && first >= 0);
    Require(ksem_open_nid_postfix(&second, "/anyps5-named", 0, 0, 7) == 0 && second >= 0 && second != first);
    Require(ksem_open_nid_postfix(&third, "/anyps5-named", OCreat, 0600, 5) == 0 && third != first && third != second);
    Require(Value(first) == 1 && Value(second) == 1 && Value(third) == 1);
    Require(ksem_trywait_nid_postfix(second) == 0 && Value(first) == 0 && Value(third) == 0);
    Require(ksem_post_nid_postfix(third) == 0 && Value(first) == 1);
    std::intptr_t rejected = -1;
    Require(ksem_open_nid_postfix(&rejected, "/anyps5-named", OCreat | OExcl, 0600, 0) == -1 && Error() == Eexist);
    Require(ksem_destroy_nid_postfix(first) == -1 && Error() == Einval);
    Require(Value(first) == 1);
    Require(ksem_close_nid_postfix(third) == 0);
    Require(ksem_post_nid_postfix(third) == -1 && Error() == Einval);
    Require(ksem_close_nid_postfix(third) == -1 && Error() == Einval);
    Require(ksem_unlink_nid_postfix("/anyps5-named") == 0);
    Require(ksem_open_nid_postfix(&rejected, "/anyps5-named", 0, 0600, 0) == -1 && Error() == Enoent);
    Require(ksem_unlink_nid_postfix("/anyps5-named") == -1 && Error() == Enoent);
    Require(ksem_post_nid_postfix(first) == 0 && Value(second) == 2);
    std::intptr_t fresh = -1;
    Require(ksem_open_nid_postfix(&fresh, "/anyps5-named", OCreat, 0600, 4) == 0);
    Require(Value(fresh) == 4 && Value(first) == 2);
    Require(ksem_close_nid_postfix(first) == 0 && Value(second) == 2);
    Require(close_nid_postfix(static_cast<int>(second)) == 0);
    Require(ksem_unlink_nid_postfix("/anyps5-named") == 0);
    Require(ksem_close_nid_postfix(fresh) == 0);
}

void NamedLimit() {
    std::intptr_t named[15] = {};
    std::intptr_t anonymous[15] = {};
    for (std::size_t index = 0; index < 15; ++index) {
        const auto name = "/anyps5-limit-" + std::to_string(index);
        Require(ksem_open_nid_postfix(&named[index], name.c_str(), OCreat, 0600, 0) == 0);
        Require(ksem_init_nid_postfix(&anonymous[index], 0) == 0);
    }
    std::intptr_t extra = -1;
    Require(ksem_open_nid_postfix(&extra, "/anyps5-limit-extra", OCreat, 0600, 0) == -1 && Error() == Enfile);
    Require(ksem_init_nid_postfix(&extra, 0) == -1 && Error() == Enospc);
    std::intptr_t shared = -1;
    Require(ksem_open_nid_postfix(&shared, "/anyps5-limit-0", 0, 0, 0) == 0);
    Require(ksem_unlink_nid_postfix("/anyps5-limit-0") == 0);
    Require(ksem_close_nid_postfix(named[0]) == 0);
    Require(ksem_init_nid_postfix(&extra, 0) == -1 && Error() == Enospc);
    Require(ksem_close_nid_postfix(shared) == 0);
    Require(ksem_open_nid_postfix(&named[0], "/anyps5-limit-extra", OCreat, 0600, 0) == 0);
    Require(ksem_unlink_nid_postfix("/anyps5-limit-extra") == 0);
    for (std::size_t index = 0; index < 15; ++index) {
        Require(ksem_close_nid_postfix(named[index]) == 0);
        Require(ksem_destroy_nid_postfix(anonymous[index]) == 0);
        if (index == 0) continue;
        const auto name = "/anyps5-limit-" + std::to_string(index);
        Require(ksem_unlink_nid_postfix(name.c_str()) == 0);
    }
    std::intptr_t ids[30] = {};
    for (auto& id : ids) Require(ksem_init_nid_postfix(&id, 0) == 0);
    for (const auto id : ids) Require(ksem_destroy_nid_postfix(id) == 0);
}

void Permissions() {
    const auto previous = umask_nid_postfix(0200);
    std::intptr_t id = -1;
    Require(ksem_open_nid_postfix(&id, "/anyps5-masked", OCreat, 0600, 0) == 0);
    umask_nid_postfix(previous);
    Require(OpenThrows("/anyps5-masked"));
    Require(UnlinkThrows("/anyps5-masked"));
    Require(ksem_close_nid_postfix(id) == 0);
    std::intptr_t open = -1;
    Require(ksem_open_nid_postfix(&open, "/anyps5-open", OCreat, 0777, 0) == 0);
    Require(ksem_open_nid_postfix(&id, "/anyps5-open", 0, 0, 0) == 0);
    Require(ksem_close_nid_postfix(id) == 0 && ksem_close_nid_postfix(open) == 0);
    Require(ksem_unlink_nid_postfix("/anyps5-open") == 0);
}

}

class UnreadablePage {
public:
    UnreadablePage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
    }

    ~UnreadablePage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size);
#endif
    }

    UnreadablePage(const UnreadablePage&) = delete;
    UnreadablePage& operator=(const UnreadablePage&) = delete;

    void* Address() const { return memory; }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

void BadPointers() {
    UnreadablePage page;
    void* const bad[] = {reinterpret_cast<void*>(0x10), page.Address()};
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0 && id >= 0);
    for (void* pointer : bad) {
        Require(ksem_init_nid_postfix(static_cast<std::intptr_t*>(pointer), 1) == -1 && Error() == Efault);
        Require(ksem_timedwait_nid_postfix(id, static_cast<const KernelTimespec*>(pointer)) == -1 && Error() == Efault);
        Require(ksem_timedwait_nid_postfix(-1, static_cast<const KernelTimespec*>(pointer)) == -1 && Error() == Efault);
    }
    Require(ksem_destroy_nid_postfix(id) == 0);
    const auto openDescriptors = [] {
        std::intptr_t probe = -1;
        Require(ksem_init_nid_postfix(&probe, 0) == 0);
        const auto first = probe;
        Require(ksem_destroy_nid_postfix(probe) == 0);
        return first;
    };
    const auto before = openDescriptors();
    Require(ksem_init_nid_postfix(reinterpret_cast<std::intptr_t*>(0x10), 1) == -1 && Error() == Efault);
    Require(openDescriptors() == before);
    KernelTimespec valid = FromNow(std::chrono::milliseconds(1));
    Require(ksem_init_nid_postfix(&id, 1) == 0 && ksem_timedwait_nid_postfix(id, &valid) == 0 && ksem_destroy_nid_postfix(id) == 0);
}

int main() {
    BadPointers();
    Counting();
    Timed();
    AbsoluteTimeouts();
    TimedWakeup();
    Blocking();
    Identifiers();
    Duplicates();
    Limit();
    NamedErrors();
    Named();
    NamedLimit();
    Permissions();
    std::puts("ksem semaphores passed");
    return 0;
}

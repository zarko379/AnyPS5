#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

static constexpr int UMTX_OP_WAIT = 2;
static constexpr int UMTX_OP_WAKE = 3;
static constexpr int UMTX_OP_MUTEX_TRYLOCK = 4;
static constexpr int UMTX_OP_MUTEX_LOCK = 5;
static constexpr int UMTX_OP_MUTEX_UNLOCK = 6;
static constexpr int UMTX_OP_SET_CEILING = 7;
static constexpr int UMTX_OP_CV_WAIT = 8;
static constexpr int UMTX_OP_CV_SIGNAL = 9;
static constexpr int UMTX_OP_CV_BROADCAST = 10;
static constexpr int UMTX_OP_RW_RDLOCK = 12;
static constexpr int UMTX_OP_RW_WRLOCK = 13;
static constexpr int UMTX_OP_RW_UNLOCK = 14;
static constexpr int UMTX_OP_MUTEX_WAIT = 17;
static constexpr int UMTX_OP_MUTEX_WAKE = 18;
static constexpr int UMTX_OP_WAIT_UINT = 11;
static constexpr int UMTX_OP_WAIT_UINT_PRIVATE = 15;
static constexpr int UMTX_OP_WAKE_PRIVATE = 16;
static constexpr int UMTX_OP_SEM_WAIT = 19;
static constexpr int UMTX_OP_SEM_WAKE = 20;
static constexpr int UMTX_OP_NWAKE_PRIVATE = 21;
static constexpr int UMTX_OP_MUTEX_WAKE2 = 22;
static constexpr int UMTX_OP_SEM2_WAIT = 23;
static constexpr int UMTX_OP_SEM2_WAKE = 24;
static constexpr int UMTX_OP_SHM = 25;
static constexpr int UMTX_OP_MAX = 27;
static constexpr std::uint32_t UMTX_ABSTIME = 1;
static constexpr int CLOCK_MONOTONIC_ID = 4;

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EOPNOTSUPP = 45;
static constexpr int GUEST_ETIMEDOUT = 60;

static constexpr std::uint32_t CONTESTED = 0x80000000u;
static constexpr std::uint32_t PRIO_INHERIT = 0x4;
static constexpr std::uint32_t PRIO_PROTECT = 0x8;
static constexpr std::uint32_t PROCESS_SHARED = 0x1;
static constexpr std::uint32_t ROBUST = 0x10;
static constexpr std::uint32_t OWNERDEAD = CONTESTED | 0x10;
static constexpr std::uint32_t NOTRECOV = CONTESTED | 0x11;
static constexpr std::uint32_t WRITE_OWNER = 0x80000000u;
static constexpr std::uint32_t WRITE_WAITERS = 0x40000000u;
static constexpr std::uint32_t READ_WAITERS = 0x20000000u;
static constexpr std::uint32_t PREFER_READER = 0x2;
static constexpr std::uint32_t SEM2_HAS_WAITERS = 0x80000000u;

struct Umutex {
    std::uint32_t owner = 0;
    std::uint32_t flags = 0;
    std::uint32_t ceilings[2]{};
    std::uint32_t spare[4]{};
};

struct Ucond {
    std::uint32_t hasWaiters = 0;
    std::uint32_t flags = 0;
    std::uint32_t clockId = 0;
    std::uint32_t spare = 0;
};

struct Urwlock {
    std::uint32_t state = 0;
    std::uint32_t flags = 0;
    std::uint32_t blockedReaders = 0;
    std::uint32_t blockedWriters = 0;
    std::uint32_t spare[4]{};
};

static constexpr auto SETTLE = std::chrono::milliseconds(100);

struct UmtxTime {
    KernelTimespec timeout;
    std::uint32_t flags;
    std::uint32_t clockId;
};

static void Require(bool value) { if (!value) std::abort(); }

static KernelTimespec FailsafeTimeout() { return {10, 0}; }

static int Umtx(void* object, int operation, std::uint64_t value, void* sizeArgument = nullptr, void* timeArgument = nullptr) {
    *__error_nid_postfix() = 0;
    return _umtx_op_nid_postfix(object, operation, value, sizeArgument, timeArgument);
}

static bool FailsWith(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static void* SizeOf(std::size_t size) { return reinterpret_cast<void*>(size); }

static void AwaitCount(const std::atomic<int>& counter, int value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (counter.load() != value) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static void ReturnsWhenTheValueDiffers() {
    std::uint64_t value = 2;
    Require(Umtx(&value, UMTX_OP_WAIT, 1) == 0);
    KernelTimespec none{0, 0};
    Require(Umtx(&value, UMTX_OP_WAIT, 1, nullptr, &none) == 0);

    std::uint32_t word = 2;
    Require(Umtx(&word, UMTX_OP_WAIT_UINT, 1) == 0);
    Require(Umtx(&word, UMTX_OP_WAIT_UINT_PRIVATE, 1, nullptr, &none) == 0);
}

static void WaitComparesALongAndWaitUintAnInt() {
    std::uint64_t value = 0x100000001ULL;
    KernelTimespec none{0, 0};
    Require(Umtx(&value, UMTX_OP_WAIT, 1, nullptr, &none) == 0);
    Require(FailsWith(Umtx(&value, UMTX_OP_WAIT, 0x100000001ULL, nullptr, &none), GUEST_ETIMEDOUT));

    auto* low = reinterpret_cast<std::uint32_t*>(&value);
    Require(FailsWith(Umtx(low, UMTX_OP_WAIT_UINT, 1, nullptr, &none), GUEST_ETIMEDOUT));
    Require(Umtx(low, UMTX_OP_WAIT_UINT_PRIVATE, 0x700000001ULL, nullptr, &none) == 0);
    Require(Umtx(low, UMTX_OP_WAIT_UINT, 0x700000001ULL, nullptr, &none) == 0);
    std::uint32_t all = 0xffffffffu;
    Require(Umtx(&all, UMTX_OP_WAIT_UINT, 0xffffffffffffffffULL, nullptr, &none) == 0);
    Require(FailsWith(Umtx(&all, UMTX_OP_WAIT_UINT, 0xffffffffULL, nullptr, &none), GUEST_ETIMEDOUT));
}

static void TimesOutWhileTheValueMatches() {
    std::uint32_t word = 1;
    KernelTimespec none{0, 0};
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, nullptr, &none), GUEST_ETIMEDOUT));

    KernelTimespec relative{0, 20000000};
    auto start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(relative)), &relative), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));

    UmtxTime time{{0, 20000000}, 0, CLOCK_MONOTONIC_ID};
    start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(time)), &time), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));

    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(CLOCK_MONOTONIC_ID, &now) == 0);
    UmtxTime absolute{{now.tv_sec, now.tv_nsec + 20000000}, UMTX_ABSTIME, CLOCK_MONOTONIC_ID};
    if (absolute.timeout.tv_nsec >= 1000000000) {
        absolute.timeout.tv_sec += 1;
        absolute.timeout.tv_nsec -= 1000000000;
    }
    start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(absolute)), &absolute), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));

    UmtxTime past{{now.tv_sec, now.tv_nsec}, UMTX_ABSTIME, CLOCK_MONOTONIC_ID};
    start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(past)), &past), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
}

static void RejectsAnInvalidTimeout() {
    std::uint32_t word = 1;
    KernelTimespec negative{-1, 0};
    KernelTimespec tooManyNanos{0, 1000000000};
    for (const int operation : {UMTX_OP_WAIT, UMTX_OP_WAIT_UINT}) {
        Require(FailsWith(Umtx(&word, operation, 1, SizeOf(sizeof(negative)), &negative), GUEST_EINVAL));
        Require(FailsWith(Umtx(&word, operation, 1, SizeOf(sizeof(tooManyNanos)), &tooManyNanos), GUEST_EINVAL));
    }
    struct {
        std::uint32_t usecs = 20000;
        std::uint32_t zero32 = 0;
        std::int64_t zero64 = 0;
    } zeroNeighbours;
    auto start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, nullptr, &zeroNeighbours), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
    UmtxTime negativeNanos{{0, -1}, 0, CLOCK_MONOTONIC_ID};
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(negativeNanos)), &negativeNanos), GUEST_EINVAL));
}

static void WakeReleasesAWaiterWithoutAValueChange() {
    std::uint64_t value = 1;
    std::atomic<int> finished{0};
    int result = -2;
    std::thread waiter([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        result = Umtx(&value, UMTX_OP_WAIT, 1, SizeOf(sizeof(failsafe)), &failsafe);
        ++finished;
    });
    while (finished.load() == 0) {
        Require(Umtx(&value, UMTX_OP_WAKE, 1) == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    waiter.join();
    Require(result == 0);
}

static void WakeHonoursTheCountAndTheAddress() {
    std::uint32_t word = 1;
    std::uint32_t other = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> otherFinished{0};
    std::vector<int> results(4, -2);
    int otherResult = -2;

    std::vector<std::thread> waiters;
    for (int i = 0; i < 4; ++i) {
        waiters.emplace_back([&, i] {
            KernelTimespec failsafe = FailsafeTimeout();
            ++started;
            results[i] = Umtx(&word, UMTX_OP_WAIT_UINT_PRIVATE, 1, SizeOf(sizeof(failsafe)), &failsafe);
            ++finished;
        });
    }
    std::thread otherWaiter([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        ++started;
        otherResult = Umtx(&other, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(failsafe)), &failsafe);
        ++otherFinished;
    });
    AwaitCount(started, 5);
    std::this_thread::sleep_for(SETTLE);

    Require(Umtx(&word, UMTX_OP_WAKE_PRIVATE, 1) == 0);
    AwaitCount(finished, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);

    Require(Umtx(&word, UMTX_OP_WAKE, 0) == 0);
    AwaitCount(finished, 2);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 2);

    Require(Umtx(&word, UMTX_OP_WAKE, INT_MAX) == 0);
    AwaitCount(finished, 4);
    std::this_thread::sleep_for(SETTLE);
    Require(otherFinished.load() == 0);

    Require(Umtx(&other, UMTX_OP_WAKE_PRIVATE, 1) == 0);
    for (auto& waiter : waiters) waiter.join();
    otherWaiter.join();
    for (const int result : results) Require(result == 0);
    Require(otherResult == 0);
}

static void WakeWithoutWaitersSucceeds() {
    std::uint32_t word = 0;
    Require(Umtx(&word, UMTX_OP_WAKE, 1) == 0);
    Require(Umtx(&word, UMTX_OP_WAKE_PRIVATE, INT_MAX) == 0);
}

static void NwakeWakesEveryWaiterOnEachListedAddress() {
    std::uint32_t first = 1;
    std::uint64_t second = 1;
    std::uint32_t unlisted = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> unlistedFinished{0};
    std::vector<int> results(4, -2);
    int unlistedResult = -2;

    std::vector<std::thread> waiters;
    for (int i = 0; i < 3; ++i) {
        waiters.emplace_back([&, i] {
            KernelTimespec failsafe = FailsafeTimeout();
            ++started;
            results[i] = Umtx(&first, UMTX_OP_WAIT_UINT_PRIVATE, 1, SizeOf(sizeof(failsafe)), &failsafe);
            ++finished;
        });
    }
    waiters.emplace_back([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        ++started;
        results[3] = Umtx(&second, UMTX_OP_WAIT, 1, SizeOf(sizeof(failsafe)), &failsafe);
        ++finished;
    });
    std::thread unlistedWaiter([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        ++started;
        unlistedResult = Umtx(&unlisted, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(failsafe)), &failsafe);
        ++unlistedFinished;
    });
    AwaitCount(started, 5);
    std::this_thread::sleep_for(SETTLE);

    void* addresses[] = {&first, nullptr, &second};
    Require(Umtx(addresses, UMTX_OP_NWAKE_PRIVATE, 3) == 0);
    AwaitCount(finished, 4);
    std::this_thread::sleep_for(SETTLE);
    Require(unlistedFinished.load() == 0);

    Require(Umtx(&unlisted, UMTX_OP_WAKE_PRIVATE, 1) == 0);
    for (auto& waiter : waiters) waiter.join();
    unlistedWaiter.join();
    for (const int result : results) Require(result == 0);
    Require(unlistedResult == 0);
}

static void NwakeReadsOnlyTheCountedEntries() {
    Require(Umtx(nullptr, UMTX_OP_NWAKE_PRIVATE, 0) == 0);
    Require(Umtx(nullptr, UMTX_OP_NWAKE_PRIVATE, 0xffffffff00000000ULL) == 0);
    Require(Umtx(nullptr, UMTX_OP_NWAKE_PRIVATE, 0xffffffffULL) == 0);
    Require(FailsWith(Umtx(nullptr, UMTX_OP_NWAKE_PRIVATE, 1), GUEST_EFAULT));
    std::uint32_t word = 0;
    void* addresses[] = {&word};
    Require(Umtx(addresses, UMTX_OP_NWAKE_PRIVATE, 1) == 0);
}

struct GuestSemaphore {
    std::uint32_t hasWaiters;
    std::uint32_t count;
    std::uint32_t flags;
};

static void SemWaitReturnsAtOnceWhileTheCountIsNonzero() {
    GuestSemaphore sem{0, 2, 0};
    KernelTimespec none{0, 0};
    Require(Umtx(&sem, UMTX_OP_SEM_WAIT, 0, SizeOf(sizeof(none)), &none) == 0);
    Require(sem.hasWaiters == 1);
    Require(sem.count == 2);
}

static void SemWaitTimesOutWhileTheCountIsZero() {
    GuestSemaphore sem{0, 0, 0};
    KernelTimespec relative{0, 20000000};
    auto start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&sem, UMTX_OP_SEM_WAIT, 0, SizeOf(sizeof(relative)), &relative), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    Require(sem.hasWaiters == 1);
}

static void SemWakeReleasesTheWaiterAndClearsTheFlag() {
    GuestSemaphore sem{0, 0, 0};
    std::atomic<int> finished{0};
    int result = -2;
    std::thread waiter([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        result = Umtx(&sem, UMTX_OP_SEM_WAIT, 0, SizeOf(sizeof(failsafe)), &failsafe);
        ++finished;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::atomic_ref<std::uint32_t>(sem.hasWaiters).load() == 0) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 0);

    std::atomic_ref<std::uint32_t>(sem.count).store(1);
    Require(Umtx(&sem, UMTX_OP_SEM_WAKE, 0) == 0);
    waiter.join();
    Require(result == 0);
    Require(sem.hasWaiters == 0);
}

static void SemWakeWithoutWaitersSucceeds() {
    GuestSemaphore sem{0, 0, 0};
    Require(Umtx(&sem, UMTX_OP_SEM_WAKE, 0) == 0);
    Require(sem.hasWaiters == 0);
    Require(FailsWith(Umtx(nullptr, UMTX_OP_SEM_WAKE, 0), GUEST_EFAULT));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_SEM_WAIT, 0), GUEST_EFAULT));
}

struct Usem2 {
    std::uint32_t count = 0;
    std::uint32_t flags = 0;
};

static std::uint32_t Sem2Count(Usem2& sem) { return std::atomic_ref<std::uint32_t>(sem.count).load(); }

static void Sem2WaitReturnsWithoutTakingACount() {
    for (const std::uint32_t count : {1u, 2u, 0x7fffffffu, SEM2_HAS_WAITERS | 1u}) {
        Usem2 sem{count, 0};
        KernelTimespec none{0, 0};
        Require(Umtx(&sem, UMTX_OP_SEM2_WAIT, 0, SizeOf(sizeof(none)), &none) == 0);
        Require(sem.count == count);
        Require(Umtx(&sem, UMTX_OP_SEM2_WAIT, 0) == 0 && sem.count == count);
    }
}

static void Sem2WaitSetsTheWaitersBitAndTimesOut() {
    for (const std::uint32_t count : {0u, SEM2_HAS_WAITERS}) {
        Usem2 sem{count, 0};
        KernelTimespec relative{0, 20000000};
        const auto start = std::chrono::steady_clock::now();
        Require(FailsWith(Umtx(&sem, UMTX_OP_SEM2_WAIT, 0, SizeOf(sizeof(relative)), &relative), GUEST_ETIMEDOUT));
        Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
        Require(sem.count == SEM2_HAS_WAITERS && sem.flags == 0);
    }
}

static void Sem2ChecksTheTimeoutBeforeTheObject() {
    Usem2 sem;
    KernelTimespec negative{-1, 0};
    KernelTimespec tooManyNanos{0, 1000000000};
    Require(FailsWith(Umtx(&sem, UMTX_OP_SEM2_WAIT, 0, SizeOf(sizeof(negative)), &negative), GUEST_EINVAL) && sem.count == 0);
    Require(FailsWith(Umtx(&sem, UMTX_OP_SEM2_WAIT, 0, SizeOf(sizeof(tooManyNanos)), &tooManyNanos), GUEST_EINVAL) && sem.count == 0);
    Require(FailsWith(Umtx(nullptr, UMTX_OP_SEM2_WAIT, 0, SizeOf(sizeof(negative)), &negative), GUEST_EINVAL));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_SEM2_WAIT, 0), GUEST_EFAULT));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_SEM2_WAKE, 0), GUEST_EFAULT));
}

static void Sem2WakeWithoutWaitersLeavesTheCount() {
    for (const std::uint32_t count : {0u, 3u, SEM2_HAS_WAITERS, SEM2_HAS_WAITERS | 3u}) {
        Usem2 sem{count, 0};
        Require(Umtx(&sem, UMTX_OP_SEM2_WAKE, 0) == 0 && sem.count == count);
    }
}

static void AwaitSem2Waiters(Usem2& sem, const std::atomic<int>& finished) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (Sem2Count(sem) != SEM2_HAS_WAITERS) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 0);
}

static void Sem2WakeReleasesOneWaiterAndClearsTheBitForTheLast() {
    Usem2 sem;
    std::atomic<int> finished{0};
    std::atomic<int> failures{0};
    std::atomic<int> entered{0};
    std::vector<std::thread> waiters;
    for (int index = 0; index < 2; ++index) {
        waiters.emplace_back([&] {
            KernelTimespec failsafe = FailsafeTimeout();
            ++entered;
            if (Umtx(&sem, UMTX_OP_SEM2_WAIT, 0, SizeOf(sizeof(failsafe)), &failsafe) != 0) ++failures;
            ++finished;
        });
    }
    AwaitCount(entered, 2);
    AwaitSem2Waiters(sem, finished);
    std::atomic_ref<std::uint32_t>(sem.count).store(SEM2_HAS_WAITERS | 1u);
    Require(Umtx(&sem, UMTX_OP_SEM2_WAKE, 0) == 0);
    AwaitCount(finished, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1 && Sem2Count(sem) == (SEM2_HAS_WAITERS | 1u));
    Require(Umtx(&sem, UMTX_OP_SEM2_WAKE, 0) == 0);
    AwaitCount(finished, 2);
    for (auto& waiter : waiters) waiter.join();
    Require(failures.load() == 0 && sem.count == 1u);
}

static void RejectsBadObjectsAndOperations() {
    Require(FailsWith(Umtx(nullptr, UMTX_OP_WAIT, 0), GUEST_EFAULT));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_WAIT_UINT, 0), GUEST_EFAULT));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_WAKE, 1), GUEST_EFAULT));
    Require(Umtx(nullptr, UMTX_OP_WAKE_PRIVATE, 1) == 0);

    std::uint64_t value = 0;
    Require(FailsWith(Umtx(&value, 0, 0), GUEST_EOPNOTSUPP));
    Require(FailsWith(Umtx(&value, UMTX_OP_MAX, 0), GUEST_EINVAL));
    Require(FailsWith(Umtx(&value, -1, 0), GUEST_EINVAL));

    for (const std::uint64_t owner : {std::uint64_t{0}, std::uint64_t{123456}, (std::uint64_t{1} << 63) | 123456}) {
        std::uint64_t word = owner;
        Require(FailsWith(Umtx(&word, 1, 123456), GUEST_EOPNOTSUPP));
        Require(FailsWith(Umtx(&word, 0, 123456), GUEST_EOPNOTSUPP));
        Require(word == owner);
    }

    Require(FailsWith(Umtx(&value, 7, 0), GUEST_EINVAL));
    bool threw = false;
    try { Umtx(&value, UMTX_OP_SHM, 0); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw);
}

static std::uint32_t Self() { return static_cast<std::uint32_t>(scePthreadGetthreadid()); }

static UmtxTime ShortTimeout() { return {{0, 1000000}, 0, CLOCK_MONOTONIC_ID}; }

static void MutexOwnershipFollowsTheConsole() {
    Umutex mutex;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_TRYLOCK, 0) == 0 && mutex.owner == Self());
    Require(Umtx(&mutex, UMTX_OP_MUTEX_TRYLOCK, 0) == 0 && mutex.owner == Self());
    UmtxTime timeout = ShortTimeout();
    Require(Umtx(&mutex, UMTX_OP_MUTEX_LOCK, 0, SizeOf(sizeof(timeout)), &timeout) == 0 && mutex.owner == Self());
    Require(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0) == 0 && mutex.owner == 0);
    Require(FailsWith(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0), GUEST_EPERM) && mutex.owner == 0);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_LOCK, 0) == 0 && mutex.owner == Self());
    Require(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0) == 0);

    mutex.owner = 12345;
    Require(FailsWith(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0), GUEST_EPERM) && mutex.owner == 12345);
    Require(FailsWith(Umtx(&mutex, UMTX_OP_MUTEX_TRYLOCK, 0), GUEST_EBUSY) && mutex.owner == 12345);
    timeout = ShortTimeout();
    Require(FailsWith(Umtx(&mutex, UMTX_OP_MUTEX_LOCK, 0, SizeOf(sizeof(timeout)), &timeout), GUEST_ETIMEDOUT) && mutex.owner == (CONTESTED | 12345));
    timeout = ShortTimeout();
    Require(FailsWith(Umtx(&mutex, UMTX_OP_MUTEX_WAIT, 0, SizeOf(sizeof(timeout)), &timeout), GUEST_ETIMEDOUT));

    mutex.owner = 0;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAIT, 0) == 0 && mutex.owner == 0);
    mutex.owner = CONTESTED;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE, 0) == 0 && mutex.owner == 0);
    mutex.owner = CONTESTED;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_TRYLOCK, 0) == 0 && mutex.owner == (CONTESTED | Self()));
    Require(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0) == 0 && mutex.owner == 0);

    Umutex inherit;
    inherit.flags = PRIO_INHERIT;
    Require(Umtx(&inherit, UMTX_OP_MUTEX_TRYLOCK, 0) == 0 && inherit.owner == Self());
    Require(Umtx(&inherit, UMTX_OP_MUTEX_UNLOCK, 0) == 0 && inherit.owner == 0);
    Umutex protect;
    protect.flags = PRIO_PROTECT;
    Require(FailsWith(Umtx(&protect, UMTX_OP_MUTEX_TRYLOCK, 0), GUEST_EINVAL) && protect.owner == 0);
    Require(FailsWith(Umtx(&mutex, UMTX_OP_SET_CEILING, 0), GUEST_EINVAL));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_MUTEX_TRYLOCK, 0), GUEST_EFAULT));
}

static void MutexLockWaitsForTheOwnerToUnlock() {
    Umutex mutex;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_LOCK, 0) == 0);
    std::atomic<int> state{0};
    std::thread waiter([&] {
        state.store(1);
        Require(Umtx(&mutex, UMTX_OP_MUTEX_LOCK, 0) == 0);
        state.store(2);
        Require(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0) == 0);
    });
    AwaitCount(state, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(state.load() == 1 && (std::atomic_ref<std::uint32_t>(mutex.owner).load() & CONTESTED) != 0);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_UNLOCK, 0) == 0);
    AwaitCount(state, 2);
    waiter.join();
    Require(mutex.owner == 0);
}

static void Wake2ChecksTheProtocolAndTheObject() {
    for (const std::uint32_t flags : {PRIO_INHERIT | PRIO_PROTECT, PRIO_INHERIT | PRIO_PROTECT | ROBUST | PROCESS_SHARED}) {
        Umutex mutex;
        mutex.owner = CONTESTED;
        Require(FailsWith(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, flags), GUEST_EINVAL) && mutex.owner == CONTESTED);
        Require(FailsWith(Umtx(nullptr, UMTX_OP_MUTEX_WAKE2, flags), GUEST_EINVAL));
    }
    for (const std::uint32_t flags : {0u, ROBUST, PRIO_INHERIT, PRIO_PROTECT, PRIO_INHERIT | ROBUST})
        Require(Umtx(nullptr, UMTX_OP_MUTEX_WAKE2, flags) == 0);
    for (const std::uint32_t flags : {PROCESS_SHARED, PROCESS_SHARED | PRIO_PROTECT, PROCESS_SHARED | ROBUST}) {
        Require(FailsWith(Umtx(nullptr, UMTX_OP_MUTEX_WAKE2, flags), GUEST_EFAULT));
        Require(FailsWith(Umtx(reinterpret_cast<void*>(0x10), UMTX_OP_MUTEX_WAKE2, flags), GUEST_EFAULT));
    }
    Require(Umtx(nullptr, UMTX_OP_MUTEX_WAKE2, std::uint64_t{PROCESS_SHARED} << 32) == 0);
}

static void Wake2WithoutWaitersWritesNothing() {
    for (const std::uint32_t owner : {0u, CONTESTED, 12345u, CONTESTED | 12345u, OWNERDEAD}) {
        for (const std::uint32_t flags : {0u, PROCESS_SHARED, PRIO_INHERIT, PRIO_PROTECT, ROBUST}) {
            Umutex mutex;
            mutex.owner = owner;
            mutex.flags = PRIO_PROTECT;
            Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, flags) == 0 && mutex.owner == owner && mutex.flags == PRIO_PROTECT);
        }
    }
}

static void StartMutexWaiter(Umutex& mutex, std::atomic<int>& finished, std::atomic<int>& failures, std::vector<std::thread>& waiters, std::atomic<int>& entered) {
    waiters.emplace_back([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        ++entered;
        if (Umtx(&mutex, UMTX_OP_MUTEX_WAIT, 0, SizeOf(sizeof(failsafe)), &failsafe) != 0) ++failures;
        ++finished;
    });
}

static void AwaitContested(Umutex& mutex) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((std::atomic_ref<std::uint32_t>(mutex.owner).load() & CONTESTED) == 0) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(SETTLE);
}

static void SetOwner(Umutex& mutex, std::uint32_t owner) { std::atomic_ref<std::uint32_t>(mutex.owner).store(owner); }
static std::uint32_t Owner(Umutex& mutex) { return std::atomic_ref<std::uint32_t>(mutex.owner).load(); }

static void Wake2WithOneWaiterMarksAnOwnedMutexAndWakesForAFreeOne() {
    Umutex mutex;
    mutex.owner = 12345;
    std::atomic<int> finished{0};
    std::atomic<int> failures{0};
    std::atomic<int> entered{0};
    std::vector<std::thread> waiters;
    StartMutexWaiter(mutex, finished, failures, waiters, entered);
    AwaitContested(mutex);
    SetOwner(mutex, 12345);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, 0) == 0 && Owner(mutex) == (CONTESTED | 12345));
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, 0) == 0 && Owner(mutex) == (CONTESTED | 12345));
    SetOwner(mutex, 0);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, PRIO_PROTECT) == 0 && Owner(mutex) == 0);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 0);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, ROBUST) == 0);
    AwaitCount(finished, 1);
    for (auto& waiter : waiters) waiter.join();
    Require(failures.load() == 0 && mutex.owner == 0);
}

static void Wake2WithTwoWaitersMarksTheMutexAndWakesOne() {
    Umutex mutex;
    mutex.owner = 12345;
    std::atomic<int> finished{0};
    std::atomic<int> failures{0};
    std::atomic<int> entered{0};
    std::vector<std::thread> waiters;
    StartMutexWaiter(mutex, finished, failures, waiters, entered);
    StartMutexWaiter(mutex, finished, failures, waiters, entered);
    AwaitCount(entered, 2);
    AwaitContested(mutex);
    SetOwner(mutex, 0);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, PRIO_INHERIT) == 0 && Owner(mutex) == CONTESTED);
    AwaitCount(finished, 1);
    SetOwner(mutex, 12345);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, 0) == 0 && Owner(mutex) == (CONTESTED | 12345));
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);
    SetOwner(mutex, OWNERDEAD + 2);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, 0) == 0 && Owner(mutex) == OWNERDEAD + 2);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);
    SetOwner(mutex, NOTRECOV);
    Require(Umtx(&mutex, UMTX_OP_MUTEX_WAKE2, 0) == 0);
    AwaitCount(finished, 2);
    for (auto& waiter : waiters) waiter.join();
    Require(failures.load() == 0 && mutex.owner == NOTRECOV);

    Umutex dead;
    dead.owner = 12345;
    waiters.clear();
    finished = 0;
    StartMutexWaiter(dead, finished, failures, waiters, entered);
    AwaitContested(dead);
    SetOwner(dead, OWNERDEAD);
    Require(Umtx(&dead, UMTX_OP_MUTEX_WAKE2, 0) == 0);
    AwaitCount(finished, 1);
    for (auto& waiter : waiters) waiter.join();
    Require(failures.load() == 0 && dead.owner == OWNERDEAD);
}

static void ConditionWaitReleasesTheMutexAndWakes() {
    Ucond condition;
    Require(Umtx(&condition, UMTX_OP_CV_SIGNAL, 0) == 0 && condition.hasWaiters == 0);
    Require(Umtx(&condition, UMTX_OP_CV_BROADCAST, 0) == 0 && condition.hasWaiters == 0);

    Umutex mutex;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_TRYLOCK, 0) == 0);
    KernelTimespec timeout{0, 1000000};
    Require(FailsWith(Umtx(&condition, UMTX_OP_CV_WAIT, 0, &mutex, &timeout), GUEST_ETIMEDOUT));
    Require(condition.hasWaiters == 0 && mutex.owner == 0);
    Require(FailsWith(Umtx(&condition, UMTX_OP_CV_WAIT, 0, &mutex, &timeout), GUEST_EPERM) && condition.hasWaiters == 0);

    std::atomic<int> state{0};
    std::vector<std::thread> waiters;
    for (int index = 0; index < 2; ++index) {
        waiters.emplace_back([&] {
            Umutex own;
            Require(Umtx(&own, UMTX_OP_MUTEX_TRYLOCK, 0) == 0);
            KernelTimespec failsafe = FailsafeTimeout();
            state.fetch_add(1);
            Require(Umtx(&condition, UMTX_OP_CV_WAIT, 0, &own, &failsafe) == 0 && own.owner == 0);
            state.fetch_add(10);
        });
    }
    AwaitCount(state, 2);
    std::this_thread::sleep_for(SETTLE);
    Require(condition.hasWaiters == 1);
    Require(Umtx(&condition, UMTX_OP_CV_SIGNAL, 0) == 0);
    AwaitCount(state, 12);
    Require(condition.hasWaiters == 1);
    Require(Umtx(&condition, UMTX_OP_CV_BROADCAST, 0) == 0);
    AwaitCount(state, 22);
    for (auto& waiter : waiters) waiter.join();
    Require(condition.hasWaiters == 0);
}

static void RwlockStateFollowsTheConsole() {
    Urwlock rwlock;
    Require(Umtx(&rwlock, UMTX_OP_RW_RDLOCK, 0) == 0 && rwlock.state == 1);
    UmtxTime timeout = ShortTimeout();
    Require(FailsWith(Umtx(&rwlock, UMTX_OP_RW_WRLOCK, 0, SizeOf(sizeof(timeout)), &timeout), GUEST_ETIMEDOUT));
    Require(rwlock.state == 1 && rwlock.blockedWriters == 0);
    Require(Umtx(&rwlock, UMTX_OP_RW_RDLOCK, 0) == 0 && rwlock.state == 2);
    Require(Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0) == 0 && Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0) == 0 && rwlock.state == 0);

    Require(Umtx(&rwlock, UMTX_OP_RW_WRLOCK, 0) == 0 && rwlock.state == WRITE_OWNER);
    timeout = ShortTimeout();
    Require(FailsWith(Umtx(&rwlock, UMTX_OP_RW_RDLOCK, 0, SizeOf(sizeof(timeout)), &timeout), GUEST_ETIMEDOUT));
    Require(rwlock.state == (WRITE_OWNER | READ_WAITERS));
    Require(Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0) == 0 && rwlock.state == READ_WAITERS);
    rwlock.state = 0;
    Require(FailsWith(Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0), GUEST_EPERM) && rwlock.state == 0);
    Require(FailsWith(Umtx(nullptr, UMTX_OP_RW_RDLOCK, 0), GUEST_EFAULT));
}

static void RwlockReaderPreferenceFollowsTheConsole() {
    const auto readLock = [](std::uint32_t state, std::uint32_t flags, std::uint64_t value, std::uint32_t& after) {
        Urwlock rwlock;
        rwlock.state = state;
        rwlock.flags = flags;
        UmtxTime timeout = ShortTimeout();
        const int result = Umtx(&rwlock, UMTX_OP_RW_RDLOCK, value, SizeOf(sizeof(timeout)), &timeout);
        after = rwlock.state;
        return result;
    };
    std::uint32_t after = 0;
    Require(FailsWith(readLock(WRITE_WAITERS, 0, 0, after), GUEST_ETIMEDOUT) && after == (WRITE_WAITERS | READ_WAITERS));
    Require(readLock(WRITE_WAITERS, 0, PREFER_READER, after) == 0 && after == (WRITE_WAITERS | 1));
    Require(readLock(WRITE_WAITERS, PREFER_READER, 0, after) == 0 && after == (WRITE_WAITERS | 1));
    Require(FailsWith(readLock(WRITE_OWNER, 0, PREFER_READER, after), GUEST_ETIMEDOUT) && after == (WRITE_OWNER | READ_WAITERS));
}

static void RwlockUnlockWakesTheBlockedWriter() {
    Urwlock rwlock;
    Require(Umtx(&rwlock, UMTX_OP_RW_RDLOCK, 0) == 0);
    std::atomic<int> state{0};
    std::thread writer([&] {
        state.store(1);
        Require(Umtx(&rwlock, UMTX_OP_RW_WRLOCK, 0) == 0);
        state.store(2);
        Require(Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0) == 0);
    });
    AwaitCount(state, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(state.load() == 1 && (std::atomic_ref<std::uint32_t>(rwlock.state).load() & WRITE_WAITERS) != 0);
    Require(Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0) == 0);
    AwaitCount(state, 2);
    writer.join();
    Require(rwlock.state == 0 && rwlock.blockedWriters == 0);
}

static void RwlockDoesNotLoseAWakeupUnderContention() {
    Urwlock rwlock;
    constexpr int threadCount = 8;
    constexpr int rounds = 6000;
    std::vector<std::thread> workers;
    for (int index = 0; index < threadCount; ++index) {
        workers.emplace_back([&, index] {
            for (int round = 0; round < rounds; ++round) {
                const bool write = (round % threadCount) == index % threadCount;
                KernelTimespec failsafe = FailsafeTimeout();
                Require(Umtx(&rwlock, write ? UMTX_OP_RW_WRLOCK : UMTX_OP_RW_RDLOCK, 0, SizeOf(sizeof(failsafe)), &failsafe) == 0);
                Require(Umtx(&rwlock, UMTX_OP_RW_UNLOCK, 0) == 0);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(rwlock.state == 0 && rwlock.blockedReaders == 0 && rwlock.blockedWriters == 0);
}

static void ConditionWaitRejectsAPriorityProtectedMutexWithoutLeakingAWaiter() {
    Ucond condition;
    Umutex protect;
    protect.flags = PRIO_PROTECT;
    KernelTimespec timeout{0, 1000000};
    try {
        Umtx(&condition, UMTX_OP_CV_WAIT, 0, &protect, &timeout);
    } catch (const std::runtime_error&) {
    }
    Require(condition.hasWaiters == 0);

    Umutex mutex;
    Require(Umtx(&mutex, UMTX_OP_MUTEX_TRYLOCK, 0) == 0);
    Require(FailsWith(Umtx(&condition, UMTX_OP_CV_WAIT, 0, &mutex, &timeout), GUEST_ETIMEDOUT));
    Require(condition.hasWaiters == 0);
}

int main() {
    ReturnsWhenTheValueDiffers();
    WaitComparesALongAndWaitUintAnInt();
    TimesOutWhileTheValueMatches();
    RejectsAnInvalidTimeout();
    WakeReleasesAWaiterWithoutAValueChange();
    WakeHonoursTheCountAndTheAddress();
    WakeWithoutWaitersSucceeds();
    NwakeWakesEveryWaiterOnEachListedAddress();
    NwakeReadsOnlyTheCountedEntries();
    SemWaitReturnsAtOnceWhileTheCountIsNonzero();
    SemWaitTimesOutWhileTheCountIsZero();
    SemWakeReleasesTheWaiterAndClearsTheFlag();
    SemWakeWithoutWaitersSucceeds();
    Sem2WaitReturnsWithoutTakingACount();
    Sem2WaitSetsTheWaitersBitAndTimesOut();
    Sem2ChecksTheTimeoutBeforeTheObject();
    Sem2WakeWithoutWaitersLeavesTheCount();
    Sem2WakeReleasesOneWaiterAndClearsTheBitForTheLast();
    RejectsBadObjectsAndOperations();
    MutexOwnershipFollowsTheConsole();
    MutexLockWaitsForTheOwnerToUnlock();
    Wake2ChecksTheProtocolAndTheObject();
    Wake2WithoutWaitersWritesNothing();
    Wake2WithOneWaiterMarksAnOwnedMutexAndWakesForAFreeOne();
    Wake2WithTwoWaitersMarksTheMutexAndWakesOne();
    ConditionWaitReleasesTheMutexAndWakes();
    RwlockStateFollowsTheConsole();
    RwlockUnlockWakesTheBlockedWriter();
    RwlockReaderPreferenceFollowsTheConsole();
    RwlockDoesNotLoseAWakeupUnderContention();
    ConditionWaitRejectsAPriorityProtectedMutexWithoutLeakingAWaiter();
}

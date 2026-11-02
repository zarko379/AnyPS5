#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

static constexpr int SCOPE_PROCESS = 0;
static constexpr int SCOPE_SYSTEM = 2;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ERANGE = 34;
static constexpr std::size_t CPUSET_BYTES = 32;
static constexpr KernelCpumask DEFAULT_AFFINITY = 0x1FFF;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr int DEFAULT_CEILING = 767;
static constexpr int SET_CEILING = 300;
static constexpr int LATER_CEILING = 400;
static constexpr int MUTEX_RECURSIVE = 2;

using Cpuset = std::array<std::uint8_t, CPUSET_BYTES>;
using WideCpuset = std::array<std::uint8_t, CPUSET_BYTES + 8>;

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), static_cast<unsigned>(location.line()));
    std::abort();
}

template<typename TException, typename TCall>
static bool ThrowsImplemented(TCall call) {
    const auto unimplemented = NotImplementedCalls_nid_no_patch();
    try {
        static_cast<void>(call());
    } catch (const TException&) {
        return NotImplementedCalls_nid_no_patch() == unimplemented;
    }
    return false;
}

static void APS5_VABI Prepare() {}
static void APS5_VABI Parent() {}
static void APS5_VABI Child() {}

static void* APS5_VABI ReportAffinity(void* affinity) {
    Require(scePthreadGetaffinity(scePthreadSelf(), static_cast<KernelCpumask*>(affinity)) == 0);
    return nullptr;
}

struct OwnerProbe {
    PthreadMutex* mutex;
    int owned;
};

static void* APS5_VABI ReportOwned(void* probe) {
    auto* report = static_cast<OwnerProbe*>(probe);
    report->owned = scePthreadMutexIsowned(report->mutex);
    return nullptr;
}

static Cpuset MaskSet(KernelCpumask mask) {
    Cpuset set{};
    std::memcpy(set.data(), &mask, sizeof(mask));
    return set;
}

static Cpuset ReadSet(const PthreadAttr* attr) {
    Cpuset set{};
    set.fill(0xAA);
    Require(pthread_attr_getaffinity_np_nid_postfix(attr, set.size(), set.data()) == 0);
    return set;
}

static bool Untouched(const WideCpuset& set) {
    for (const auto byte : set) {
        if (byte != 0xAA) return false;
    }
    return true;
}

static bool GetFails(const PthreadAttr* attr, std::size_t size, int error) {
    WideCpuset set{};
    set.fill(0xAA);
    return pthread_attr_getaffinity_np_nid_postfix(attr, size, set.data()) == error && Untouched(set);
}

static bool GetThrows(const PthreadAttr* attr, std::size_t size) {
    WideCpuset set{};
    set.fill(0xAA);
    return ThrowsImplemented<std::runtime_error>([&] { return pthread_attr_getaffinity_np_nid_postfix(attr, size, set.data()); }) && Untouched(set);
}

static bool SetThrows(PthreadAttr* attr, std::size_t size, const std::uint8_t* set) {
    return ThrowsImplemented<std::runtime_error>([&] { return pthread_attr_setaffinity_np_nid_postfix(attr, size, set); });
}

static KernelCpumask CreatedAffinity(const PthreadAttr* attr) {
    KernelCpumask affinity = 0;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, attr, ReportAffinity, &affinity, "affinity") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    return affinity;
}

static void TestScope() {
    PthreadAttr attr{};
    Require(scePthreadAttrInit(&attr) == 0);
    int scope = -1;
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrSetscope(&attr, SCOPE_PROCESS) == 0);
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    for (const int invalid : {1, 3, -1}) {
        Require(scePthreadAttrSetscope(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
        scope = -1;
        Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    }
    Require(scePthreadAttrSetscope(&attr, SCOPE_SYSTEM) == 0);
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrGetscope(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetscope(nullptr, &scope) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetscope(nullptr, SCOPE_PROCESS) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == 0);
}

static void TestConcurrency() {
    Require(scePthreadSetconcurrency(3) == 0 && scePthreadGetconcurrency() == 3);
    Require(scePthreadSetconcurrency(-1) == SCE_KERNEL_ERROR_EINVAL && scePthreadGetconcurrency() == 3);
    Require(scePthreadSetconcurrency(0) == 0 && scePthreadGetconcurrency() == 0);
}

static void TestAtfork() {
    const auto before = PthreadAtforkHandlers_nid_no_patch().size();
    Require(scePthreadAtfork(Prepare, Parent, Child) == 0);
    Require(scePthreadAtfork(nullptr, nullptr, nullptr) == 0);
    const auto handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == before + 2);
    Require(handlers[before].prepare == Prepare && handlers[before].parent == Parent && handlers[before].child == Child);
    Require(handlers[before + 1].prepare == nullptr && handlers[before + 1].parent == nullptr && handlers[before + 1].child == nullptr);
}

static void TestAttrAffinity() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    for (const std::size_t size : {std::size_t{0}, sizeof(KernelCpumask) - 1}) Require(GetFails(&attr, size, GUEST_ERANGE));
    for (const std::size_t size : {CPUSET_BYTES, CPUSET_BYTES + 8}) Require(GetThrows(&attr, size));

    const Cpuset set = MaskSet(0x6);
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, set.size(), set.data()) == 0);
    Require(ReadSet(&attr) == set);
    WideCpuset wide{};
    wide.fill(0xAA);
    Require(pthread_attr_getaffinity_np_nid_postfix(&attr, wide.size(), wide.data()) == 0);
    for (std::size_t index = 0; index < wide.size(); ++index) Require(wide[index] == (index < CPUSET_BYTES ? set[index] : 0));
    KernelCpumask mask = 0;
    Require(scePthreadAttrGetaffinity(&attr, &mask) == 0 && mask == 0x6);
    for (const std::size_t size : {std::size_t{0}, sizeof(KernelCpumask) - 1}) Require(GetFails(&attr, size, GUEST_ERANGE));
    for (const std::size_t size : {sizeof(KernelCpumask), CPUSET_BYTES - 1}) Require(GetThrows(&attr, size));

    const Cpuset other = MaskSet(0x1);
    for (const std::size_t size : {std::size_t{1}, sizeof(KernelCpumask) - 1}) {
        Require(pthread_attr_setaffinity_np_nid_postfix(&attr, size, other.data()) == GUEST_ERANGE);
        Require(ReadSet(&attr) == set);
    }
    for (const std::size_t size : {sizeof(KernelCpumask), CPUSET_BYTES - 1}) {
        Require(SetThrows(&attr, size, other.data()));
        Require(ReadSet(&attr) == set);
    }
    for (const std::size_t high : {sizeof(KernelCpumask), CPUSET_BYTES - 1}) {
        Cpuset wider = other;
        wider[high] = 0x80;
        Require(SetThrows(&attr, wider.size(), wider.data()));
        Require(ReadSet(&attr) == set);
    }
    WideCpuset longer{};
    longer[0] = 0x09;
    for (const std::size_t tail : {CPUSET_BYTES, longer.size() - 1}) {
        longer[tail] = 1;
        Require(pthread_attr_setaffinity_np_nid_postfix(&attr, longer.size(), longer.data()) == GUEST_EINVAL);
        Require(ReadSet(&attr) == set);
        longer[CPUSET_BYTES - 1] = 0x40;
        Require(pthread_attr_setaffinity_np_nid_postfix(&attr, longer.size(), longer.data()) == GUEST_EINVAL);
        Require(ReadSet(&attr) == set);
        longer[CPUSET_BYTES - 1] = 0;
        longer[tail] = 0;
    }
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, longer.size(), longer.data()) == 0);
    Require(ReadSet(&attr) == MaskSet(0x9));
    const Cpuset high = MaskSet(0x8000000000000100);
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, high.size(), high.data()) == 0);
    Require(ReadSet(&attr) == high);
    Require(scePthreadAttrGetaffinity(&attr, &mask) == 0 && mask == 0x8000000000000100);

    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, 0, nullptr) == 0);
    Require(scePthreadAttrSetaffinity(&attr, 0x5) == 0);
    Require(ReadSet(&attr) == MaskSet(0x5));

    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, set.size(), set.data()) == 0);
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, 0, set.data()) == 0);
    Require(GetThrows(&attr, CPUSET_BYTES));
    Require(scePthreadAttrGetaffinity(&attr, &mask) == 0 && mask == DEFAULT_AFFINITY);
    for (const std::size_t size : {CPUSET_BYTES, std::size_t{16}, std::size_t{1}}) {
        Require(pthread_attr_setaffinity_np_nid_postfix(&attr, set.size(), set.data()) == 0);
        Require(pthread_attr_setaffinity_np_nid_postfix(&attr, size, nullptr) == 0);
        Require(GetThrows(&attr, CPUSET_BYTES));
    }

    KernelCpumask own = 0;
    Require(scePthreadGetaffinity(scePthreadSelf(), &own) == 0);
    Require(scePthreadSetaffinity(scePthreadSelf(), 0x4) == 0);
    Require(CreatedAffinity(&attr) == 0x4);
    const Cpuset second = MaskSet(0x2);
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr, second.size(), second.data()) == 0);
    Require(CreatedAffinity(&attr) == 0x2);

    Require(scePthreadSetaffinity(scePthreadSelf(), 0x8) == 0);
    for (const bool sce : {true, false}) {
        for (const std::size_t size : {CPUSET_BYTES, std::size_t{0}}) {
            Require(pthread_attr_setaffinity_np_nid_postfix(&attr, size, set.data()) == 0);
            Require((sce ? scePthreadAttrGet(scePthreadSelf(), &attr) : pthread_attr_get_np_nid_postfix(scePthreadSelf(), &attr)) == 0);
            Require(ReadSet(&attr) == MaskSet(0x8));
        }
    }
    Require(scePthreadSetaffinity(scePthreadSelf(), own) == 0);

    PthreadAttr unset = nullptr;
    for (const std::size_t size : {CPUSET_BYTES + 8, CPUSET_BYTES - 1, sizeof(KernelCpumask) - 1}) {
        Require(GetFails(nullptr, size, GUEST_EINVAL));
        Require(GetFails(&unset, size, GUEST_EINVAL));
        Require(pthread_attr_setaffinity_np_nid_postfix(nullptr, size, longer.data()) == GUEST_EINVAL);
        Require(pthread_attr_setaffinity_np_nid_postfix(&unset, size, longer.data()) == GUEST_EINVAL && unset == nullptr);
    }
    Require(pthread_attr_setaffinity_np_nid_postfix(&unset, 0, nullptr) == GUEST_EINVAL && unset == nullptr);
    Require(pthread_attr_getaffinity_np_nid_postfix(&attr, sizeof(KernelCpumask) - 1, nullptr) == GUEST_ERANGE);
    Require(ThrowsImplemented<std::runtime_error>([&] { return pthread_attr_getaffinity_np_nid_postfix(&attr, CPUSET_BYTES, nullptr); }));
    Require(scePthreadAttrDestroy(&attr) == 0);
}

static void TestAttrStackaddr() {
    PthreadAttr attr = nullptr;
    PthreadAttr unset = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    char stack[16]{};
    void* reported = nullptr;
    std::size_t sizeBefore = 0;
    std::size_t sizeAfter = 0;
    Require(scePthreadAttrGetstacksize(&attr, &sizeBefore) == 0);
    Require(pthread_attr_setstackaddr_nid_postfix(&attr, stack) == 0);
    Require(pthread_attr_getstackaddr_nid_postfix(&attr, &reported) == 0 && reported == stack);
    Require(scePthreadAttrGetstacksize(&attr, &sizeAfter) == 0 && sizeAfter == sizeBefore);
    Require(pthread_attr_setstackaddr_nid_postfix(&attr, nullptr) == GUEST_EINVAL);
    reported = nullptr;
    Require(pthread_attr_getstackaddr_nid_postfix(&attr, &reported) == 0 && reported == stack);
    Require(pthread_attr_setstackaddr_nid_postfix(nullptr, stack) == GUEST_EINVAL);
    Require(pthread_attr_setstackaddr_nid_postfix(&unset, stack) == GUEST_EINVAL && unset == nullptr);
    Require(scePthreadAttrDestroy(&attr) == 0);
}

static void TestAttrSolosched() {
    PthreadAttr attr = nullptr;
    PthreadAttr unset = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    int solosched = -1;
    Require(pthread_attr_getsolosched_np_nid_postfix(&attr, &solosched) == 0 && solosched == 0);
    Require(pthread_attr_setsolosched_np_nid_postfix(&attr, 16) == 0);
    Require(pthread_attr_getsolosched_np_nid_postfix(&attr, &solosched) == 0 && solosched == 16);
    Require(pthread_attr_getsolosched_np_nid_postfix(&attr, nullptr) == GUEST_EINVAL);
    solosched = -1;
    Require(pthread_attr_getsolosched_np_nid_postfix(nullptr, &solosched) == GUEST_EINVAL);
    Require(pthread_attr_getsolosched_np_nid_postfix(&unset, &solosched) == GUEST_EINVAL && solosched == -1);
    Require(scePthreadAttrDestroy(&attr) == 0);
}

static void TestMutexPrioceiling() {
    int ceiling = -1;
    PthreadMutex none = nullptr;
    Require(pthread_mutex_init_nid_postfix(&none, nullptr) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&none, &ceiling) == GUEST_EINVAL);
    Require(scePthreadMutexGetprioceiling(&none, &ceiling) == SCE_KERNEL_ERROR_EINVAL && ceiling == -1);
    Require(pthread_mutex_getprioceiling_nid_postfix(&none, nullptr) == GUEST_EINVAL);

    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_INHERIT) == 0);
    PthreadMutex inherit = nullptr;
    Require(pthread_mutex_init_nid_postfix(&inherit, &attr) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&inherit, &ceiling) == GUEST_EINVAL && ceiling == -1);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    PthreadMutex protect = nullptr;
    Require(pthread_mutex_init_nid_postfix(&protect, &attr) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&protect, &ceiling) == 0 && ceiling == DEFAULT_CEILING);
    ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&protect, &ceiling) == 0 && ceiling == DEFAULT_CEILING);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, SET_CEILING) == 0);
    PthreadMutex named = nullptr;
    Require(scePthreadMutexInit(&named, &attr, "prioceiling") == 0);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, LATER_CEILING) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&named, &ceiling) == 0 && ceiling == SET_CEILING);
    ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&named, &ceiling) == 0 && ceiling == SET_CEILING);
    Require(pthread_mutex_getprioceiling_nid_postfix(&protect, &ceiling) == 0 && ceiling == DEFAULT_CEILING);
    Require(pthread_mutex_lock_nid_postfix(&named) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&named, &ceiling) == 0 && ceiling == SET_CEILING);
    Require(pthread_mutex_unlock_nid_postfix(&named) == 0);
    Require(ThrowsImplemented<std::invalid_argument>([&] { return pthread_mutex_getprioceiling_nid_postfix(&protect, nullptr); }));
    Require(ThrowsImplemented<std::invalid_argument>([&] { return pthread_mutex_getprioceiling_nid_postfix(nullptr, &ceiling); }));

    const auto adaptiveInitializer = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    PthreadMutex unset = nullptr;
    PthreadMutex adaptive = adaptiveInitializer;
    ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(&unset, &ceiling) == GUEST_EINVAL && unset == nullptr);
    Require(scePthreadMutexGetprioceiling(&adaptive, &ceiling) == SCE_KERNEL_ERROR_EINVAL && adaptive == adaptiveInitializer);
    PthreadMutex lazy = nullptr;
    Require(pthread_mutex_lock_nid_postfix(&lazy) == 0 && lazy != nullptr);
    Require(pthread_mutex_getprioceiling_nid_postfix(&lazy, &ceiling) == GUEST_EINVAL);
    Require(pthread_mutex_unlock_nid_postfix(&lazy) == 0 && pthread_mutex_destroy_nid_postfix(&lazy) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&named) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&named, &ceiling) == GUEST_EINVAL);
    Require(scePthreadMutexGetprioceiling(&named, &ceiling) == SCE_KERNEL_ERROR_EINVAL && ceiling == -1);

    Require(pthread_mutex_destroy_nid_postfix(&protect) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&inherit) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&none) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static int OwnedInOtherThread(PthreadMutex* mutex) {
    OwnerProbe probe{mutex, -1};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, ReportOwned, &probe, "isowned") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    return probe.owned;
}

static void TestMutexIsowned() {
    PthreadMutex unset = nullptr;
    Require(scePthreadMutexIsowned(&unset) == 0 && unset == nullptr);
    const auto adaptiveInitializer = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    PthreadMutex adaptive = adaptiveInitializer;
    Require(scePthreadMutexIsowned(&adaptive) == 0 && adaptive == adaptiveInitializer);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(scePthreadMutexIsowned(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(scePthreadMutexIsowned(&mutex) == SCE_KERNEL_ERROR_EPERM);
    Require(OwnedInOtherThread(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(scePthreadMutexIsowned(&mutex) == 0);

    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MUTEX_RECURSIVE) == 0);
    PthreadMutex recursive = nullptr;
    Require(pthread_mutex_init_nid_postfix(&recursive, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&recursive) == 0 && pthread_mutex_lock_nid_postfix(&recursive) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&recursive) == 0);
    Require(scePthreadMutexIsowned(&recursive) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_mutex_unlock_nid_postfix(&recursive) == 0);
    Require(scePthreadMutexIsowned(&recursive) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&recursive) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    const auto destroyed = mutex;
    Require(scePthreadMutexIsowned(&mutex) == 0 && mutex == destroyed);
    Require(ThrowsImplemented<std::invalid_argument>([] { return scePthreadMutexIsowned(nullptr); }));
}

int main() {
    TestScope();
    TestConcurrency();
    TestAtfork();
    TestAttrAffinity();
    TestAttrStackaddr();
    TestAttrSolosched();
    TestMutexPrioceiling();
    TestMutexIsowned();
    return 0;
}

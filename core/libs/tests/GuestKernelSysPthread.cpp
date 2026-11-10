#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <thread>

static constexpr int SCOPE_PROCESS = 0;
static constexpr int SCOPE_SYSTEM = 2;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ERANGE = 34;
static constexpr std::size_t CPUSET_BYTES = 32;
static constexpr KernelCpumask DEFAULT_AFFINITY = 0x1FFF;

using Cpuset = std::array<std::uint8_t, CPUSET_BYTES>;
using WideCpuset = std::array<std::uint8_t, CPUSET_BYTES + 8>;

static void Require(bool value) { if (!value) std::abort(); }

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

static void TestMutexAttributeGeneration() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    int generation = -1;
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, &generation) == 0 && generation == 0);
    Require(pthread_mutexattr_setgen_np_nid_postfix(&attr, 1) == 0);
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, &generation) == 0 && generation == 1);
    Require(pthread_mutexattr_setgen_np_nid_postfix(&attr, 2) == 45);
    Require(pthread_mutexattr_getgen_np_nid_postfix(nullptr, &generation) == 22);
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, nullptr) == 22);
    Require(pthread_mutexattr_setgen_np_nid_postfix(nullptr, 0) == 22);
    PthreadMutexattr destroyedAttr = nullptr;
    Require(pthread_mutexattr_getgen_np_nid_postfix(&destroyedAttr, &generation) == 22);
    Require(pthread_mutexattr_setgen_np_nid_postfix(&destroyedAttr, 0) == 22);

    Require(scePthreadMutexattrSetgen(&attr, 0) == 0);
    Require(scePthreadMutexattrGetgen(&attr, &generation) == 0 && generation == 0);
    Require(scePthreadMutexattrSetgen(&attr, 2) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    Require(scePthreadMutexattrGetgen(nullptr, &generation) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, reinterpret_cast<int*>(0x10)) == 14);
    Require(pthread_mutexattr_getgen_np_nid_postfix(reinterpret_cast<const PthreadMutexattr*>(0x10), &generation) == 22);
    Require(pthread_mutexattr_setgen_np_nid_postfix(reinterpret_cast<PthreadMutexattr*>(0x10), 0) == 22);
    Require(scePthreadMutexattrGetgen(&attr, reinterpret_cast<int*>(0x10)) == static_cast<int>(0x80020000u | 14u));
    Require(scePthreadMutexattrGetgen(reinterpret_cast<const PthreadMutexattr*>(0x10), &generation) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetgen(reinterpret_cast<PthreadMutexattr*>(0x10), 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestThreadAttrCreateSuspend() {
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(nullptr) == 22);
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(reinterpret_cast<PthreadAttr*>(0x10)) == 22);
    PthreadAttr attr{};
    Require(scePthreadAttrInit(&attr) == 0);
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(&attr) == 0);
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

struct TimedjoinGate {
    std::atomic<bool> release{false};
    std::atomic<bool> entered{false};
};

static void* APS5_VABI TimedjoinWorker(void* argument) {
    auto& gate = *static_cast<TimedjoinGate*>(argument);
    gate.entered.store(true, std::memory_order_release);
    while (!gate.release.load(std::memory_order_acquire)) std::this_thread::yield();
    return reinterpret_cast<void*>(std::uintptr_t{0x12345678});
}

static void TestTimedjoin() {
    Require(scePthreadTimedjoin(nullptr, nullptr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadTimedjoin(scePthreadSelf(), nullptr, 1000000) == SCE_KERNEL_ERROR_EDEADLK);

    TimedjoinGate gate;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, TimedjoinWorker, &gate, nullptr) == 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!gate.entered.load(std::memory_order_acquire)) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }

    void* returnValue = reinterpret_cast<void*>(std::uintptr_t{0x55});
    Require(scePthreadTimedjoin(thread, &returnValue, 0) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(returnValue == reinterpret_cast<void*>(std::uintptr_t{0x55}));

    gate.release.store(true, std::memory_order_release);
    Require(scePthreadTimedjoin(thread, &returnValue, 3000000) == 0);
    Require(returnValue == reinterpret_cast<void*>(std::uintptr_t{0x12345678}));
    Require(scePthreadTimedjoin(thread, nullptr, 3000000) == SCE_KERNEL_ERROR_ESRCH);
}

int main() {
    TestScope();
    TestMutexAttributeGeneration();
    TestThreadAttrCreateSuspend();
    TestConcurrency();
    TestAtfork();
    TestAttrAffinity();
    TestTimedjoin();
    return 0;
}

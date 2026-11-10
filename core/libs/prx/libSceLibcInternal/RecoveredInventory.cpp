#include "prx/libSceLibcInternal/include/RecoveredInventory.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#include "prx/libc/include/PreciseWait.hpp"
#endif
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <time.h>

namespace {

constexpr long nanosPerSecond = 1000000000;
constexpr time_t longestSleepSeconds = 86400;

bool Reached(const timespec& now, const timespec& deadline) {
    return now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec);
}

timespec MonotonicNow() {
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        throw std::runtime_error("_Thrd_sleep_for: reading the monotonic clock failed");
    return now;
}

std::uint8_t ByteExtremum(const void* first, const void* last, bool minimum) {
    if (!first || !last || reinterpret_cast<std::uintptr_t>(last) <= reinterpret_cast<std::uintptr_t>(first)) APS5_INVALID_ARG_EX;
    auto* current = static_cast<const std::uint8_t*>(first);
    const auto* end = static_cast<const std::uint8_t*>(last);
    std::uint8_t result = *current++;
    for (; current != end; ++current) {
        if (minimum ? *current < result : *current > result) result = *current;
    }
    return result;
}

}

extern "C" {

void APS5_VABI _ZNSt8__sce_v215_Thrd_sleep_forEm_nid_postfix(std::uint64_t milliseconds) {
    if (milliseconds == 0)
        return;
    const auto start = MonotonicNow();
    timespec deadline{static_cast<time_t>(start.tv_sec + static_cast<time_t>(milliseconds / 1000)),
        static_cast<long>(start.tv_nsec + static_cast<long>(milliseconds % 1000) * 1000000)};
    if (deadline.tv_nsec >= nanosPerSecond) {
        deadline.tv_nsec -= nanosPerSecond;
        ++deadline.tv_sec;
    }
    for (;;) {
        const auto now = MonotonicNow();
        if (Reached(now, deadline))
            return;
        timespec remaining{static_cast<time_t>(deadline.tv_sec - now.tv_sec), static_cast<long>(deadline.tv_nsec - now.tv_nsec)};
        if (remaining.tv_nsec < 0) {
            remaining.tv_nsec += nanosPerSecond;
            --remaining.tv_sec;
        }
        remaining.tv_sec = std::min(remaining.tv_sec, longestSleepSeconds);
#ifdef _WIN32
        PreciseSleepHundredNanos(static_cast<unsigned long long>(remaining.tv_sec) * 10000000ULL +
            (static_cast<unsigned long long>(remaining.tv_nsec) + 99ULL) / 100ULL);
#else
        if (nanosleep(&remaining, nullptr) != 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("_Thrd_sleep_for: nanosleep failed");
        }
#endif
    }
}

int APS5_VABI __std_atomic_notify_multi_direct_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::uint8_t APS5_VABI __std_max_1u_nid_postfix(const void* first, const void* last) {
    return ByteExtremum(first, last, false);
}

std::uint8_t APS5_VABI __std_min_1u_nid_postfix(const void* first, const void* last) {
    return ByteExtremum(first, last, true);
}

int APS5_VABI __truncdfhf2_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcInternalPrimitiveMallocStatsFast_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __std_atomic_wait_direct_8_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcInternalPrimitiveMallocStats_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __std_atomic_wait_direct_16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}

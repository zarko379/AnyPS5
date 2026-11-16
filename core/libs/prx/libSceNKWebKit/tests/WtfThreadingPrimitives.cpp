#include "prx/libSceNKWebKit/include/WtfThreadingPrimitivesExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF threading primitives check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Exception, class Function>
void RequireThrows(Function function, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        function();
    } catch (const Exception&) {
        threw = true;
    }
    Require(threw, location);
}

bool HeldByAnotherThread(WTF::Mutex& mutex) {
    bool acquired = false;
    std::thread probe([&] {
        acquired = _ZN3WTF5Mutex7tryLockEv(&mutex);
        if (acquired) _ZN3WTF5Mutex6unlockEv(&mutex);
    });
    probe.join();
    return !acquired;
}

double Now() {
    return _ZN3WTF8WallTime3nowEv().m_value;
}

void CheckMutex() {
    WTF::Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    Require(mutex.m_mutex != nullptr);
    Require(!HeldByAnotherThread(mutex));
    _ZN3WTF5Mutex4lockEv(&mutex);
    Require(HeldByAnotherThread(mutex));
    Require(!_ZN3WTF5Mutex7tryLockEv(&mutex));
    _ZN3WTF5Mutex6unlockEv(&mutex);
    Require(!HeldByAnotherThread(mutex));
    Require(_ZN3WTF5Mutex7tryLockEv(&mutex));
    Require(HeldByAnotherThread(mutex));
    _ZN3WTF5Mutex6unlockEv(&mutex);
    _ZN3WTF5MutexD1Ev(&mutex);

    WTF::Mutex second{};
    _ZN3WTF5MutexC2Ev(&second);
    Require(second.m_mutex != nullptr);
    _ZN3WTF5Mutex4lockEv(&second);
    Require(!_ZN3WTF5Mutex7tryLockEv(&second));
    _ZN3WTF5Mutex6unlockEv(&second);
    _ZN3WTF5MutexD2Ev(&second);

    RequireThrows<std::invalid_argument>([] { _ZN3WTF5MutexC1Ev(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF5MutexD1Ev(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF5Mutex4lockEv(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF5Mutex7tryLockEv(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF5Mutex6unlockEv(nullptr); });
}

void CheckTimedWaitWithoutSignal() {
    WTF::Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    WTF::ThreadCondition condition{};
    _ZN3WTF5Mutex4lockEv(&mutex);
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{-std::numeric_limits<double>::infinity()}));
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{Now() - 1}));
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{0}));
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{-1e300}));
    auto start = std::chrono::steady_clock::now();
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{Now() + 0.05}));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(30));
    Require(HeldByAnotherThread(mutex));
    RequireThrows<std::domain_error>([&] {
        _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{std::numeric_limits<double>::quiet_NaN()});
    });
    RequireThrows<std::out_of_range>([&] {
        _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{9223372036854775808.0});
    });
    _ZN3WTF5Mutex6unlockEv(&mutex);
    _ZN3WTF15ThreadCondition6signalEv(&condition);
    _ZN3WTF15ThreadCondition9broadcastEv(&condition);
    _ZN3WTF15ThreadConditionD1Ev(&condition);
    _ZN3WTF5MutexD1Ev(&mutex);
}

template<class Wait>
void CheckSignalled(Wait wait, bool useBroadcast, std::source_location location = std::source_location::current()) {
    WTF::Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    WTF::ThreadCondition condition{};
    std::atomic<bool> waiting{false};
    bool ready = false;
    bool result = false;
    std::thread waiter([&] {
        _ZN3WTF5Mutex4lockEv(&mutex);
        waiting = true;
        while (!ready) result = wait(condition, mutex);
        _ZN3WTF5Mutex6unlockEv(&mutex);
    });
    while (!waiting) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    _ZN3WTF5Mutex4lockEv(&mutex);
    ready = true;
    if (useBroadcast) {
        _ZN3WTF15ThreadCondition9broadcastEv(&condition);
    } else {
        _ZN3WTF15ThreadCondition6signalEv(&condition);
    }
    _ZN3WTF5Mutex6unlockEv(&mutex);
    waiter.join();
    Require(result, location);
    _ZN3WTF15ThreadConditionD2Ev(&condition);
    _ZN3WTF5MutexD2Ev(&mutex);
}

void CheckWaits() {
    auto plainWait = [](WTF::ThreadCondition& condition, WTF::Mutex& mutex) {
        _ZN3WTF15ThreadCondition4waitERNS_5MutexE(&condition, mutex);
        return true;
    };
    auto timed = [](double offset) {
        return [offset](WTF::ThreadCondition& condition, WTF::Mutex& mutex) {
            double target = std::isinf(offset) || offset > 1e18 ? offset : Now() + offset;
            return _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, mutex, WTF::WallTime{target});
        };
    };
    CheckSignalled(plainWait, false);
    CheckSignalled(plainWait, true);
    CheckSignalled(timed(30), false);
    CheckSignalled(timed(30), true);
    CheckSignalled(timed(std::numeric_limits<double>::infinity()), false);
    CheckSignalled(timed(1e19), true);
    CheckSignalled(timed(std::numeric_limits<double>::max()), false);

    WTF::Mutex mutex{};
    WTF::ThreadCondition condition{};
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(nullptr, mutex); });
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(nullptr, mutex, WTF::WallTime{0}); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF15ThreadCondition6signalEv(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF15ThreadCondition9broadcastEv(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF15ThreadConditionD1Ev(nullptr); });
}

}

int main() {
    CheckMutex();
    CheckTimedWaitWithoutSignal();
    CheckWaits();
    return 0;
}

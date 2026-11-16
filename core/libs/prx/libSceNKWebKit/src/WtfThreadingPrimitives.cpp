// Derived from Source/WTF/wtf/posix/ThreadingPOSIX.cpp and Source/WTF/wtf/ThreadingPrimitives.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood <jhaygood@reaktix.com>
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WtfThreadingPrimitivesExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int PosixMutexNormal = 3;

PthreadMutex* MutexImpl(WTF::Mutex* mutex, const char* function) {
    if (!mutex) throw std::invalid_argument(std::string(function) + ": null WTF::Mutex");
    return &mutex->m_mutex;
}

PthreadCond* ConditionImpl(WTF::ThreadCondition* condition, const char* function) {
    if (!condition) throw std::invalid_argument(std::string(function) + ": null WTF::ThreadCondition");
    return &condition->m_condition;
}

void ConstructMutex(WTF::Mutex* mutex) {
    PthreadMutex* impl = MutexImpl(mutex, "WTF::Mutex::Mutex");
    PthreadMutexattr attr = nullptr;
    pthread_mutexattr_init_nid_postfix(&attr);
    pthread_mutexattr_settype_nid_postfix(&attr, PosixMutexNormal);
    pthread_mutex_init_nid_postfix(impl, &attr);
    pthread_mutex_setname_np_nid_postfix(impl, "SceNKWTF");
    pthread_mutexattr_destroy_nid_postfix(&attr);
}

void DestroyMutex(WTF::Mutex* mutex) {
    pthread_mutex_destroy_nid_postfix(MutexImpl(mutex, "WTF::Mutex::~Mutex"));
}

void DestroyCondition(WTF::ThreadCondition* condition) {
    pthread_cond_destroy_nid_postfix(ConditionImpl(condition, "WTF::ThreadCondition::~ThreadCondition"));
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF5MutexC1Ev(WTF::Mutex* self) {
    ConstructMutex(self);
}

void APS5_VABI _ZN3WTF5MutexC2Ev(WTF::Mutex* self) {
    ConstructMutex(self);
}

void APS5_VABI _ZN3WTF5MutexD1Ev(WTF::Mutex* self) {
    DestroyMutex(self);
}

void APS5_VABI _ZN3WTF5MutexD2Ev(WTF::Mutex* self) {
    DestroyMutex(self);
}

void APS5_VABI _ZN3WTF5Mutex4lockEv(WTF::Mutex* self) {
    pthread_mutex_lock_nid_postfix(MutexImpl(self, __func__));
}

bool APS5_VABI _ZN3WTF5Mutex7tryLockEv(WTF::Mutex* self) {
    return pthread_mutex_trylock_nid_postfix(MutexImpl(self, __func__)) == 0;
}

void APS5_VABI _ZN3WTF5Mutex6unlockEv(WTF::Mutex* self) {
    pthread_mutex_unlock_nid_postfix(MutexImpl(self, __func__));
}

void APS5_VABI _ZN3WTF15ThreadConditionD1Ev(WTF::ThreadCondition* self) {
    DestroyCondition(self);
}

void APS5_VABI _ZN3WTF15ThreadConditionD2Ev(WTF::ThreadCondition* self) {
    DestroyCondition(self);
}

void APS5_VABI _ZN3WTF15ThreadCondition4waitERNS_5MutexE(WTF::ThreadCondition* self, WTF::Mutex& mutex) {
    pthread_cond_wait_nid_postfix(ConditionImpl(self, __func__), MutexImpl(&mutex, __func__));
}

bool APS5_VABI _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(WTF::ThreadCondition* self, WTF::Mutex& mutex, WTF::WallTime absoluteTime) {
    PthreadCond* condition = ConditionImpl(self, __func__);
    PthreadMutex* impl = MutexImpl(&mutex, __func__);
    double value = absoluteTime.m_value;
    if (std::isinf(value)) {
        if (value < 0) return false;
        pthread_cond_wait_nid_postfix(condition, impl);
        return true;
    }
    if (std::isnan(value)) throw std::domain_error(std::string(__func__) + ": NaN absolute time");
    if (value < _ZN3WTF8WallTime3nowEv().m_value) return false;
    if (value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        pthread_cond_wait_nid_postfix(condition, impl);
        return true;
    }
    if (value >= 9223372036854775808.0) throw std::out_of_range(std::string(__func__) + ": absolute time equal to 2^63 seconds");
    std::int64_t timeSeconds = static_cast<std::int64_t>(value);
    KernelTimespec targetTime{timeSeconds, static_cast<std::int64_t>((value - static_cast<double>(timeSeconds)) * 1E9)};
    return pthread_cond_timedwait_nid_postfix(condition, impl, &targetTime) == 0;
}

void APS5_VABI _ZN3WTF15ThreadCondition6signalEv(WTF::ThreadCondition* self) {
    pthread_cond_signal_nid_postfix(ConditionImpl(self, __func__));
}

void APS5_VABI _ZN3WTF15ThreadCondition9broadcastEv(WTF::ThreadCondition* self) {
    pthread_cond_broadcast_nid_postfix(ConditionImpl(self, __func__));
}

}

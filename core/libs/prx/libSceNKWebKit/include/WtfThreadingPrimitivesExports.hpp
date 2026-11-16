#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <cstddef>

namespace WTF {

struct Mutex {
    PthreadMutex m_mutex;
};

struct ThreadCondition {
    PthreadCond m_condition;
};

static_assert(sizeof(Mutex) == 8);
static_assert(sizeof(ThreadCondition) == 8);

}  // namespace WTF

extern "C" {

void APS5_VABI _ZN3WTF5MutexC1Ev(WTF::Mutex* self);
void APS5_VABI _ZN3WTF5MutexC2Ev(WTF::Mutex* self);
void APS5_VABI _ZN3WTF5MutexD1Ev(WTF::Mutex* self);
void APS5_VABI _ZN3WTF5MutexD2Ev(WTF::Mutex* self);
void APS5_VABI _ZN3WTF5Mutex4lockEv(WTF::Mutex* self);
bool APS5_VABI _ZN3WTF5Mutex7tryLockEv(WTF::Mutex* self);
void APS5_VABI _ZN3WTF5Mutex6unlockEv(WTF::Mutex* self);
void APS5_VABI _ZN3WTF15ThreadConditionD1Ev(WTF::ThreadCondition* self);
void APS5_VABI _ZN3WTF15ThreadConditionD2Ev(WTF::ThreadCondition* self);
void APS5_VABI _ZN3WTF15ThreadCondition4waitERNS_5MutexE(WTF::ThreadCondition* self, WTF::Mutex& mutex);
bool APS5_VABI _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(WTF::ThreadCondition* self, WTF::Mutex& mutex, WTF::WallTime absoluteTime);
void APS5_VABI _ZN3WTF15ThreadCondition6signalEv(WTF::ThreadCondition* self);
void APS5_VABI _ZN3WTF15ThreadCondition9broadcastEv(WTF::ThreadCondition* self);

}

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::InspectorAbi {

struct RemoteTarget {
    const void* vtable;
    unsigned targetIdentifier;
    bool flag;
};

struct FrameVector {
    void* buffer;
    unsigned capacity;
    unsigned size;
};

struct ScriptCallStack {
    unsigned refCount;
    FrameVector frames;
};

struct AsyncCallIdentifier {
    unsigned type;
    int callbackId;
};

static_assert(sizeof(RemoteTarget) == 16);
static_assert(offsetof(RemoteTarget, targetIdentifier) == 8);
static_assert(offsetof(RemoteTarget, flag) == 12);
static_assert(sizeof(FrameVector) == 16);
static_assert(offsetof(FrameVector, capacity) == 8);
static_assert(offsetof(FrameVector, size) == 12);
static_assert(sizeof(ScriptCallStack) == 24);
static_assert(offsetof(ScriptCallStack, frames) == 8);
static_assert(sizeof(AsyncCallIdentifier) == 8);
static_assert(offsetof(AsyncCallIdentifier, callbackId) == 4);
static_assert(std::is_trivially_copyable_v<AsyncCallIdentifier>);

} // namespace Jsc::InspectorAbi

extern "C" {

bool APS5_VABI _ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(const Jsc::InspectorAbi::RemoteTarget* self);
bool APS5_VABI _ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(const Jsc::InspectorAbi::RemoteTarget* self);
void APS5_VABI _ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    Jsc::InspectorAbi::ScriptCallStack* self, Jsc::InspectorAbi::FrameVector* frames);
void APS5_VABI _ZN9Inspector15ScriptCallStackC2ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    Jsc::InspectorAbi::ScriptCallStack* self, Jsc::InspectorAbi::FrameVector* frames);
Jsc::InspectorAbi::AsyncCallIdentifier APS5_VABI _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(int asyncCallType, int callbackId);

}

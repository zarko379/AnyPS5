#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace Jsc::InspectorAbi {

struct FrameVector {
    void* buffer;
    unsigned capacity;
    unsigned size;
};

struct ScriptCallStack {
    unsigned refCount;
    FrameVector frames;
};

static_assert(sizeof(FrameVector) == 16);
static_assert(offsetof(FrameVector, capacity) == 8);
static_assert(offsetof(FrameVector, size) == 12);
static_assert(sizeof(ScriptCallStack) == 24);
static_assert(offsetof(ScriptCallStack, frames) == 8);

} // namespace Jsc::InspectorAbi

extern "C" {

void APS5_VABI _ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    Jsc::InspectorAbi::ScriptCallStack* self, Jsc::InspectorAbi::FrameVector* frames);
void APS5_VABI _ZN9Inspector15ScriptCallStackC2ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    Jsc::InspectorAbi::ScriptCallStack* self, Jsc::InspectorAbi::FrameVector* frames);

}

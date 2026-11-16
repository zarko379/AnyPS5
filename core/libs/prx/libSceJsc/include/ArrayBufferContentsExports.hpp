#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::JscAbi {

inline constexpr std::size_t PackedPointerSize = 6;

struct ArrayBufferContents {
    void* data;
    std::uint8_t destructor[PackedPointerSize];
    std::uint8_t shared[PackedPointerSize];
    unsigned sizeInBytes;
};

static_assert(sizeof(ArrayBufferContents) == 24);
static_assert(alignof(ArrayBufferContents) == 8);
static_assert(offsetof(ArrayBufferContents, destructor) == 8);
static_assert(offsetof(ArrayBufferContents, shared) == 14);
static_assert(offsetof(ArrayBufferContents, sizeInBytes) == 20);

} // namespace Jsc::JscAbi

extern "C" {

void APS5_VABI _ZN3JSC19ArrayBufferContentsC1Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContentsC2Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContentsC1EOS0_(Jsc::JscAbi::ArrayBufferContents* self, Jsc::JscAbi::ArrayBufferContents* other);
void APS5_VABI _ZN3JSC19ArrayBufferContentsC2EOS0_(Jsc::JscAbi::ArrayBufferContents* self, Jsc::JscAbi::ArrayBufferContents* other);
Jsc::JscAbi::ArrayBufferContents* APS5_VABI _ZN3JSC19ArrayBufferContentsaSEOS0_(Jsc::JscAbi::ArrayBufferContents* self, Jsc::JscAbi::ArrayBufferContents* other);
void APS5_VABI _ZN3JSC19ArrayBufferContentsD1Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContentsD2Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContents5clearEv(Jsc::JscAbi::ArrayBufferContents* self);

}

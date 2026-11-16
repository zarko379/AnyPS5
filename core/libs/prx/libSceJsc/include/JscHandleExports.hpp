#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::JscAbi {

struct HandleNode {
    std::uint64_t value;
    HandleNode* prev;
    HandleNode* next;
};

struct HandleSet {
    void* vm;
    void* blockHead;
    void* blockTail;
    HandleNode strongHead;
    HandleNode strongTail;
    HandleNode immediateHead;
    HandleNode immediateTail;
    HandleNode* freeHead;
};

struct HandleBlock {
    HandleBlock* prev;
    HandleBlock* next;
    HandleSet* handleSet;
};

struct WeakImpl {
    std::uint64_t jsValue;
    std::uintptr_t weakHandleOwner;
    void* context;
};

inline constexpr std::size_t HandleBlockSize = 4096;

static_assert(sizeof(HandleNode) == 24);
static_assert(offsetof(HandleNode, value) == 0);
static_assert(offsetof(HandleNode, prev) == 8);
static_assert(offsetof(HandleNode, next) == 16);
static_assert(sizeof(HandleSet) == 128);
static_assert(offsetof(HandleSet, strongHead) == 24);
static_assert(offsetof(HandleSet, strongTail) == 48);
static_assert(offsetof(HandleSet, immediateHead) == 72);
static_assert(offsetof(HandleSet, immediateTail) == 96);
static_assert(offsetof(HandleSet, freeHead) == 120);
static_assert(offsetof(HandleBlock, handleSet) == 16);
static_assert(sizeof(WeakImpl) == 24);
static_assert(offsetof(WeakImpl, weakHandleOwner) == 8);

} // namespace Jsc::JscAbi

extern "C" {

void APS5_VABI _ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE(Jsc::JscAbi::WeakImpl** impl);
void APS5_VABI _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(Jsc::JscAbi::HandleSet* self, std::uint64_t* slot, const std::uint64_t* value);

}

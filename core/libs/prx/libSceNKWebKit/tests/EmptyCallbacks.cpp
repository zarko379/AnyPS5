#include "prx/libSceNKWebKit/include/Api.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "empty callback check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}

int main() {
    std::uint64_t before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore22EmptyFrameLoaderClient12didDetectXSSERKN3WTF3URLEb();
    _ZN7WebCore22EmptyFrameLoaderClient13committedLoadEPNS_14DocumentLoaderEPKci();
    _ZN7WebCore22EmptyFrameLoaderClient25setMainFrameDocumentReadyEb();
    _ZN7WebCore22EmptyFrameLoaderClient29savePlatformDataToCachedFrameEPNS_11CachedFrameE();
    _ZN7WebCore22EmptyFrameLoaderClient30dispatchDidFailProvisionalLoadERKNS_13ResourceErrorENS_19WillContinueLoadingE();
    _ZN7WebCore22EmptyFrameLoaderClient31dispatchDidReachLayoutMilestoneEN3WTF9OptionSetINS_15LayoutMilestoneEEE();
    _ZN7WebCore22EmptyFrameLoaderClient33dispatchWillPerformClientRedirectERKN3WTF3URLEdNS1_8WallTimeENS_19LockBackForwardListE();
    _ZN3PAL8KillRing6appendERKN3WTF6StringE();
    _ZN3PAL8KillRing7prependERKN3WTF6StringE();
    _ZN3PAL8KillRing16startNewSequenceEv();
    _ZN3PAL8KillRing16setToYankedStateEv();
    {
        NKWebKit::WtfAbi::String yanked = _ZN3PAL8KillRing4yankEv();
        Require(yanked.impl == nullptr);
    }
    _ZN3PAL8KillRing6appendERKN3WTF6StringE();
    {
        NKWebKit::WtfAbi::String yanked = _ZN3PAL8KillRing4yankEv();
        Require(yanked.impl == nullptr);
    }
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}

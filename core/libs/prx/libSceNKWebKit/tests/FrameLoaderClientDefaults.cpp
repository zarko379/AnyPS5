#include "prx/libSceNKWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FrameLoaderClient default check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}

int main() {
    std::uint64_t before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore17FrameLoaderClient15didCreateWindowERNS_9DOMWindowE();
    _ZN7WebCore17FrameLoaderClient15didDisownOpenerEv();
    _ZN7WebCore17FrameLoaderClient17dispatchDidLayoutEv();
    _ZN7WebCore17FrameLoaderClient19finishedLoadingIconEmPNS_12SharedBufferE();
    _ZN7WebCore17FrameLoaderClient20willInjectUserScriptERNS_15DOMWrapperWorldE();
    _ZN7WebCore17FrameLoaderClient21didChangeScrollOffsetEv();
    _ZN7WebCore17FrameLoaderClient22dispatchDidReceiveIconEv();
    _ZN7WebCore17FrameLoaderClient23dispatchDidExplicitOpenERKN3WTF3URLERKNS1_6StringE();
    _ZN7WebCore17FrameLoaderClient23getLoadDecisionForIconsERKN3WTF6VectorISt4pairIRNS_8LinkIconEmELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE();
    _ZN7WebCore17FrameLoaderClient24didRestoreScrollPositionEv();
    _ZN7WebCore17FrameLoaderClient26dispatchWillChangeDocumentERKN3WTF3URLES4_();
    _ZN7WebCore17FrameLoaderClient27registerForIconNotificationEv();
    _ZN7WebCore17FrameLoaderClient28didLoadFromRegistrableDomainEONS_17RegistrableDomainE();
    _ZN7WebCore17FrameLoaderClient28notifyPageOfAppBoundBehaviorEv();
    _ZN7WebCore17FrameLoaderClient29dispatchDidChangeMainDocumentEv();
    _ZN7WebCore17FrameLoaderClient29dispatchDidNavigateWithinPageEv();
    _ZN7WebCore17FrameLoaderClient29dispatchGlobalObjectAvailableERNS_15DOMWrapperWorldE();
    _ZN7WebCore17FrameLoaderClient30completePageTransitionIfNeededEv();
    _ZN7WebCore17FrameLoaderClient31dispatchDidChangeProvisionalURLEv();
    _ZN7WebCore17FrameLoaderClient31dispatchDidReachLayoutMilestoneEN3WTF9OptionSetINS_15LayoutMilestoneEEE();
    _ZN7WebCore17FrameLoaderClient52dispatchDidReconnectDOMWindowExtensionToGlobalObjectEPNS_18DOMWindowExtensionE();
    _ZN7WebCore17FrameLoaderClient52dispatchWillDestroyGlobalObjectForDOMWindowExtensionEPNS_18DOMWindowExtensionE();
    _ZN7WebCore17FrameLoaderClient56dispatchWillDisconnectDOMWindowExtensionFromGlobalObjectEPNS_18DOMWindowExtensionE();
    int client = 0;
    Require(_ZN7WebCore17FrameLoaderClient11allowScriptEb(&client, true));
    Require(!_ZN7WebCore17FrameLoaderClient11allowScriptEb(&client, false));
    Require(_ZN7WebCore17FrameLoaderClient11allowScriptEb(nullptr, true));
    Require(!_ZN7WebCore17FrameLoaderClient11allowScriptEb(nullptr, false));
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}

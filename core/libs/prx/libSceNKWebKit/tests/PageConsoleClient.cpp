#include "prx/libSceNKWebKit/include/WebCoreProcessSwitchesExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PageConsoleClient check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using NKWebKit::WebCoreAbi::PageConsoleClientMuteCount;

}

int main() {
    Require(!_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv());
    _ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb(true);
    Require(_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv());
    _ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb(true);
    Require(_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv());
    _ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb(false);
    Require(!_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv());

    Require(PageConsoleClientMuteCount() == 0);
    _ZN7WebCore17PageConsoleClient4muteEv();
    Require(PageConsoleClientMuteCount() == 1);
    _ZN7WebCore17PageConsoleClient4muteEv();
    _ZN7WebCore17PageConsoleClient4muteEv();
    Require(PageConsoleClientMuteCount() == 3);
    _ZN7WebCore17PageConsoleClient6unmuteEv();
    Require(PageConsoleClientMuteCount() == 2);
    _ZN7WebCore17PageConsoleClient6unmuteEv();
    _ZN7WebCore17PageConsoleClient6unmuteEv();
    Require(PageConsoleClientMuteCount() == 0);
    _ZN7WebCore17PageConsoleClient6unmuteEv();
    Require(PageConsoleClientMuteCount() == -1);
    _ZN7WebCore17PageConsoleClient4muteEv();
    Require(PageConsoleClientMuteCount() == 0);
    Require(!_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv());
    return 0;
}

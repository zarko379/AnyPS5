#include "prx/libSceNKWebKit/include/WebCoreProcessSwitchesExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore global settings check failed at line %u\n", location.line());
        std::abort();
    }
}

void CheckFontSmoothing() {
    Require(_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(false);
    Require(!_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(false);
    Require(!_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(true);
    Require(_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
}

}

int main() {
    CheckFontSmoothing();
    CheckFontSmoothing();
    return 0;
}

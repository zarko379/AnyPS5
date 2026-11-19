#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage zoom and layout check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Exception, class Function>
void RequireThrows(Function function, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        function();
    } catch (const Exception&) {
        threw = true;
    }
    Require(threw, location);
}

template<class Function>
void RequireInvalidArgument(Function function, std::source_location location = std::source_location::current()) {
    RequireThrows<std::invalid_argument>(function, location);
}

struct PageHandle {
    PageHandle() : ref(NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Page>())) {}
    ~PageHandle() { WKRelease(ref); }
    const void* ref;
};

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

void CheckBackingScaleFactor() {
    PageHandle page;
    Require(WKPageGetBackingScaleFactor(page.ref) == 1);
    WKPageSetCustomBackingScaleFactor(page.ref, 2);
    Require(WKPageGetBackingScaleFactor(page.ref) == 2);
    WKPageSetCustomBackingScaleFactor(page.ref, 1.1);
    Require(WKPageGetBackingScaleFactor(page.ref) == static_cast<double>(1.1f));
    Require(WKPageGetBackingScaleFactor(page.ref) != 1.1);
    WKPageSetCustomBackingScaleFactor(page.ref, 0);
    Require(WKPageGetBackingScaleFactor(page.ref) == 1);
    WKPageSetCustomBackingScaleFactor(page.ref, -0.5);
    Require(WKPageGetBackingScaleFactor(page.ref) == -0.5);
    WKPageSetCustomBackingScaleFactor(page.ref, 1e-50);
    Require(WKPageGetBackingScaleFactor(page.ref) == 1);
    WKPageSetCustomBackingScaleFactor(page.ref, 3);
    WKPageSetCustomBackingScaleFactor(page.ref, -0.0);
    Require(WKPageGetBackingScaleFactor(page.ref) == 1);
    WKPageSetCustomBackingScaleFactor(page.ref, kNaN);
    Require(std::isnan(WKPageGetBackingScaleFactor(page.ref)));
    WKPageSetCustomBackingScaleFactor(page.ref, 1e300);
    Require(std::isinf(WKPageGetBackingScaleFactor(page.ref)));
    RequireInvalidArgument([] { WKPageSetCustomBackingScaleFactor(nullptr, 0); });
    RequireInvalidArgument([] { WKPageGetBackingScaleFactor(nullptr); });
}

void CheckScaleFactor() {
    PageHandle page;
    Require(WKPageGetScaleFactor(page.ref) == 1);
    WKPageSetScaleFactor(page.ref, 1.5, NKWebKit::PointValue{10.5, -20});
    Require(WKPageGetScaleFactor(page.ref) == 1.5);
    WKPageSetScaleFactor(page.ref, -2, NKWebKit::PointValue{kNaN, 1e300});
    Require(WKPageGetScaleFactor(page.ref) == -2);
    WKPageSetScaleFactor(page.ref, 0, NKWebKit::PointValue{0, 0});
    Require(WKPageGetScaleFactor(page.ref) == 0);
    Require(WKPageGetBackingScaleFactor(page.ref) == 1);
    RequireInvalidArgument([] { WKPageSetScaleFactor(nullptr, 1, NKWebKit::PointValue{0, 0}); });
    RequireInvalidArgument([] { WKPageGetScaleFactor(nullptr); });
}

void CheckFlags() {
    PageHandle page;
    Require(!WKPageGetIsControlledByAutomation(page.ref));
    Require(WKPageGetMediaCaptureEnabled(page.ref));
    WKPageSetControlledByAutomation(page.ref, true);
    Require(WKPageGetIsControlledByAutomation(page.ref));
    Require(WKPageGetMediaCaptureEnabled(page.ref));
    WKPageSetMediaCaptureEnabled(page.ref, false);
    Require(!WKPageGetMediaCaptureEnabled(page.ref));
    Require(WKPageGetIsControlledByAutomation(page.ref));
    WKPageSetMediaCaptureEnabled(page.ref, false);
    Require(!WKPageGetMediaCaptureEnabled(page.ref));
    WKPageSetControlledByAutomation(page.ref, false);
    Require(!WKPageGetIsControlledByAutomation(page.ref));
    WKPageSetMediaCaptureEnabled(page.ref, true);
    Require(WKPageGetMediaCaptureEnabled(page.ref));
    RequireInvalidArgument([] { WKPageGetIsControlledByAutomation(nullptr); });
    RequireInvalidArgument([] { WKPageSetControlledByAutomation(nullptr, false); });
    RequireInvalidArgument([] { WKPageGetMediaCaptureEnabled(nullptr); });
    RequireInvalidArgument([] { WKPageSetMediaCaptureEnabled(nullptr, true); });
}

}

int main() {
    CheckBackingScaleFactor();
    CheckScaleFactor();
    CheckFlags();
    return 0;
}

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
#include <string>

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

void RequireEncodingName(const void* pageRef, const std::u16string& expected, std::source_location location = std::source_location::current()) {
    const void* copy = WKPageCopyCustomTextEncodingName(pageRef);
    const auto* string = NKWebKit::Cast<NKWebKit::String>(copy, __func__);
    Require(!string->null, location);
    Require(string->characters == expected, location);
    const void* second = WKPageCopyCustomTextEncodingName(pageRef);
    Require(second != copy, location);
    WKRelease(second);
    WKRelease(copy);
}

void CheckCustomTextEncodingName() {
    PageHandle page;
    RequireEncodingName(page.ref, u"");

    auto name = NKWebKit::Make<NKWebKit::String>(std::u16string(u"ISO-8859-1"));
    const void* nameRef = NKWebKit::ToRef(name);
    WKPageSetCustomTextEncodingName(page.ref, nameRef);
    RequireEncodingName(page.ref, u"ISO-8859-1");
    NKWebKit::Cast<NKWebKit::String>(nameRef, __func__)->characters = u"changed";
    RequireEncodingName(page.ref, u"ISO-8859-1");
    WKRelease(nameRef);
    RequireEncodingName(page.ref, u"ISO-8859-1");

    WKPageSetCustomTextEncodingName(page.ref, nullptr);
    RequireEncodingName(page.ref, u"");
    Require(!NKWebKit::Cast<NKWebKit::Page>(page.ref, __func__)->customTextEncodingName.has_value());

    const void* emptyRef = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    WKPageSetCustomTextEncodingName(page.ref, emptyRef);
    RequireEncodingName(page.ref, u"");
    Require(NKWebKit::Cast<NKWebKit::Page>(page.ref, __func__)->customTextEncodingName == std::u16string());
    WKRelease(emptyRef);

    const void* nullStringRef = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    WKPageSetCustomTextEncodingName(page.ref, nullStringRef);
    Require(!NKWebKit::Cast<NKWebKit::Page>(page.ref, __func__)->customTextEncodingName.has_value());
    RequireEncodingName(page.ref, u"");
    WKRelease(nullStringRef);

    const void* utf16Ref = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"é€\U0001F600")));
    WKPageSetCustomTextEncodingName(page.ref, utf16Ref);
    RequireEncodingName(page.ref, u"é€\U0001F600");
    WKRelease(utf16Ref);

    const void* otherPage = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Page>());
    RequireInvalidArgument([&page, otherPage] { WKPageSetCustomTextEncodingName(page.ref, otherPage); });
    RequireEncodingName(page.ref, u"é€\U0001F600");
    RequireInvalidArgument([otherPage] { WKPageSetCustomTextEncodingName(nullptr, otherPage); });
    RequireInvalidArgument([] { WKPageSetCustomTextEncodingName(nullptr, nullptr); });
    RequireInvalidArgument([] { WKPageCopyCustomTextEncodingName(nullptr); });
    WKRelease(otherPage);
}

void CheckWrongObjectType() {
    const void* stringRef = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"x")));
    RequireInvalidArgument([stringRef] { WKPageCopyCustomTextEncodingName(stringRef); });
    WKRelease(stringRef);
}

}

int main() {
    CheckBackingScaleFactor();
    CheckScaleFactor();
    CheckFlags();
    CheckCustomTextEncodingName();
    CheckWrongObjectType();
    return 0;
}

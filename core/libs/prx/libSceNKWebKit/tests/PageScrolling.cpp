#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage scrolling check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, location);
}

using Getter = bool (APS5_VABI*)(const void*);
using Setter = void (APS5_VABI*)(const void*, bool);

void CheckBoolean(Getter get, Setter set, bool initial, std::source_location location = std::source_location::current()) {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(get(pageRef) == initial, location);
    set(pageRef, !initial);
    Require(get(pageRef) == !initial, location);
    set(pageRef, !initial);
    Require(get(pageRef) == !initial, location);
    set(pageRef, initial);
    Require(get(pageRef) == initial, location);
    RequireInvalidArgument([get] { get(nullptr); }, location);
    RequireInvalidArgument([set] { set(nullptr, true); }, location);
    RequireInvalidArgument([set] { set(nullptr, false); }, location);
    WKRelease(pageRef);
}

void CheckWrongObjectType() {
    auto preferences = NKWebKit::Make<NKWebKit::Preferences>();
    const void* preferencesRef = NKWebKit::ToRef(preferences);
    RequireInvalidArgument([preferencesRef] { WKPageBackgroundExtendsBeyondPage(preferencesRef); });
    RequireInvalidArgument([preferencesRef] { WKPageSetBackgroundExtendsBeyondPage(preferencesRef, false); });
    WKRelease(preferencesRef);
}

}

int main() {
    CheckBoolean(WKPageBackgroundExtendsBeyondPage, WKPageSetBackgroundExtendsBeyondPage, true);
    CheckWrongObjectType();
    return 0;
}

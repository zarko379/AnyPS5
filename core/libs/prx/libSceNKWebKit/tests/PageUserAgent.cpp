#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PageUserAgent check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

bool CopiedEquals(const void* string, const char* expected) {
    const bool equal = string && WKGetTypeID(string) == WKStringGetTypeID() && WKStringIsEqualToUTF8CString(string, expected);
    if (string) WKRelease(string);
    return equal;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    const void* agent = WKStringCreateWithUTF8CString("Agent/1.0");
    const void* name = WKStringCreateWithUTF8CString("App/2.0");
    const void* empty = WKStringCreateWithUTF8CString("");
    const void* number = WKUInt64Create(1);

    Require(!page->customUserAgent.has_value());
    Require(!page->applicationNameForUserAgent.has_value());
    Require(CopiedEquals(WKPageCopyCustomUserAgent(pageRef), ""));
    Require(CopiedEquals(WKPageCopyApplicationNameForUserAgent(pageRef), ""));

    WKPageSetCustomUserAgent(pageRef, agent);
    Require(page->customUserAgent == std::optional<std::u16string>(u"Agent/1.0"));
    Require(CopiedEquals(WKPageCopyCustomUserAgent(pageRef), "Agent/1.0"));
    Require(!page->applicationNameForUserAgent.has_value());

    WKPageSetApplicationNameForUserAgent(pageRef, name);
    Require(page->applicationNameForUserAgent == std::optional<std::u16string>(u"App/2.0"));
    Require(CopiedEquals(WKPageCopyApplicationNameForUserAgent(pageRef), "App/2.0"));
    Require(CopiedEquals(WKPageCopyCustomUserAgent(pageRef), "Agent/1.0"));

    WKPageSetCustomUserAgent(pageRef, empty);
    Require(page->customUserAgent == std::optional<std::u16string>(u""));
    Require(CopiedEquals(WKPageCopyCustomUserAgent(pageRef), ""));

    WKPageSetCustomUserAgent(pageRef, nullptr);
    Require(!page->customUserAgent.has_value());
    WKPageSetApplicationNameForUserAgent(pageRef, nullptr);
    Require(!page->applicationNameForUserAgent.has_value());
    Require(CopiedEquals(WKPageCopyApplicationNameForUserAgent(pageRef), ""));

    WKPageSetCustomUserAgent(pageRef, agent);
    RequireInvalidArgument([&] { WKPageSetCustomUserAgent(pageRef, number); });
    RequireInvalidArgument([&] { WKPageSetApplicationNameForUserAgent(pageRef, number); });
    Require(page->customUserAgent == std::optional<std::u16string>(u"Agent/1.0"));
    Require(!page->applicationNameForUserAgent.has_value());
    RequireInvalidArgument([] { WKPageCopyCustomUserAgent(nullptr); });
    RequireInvalidArgument([] { WKPageCopyApplicationNameForUserAgent(nullptr); });
    RequireInvalidArgument([&] { WKPageSetCustomUserAgent(nullptr, agent); });
    RequireInvalidArgument([&] { WKPageSetApplicationNameForUserAgent(agent, name); });
    RequireInvalidArgument([&] { WKPageCopyCustomUserAgent(agent); });

    WKRelease(number);
    WKRelease(empty);
    WKRelease(name);
    WKRelease(agent);
    WKRelease(pageRef);
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    const void* schemeA = WKStringCreateWithUTF8CString("custom-csp");
    const void* schemeB = WKStringCreateWithUTF8CString("custom-cache");
    const void* schemeC = WKStringCreateWithUTF8CString("custom-display");
    const void* schemeD = WKStringCreateWithUTF8CString("custom-empty");
    const void* schemeE = WKStringCreateWithUTF8CString("custom-secure");

    WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(contextRef, schemeA);
    WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(contextRef, schemeA);
    Require(pool->bypassingCSPSchemes.size() == 1);
    Require(pool->bypassingCSPSchemes[0] == "custom-csp");

    WKContextRegisterURLSchemeAsCachePartitioned(contextRef, schemeB);
    Require(pool->cachePartitionedSchemes.size() == 1);
    Require(pool->cachePartitionedSchemes[0] == "custom-cache");

    WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(contextRef, schemeC);
    Require(pool->canDisplayOnlyIfCanRequestSchemes.size() == 1);
    Require(pool->canDisplayOnlyIfCanRequestSchemes[0] == "custom-display");

    WKContextRegisterURLSchemeAsEmptyDocument(contextRef, schemeD);
    Require(pool->emptyDocumentSchemes.size() == 1);
    Require(pool->emptyDocumentSchemes[0] == "custom-empty");

    WKContextRegisterURLSchemeAsSecure(contextRef, schemeE);
    Require(pool->secureSchemes.size() == 1);
    Require(pool->secureSchemes[0] == "custom-secure");

    RequireThrows([&] { WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(nullptr, schemeA); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsCachePartitioned(contextRef, nullptr); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(schemeA, schemeC); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsEmptyDocument(contextRef, contextRef); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsSecure(nullptr, nullptr); });

    Require(pool->domainRelaxationForbiddenSchemes.empty());
    WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, schemeA);
    WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, schemeB);
    WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, schemeA);
    Require(pool->domainRelaxationForbiddenSchemes.size() == 2);
    Require(pool->domainRelaxationForbiddenSchemes[0] == "custom-csp");
    Require(pool->domainRelaxationForbiddenSchemes[1] == "custom-cache");
    RequireThrows([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(nullptr, schemeA); });
    RequireThrows([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, nullptr); });
    RequireThrows([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(schemeA, schemeB); });
    Require(pool->domainRelaxationForbiddenSchemes.size() == 2);

    const void* fontA = WKStringCreateWithUTF8CString("Font A");
    const void* fontB = WKStringCreateWithUTF8CString("Font B");
    const void* number = WKUInt64Create(7);
    const void* fontItems[] = {fontA, number, fontB};
    const void* fonts = WKArrayCreate(fontItems, 3);
    const void* otherItems[] = {fontB};
    const void* otherFonts = WKArrayCreate(otherItems, 1);
    Require(pool->fontAllowList.empty());
    WKContextSetFontAllowList(contextRef, fonts);
    Require(pool->fontAllowList.size() == 2);
    Require(pool->fontAllowList[0] == u"Font A");
    Require(pool->fontAllowList[1] == u"Font B");
    WKContextSetFontAllowList(contextRef, otherFonts);
    Require(pool->fontAllowList.size() == 1);
    Require(pool->fontAllowList[0] == u"Font B");
    WKContextSetFontAllowList(contextRef, nullptr);
    Require(pool->fontAllowList.empty());
    WKContextSetFontAllowList(contextRef, fonts);
    RequireThrows([&] { WKContextSetFontAllowList(nullptr, fonts); });
    RequireThrows([&] { WKContextSetFontAllowList(contextRef, fontA); });
    Require(pool->fontAllowList.size() == 2);

    Require(!pool->injectedBundleInitializationUserData);
    WKContextSetInitializationUserDataForInjectedBundle(contextRef, number);
    Require(pool->injectedBundleInitializationUserData.Get() == static_cast<const NKWebKit::Object*>(number));
    WKContextSetInitializationUserDataForInjectedBundle(contextRef, fonts);
    Require(pool->injectedBundleInitializationUserData.Get() == static_cast<const NKWebKit::Object*>(fonts));
    WKContextSetInitializationUserDataForInjectedBundle(contextRef, nullptr);
    Require(!pool->injectedBundleInitializationUserData);
    WKContextSetInitializationUserDataForInjectedBundle(contextRef, fontA);
    RequireThrows([&] { WKContextSetInitializationUserDataForInjectedBundle(nullptr, fontA); });
    WKRelease(fontA);
    Require(WKStringIsEqualToUTF8CString(static_cast<const NKWebKit::Object*>(pool->injectedBundleInitializationUserData.Get()), "Font A"));

    WKRelease(otherFonts);
    WKRelease(fonts);
    WKRelease(number);
    WKRelease(fontB);

    WKRelease(schemeE);
    WKRelease(schemeD);
    WKRelease(schemeC);
    WKRelease(schemeB);
    WKRelease(schemeA);
    WKRelease(contextRef);
    std::printf("All libSceNKWebKit ContextURLSchemes tests passed!\n");
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <climits>
#include <cstdint>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit ApiObjects check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static bool StringIs(const void* string, const char* expected) {
    const bool equal = WKStringIsEqualToUTF8CString(string, expected);
    WKRelease(string);
    return equal;
}

static void TestKeyboardEvents() {
    const char text[] = "abc";
    const char identifier[] = "U+0041";
    auto normal = WKKeyboardEventMake(7, 0, text, 3, identifier, 65, -4, 0xFFFFFFFFu, 0x5u);
    Require(normal.type == 7);
    Require(normal.text == text);
    Require(normal.length == 3);
    Require(normal.keyIdentifier == identifier);
    Require(normal.virtualKeyCode == 65);
    Require(normal.caretOffset == -4);
    Require(normal.modifiers == 0x5u);

    auto convert = WKKeyboardEventMake(8, 1, text, 1, identifier, 1, 2, 0, 3);
    Require(std::strcmp(convert.keyIdentifier, "Convert") == 0);
    Require(convert.text == text);
    Require(convert.length == 1);
    Require(convert.type == 8);
    Require(convert.virtualKeyCode == 1);
    Require(convert.caretOffset == 2);
    Require(convert.modifiers == 3);
    Require(std::strcmp(WKKeyboardEventMake(7, 2, text, 1, identifier, 0, 0, 0, 0).keyIdentifier, "Accept") == 0);
    Require(std::strcmp(WKKeyboardEventMake(7, 3, text, 1, identifier, 0, 0, 0, 0).keyIdentifier, "Cancel") == 0);
    Require(WKKeyboardEventMake(7, 4, text, 1, identifier, 0, 0, 0, 0).keyIdentifier == identifier);
    Require(WKKeyboardEventMake(7, 255, text, 1, nullptr, 0, 0, 0, 0).keyIdentifier == nullptr);

    auto empty = WKKeyboardEventMake(-1, 0, text, 0, identifier, 0, 0, 0, 0);
    Require(empty.type == -1);
    Require(empty.text == nullptr);
    Require(empty.length == 0);
    Require(empty.keyIdentifier == identifier);

    auto wide = WKKeyboardEventMake(7, 0, text, 0x80000000u, identifier, 0, 0, 0, 0);
    Require(wide.text == text);
    Require(wide.length == INT_MIN);
}

static void TestMouseAndWheelEvents() {
    auto mouse = WKMouseEventMake(1, -2, NKWebKit::PointValue{12.5, -3.25}, 2, 0x10u);
    Require(mouse.type == 1);
    Require(mouse.button == -2);
    Require(mouse.position.x == 12.5);
    Require(mouse.position.y == -3.25);
    Require(mouse.clickCount == 2);
    Require(mouse.modifiers == 0x10u);

    auto wheel = WKWheelEventMake(6, NKWebKit::PointValue{1.5, 2.5}, NKWebKit::SizeValue{-10.0, 20.0}, NKWebKit::SizeValue{-1.0, 2.0}, 0x3u);
    Require(wheel.type == 6);
    Require(wheel.position.x == 1.5);
    Require(wheel.position.y == 2.5);
    Require(wheel.delta.width == -10.0);
    Require(wheel.delta.height == 20.0);
    Require(wheel.wheelTicks.width == -1.0);
    Require(wheel.wheelTicks.height == 2.0);
    Require(wheel.modifiers == 0x3u);
}

static const void* OriginFromIdentifier(const char* identifier) {
    const void* string = WKStringCreateWithUTF8CString(identifier);
    const void* origin = WKSecurityOriginCreateFromDatabaseIdentifier(string);
    WKRelease(string);
    return origin;
}

static bool OriginIs(const void* origin, const char* protocol, const char* host, std::uint16_t port) {
    if (!origin) return false;
    const bool matches = WKGetTypeID(origin) == WKSecurityOriginGetTypeID() && StringIs(WKSecurityOriginCopyProtocol(origin), protocol) &&
        StringIs(WKSecurityOriginCopyHost(origin), host) && WKSecurityOriginGetPort(origin) == port;
    return matches;
}

static bool IdentifierIs(const char* identifier, const char* protocol, const char* host, std::uint16_t port, const char* roundTrip) {
    const void* origin = OriginFromIdentifier(identifier);
    const bool matches = OriginIs(origin, protocol, host, port) && StringIs(WKSecurityOriginCopyDatabaseIdentifier(origin), roundTrip);
    if (origin) WKRelease(origin);
    return matches;
}

static void TestSecurityOriginDatabaseIdentifiers() {
    Require(IdentifierIs("https_example.com_8443", "https", "example.com", 8443, "https_example.com_8443"));
    Require(IdentifierIs("http_my_intranet_host_81", "http", "my_intranet_host", 81, "http_my_intranet_host_81"));
    Require(IdentifierIs("foo_h_80", "foo", "h", 80, "foo_h_80"));
    Require(IdentifierIs("HTTP_h_80", "HTTP", "h", 80, "HTTP_h_80"));

    Require(IdentifierIs("http_h_80", "http", "h", 0, "http_h_0"));
    Require(IdentifierIs("https_h_443", "https", "h", 0, "https_h_0"));
    Require(IdentifierIs("ftp_h_21", "ftp", "h", 0, "ftp_h_0"));
    Require(IdentifierIs("ws_h_80", "ws", "h", 0, "ws_h_0"));
    Require(IdentifierIs("wss_h_443", "wss", "h", 0, "wss_h_0"));
    Require(IdentifierIs("wss_h_80", "wss", "h", 80, "wss_h_80"));
    Require(IdentifierIs("http_h_00080", "http", "h", 0, "http_h_0"));

    Require(IdentifierIs("http_h_0", "http", "h", 0, "http_h_0"));
    Require(IdentifierIs("http_h_", "http", "h", 0, "http_h_0"));
    Require(IdentifierIs("http_h_ \t\n\r\f\v8080", "http", "h", 8080, "http_h_8080"));
    Require(IdentifierIs("http_h_+8080", "http", "h", 8080, "http_h_8080"));
    Require(IdentifierIs("http_h_ +8080", "http", "h", 8080, "http_h_8080"));
    Require(IdentifierIs("http_h_8080junk", "http", "h", 8080, "http_h_8080"));
    Require(IdentifierIs("http_h_65535", "http", "h", 65535, "http_h_65535"));
    Require(OriginFromIdentifier("http_h_65536") == nullptr);
    Require(OriginFromIdentifier("http_h_99999999999999999999") == nullptr);
    Require(OriginFromIdentifier("http_h_x") == nullptr);
    Require(OriginFromIdentifier("http_h_-1") == nullptr);
    Require(OriginFromIdentifier("http_h_+") == nullptr);
    Require(OriginFromIdentifier("http_h_++1") == nullptr);
    Require(OriginFromIdentifier("http_h_+ 1") == nullptr);
    Require(OriginFromIdentifier("http_h_ ") == nullptr);
    Require(OriginFromIdentifier("http_h_\xC2\xA0" "1") == nullptr);

    Require(OriginFromIdentifier("") == nullptr);
    Require(OriginFromIdentifier("nounderscore") == nullptr);
    Require(OriginFromIdentifier("http_80") == nullptr);
    Require(OriginFromIdentifier("_") == nullptr);
    Require(IdentifierIs("__", "", "", 0, "__0"));
    Require(IdentifierIs("_host_81", "", "host", 81, "_host_81"));

    const void* empty = OriginFromIdentifier("__");
    Require(StringIs(WKSecurityOriginCopyToString(empty), ""));
    WKRelease(empty);
    const void* hostOnly = OriginFromIdentifier("_host_81");
    Require(StringIs(WKSecurityOriginCopyToString(hostOnly), "://host:81"));
    WKRelease(hostOnly);
    const void* withPort = OriginFromIdentifier("https_example.com_8443");
    Require(StringIs(WKSecurityOriginCopyToString(withPort), "https://example.com:8443"));
    WKRelease(withPort);

    Require(IdentifierIs("file_ignored_99", "file", "ignored", 99, "file__0"));
    Require(IdentifierIs("FiLe_x_1", "FiLe", "x", 1, "file__0"));
    Require(IdentifierIs("fileX_x_1", "fileX", "x", 1, "fileX_x_1"));

    Require(IdentifierIs("http_a%3Ab_81", "http", "a%3Ab", 81, "http_a%253Ab_81"));
    Require(IdentifierIs("http_a b_81", "http", "a b", 81, "http_a%20b_81"));
    Require(IdentifierIs("http_a/b:c*d_81", "http", "a/b:c*d", 81, "http_a%2Fb%3Ac%2Ad_81"));
    Require(IdentifierIs("http_\xC3\xA9_81", "http", "\xC3\xA9", 81, "http_\xC3\xA9_81"));

    const unsigned char foo[] = "foo";
    NKWebKit::WtfAbi::String fooScheme = _ZN3WTF10StringImpl6createEPKhj(foo, 3);
    _ZN3WTF40registerDefaultPortForProtocolForTestingEtRKNS_6StringE(1234, fooScheme);
    Require(IdentifierIs("foo_h_1234", "foo", "h", 0, "foo_h_0"));
    _ZN3WTF40clearDefaultPortForProtocolMapForTestingEv();
    Require(IdentifierIs("foo_h_1234", "foo", "h", 1234, "foo_h_1234"));

    const void* protocol = WKStringCreateWithUTF8CString("https");
    const void* host = WKStringCreateWithUTF8CString("example.org");
    const void* created = WKSecurityOriginCreate(protocol, host, 8443);
    Require(StringIs(WKSecurityOriginCopyDatabaseIdentifier(created), "https_example.org_8443"));
    WKRelease(created);
    const void* noPort = WKSecurityOriginCreate(protocol, host, 0);
    Require(StringIs(WKSecurityOriginCopyDatabaseIdentifier(noPort), "https_example.org_0"));
    WKRelease(noPort);
    WKRelease(host);
    WKRelease(protocol);

    Require(Throws([] { WKSecurityOriginCreateFromDatabaseIdentifier(nullptr); }));
    Require(Throws([] { WKSecurityOriginCopyDatabaseIdentifier(nullptr); }));
    const void* notAnOrigin = WKStringCreateWithUTF8CString("http_h_81");
    Require(Throws([notAnOrigin] { WKSecurityOriginCopyDatabaseIdentifier(notAnOrigin); }));
    const void* url = WKURLCreateWithUTF8CString("http_h_81");
    Require(Throws([url] { WKSecurityOriginCreateFromDatabaseIdentifier(url); }));
    WKRelease(url);
    WKRelease(notAnOrigin);
}

static void TestDefaultTimeoutInterval() {
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 0.0);
    WKURLRequestSetDefaultTimeoutInterval(12.5);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 12.5);
    WKURLRequestSetDefaultTimeoutInterval(-1.0);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == -1.0);
    WKURLRequestSetDefaultTimeoutInterval(0.0);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 0.0);
}

static void TestWebsiteDataStores() {
    const void* defaultStore = WKWebsiteDataStoreGetDefaultDataStore();
    Require(defaultStore != nullptr);
    Require(WKWebsiteDataStoreGetDefaultDataStore() == defaultStore);
    Require(WKGetTypeID(defaultStore) == WKWebsiteDataStoreGetTypeID());
    const void* context = WKContextCreate();
    auto getContextDataStore = reinterpret_cast<const void* (APS5_VABI*)(const void*)>(&WKContextGetWebsiteDataStore);
    Require(getContextDataStore(context) == defaultStore);
    WKRelease(context);

    const void* ephemeral = WKWebsiteDataStoreCreateNonPersistentDataStore();
    Require(ephemeral != nullptr && ephemeral != defaultStore);
    Require(WKGetTypeID(ephemeral) == WKWebsiteDataStoreGetTypeID());
    const void* configuration = WKWebsiteDataStoreConfigurationCreate();
    const void* configured = WKWebsiteDataStoreCreateWithConfiguration(configuration);
    Require(configured != nullptr && configured != defaultStore && configured != ephemeral);
    Require(WKGetTypeID(configured) == WKWebsiteDataStoreGetTypeID());
    const void* secondConfigured = WKWebsiteDataStoreCreateWithConfiguration(configuration);
    Require(secondConfigured != configured);
    WKRelease(secondConfigured);

    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(defaultStore));
    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(ephemeral));
    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(configured));
    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(ephemeral, true);
    Require(WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(ephemeral));
    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(configured));
    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(defaultStore));
    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(ephemeral, true);
    Require(WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(ephemeral));
    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(ephemeral, false);
    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(ephemeral));

    const void* cookieStore = WKWebsiteDataStoreGetHTTPCookieStore(configured);
    Require(cookieStore != nullptr);
    Require(WKGetTypeID(cookieStore) == WKHTTPCookieStoreGetTypeID());
    Require(WKWebsiteDataStoreGetHTTPCookieStore(configured) == cookieStore);
    const void* otherCookieStore = WKWebsiteDataStoreGetHTTPCookieStore(ephemeral);
    Require(otherCookieStore != cookieStore);
    Require(WKWebsiteDataStoreGetHTTPCookieStore(defaultStore) == WKWebsiteDataStoreGetHTTPCookieStore(defaultStore));

    Require(Throws([] { WKWebsiteDataStoreCreateWithConfiguration(nullptr); }));
    Require(Throws([ephemeral] { WKWebsiteDataStoreCreateWithConfiguration(ephemeral); }));
    Require(Throws([] { WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(nullptr); }));
    Require(Throws([] { WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(nullptr, true); }));
    Require(Throws([configuration] { WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(configuration, true); }));
    Require(Throws([] { WKWebsiteDataStoreGetHTTPCookieStore(nullptr); }));
    Require(Throws([configuration] { WKWebsiteDataStoreGetHTTPCookieStore(configuration); }));

    WKRelease(configured);
    WKRelease(configuration);
    WKRelease(ephemeral);
}

static void TestScriptWorlds() {
    const void* first = WKBundleScriptWorldCreateWorld();
    const void* second = WKBundleScriptWorldCreateWorld();
    Require(first != nullptr && second != nullptr && first != second);
    Require(first != WKBundleScriptWorldNormalWorld());
    Require(WKGetTypeID(first) == WKBundleScriptWorldGetTypeID());
    const void* firstName = WKBundleScriptWorldCopyName(first);
    const void* secondName = WKBundleScriptWorldCopyName(second);
    char firstText[64] = {};
    char secondText[64] = {};
    WKStringGetUTF8CString(firstName, firstText, sizeof(firstText));
    WKStringGetUTF8CString(secondName, secondText, sizeof(secondText));
    WKRelease(firstName);
    WKRelease(secondName);
    Require(std::strncmp(firstText, "UniqueWorld_", 12) == 0);
    Require(std::strncmp(secondText, "UniqueWorld_", 12) == 0);
    char* end = nullptr;
    const auto firstNumber = std::strtoull(firstText + 12, &end, 10);
    Require(end != firstText + 12 && *end == 0);
    const auto secondNumber = std::strtoull(secondText + 12, &end, 10);
    Require(end != secondText + 12 && *end == 0);
    Require(secondNumber == firstNumber + 1);
    WKRelease(first);
    WKRelease(second);
}

static NKWebKit::JSValueRef Evaluate(NKWebKit::JSContextRef context, const char* code) {
    NKWebKit::JSStringRef script = JSStringCreateWithUTF8CString_nid_postfix(code);
    NKWebKit::JSValueRef value = JSEvaluateScript(context, script, nullptr, nullptr, 1, nullptr);
    JSStringRelease_nid_postfix(script);
    return value;
}

static bool DataIs(const void* data, std::initializer_list<std::uint8_t> expected) {
    const bool matches = WKGetTypeID(data) == WKDataGetTypeID() && WKDataGetSize(data) == expected.size() &&
        (expected.size() == 0 || std::memcmp(WKDataGetBytes(data), expected.begin(), expected.size()) == 0);
    WKRelease(data);
    return matches;
}

static void TestDataFromUint8Array() {
    NKWebKit::JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
    Require(DataIs(WKBundleCreateWKDataFromUInt8Array(nullptr, context, Evaluate(context, "new Uint8Array([1, 2, 3, 250])")), {1, 2, 3, 250}));
    Require(DataIs(WKBundleCreateWKDataFromUInt8Array(nullptr, context, Evaluate(context, "new Uint8Array([9, 8, 7, 6, 5]).subarray(1, 4)")), {8, 7, 6}));
    Require(DataIs(WKBundleCreateWKDataFromUInt8Array(nullptr, context, Evaluate(context, "new Uint8Array(new ArrayBuffer(8), 8)")), {}));
    Require(DataIs(WKBundleCreateWKDataFromUInt8Array(nullptr, context, Evaluate(context, "new Uint8Array(0)")), {}));

    NKWebKit::JSValueRef array = Evaluate(context, "var shared = new Uint8Array([4, 5]); shared");
    const void* copy = WKBundleCreateWKDataFromUInt8Array(nullptr, context, array);
    Evaluate(context, "shared[0] = 99");
    Require(DataIs(copy, {4, 5}));

    NKWebKit::JSValueRef clamped = Evaluate(context, "new Uint8ClampedArray([1])");
    NKWebKit::JSValueRef wide = Evaluate(context, "new Uint16Array([1])");
    NKWebKit::JSValueRef buffer = Evaluate(context, "new ArrayBuffer(4)");
    NKWebKit::JSValueRef plain = Evaluate(context, "[1, 2]");
    NKWebKit::JSValueRef number = JSValueMakeNumber(context, 1);
    NKWebKit::JSValueRef shared = Evaluate(context, "new Uint8Array(new SharedArrayBuffer(4))");
    Require(JSValueGetTypedArrayType(context, shared, nullptr) == NKWebKit::kJSTypedArrayTypeUint8Array);
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, shared); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, clamped); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, wide); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, buffer); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, plain); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, number); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, context, nullptr); }));
    Require(Throws([&] { WKBundleCreateWKDataFromUInt8Array(nullptr, nullptr, array); }));
    JSGlobalContextRelease(context);
}

static void TestSessionValueTypes() {
    const void* url = WKPageGetSessionHistoryURLValueType();
    const void* item = WKPageGetSessionBackForwardListItemValueType();
    Require(url != nullptr && item != nullptr && url != item);
    Require(WKPageGetSessionHistoryURLValueType() == url);
    Require(WKPageGetSessionBackForwardListItemValueType() == item);
    Require(WKGetTypeID(url) == WKStringGetTypeID());
    Require(WKStringIsEqualToUTF8CString(url, "SessionHistoryURL"));
    Require(WKStringIsEqualToUTF8CString(item, "SessionBackForwardListItem"));
}

static void TestEnhancedAccessibility() {
    auto enable = reinterpret_cast<void (APS5_VABI*)(bool)>(&WKAccessibilityEnableEnhancedAccessibility);
    Require(!WKAccessibilityEnhancedAccessibilityEnabled());
    enable(true);
    Require(WKAccessibilityEnhancedAccessibilityEnabled());
    _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(false);
    Require(!WKAccessibilityEnhancedAccessibilityEnabled());
    _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(true);
    Require(WKAccessibilityEnhancedAccessibilityEnabled());
    enable(false);
    Require(!WKAccessibilityEnhancedAccessibilityEnabled());
}

static int callbackCalls = 0;

static void APS5_VABI CountCallback() {
    ++callbackCalls;
}

static void TestEmptyExports() {
    const auto before = NotImplementedCalls_nid_no_patch();
    using Callback = void (APS5_VABI*)();
    using CallbackExport = void (APS5_VABI*)(const void*, void*, Callback);
    using OriginCallbackExport = void (APS5_VABI*)(const void*, const void*, std::uint32_t);
    const void* store = WKWebsiteDataStoreGetDefaultDataStore();
    reinterpret_cast<CallbackExport>(&WKCookieManagerGetHostnamesWithCookies)(store, nullptr, &CountCallback);
    reinterpret_cast<CallbackExport>(&WKCookieManagerGetHTTPCookieAcceptPolicy)(store, nullptr, &CountCallback);
    reinterpret_cast<CallbackExport>(&WKResourceCacheManagerGetCacheOrigins)(store, nullptr, &CountCallback);
    reinterpret_cast<OriginCallbackExport>(&WKResourceCacheManagerClearCacheForOrigin)(store, nullptr, 0);
    Require(callbackCalls == 0);
    WKCookieManagerSetClient();
    WKCookieManagerDeleteCookiesForHostname();
    WKCookieManagerDeleteAllCookiesModifiedAfterDate();
    WKCookieManagerStartObservingCookieChanges();
    WKCookieManagerStopObservingCookieChanges();
    WKResourceCacheManagerClearCacheForAllOrigins();
    WKContextAllowSpecificHTTPSCertificateForHost();
    WKContextPreconnectToServer();
    WKContextSetCanHandleHTTPSServerTrustEvaluation();
    WKContextSetCustomWebContentServiceBundleIdentifier();
    WKContextSetDiskCacheSpeculativeValidationEnabled();
    WKContextSetMaximumNumberOfProcesses();
    WKFrameStopLoading();
    WKPageLoadWebArchiveData();
    WKPageLoadWebArchiveDataWithUserData();
    WKPageSetPaginationLineGridEnabled();
    WKBundleBackForwardListClear();
    WKBundlePageCopyRenderLayerTree();
    WKBundlePageCopyRenderTree();
    WKBundlePageSetDefersLoading();
    WKBundlePageSetPolicyClient();
    Require(NotImplementedCalls_nid_no_patch() == before);
}

static void TestContextSupplements() {
    const void* context = WKContextCreate();
    const void* other = WKContextCreate();
    const void* geolocation = WKContextGetGeolocationManager(context);
    const void* notification = WKContextGetNotificationManager(context);
    Require(geolocation != nullptr && notification != nullptr);
    Require(WKGetTypeID(geolocation) == WKGeolocationManagerGetTypeID());
    Require(WKGetTypeID(notification) == WKNotificationManagerGetTypeID());
    Require(WKContextGetGeolocationManager(context) == geolocation);
    Require(WKContextGetNotificationManager(context) == notification);
    Require(WKContextGetGeolocationManager(other) != geolocation);
    Require(WKContextGetNotificationManager(other) != notification);
    Require(Throws([] { WKContextGetGeolocationManager(nullptr); }));
    Require(Throws([] { WKContextGetNotificationManager(nullptr); }));
    Require(Throws([geolocation] { WKContextGetNotificationManager(geolocation); }));
    WKRelease(other);
    WKRelease(context);
}

template<typename Function>
static bool ThrowsNotImplemented(Function function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        function();
    } catch (const std::runtime_error&) {
        return NotImplementedCalls_nid_no_patch() == before + 1;
    }
    return false;
}

static NKWebKit::WtfAbi::StringImpl* RawImpl(unsigned length, unsigned flags, std::size_t tail = 0) {
    auto* impl = static_cast<NKWebKit::WtfAbi::StringImpl*>(_ZN3WTF10fastMallocEm(sizeof(NKWebKit::WtfAbi::StringImpl) + tail));
    *impl = NKWebKit::WtfAbi::StringImpl{0, length, "", flags};
    if (tail) std::memset(impl + 1, 0, tail);
    return impl;
}

static void TestStringImplDestroy() {
    const char16_t text[] = u"a fairly long string used to share a substring buffer";
    NKWebKit::WtfAbi::String base = _ZN3WTF10StringImpl6createEPKDsj(text, 54);
    Require(base.impl->refCount == 2);
    NKWebKit::WtfAbi::String part = _ZNK3WTF6String20substringSharingImplEjj(&base, 2, 40);
    Require((part.impl->hashAndFlags & 3) == 2);
    Require(base.impl->refCount == 4);
    _ZN3WTF10StringImpl7destroyEPS0_(part.impl);
    part.impl = nullptr;
    Require(base.impl->refCount == 2);

    NKWebKit::WtfAbi::String internal = _ZN3WTF10StringImpl6createEPKDsj(text, 54);
    _ZN3WTF10StringImpl7destroyEPS0_(internal.impl);
    internal.impl = nullptr;

    auto* owned = RawImpl(3, 1 | 4);
    auto* buffer = static_cast<char*>(_ZN3WTF10fastMallocEm(3));
    std::memcpy(buffer, "abc", 3);
    owned->data = buffer;
    _ZN3WTF10StringImpl7destroyEPS0_(owned);

    _ZN3WTF10StringImpl7destroyEPS0_(RawImpl(0, 16 | 4));

    auto* atom = RawImpl(1, 16 | 4);
    Require(ThrowsNotImplemented([atom] { _ZN3WTF10StringImpl7destroyEPS0_(atom); }));
    auto* symbol = RawImpl(0, 32 | 2 | 4, 8);
    Require(ThrowsNotImplemented([symbol] { _ZN3WTF10StringImpl7destroyEPS0_(symbol); }));
    auto* external = RawImpl(1, 3 | 4);
    Require(ThrowsNotImplemented([external] { _ZN3WTF10StringImpl7destroyEPS0_(external); }));
    Require(Throws([] { _ZN3WTF10StringImpl7destroyEPS0_(nullptr); }));
}

int main() {
    TestContextSupplements();
    TestStringImplDestroy();
    TestScriptWorlds();
    TestDataFromUint8Array();
    TestSessionValueTypes();
    TestEnhancedAccessibility();
    TestEmptyExports();
    TestKeyboardEvents();
    TestMouseAndWheelEvents();
    TestSecurityOriginDatabaseIdentifiers();
    TestDefaultTimeoutInterval();
    TestWebsiteDataStores();
    std::printf("All libSceNKWebKit ApiObjects tests passed!\n");
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
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
        std::fprintf(stderr, "ContextConfigurationCopy check failed at line %u\n", location.line());
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

const void* AsRef(const NKWebKit::ProcessPoolConfiguration* configuration) {
    return static_cast<const NKWebKit::Object*>(configuration);
}

void TestCreateWithConfigurationCopies() {
    const void* config = WKContextConfigurationCreate();
    const void* bundle = WKStringCreateWithUTF8CString("/app0/bundle.js");
    const void* webProcess = WKStringCreateWithUTF8CString("/app0/web.self");
    const void* networkProcess = WKStringCreateWithUTF8CString("/app0/network.self");
    const void* cookiePath = WKStringCreateWithUTF8CString("/app0/cookies");
    const void* otherBundle = WKStringCreateWithUTF8CString("/app0/other.js");
    const void* language = WKStringCreateWithUTF8CString("en-US");
    const void* languages = WKArrayCreate(&language, 1);

    WKContextConfigurationSetInjectedBundlePath(config, bundle);
    WKContextConfigurationSetWebProcessPath(config, webProcess);
    WKContextConfigurationSetNetworkProcessPath(config, networkProcess);
    WKContextConfigurationSetCookieStoragePath(config, cookiePath);
    WKContextConfigurationSetUserId(config, 7);
    WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(config, true);
    WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(config, true);
    WKContextConfigurationSetShouldConfigureJSCForTesting(config, true);
    WKContextConfigurationSetProcessSwapsOnNavigation(config, true);
    WKContextConfigurationSetProcessSwapsOnWindowOpenWithOpener(config, true);
    WKContextConfigurationSetAlwaysKeepAndReuseSwappedProcesses(config, true);
    WKContextConfigurationSetPrewarmsProcessesAutomatically(config, true);
    WKContextConfigurationSetUsesWebProcessCache(config, true);
    WKContextConfigurationSetOverrideLanguages(config, languages);
    auto* source = NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(config, "test");
    source->processSwapsOnNavigationFromExperimentalFeatures = true;
    source->clientWouldBenefitFromAutomaticProcessPrewarming = true;

    const void* context = WKContextCreateWithConfiguration(config);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(context, "test");
    const auto* copied = pool->configuration.Get();
    Require(copied != nullptr);
    Require(copied != source);
    Require(copied->injectedBundlePath == std::optional<std::u16string>(u"/app0/bundle.js"));
    Require(copied->webProcessPath == std::optional<std::u16string>(u"/app0/web.self"));
    Require(copied->networkProcessPath == std::optional<std::u16string>(u"/app0/network.self"));
    Require(!copied->cookieStoragePath.has_value());
    Require(copied->userId == 7);
    Require(copied->fullySynchronousModeIsAllowedForTesting);
    Require(copied->ignoreSynchronousMessagingTimeoutsForTesting);
    Require(copied->shouldConfigureJSCForTesting);
    Require(copied->processSwapsOnNavigationFromClient == std::optional<bool>(true));
    Require(copied->processSwapsOnNavigationFromExperimentalFeatures);
    Require(copied->processSwapsOnWindowOpenWithOpener);
    Require(copied->alwaysKeepAndReuseSwappedProcesses);
    Require(copied->isAutomaticProcessWarmingEnabledByClient == std::optional<bool>(true));
    Require(!copied->clientWouldBenefitFromAutomaticProcessPrewarming);
    Require(copied->usesWebProcessCache);
    Require(copied->overrideLanguages.size() == 1);
    Require(copied->overrideLanguages[0] == u"en-US");

    WKContextConfigurationSetInjectedBundlePath(config, otherBundle);
    WKContextConfigurationSetWebProcessPath(config, otherBundle);
    WKContextConfigurationSetNetworkProcessPath(config, otherBundle);
    WKContextConfigurationSetUserId(config, 9);
    WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(config, false);
    WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(config, false);
    WKContextConfigurationSetShouldConfigureJSCForTesting(config, false);
    WKContextConfigurationSetProcessSwapsOnNavigation(config, false);
    WKContextConfigurationSetProcessSwapsOnWindowOpenWithOpener(config, false);
    WKContextConfigurationSetAlwaysKeepAndReuseSwappedProcesses(config, false);
    WKContextConfigurationSetPrewarmsProcessesAutomatically(config, false);
    WKContextConfigurationSetUsesWebProcessCache(config, false);
    WKContextConfigurationSetOverrideLanguages(config, nullptr);
    source->processSwapsOnNavigationFromExperimentalFeatures = false;

    Require(copied->injectedBundlePath == std::optional<std::u16string>(u"/app0/bundle.js"));
    Require(copied->webProcessPath == std::optional<std::u16string>(u"/app0/web.self"));
    Require(copied->networkProcessPath == std::optional<std::u16string>(u"/app0/network.self"));
    Require(WKContextConfigurationGetUserId(AsRef(copied)) == 7);
    Require(WKContextConfigurationFullySynchronousModeIsAllowedForTesting(AsRef(copied)));
    Require(WKContextConfigurationIgnoreSynchronousMessagingTimeoutsForTesting(AsRef(copied)));
    Require(copied->shouldConfigureJSCForTesting);
    Require(WKContextConfigurationProcessSwapsOnNavigation(AsRef(copied)));
    Require(copied->processSwapsOnNavigationFromExperimentalFeatures);
    Require(WKContextConfigurationProcessSwapsOnWindowOpenWithOpener(AsRef(copied)));
    Require(WKContextConfigurationAlwaysKeepAndReuseSwappedProcesses(AsRef(copied)));
    Require(WKContextConfigurationPrewarmsProcessesAutomatically(AsRef(copied)));
    Require(WKContextConfigurationUsesWebProcessCache(AsRef(copied)));
    Require(copied->overrideLanguages.size() == 1);
    Require(source->overrideLanguages.empty());
    Require(source->userId == 9);

    WKContextSetPrewarmsProcessesAutomatically(context, false);
    Require(copied->isAutomaticProcessWarmingEnabledByClient == std::optional<bool>(false));
    Require(!WKContextConfigurationPrewarmsProcessesAutomatically(AsRef(copied)));
    WKContextSetPrewarmsProcessesAutomatically(context, true);
    Require(WKContextConfigurationPrewarmsProcessesAutomatically(AsRef(copied)));
    Require(source->isAutomaticProcessWarmingEnabledByClient == std::optional<bool>(false));

    WKRelease(config);
    Require(copied->userId == 7);
    WKRelease(context);
    WKRelease(languages);
    WKRelease(language);
    WKRelease(otherBundle);
    WKRelease(cookiePath);
    WKRelease(networkProcess);
    WKRelease(webProcess);
    WKRelease(bundle);
}

void TestCreateWithoutConfiguration() {
    const void* context = WKContextCreateWithConfiguration(nullptr);
    const auto* configuration = NKWebKit::Cast<NKWebKit::ProcessPool>(context, "test")->configuration.Get();
    Require(configuration != nullptr);
    Require(configuration->userId == -1);
    Require(!configuration->injectedBundlePath.has_value());
    Require(!configuration->isAutomaticProcessWarmingEnabledByClient.has_value());
    WKContextSetPrewarmsProcessesAutomatically(context, true);
    Require(configuration->isAutomaticProcessWarmingEnabledByClient == std::optional<bool>(true));
    WKRelease(context);
}

void TestInvalidReferences() {
    const void* string = WKStringCreateWithUTF8CString("configuration");
    RequireInvalidArgument([&] { WKContextCreateWithConfiguration(string); });
    RequireInvalidArgument([] { WKContextSetPrewarmsProcessesAutomatically(nullptr, true); });
    RequireInvalidArgument([&] { WKContextSetPrewarmsProcessesAutomatically(string, true); });
    WKRelease(string);
}

}

int main() {
    TestCreateWithConfigurationCopies();
    TestCreateWithoutConfiguration();
    TestInvalidReferences();
    return 0;
}

#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

namespace {

NKWebKit::WebsiteDataStore* DefaultDataStore() {
    static NKWebKit::WebsiteDataStore* dataStore = NKWebKit::Make<NKWebKit::WebsiteDataStore>().Leak();
    return dataStore;
}

}  // namespace

extern "C" {

const void* APS5_VABI WKWebsiteDataStoreGetDefaultDataStore() {
    return static_cast<const NKWebKit::Object*>(DefaultDataStore());
}

const void* APS5_VABI WKWebsiteDataStoreCreateNonPersistentDataStore() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::WebsiteDataStore>());
}

const void* APS5_VABI WKWebsiteDataStoreCreateWithConfiguration(const void* configuration) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::WebsiteDataStore>());
}

bool APS5_VABI WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(const void* dataStore) {
    return NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__)->trackingPreventionEnabled;
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(const void* dataStore, bool enable) {
    auto* websiteDataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    websiteDataStore->trackingPreventionEnabled = enable;
}

const void* APS5_VABI WKWebsiteDataStoreGetHTTPCookieStore(const void* dataStore) {
    auto* websiteDataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    if (!websiteDataStore->cookieStore) websiteDataStore->cookieStore = NKWebKit::Make<NKWebKit::HTTPCookieStore>();
    return static_cast<const NKWebKit::Object*>(websiteDataStore->cookieStore.Get());
}

}

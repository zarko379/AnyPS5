#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <optional>
#include <string>

namespace {

std::optional<std::u16string> UserAgentString(const void* stringRef, const char* function) {
    if (!stringRef) return std::nullopt;
    return NKWebKit::OptionalString(stringRef, function);
}

}  // namespace

extern "C" {

const void* APS5_VABI WKPageCopyCustomUserAgent(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->customUserAgent);
}

void APS5_VABI WKPageSetCustomUserAgent(const void* pageRef, const void* userAgentRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->customUserAgent = UserAgentString(userAgentRef, __func__);
}

const void* APS5_VABI WKPageCopyApplicationNameForUserAgent(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->applicationNameForUserAgent);
}

void APS5_VABI WKPageSetApplicationNameForUserAgent(const void* pageRef, const void* applicationNameRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->applicationNameForUserAgent = UserAgentString(applicationNameRef, __func__);
}

}

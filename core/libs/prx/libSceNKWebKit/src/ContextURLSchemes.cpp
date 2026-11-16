#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

void RegisterScheme(const void* contextRef, const void* urlSchemeRef, std::vector<std::string> NKWebKit::ProcessPool::*member, const char* func) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, func);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, func);
    auto& schemes = context->*member;
    auto utf8 = scheme->Utf8();
    if (std::find(schemes.begin(), schemes.end(), utf8) == schemes.end()) {
        schemes.push_back(std::move(utf8));
    }
}

}  // namespace

extern "C" {

void APS5_VABI WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::bypassingCSPSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsCachePartitioned(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::cachePartitionedSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::canDisplayOnlyIfCanRequestSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsEmptyDocument(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::emptyDocumentSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsSecure(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::secureSchemes, __func__);
}

void APS5_VABI WKContextSetDomainRelaxationForbiddenForURLScheme(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::domainRelaxationForbiddenSchemes, __func__);
}

void APS5_VABI WKContextSetFontAllowList(const void* contextRef, const void* arrayRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* array = NKWebKit::CastOrNull<NKWebKit::Array>(arrayRef, __func__);
    context->fontAllowList.clear();
    if (!array) return;
    for (const auto& element : array->elements) {
        if (element && element->Type() == NKWebKit::ObjectType::String)
            context->fontAllowList.push_back(static_cast<const NKWebKit::String*>(element.Get())->characters);
    }
}

void APS5_VABI WKContextSetInitializationUserDataForInjectedBundle(const void* contextRef, const void* userDataRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    auto* userData = userDataRef ? const_cast<NKWebKit::Object*>(NKWebKit::ToObject(userDataRef, __func__)) : nullptr;
    context->injectedBundleInitializationUserData = NKWebKit::Retained<NKWebKit::Object>::Share(userData);
}

}

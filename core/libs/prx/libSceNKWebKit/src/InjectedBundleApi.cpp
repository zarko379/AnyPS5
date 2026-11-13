#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <atomic>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<std::uint64_t> uniqueWorldNameNumber{0};
std::atomic<bool> enhancedUserInterfaceAccessibility{false};

std::u16string UniqueWorldName() {
    const auto text = "UniqueWorld_" + std::to_string(uniqueWorldNameNumber.fetch_add(1, std::memory_order_relaxed));
    return {text.begin(), text.end()};
}

}  // namespace

extern "C" {

const void* APS5_VABI WKBundleScriptWorldCreateWorld() {
    auto world = NKWebKit::Make<NKWebKit::BundleScriptWorld>();
    world->name = NKWebKit::Make<NKWebKit::String>(UniqueWorldName());
    return NKWebKit::ToRef(std::move(world));
}

const void* APS5_VABI WKBundleCreateWKDataFromUInt8Array(const void*, NKWebKit::JSContextRef context, NKWebKit::JSValueRef data) {
    if (JSValueGetTypedArrayType(context, data, nullptr) != NKWebKit::kJSTypedArrayTypeUint8Array)
        throw std::invalid_argument(std::string(__func__) + ": value is not a Uint8Array");
    auto* object = const_cast<NKWebKit::OpaqueJSValue*>(data);
    NKWebKit::JSObjectRef buffer = JSObjectGetTypedArrayBuffer(context, object, nullptr);
    if (!buffer || JSValueGetTypedArrayType(context, buffer, nullptr) != NKWebKit::kJSTypedArrayTypeArrayBuffer)
        throw std::invalid_argument(std::string(__func__) + ": Uint8Array over a shared buffer");
    const auto length = JSObjectGetTypedArrayLength(context, object, nullptr);
    std::vector<std::uint8_t> bytes;
    if (length) {
        const auto* storage = static_cast<const std::uint8_t*>(JSObjectGetTypedArrayBytesPtr(context, object, nullptr));
        if (!storage) throw std::invalid_argument(std::string(__func__) + ": Uint8Array without storage");
        const auto* start = storage + JSObjectGetTypedArrayByteOffset(context, object, nullptr);
        bytes.assign(start, start + length);
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Data>(std::move(bytes)));
}

const void* APS5_VABI WKPageGetSessionHistoryURLValueType() {
    static const NKWebKit::Object* type = static_cast<const NKWebKit::Object*>(NKWebKit::String::FromUtf8("SessionHistoryURL").Leak());
    return type;
}

const void* APS5_VABI WKPageGetSessionBackForwardListItemValueType() {
    static const NKWebKit::Object* type = static_cast<const NKWebKit::Object*>(NKWebKit::String::FromUtf8("SessionBackForwardListItem").Leak());
    return type;
}

void APS5_VABI _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(bool flag) {
    enhancedUserInterfaceAccessibility.store(flag, std::memory_order_relaxed);
}

bool APS5_VABI WKAccessibilityEnhancedAccessibilityEnabled() {
    return enhancedUserInterfaceAccessibility.load(std::memory_order_relaxed);
}

}

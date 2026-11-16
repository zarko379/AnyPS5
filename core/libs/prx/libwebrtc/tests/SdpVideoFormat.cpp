#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::set<void*> live;
std::size_t lastSize = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    live.insert(pointer);
    lastSize = size;
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer)
        return;
    if (live.erase(pointer) != 1)
        std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return 0;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SdpVideoFormat check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

void InitGuestString(WebRtcGuestString& target, std::string_view text) {
    std::memset(&target, 0, sizeof(target));
    target.size = text.size();
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        std::memcpy(target.buffer, text.data(), text.size());
        target.capacity = WebRtcGuestStringInlineCapacity;
        return;
    }
    const std::size_t capacity = text.size() | WebRtcGuestStringInlineCapacity;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = '\0';
    target.pointer = data;
    target.capacity = capacity;
}

std::string Read(const WebRtcGuestString& value) {
    return std::string(WebRtcGuestStringData(value), value.size);
}

WebRtcStringMapNode* MakeNode(std::string_view key, std::string_view value, bool isBlack) {
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcStringMapNode))) WebRtcStringMapNode{};
    node->links.isBlack = isBlack;
    InitGuestString(node->key, key);
    InitGuestString(node->value, value);
    return node;
}

void InitFormat(WebRtcSdpVideoFormat& format, std::string_view name) {
    std::memset(&format, 0, sizeof(format));
    InitGuestString(format.name, name);
    format.parameters.beginNode = EndNode(format.parameters);
}

void AttachParameters(WebRtcSdpVideoFormat& format) {
    auto* root = MakeNode("profile-id", "0", true);
    auto* right = MakeNode("x-google-start-bitrate-for-a-long-key", "800", false);
    root->links.parent = EndNode(format.parameters);
    root->links.right = right;
    right->links.parent = root;
    format.parameters.root = root;
    format.parameters.beginNode = root;
    format.parameters.size = 2;
}

void SetInlineModes(WebRtcSdpVideoFormat& format, std::initializer_list<std::uint8_t> modes) {
    std::size_t index = 0;
    for (std::uint8_t mode : modes)
        format.scalabilityModes.inlined[index++] = mode;
    format.scalabilityModes.metadata = static_cast<std::uint64_t>(modes.size()) << 1;
}

void SetAllocatedModes(WebRtcSdpVideoFormat& format, std::size_t count, std::size_t capacity) {
    auto* data = static_cast<std::uint8_t*>(_Znwm_nid_postfix(capacity));
    for (std::size_t index = 0; index < count; ++index)
        data[index] = static_cast<std::uint8_t>(index % 34);
    format.scalabilityModes.allocated.data = data;
    format.scalabilityModes.allocated.capacity = capacity;
    format.scalabilityModes.metadata = (static_cast<std::uint64_t>(count) << 1) | 1;
}

void TestCopyInlineModes() {
    WebRtcSdpVideoFormat source;
    InitFormat(source, "VP9");
    AttachParameters(source);
    SetInlineModes(source, {0, 2, 20});
    WebRtcSdpVideoFormat copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    const std::size_t before = live.size();
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source);
    Require(live.size() == before + 3);
    Require(Read(copy.name) == "VP9" && copy.parameters.size == 2 && copy.parameters.root != source.parameters.root);
    Require(copy.parameters.root->links.parent == EndNode(copy.parameters) && copy.parameters.beginNode == copy.parameters.root);
    Require(Read(copy.parameters.root->links.right->key) == "x-google-start-bitrate-for-a-long-key");
    Require(copy.scalabilityModes.metadata == 6 && copy.scalabilityModes.inlined[0] == 0 && copy.scalabilityModes.inlined[1] == 2 && copy.scalabilityModes.inlined[2] == 20);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copy);
    Require(live.size() == before);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestCopyAllocatedModes() {
    WebRtcSdpVideoFormat source;
    InitFormat(source, "a-codec-name-longer-than-fifteen");
    SetAllocatedModes(source, 5, 40);
    WebRtcSdpVideoFormat copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source);
    Require(copy.scalabilityModes.metadata == ((5u << 1) | 1u) && copy.scalabilityModes.allocated.capacity == 68);
    Require(copy.scalabilityModes.allocated.data != source.scalabilityModes.allocated.data);
    Require(std::memcmp(copy.scalabilityModes.allocated.data, source.scalabilityModes.allocated.data, 5) == 0);
    Require(Read(copy.name) == "a-codec-name-longer-than-fifteen" && copy.name.pointer != source.name.pointer);
    Require(copy.parameters.size == 0 && copy.parameters.beginNode == EndNode(copy.parameters));
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copy);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    Require(live.empty());

    InitFormat(source, "AV1");
    SetAllocatedModes(source, 70, 80);
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source);
    Require(copy.scalabilityModes.allocated.capacity == 70 && lastSize == 70);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copy);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);

    InitFormat(source, "H264");
    SetAllocatedModes(source, 0, 40);
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source);
    Require(copy.scalabilityModes.metadata == 0);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copy);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestMove() {
    WebRtcSdpVideoFormat source;
    InitFormat(source, "a-codec-name-longer-than-fifteen");
    AttachParameters(source);
    SetAllocatedModes(source, 3, 40);
    char* const name = source.name.pointer;
    void* const modes = source.scalabilityModes.allocated.data;
    WebRtcStringMapNode* const root = source.parameters.root;
    WebRtcSdpVideoFormat moved;
    std::memset(&moved, 0xA5, sizeof(moved));
    const std::size_t before = live.size();
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&moved, &source);
    Require(live.size() == before);
    Require(moved.name.pointer == name && moved.parameters.root == root && root->links.parent == EndNode(moved.parameters));
    Require(moved.scalabilityModes.allocated.data == modes && moved.scalabilityModes.metadata == ((3u << 1) | 1u));
    Require(source.scalabilityModes.metadata == 0 && source.parameters.size == 0 && source.parameters.root == nullptr);
    Require(source.parameters.beginNode == EndNode(source.parameters) && source.name.size == 0);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    Require(live.size() == before);
    WebRtcSdpVideoFormat inlineSource;
    InitFormat(inlineSource, "VP8");
    SetInlineModes(inlineSource, {1});
    WebRtcSdpVideoFormat inlineMoved;
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&inlineMoved, &inlineSource);
    Require(inlineMoved.scalabilityModes.metadata == 2 && inlineMoved.scalabilityModes.inlined[0] == 1 && inlineSource.scalabilityModes.metadata == 0);
    Require(Read(inlineMoved.name) == "VP8" && inlineMoved.parameters.beginNode == EndNode(inlineMoved.parameters));
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&inlineMoved);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&inlineSource);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&moved);
    Require(live.empty());
}

void TestInvalidFormatsThrow() {
    WebRtcSdpVideoFormat source;
    InitFormat(source, "VP9");
    WebRtcSdpVideoFormat copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    source.scalabilityModes.metadata = 35u << 1;
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source); });
    source.scalabilityModes.metadata = (3u << 1) | 1u;
    source.scalabilityModes.allocated.data = nullptr;
    source.scalabilityModes.allocated.capacity = 8;
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source); });
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source); });
    std::uint8_t storage[4] = {};
    source.scalabilityModes.allocated.data = storage;
    source.scalabilityModes.allocated.capacity = 2;
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source); });
    source.scalabilityModes.metadata = 0;
    source.name.capacity = 3;
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source); });
    WebRtcSdpVideoFormat other;
    InitFormat(other, "VP8");
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&other, &other); });
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&other, nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(nullptr, &other); });
    RequireInvalidArgument([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(nullptr); });
    Require(live.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestCopyInlineModes();
    TestCopyAllocatedModes();
    TestMove();
    TestInvalidFormatsThrow();
    return 0;
}

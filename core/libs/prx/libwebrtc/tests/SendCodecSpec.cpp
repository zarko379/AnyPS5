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

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    live.insert(pointer);
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
    std::fprintf(stderr, "libwebrtc SendCodecSpec check failed at line %u\n", location.line());
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

void InitShortString(WebRtcGuestString& target, std::string_view text) {
    std::memset(&target, 0, sizeof(target));
    std::memcpy(target.buffer, text.data(), text.size());
    target.size = text.size();
    target.capacity = WebRtcGuestStringInlineCapacity;
}

std::string Read(const WebRtcGuestString& value) {
    return std::string(WebRtcGuestStringData(value), value.size);
}

void InitFormat(WebRtcSdpAudioFormat& format) {
    std::memset(&format, 0, sizeof(format));
    InitShortString(format.name, "opus");
    format.clockrateHz = 48000;
    format.numChannels = 2;
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcStringMapNode))) WebRtcStringMapNode{};
    node->links.isBlack = true;
    node->links.parent = EndNode(format.parameters);
    InitShortString(node->key, "stereo");
    InitShortString(node->value, "1");
    format.parameters.root = node;
    format.parameters.beginNode = node;
    format.parameters.size = 1;
}

void TestConstructorCopiesTheFormat() {
    WebRtcSdpAudioFormat format;
    InitFormat(format);
    const std::size_t before = live.size();
    WebRtcSendCodecSpec spec;
    std::memset(&spec, 0xA5, sizeof(spec));
    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&spec, 111, &format);
    Require(spec.payloadType == 111);
    Require(Read(spec.format.name) == "opus" && spec.format.clockrateHz == 48000 && spec.format.numChannels == 2);
    Require(spec.format.parameters.size == 1 && spec.format.parameters.root != format.parameters.root);
    Require(spec.format.parameters.root->links.parent == EndNode(spec.format.parameters));
    Require(spec.format.parameters.beginNode == spec.format.parameters.root);
    Require(Read(spec.format.parameters.root->key) == "stereo" && Read(spec.format.parameters.root->value) == "1");
    Require(!spec.flags[0] && !spec.flags[1] && !spec.flags[2]);
    Require(!spec.cngPayloadType.engaged && !spec.redPayloadType.engaged && !spec.targetBitrateBps.engaged);
    Require(live.size() == before + 1);
    Require(Read(format.name) == "opus" && format.parameters.size == 1);
    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(&spec);
    Require(live.size() == before);
    Require(spec.format.parameters.size == 0 && spec.format.parameters.root == nullptr);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&format);
    Require(live.empty());
}

void TestEmptyParameters() {
    WebRtcSdpAudioFormat format;
    std::memset(&format, 0, sizeof(format));
    InitShortString(format.name, "PCMU");
    format.clockrateHz = 8000;
    format.numChannels = 1;
    format.parameters.beginNode = EndNode(format.parameters);
    WebRtcSendCodecSpec spec;
    std::memset(&spec, 0xA5, sizeof(spec));
    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&spec, -1, &format);
    Require(spec.payloadType == -1 && Read(spec.format.name) == "PCMU");
    Require(spec.format.parameters.size == 0 && spec.format.parameters.beginNode == EndNode(spec.format.parameters));
    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(&spec);
    Require(live.empty());
}

void TestInvalidArgumentsThrow() {
    WebRtcSdpAudioFormat format;
    std::memset(&format, 0, sizeof(format));
    InitShortString(format.name, "opus");
    format.parameters.beginNode = EndNode(format.parameters);
    WebRtcSendCodecSpec spec;
    std::memset(&spec, 0xA5, sizeof(spec));
    RequireInvalidArgument([&] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(nullptr, 1, &format); });
    RequireInvalidArgument([&] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&spec, 1, nullptr); });
    format.name.capacity = 3;
    RequireInvalidArgument([&] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&spec, 1, &format); });
    std::uint32_t untouched = 0;
    std::memset(&untouched, 0xA5, sizeof(untouched));
    Require(static_cast<std::uint32_t>(spec.payloadType) == untouched);
    RequireInvalidArgument([] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(nullptr); });
    Require(live.empty());
}

void TestStatsDestructor() {
    WebRtcAudioSendStreamStats stats;
    std::memset(&stats, 0, sizeof(stats));
    InitShortString(stats.codecName, "opus");
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats);
    Require(live.empty());
    const std::string name(40, 'c');
    stats.codecName.size = name.size();
    stats.codecName.capacity = name.size() | WebRtcGuestStringInlineCapacity;
    stats.codecName.pointer = static_cast<char*>(GuestStringStorage::Allocate(stats.codecName.capacity + 1));
    std::memcpy(stats.codecName.pointer, name.data(), name.size());
    stats.reportBlockDatasBegin = _Znwm_nid_postfix(96);
    stats.reportBlockDatasEnd = static_cast<unsigned char*>(stats.reportBlockDatasBegin) + 48;
    stats.reportBlockDatasCapacity = static_cast<unsigned char*>(stats.reportBlockDatasBegin) + 96;
    Require(live.size() == 2);
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats);
    Require(live.empty());
}

void TestStatsDestructorRejectsMalformedMembers() {
    WebRtcAudioSendStreamStats stats;
    std::memset(&stats, 0, sizeof(stats));
    InitShortString(stats.codecName, "opus");
    stats.reportBlockDatasBegin = _Znwm_nid_postfix(16);
    stats.reportBlockDatasEnd = static_cast<unsigned char*>(stats.reportBlockDatasBegin) + 16;
    stats.reportBlockDatasCapacity = stats.reportBlockDatasBegin;
    RequireInvalidArgument([&] { _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats); });
    stats.reportBlockDatasCapacity = stats.reportBlockDatasEnd;
    stats.codecName.capacity = 3;
    RequireInvalidArgument([&] { _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats); });
    Require(live.size() == 1);
    stats.codecName.capacity = WebRtcGuestStringInlineCapacity;
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats);
    Require(live.empty());
    RequireInvalidArgument([] { _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(nullptr); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestConstructorCopiesTheFormat();
    TestEmptyParameters();
    TestInvalidArgumentsThrow();
    TestStatsDestructor();
    TestStatsDestructorRejectsMalformedMembers();
    return 0;
}

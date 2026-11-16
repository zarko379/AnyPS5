#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void* lastFreed = nullptr;
int allocations = 0;
int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { ++allocations; return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { ++allocations; return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { ++allocations; return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++allocations;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void RegisterHeap() {
    static const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::Remb check failed at line %u\n", location.line());
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

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

struct Sent {
    std::vector<std::uint8_t> bytes;
    int calls = 0;
};

void APS5_VABI Send(void* function, const std::uint8_t* data, std::size_t size) {
    auto* sent = static_cast<Sent*>(function);
    sent->bytes.assign(data, data + size);
    ++sent->calls;
}

template<typename TFunction> TFunction Slot(const void* object, int slot) {
    const void* const* vtable = *static_cast<const void* const* const*>(object);
    return reinterpret_cast<TFunction>(const_cast<void*>(vtable[slot]));
}

using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpRemb*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpRemb*, std::uint8_t*, std::size_t*, std::size_t, void*, PacketReady);
using Destroy = void (APS5_VABI*)(WebRtcRtcpRemb*);

WebRtcRtcpRemb Constructed() {
    WebRtcRtcpRemb remb;
    std::memset(&remb, 0xA5, sizeof(remb));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(&remb);
    return remb;
}

std::vector<std::uint8_t> Payload(std::uint8_t exponentByte, std::uint8_t mantissaHigh, std::uint8_t mantissaLow, const std::vector<std::uint32_t>& ssrcs) {
    std::vector<std::uint8_t> payload {0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0, 0x52, 0x45, 0x4D, 0x42, static_cast<std::uint8_t>(ssrcs.size()), exponentByte, mantissaHigh, mantissaLow};
    for (std::uint32_t ssrc : ssrcs) {
        payload.push_back(static_cast<std::uint8_t>(ssrc >> 24));
        payload.push_back(static_cast<std::uint8_t>(ssrc >> 16));
        payload.push_back(static_cast<std::uint8_t>(ssrc >> 8));
        payload.push_back(static_cast<std::uint8_t>(ssrc));
    }
    return payload;
}

bool Parse(WebRtcRtcpRemb& remb, const std::vector<std::uint8_t>& payload) {
    WebRtcRtcpCommonHeader header {206, 15, 0, static_cast<std::uint32_t>(payload.size()), payload.data()};
    return _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(&remb, &header);
}

std::size_t Count(const WebRtcRtcpRemb& remb) {
    return static_cast<std::size_t>(remb.ssrcsEnd - remb.ssrcsBegin);
}

void TestConstructor() {
    WebRtcRtcpRemb remb = Constructed();
    Require(remb.vtable != nullptr && remb.senderSsrc == 0 && remb.mediaSsrc == 0 && remb.bitrateBps == 0);
    Require(remb.ssrcsBegin == nullptr && remb.ssrcsEnd == nullptr && remb.ssrcsCapacity == nullptr);
    Require(Slot<BlockLength>(&remb, 2)(&remb) == 20);
    const int before = frees;
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    Require(frees == before);
}

void TestParseAndCreate() {
    WebRtcRtcpRemb remb = Constructed();
    const auto payload = Payload(0x06, 0x46, 0x8A, {0x01020304u, 0xA0B0C0D0u});
    const int allocated = allocations;
    Require(Parse(remb, payload));
    Require(allocations == allocated + 1);
    Require(remb.senderSsrc == 0x12345678u && remb.mediaSsrc == 0);
    Require(remb.bitrateBps == 0x48D14);
    Require(Count(remb) == 2 && remb.ssrcsCapacity == remb.ssrcsEnd);
    Require(remb.ssrcsBegin[0] == 0x01020304u && remb.ssrcsBegin[1] == 0xA0B0C0D0u);
    Require(Slot<BlockLength>(&remb, 2)(&remb) == 28);
    std::vector<std::uint8_t> buffer(40, 0xee);
    std::size_t index = 0;
    Require(Slot<Create>(&remb, 3)(&remb, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 28);
    const std::vector<std::uint8_t> header {0x8F, 206, 0, 6};
    Require(std::memcmp(buffer.data(), header.data(), header.size()) == 0);
    Require(std::memcmp(buffer.data() + 4, payload.data(), payload.size()) == 0);
    Require(buffer[28] == 0xee);
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
}

void TestCreateNormalizesTheMantissa() {
    WebRtcRtcpRemb remb = Constructed();
    remb.bitrateBps = 0x48D14;
    remb.mediaSsrc = 0x0BADF00Du;
    std::vector<std::uint8_t> buffer(20, 0);
    std::size_t index = 0;
    Require(Slot<Create>(&remb, 3)(&remb, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 20);
    Require(buffer[0] == 0x8F && buffer[1] == 206 && buffer[2] == 0 && buffer[3] == 4);
    Require(buffer[8] == 0x0B && buffer[9] == 0xAD && buffer[10] == 0xF0 && buffer[11] == 0x0D);
    Require(buffer[12] == 0x52 && buffer[15] == 0x42);
    Require(buffer[16] == 0 && buffer[17] == 0x06 && buffer[18] == 0x46 && buffer[19] == 0x8A);
    remb.bitrateBps = 0x3ffff;
    index = 0;
    Require(Slot<Create>(&remb, 3)(&remb, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(buffer[17] == 0x03 && buffer[18] == 0xff && buffer[19] == 0xff);
}

void TestReparseReusesOrGrowsTheSsrcs() {
    WebRtcRtcpRemb remb = Constructed();
    Require(Parse(remb, Payload(0, 0, 1, {1, 2})));
    std::uint32_t* const first = remb.ssrcsBegin;
    const int allocated = allocations;
    const int freed = frees;
    Require(Parse(remb, Payload(0, 0, 2, {7})));
    Require(allocations == allocated && frees == freed);
    Require(remb.ssrcsBegin == first && Count(remb) == 1 && remb.ssrcsBegin[0] == 7 && remb.ssrcsCapacity == first + 2);
    Require(remb.bitrateBps == 2);
    Require(Parse(remb, Payload(0, 0, 3, {})));
    Require(remb.ssrcsBegin == first && Count(remb) == 0);
    Require(Parse(remb, Payload(0, 0, 4, {4, 5, 6})));
    Require(allocations == allocated + 1 && frees == freed + 1 && lastFreed == first);
    Require(Count(remb) == 3 && remb.ssrcsCapacity == remb.ssrcsEnd && remb.ssrcsBegin[2] == 6);
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    Require(frees == freed + 2);
}

void TestRejectedPacketsLeaveThePacketUntouched() {
    WebRtcRtcpRemb remb = Constructed();
    Require(Parse(remb, Payload(0, 0, 9, {3})));
    const WebRtcRtcpRemb before = remb;
    auto payload = Payload(0, 0, 1, {});
    payload.resize(12);
    Require(!Parse(remb, payload));
    Require(std::memcmp(&before, &remb, sizeof(remb)) == 0);
    payload = Payload(0, 0, 1, {});
    payload[11] = 0x43;
    Require(!Parse(remb, payload));
    Require(std::memcmp(&before, &remb, sizeof(remb)) == 0);
    payload = Payload(0, 0, 1, {5, 6});
    payload.resize(20);
    Require(!Parse(remb, payload));
    Require(std::memcmp(&before, &remb, sizeof(remb)) == 0);
    payload = Payload(0, 0, 1, {5});
    payload.insert(payload.end(), 4, 0);
    Require(!Parse(remb, payload));
    Require(std::memcmp(&before, &remb, sizeof(remb)) == 0);
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
}

void TestInvalidBitratesKeepTheCommonFeedback() {
    WebRtcRtcpRemb remb = Constructed();
    Require(Parse(remb, Payload(0, 0, 9, {3})));
    std::uint32_t* const ssrcs = remb.ssrcsBegin;
    auto payload = Payload(static_cast<std::uint8_t>(63 << 2), 0, 1, {8});
    payload[0] = 0xAA;
    payload[7] = 0x01;
    Require(!Parse(remb, payload));
    Require(remb.senderSsrc == 0xAA345678u && remb.mediaSsrc == 1);
    Require(static_cast<std::uint64_t>(remb.bitrateBps) == 0x8000000000000000ull);
    Require(remb.ssrcsBegin == ssrcs && Count(remb) == 1 && remb.ssrcsBegin[0] == 3);
    Require(!Parse(remb, Payload(static_cast<std::uint8_t>((47 << 2) | 2), 0, 1, {8})));
    Require(remb.bitrateBps == (std::int64_t {1} << 47));
    Require(remb.ssrcsBegin == ssrcs && Count(remb) == 1 && remb.ssrcsBegin[0] == 3);
    Require(Parse(remb, Payload(static_cast<std::uint8_t>((45 << 2) | 3), 0xff, 0xff, {8})));
    Require(static_cast<std::uint64_t>(remb.bitrateBps) == (0x3ffffull << 45));
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
}

void TestCreateSendsAFullBuffer() {
    WebRtcRtcpRemb remb = Constructed();
    std::vector<std::uint8_t> buffer(24, 0x11);
    std::size_t index = 5;
    Sent sent;
    Require(Slot<Create>(&remb, 3)(&remb, buffer.data(), &index, buffer.size(), &sent, &Send));
    Require(sent.calls == 1 && sent.bytes.size() == 5 && index == 20);
    Require(buffer[0] == 0x8F && buffer[1] == 206);
    index = 0;
    Require(!Slot<Create>(&remb, 3)(&remb, buffer.data(), &index, 19, &sent, &Send));
    Require(index == 0 && sent.calls == 1);
    index = 4;
    RequireInvalidArgument([&] { Slot<Create>(&remb, 3)(&remb, buffer.data(), &index, 20, nullptr, nullptr); });
}

void TestDeletingDestructor() {
    auto* remb = static_cast<WebRtcRtcpRemb*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpRemb)));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(remb);
    Require(Parse(*remb, Payload(0, 0, 1, {1})));
    const int before = frees;
    Slot<Destroy>(remb, 1)(remb);
    Require(frees == before + 2 && lastFreed == remb);
}

void TestNullArgumentsThrow() {
    const auto payload = Payload(0, 0, 1, {});
    WebRtcRtcpCommonHeader header {206, 15, 0, static_cast<std::uint32_t>(payload.size()), payload.data()};
    WebRtcRtcpRemb remb = Constructed();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(nullptr, &header); });
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(&remb, nullptr); });
    header.payload = nullptr;
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(&remb, &header); });
    header.payloadSize = 15;
    Require(!_ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(&remb, &header));
    RequireInvalidArgument([] { _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(nullptr); });
}

}

int main() {
    RegisterHeap();
    TestConstructor();
    TestParseAndCreate();
    TestCreateNormalizesTheMantissa();
    TestReparseReusesOrGrowsTheSsrcs();
    TestRejectedPacketsLeaveThePacketUntouched();
    TestInvalidBitratesKeepTheCommonFeedback();
    TestCreateSendsAFullBuffer();
    TestDeletingDestructor();
    TestNullArgumentsThrow();
    return 0;
}

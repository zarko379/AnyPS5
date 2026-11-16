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
int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::Pli check failed at line %u\n", location.line());
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

WebRtcRtcpPli Filled() {
    WebRtcRtcpPli pli;
    std::memset(&pli, 0xA5, sizeof(pli));
    return pli;
}

bool Unchanged(const WebRtcRtcpPli& pli) {
    WebRtcRtcpPli filled = Filled();
    return std::memcmp(&pli, &filled, sizeof(pli)) == 0;
}

bool ParsePacket(WebRtcRtcpPli& pli, const std::vector<std::uint8_t>& packet) {
    WebRtcRtcpCommonHeader header;
    Require(_ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, packet.data(), packet.size()));
    return _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header);
}

void TestPli() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    WebRtcRtcpPli pli = Filled();
    WebRtcRtcpPli filled = Filled();
    Require(ParsePacket(pli, packet));
    Require(pli.senderSsrc == 0x12345678u);
    Require(pli.mediaSsrc == 0x9ABCDEF0u);
    Require(pli.vtable == filled.vtable);
}

void TestLongerPayloadReadsTheCommonFeedbackOnly() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE, 0x55, 0x55, 0x55, 0x55};
    WebRtcRtcpPli pli = Filled();
    Require(ParsePacket(pli, packet));
    Require(pli.senderSsrc == 1);
    Require(pli.mediaSsrc == 0xFFFFFFFEu);
}

void TestShortPayloadLeavesThePacketUntouched() {
    std::vector<std::uint8_t> empty {0x81, 206, 0x00, 0x00};
    WebRtcRtcpPli pli = Filled();
    Require(!ParsePacket(pli, empty));
    Require(Unchanged(pli));

    std::vector<std::uint8_t> padded {0xA1, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0x01};
    Require(!ParsePacket(pli, padded));
    Require(Unchanged(pli));

    WebRtcRtcpCommonHeader header {206, 1, 0, 7, nullptr};
    Require(!_ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header));
    Require(Unchanged(pli));
}

void TestNullArgumentsThrow() {
    std::vector<std::uint8_t> payload(8, 0x11);
    WebRtcRtcpCommonHeader header {206, 1, 0, 8, payload.data()};
    WebRtcRtcpPli pli = Filled();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(nullptr, &header); });
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, nullptr); });
    header.payload = nullptr;
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header); });
    Require(Unchanged(pli));
}

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpPli*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpPli*, std::uint8_t*, std::size_t*, std::size_t, void*, PacketReady);
using Destroy = void (APS5_VABI*)(WebRtcRtcpPli*);

template<typename TFunction> TFunction Slot(const WebRtcRtcpPli& pli, int slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(static_cast<const void* const*>(pli.vtable)[slot]));
}

struct Sent {
    std::vector<std::uint8_t> bytes;
    int calls = 0;
};

void APS5_VABI Send(void* function, const std::uint8_t* data, std::size_t size) {
    auto* sent = static_cast<Sent*>(function);
    sent->bytes.assign(data, data + size);
    ++sent->calls;
}

WebRtcRtcpPli Constructed() {
    WebRtcRtcpPli pli = Filled();
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    return pli;
}

void TestConstructor() {
    WebRtcRtcpPli pli = Constructed();
    Require(pli.vtable != nullptr && pli.senderSsrc == 0 && pli.mediaSsrc == 0);
    Require(Slot<BlockLength>(pli, 2)(&pli) == 12);
    RequireInvalidArgument([] { _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(nullptr); });
}

void TestCreateWritesTheParsedPacket() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    WebRtcRtcpPli pli = Constructed();
    Require(ParsePacket(pli, packet));
    std::vector<std::uint8_t> buffer(16, 0xee);
    std::size_t index = 2;
    Require(Slot<Create>(pli, 3)(&pli, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 14);
    Require(std::memcmp(buffer.data() + 2, packet.data(), packet.size()) == 0);
    Require(buffer[0] == 0xee && buffer[1] == 0xee && buffer[14] == 0xee);
}

void TestCreateSendsAFullBuffer() {
    WebRtcRtcpPli pli = Constructed();
    std::vector<std::uint8_t> buffer(16, 0);
    std::size_t index = 5;
    Sent sent;
    Require(Slot<Create>(pli, 3)(&pli, buffer.data(), &index, buffer.size(), &sent, &Send));
    Require(sent.calls == 1 && sent.bytes.size() == 5 && index == 12);
    Require(buffer[0] == 0x81 && buffer[1] == 206 && buffer[2] == 0 && buffer[3] == 2);
    index = 0;
    Require(!Slot<Create>(pli, 3)(&pli, buffer.data(), &index, 11, &sent, &Send));
    Require(index == 0 && sent.calls == 1);
    index = 1;
    RequireInvalidArgument([&] { Slot<Create>(pli, 3)(&pli, buffer.data(), &index, 12, nullptr, nullptr); });
}

void TestDeletingDestructor() {
    auto* pli = static_cast<WebRtcRtcpPli*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpPli)));
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(pli);
    const int before = frees;
    Slot<Destroy>(*pli, 1)(pli);
    Require(frees == before + 1 && lastFreed == pli);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestPli();
    TestLongerPayloadReadsTheCommonFeedbackOnly();
    TestShortPayloadLeavesThePacketUntouched();
    TestNullArgumentsThrow();
    TestConstructor();
    TestCreateWritesTheParsedPacket();
    TestCreateSendsAFullBuffer();
    TestDeletingDestructor();
    return 0;
}

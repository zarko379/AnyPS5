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
    std::fprintf(stderr, "libwebrtc rtcp::RtcpPacket::Build check failed at line %u\n", location.line());
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

struct FakePacket {
    const void* vtable;
    std::size_t length;
    std::size_t written;
    bool throws;
    std::size_t seenMaxLength;
    bool sawCallback;
};

std::size_t APS5_VABI FakeBlockLength_nid_no_patch(const FakePacket* packet) {
    return packet->length;
}

bool APS5_VABI FakeCreate_nid_no_patch(FakePacket* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    packet->seenMaxLength = maxLength;
    packet->sawCallback = function != nullptr || call != nullptr;
    if (packet->throws)
        throw std::invalid_argument("fake packet");
    for (std::size_t byte = 0; byte < packet->written; ++byte)
        buffer[(*index)++] = static_cast<std::uint8_t>(0x40 + byte);
    return false;
}

void* const FakeVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&FakeBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&FakeCreate_nid_no_patch),
};

void* const MissingCreateVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&FakeBlockLength_nid_no_patch),
    nullptr,
};

void TestBuildsAPli() {
    WebRtcRtcpPli pli;
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    pli.senderSsrc = 0x11223344u;
    pli.mediaSsrc = 0x55667788u;
    WebRtcBuffer buffer;
    std::memset(&buffer, 0xA5, sizeof(buffer));
    Require(_ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &pli) == &buffer);
    Require(buffer.size == 12 && buffer.capacity == 12 && buffer.data != nullptr);
    const std::vector<std::uint8_t> expected {0x81, 206, 0, 2, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    Require(std::memcmp(buffer.data, expected.data(), expected.size()) == 0);
    _ZdaPv_nid_postfix(buffer.data);
}

void TestSizeComesFromBlockLength() {
    FakePacket packet {FakeVtable, 6, 2, false, 0, true};
    WebRtcBuffer buffer {};
    const int allocated = allocations;
    _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &packet);
    Require(allocations == allocated + 1);
    Require(buffer.size == 6 && buffer.capacity == 6 && packet.seenMaxLength == 6 && !packet.sawCallback);
    Require(buffer.data[0] == 0x40 && buffer.data[1] == 0x41);
    _ZdaPv_nid_postfix(buffer.data);
    packet.length = 0;
    packet.written = 0;
    _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &packet);
    Require(allocations == allocated + 1);
    Require(buffer.size == 0 && buffer.capacity == 0 && buffer.data == nullptr);
}

void TestCreateFailureFreesTheBuffer() {
    FakePacket packet {FakeVtable, 8, 0, true, 0, false};
    WebRtcBuffer buffer {};
    const int before = frees;
    RequireInvalidArgument([&] { _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &packet); });
    Require(frees == before + 1);
    Require(buffer.size == 0 && buffer.capacity == 0 && buffer.data == nullptr);
}

void TestInvalidArgumentsThrow() {
    WebRtcRtcpPli pli;
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    WebRtcBuffer buffer {};
    FakePacket missing {MissingCreateVtable, 4, 0, false, 0, false};
    FakePacket noVtable {nullptr, 4, 0, false, 0, false};
    const int allocated = allocations;
    RequireInvalidArgument([&] { _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(nullptr, &pli); });
    RequireInvalidArgument([&] { _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &missing); });
    RequireInvalidArgument([&] { _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &noVtable); });
    Require(allocations == allocated);
}

}

int main() {
    RegisterHeap();
    TestBuildsAPli();
    TestSizeComesFromBlockLength();
    TestCreateFailureFreesTheBuffer();
    TestInvalidArgumentsThrow();
    return 0;
}

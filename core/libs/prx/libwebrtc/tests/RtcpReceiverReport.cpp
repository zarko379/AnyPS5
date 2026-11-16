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
    std::fprintf(stderr, "libwebrtc rtcp::ReceiverReport check failed at line %u\n", location.line());
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

using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpReceiverReport*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpReceiverReport*, std::uint8_t*, std::size_t*, std::size_t, void*, PacketReady);
using Destroy = void (APS5_VABI*)(WebRtcRtcpReceiverReport*);

WebRtcRtcpReceiverReport Constructed() {
    WebRtcRtcpReceiverReport report;
    std::memset(&report, 0xA5, sizeof(report));
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    return report;
}

void SetBlocks(WebRtcRtcpReceiverReport& report, const std::vector<WebRtcRtcpReportBlock>& blocks) {
    auto* storage = static_cast<WebRtcRtcpReportBlock*>(_Znwm_nid_postfix(blocks.size() * sizeof(WebRtcRtcpReportBlock)));
    std::memcpy(storage, blocks.data(), blocks.size() * sizeof(WebRtcRtcpReportBlock));
    report.blocksBegin = storage;
    report.blocksEnd = storage + blocks.size();
    report.blocksCapacity = storage + blocks.size();
}

void TestConstructor() {
    WebRtcRtcpReceiverReport report = Constructed();
    Require(report.vtable != nullptr && report.senderSsrc == 0);
    Require(report.blocksBegin == nullptr && report.blocksEnd == nullptr && report.blocksCapacity == nullptr);
    Require(Slot<BlockLength>(&report, 2)(&report) == 8);
    report.senderSsrc = 0x01020304u;
    std::vector<std::uint8_t> buffer(10, 0xee);
    std::size_t index = 1;
    Require(Slot<Create>(&report, 3)(&report, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 9);
    const std::vector<std::uint8_t> expected {0xee, 0x80, 201, 0, 1, 1, 2, 3, 4, 0xee};
    Require(buffer == expected);
    const int before = frees;
    Slot<Destroy>(&report, 0)(&report);
    Require(frees == before);
    RequireInvalidArgument([] { _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(nullptr); });
}

void TestCreateWritesTheReportBlocks() {
    WebRtcRtcpReceiverReport report = Constructed();
    report.senderSsrc = 0xCAFEBABEu;
    WebRtcRtcpReportBlock first {};
    first.sourceSsrc = 0x11223344u;
    first.fractionLost = 0x80;
    first.cumulativeLost = -2;
    first.extendedHighSeqNum = 0x00010002u;
    first.jitter = 0x55;
    first.lastSr = 0xAABBCCDDu;
    first.delaySinceLastSr = 0x01000000u;
    WebRtcRtcpReportBlock second {};
    second.sourceSsrc = 7;
    second.cumulativeLost = 0x123456;
    SetBlocks(report, {first, second});
    Require(Slot<BlockLength>(&report, 2)(&report) == 56);
    std::vector<std::uint8_t> buffer(56, 0);
    std::size_t index = 0;
    Require(Slot<Create>(&report, 3)(&report, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 56);
    const std::vector<std::uint8_t> expected {
        0x82, 201, 0, 13, 0xCA, 0xFE, 0xBA, 0xBE,
        0x11, 0x22, 0x33, 0x44, 0x80, 0xFF, 0xFF, 0xFE, 0x00, 0x01, 0x00, 0x02, 0, 0, 0, 0x55, 0xAA, 0xBB, 0xCC, 0xDD, 0x01, 0, 0, 0,
        0, 0, 0, 7, 0, 0x12, 0x34, 0x56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Require(buffer == expected);
    const int before = frees;
    Slot<Destroy>(&report, 0)(&report);
    Require(frees == before + 1);
}

void TestCreateSendsAFullBuffer() {
    WebRtcRtcpReceiverReport report = Constructed();
    std::vector<std::uint8_t> buffer(10, 0);
    std::size_t index = 3;
    Sent sent;
    Require(Slot<Create>(&report, 3)(&report, buffer.data(), &index, buffer.size(), &sent, &Send));
    Require(sent.calls == 1 && sent.bytes.size() == 3 && index == 8);
    index = 0;
    Require(!Slot<Create>(&report, 3)(&report, buffer.data(), &index, 7, &sent, &Send));
    Require(index == 0 && sent.calls == 1);
    index = 3;
    RequireInvalidArgument([&] { Slot<Create>(&report, 3)(&report, buffer.data(), &index, 8, nullptr, nullptr); });
}

void TestDeletingDestructor() {
    auto* report = static_cast<WebRtcRtcpReceiverReport*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpReceiverReport)));
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(report);
    SetBlocks(*report, {WebRtcRtcpReportBlock {}});
    const int before = frees;
    Slot<Destroy>(report, 1)(report);
    Require(frees == before + 2 && lastFreed == report);
}

}

int main() {
    RegisterHeap();
    TestConstructor();
    TestCreateWritesTheReportBlocks();
    TestCreateSendsAFullBuffer();
    TestDeletingDestructor();
    return 0;
}

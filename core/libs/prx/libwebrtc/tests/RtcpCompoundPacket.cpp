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
    std::fprintf(stderr, "libwebrtc rtcp::CompoundPacket check failed at line %u
", location.line());
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

using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpCompoundPacket*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpCompoundPacket*, std::uint8_t*, std::size_t*, std::size_t, void*, PacketReady);
using Destroy = void (APS5_VABI*)(WebRtcRtcpCompoundPacket*);

struct FakePacket {
    const void* vtable;
    int id;
    std::size_t length;
    bool result;
};

std::vector<int> deleted;
std::vector<int> created;

void APS5_VABI FakeDestroy_nid_no_patch(FakePacket*) {}

void APS5_VABI FakeDelete_nid_no_patch(FakePacket* packet) {
    deleted.push_back(packet->id);
    delete packet;
}

std::size_t APS5_VABI FakeBlockLength_nid_no_patch(const FakePacket* packet) {
    return packet->length;
}

bool APS5_VABI FakeCreate_nid_no_patch(const FakePacket* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    created.push_back(packet->id);
    if (*index + packet->length > maxLength) {
        if (call)
            call(function, buffer, *index);
        return false;
    }
    for (std::size_t byte = 0; byte < packet->length; ++byte)
        buffer[(*index)++] = static_cast<std::uint8_t>(packet->id);
    return packet->result;
}

void* const FakeVtable[] = {
    reinterpret_cast<void*>(&FakeDestroy_nid_no_patch),
    reinterpret_cast<void*>(&FakeDelete_nid_no_patch),
    reinterpret_cast<void*>(&FakeBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&FakeCreate_nid_no_patch),
};

void* const MissingVtable[] = {nullptr, nullptr, nullptr, nullptr};

FakePacket* Fake(int id, std::size_t length, bool result = true) {
    return new FakePacket {FakeVtable, id, length, result};
}

WebRtcRtcpCompoundPacket Constructed() {
    WebRtcRtcpCompoundPacket compound;
    std::memset(&compound, 0xA5, sizeof(compound));
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(&compound);
    return compound;
}

void Append(WebRtcRtcpCompoundPacket& compound, const std::vector<void*>& packets) {
    auto** storage = static_cast<void**>(_Znwm_nid_postfix(packets.size() * sizeof(void*)));
    std::memcpy(storage, packets.data(), packets.size() * sizeof(void*));
    compound.packetsBegin = storage;
    compound.packetsEnd = storage + packets.size();
    compound.packetsCapacity = storage + packets.size();
}

void TestConstructor() {
    WebRtcRtcpCompoundPacket compound = Constructed();
    Require(compound.vtable != nullptr && compound.senderSsrc == 0);
    Require(compound.packetsBegin == nullptr && compound.packetsEnd == nullptr && compound.packetsCapacity == nullptr);
    Require(Slot<BlockLength>(&compound, 2)(&compound) == 0);
    std::uint8_t buffer[4] {0xee, 0xee, 0xee, 0xee};
    std::size_t index = 0;
    Require(Slot<Create>(&compound, 3)(&compound, buffer, &index, 0, nullptr, nullptr));
    Require(index == 0 && buffer[0] == 0xee);
    const int before = frees;
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require(frees == before);
    RequireInvalidArgument([] { _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(nullptr); });
}

void TestNestedPackets() {
    auto* pli = static_cast<WebRtcRtcpPli*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpPli)));
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(pli);
    pli->senderSsrc = 0x01020304u;
    pli->mediaSsrc = 0x05060708u;
    auto* remb = static_cast<WebRtcRtcpRemb*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpRemb)));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(remb);
    remb->bitrateBps = 1;
    WebRtcRtcpCompoundPacket compound = Constructed();
    Append(compound, {pli, remb});
    Require(Slot<BlockLength>(&compound, 2)(&compound) == 32);
    std::vector<std::uint8_t> buffer(32, 0);
    std::size_t index = 0;
    Require(Slot<Create>(&compound, 3)(&compound, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 32);
    const std::vector<std::uint8_t> expected {
        0x81, 206, 0, 2, 1, 2, 3, 4, 5, 6, 7, 8,
        0x8F, 206, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0x52, 0x45, 0x4D, 0x42, 0, 0, 0, 1};
    Require(buffer == expected);
    const int before = frees;
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require(frees == before + 3 && lastFreed == compound.packetsBegin);
}

void TestCreateStopsAtTheFirstFailure() {
    WebRtcRtcpCompoundPacket compound = Constructed();
    Append(compound, {Fake(1, 4), Fake(2, 4, false), Fake(3, 4)});
    Require(Slot<BlockLength>(&compound, 2)(&compound) == 12);
    std::vector<std::uint8_t> buffer(12, 0);
    std::size_t index = 0;
    created.clear();
    Require(!Slot<Create>(&compound, 3)(&compound, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require((created == std::vector<int> {1, 2}) && index == 8);
    Sent sent;
    created.clear();
    index = 0;
    Require(!Slot<Create>(&compound, 3)(&compound, buffer.data(), &index, 6, &sent, &Send));
    Require((created == std::vector<int> {1, 2}) && sent.calls == 1 && sent.bytes.size() == 4);
    deleted.clear();
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require((deleted == std::vector<int> {3, 2, 1}));
}

void TestPacketsWithoutVirtualFunctionsThrow() {
    FakePacket missing {MissingVtable, 9, 0, true};
    WebRtcRtcpCompoundPacket compound = Constructed();
    Append(compound, {Fake(1, 4), &missing});
    std::vector<std::uint8_t> buffer(8, 0);
    std::size_t index = 0;
    RequireInvalidArgument([&] { Slot<BlockLength>(&compound, 2)(&compound); });
    RequireInvalidArgument([&] { Slot<Create>(&compound, 3)(&compound, buffer.data(), &index, buffer.size(), nullptr, nullptr); });
    deleted.clear();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound); });
    Require(deleted.empty() && compound.packetsEnd == compound.packetsBegin + 2);
    compound.packetsEnd = compound.packetsBegin + 1;
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require((deleted == std::vector<int> {1}));
}

void TestDeletingDestructor() {
    auto* compound = static_cast<WebRtcRtcpCompoundPacket*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpCompoundPacket)));
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(compound);
    Append(*compound, {Fake(4, 0)});
    deleted.clear();
    const int before = frees;
    Slot<Destroy>(compound, 1)(compound);
    Require(frees == before + 2 && lastFreed == compound && (deleted == std::vector<int> {4}));
}

}

int main() {
    RegisterHeap();
    TestConstructor();
    TestNestedPackets();
    TestCreateStopsAtTheFirstFailure();
    TestPacketsWithoutVirtualFunctionsThrow();
    TestDeletingDestructor();
    return 0;
}

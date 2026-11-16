#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc I420Buffer check failed at line %u\n", location.line());
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

const std::uint8_t* APS5_VABI BaseDataY(const WebRtcI420Buffer* buffer) {
    return buffer->data;
}

const std::uint8_t* APS5_VABI BaseDataU(const WebRtcI420Buffer* buffer) {
    return buffer->data + buffer->strideY * buffer->height;
}

const std::uint8_t* APS5_VABI BaseDataV(const WebRtcI420Buffer* buffer) {
    return BaseDataU(buffer) + buffer->strideU * ((buffer->height + 1) / 2);
}

std::uint8_t overridePlanes[3];

const std::uint8_t* APS5_VABI DerivedDataY(const WebRtcI420Buffer*) {
    return &overridePlanes[0];
}

const std::uint8_t* APS5_VABI DerivedDataU(const WebRtcI420Buffer*) {
    return &overridePlanes[1];
}

const std::uint8_t* APS5_VABI DerivedDataV(const WebRtcI420Buffer*) {
    return &overridePlanes[2];
}

struct Vtable {
    const void* slots[20];
};

Vtable MakeVtable(const void* dataY, const void* dataU, const void* dataV) {
    Vtable vtable {};
    vtable.slots[17] = dataY;
    vtable.slots[18] = dataU;
    vtable.slots[19] = dataV;
    return vtable;
}

const Vtable i420Vtable = MakeVtable(reinterpret_cast<const void*>(BaseDataY), reinterpret_cast<const void*>(BaseDataU), reinterpret_cast<const void*>(BaseDataV));
const Vtable derivedVtable = MakeVtable(reinterpret_cast<const void*>(DerivedDataY), reinterpret_cast<const void*>(DerivedDataU), reinterpret_cast<const void*>(DerivedDataV));

void Check(std::int32_t width, std::int32_t height, std::int32_t strideY, std::int32_t strideU, std::int32_t strideV) {
    const std::int32_t chromaHeight = (height + 1) / 2;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(strideY * height + (strideU + strideV) * chromaHeight) + 1);
    WebRtcI420Buffer buffer {i420Vtable.slots, width, height, strideY, strideU, strideV, data.data()};
    Require(_ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(&buffer) == data.data());
    Require(_ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(&buffer) == data.data() + strideY * height);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&buffer) == data.data() + strideY * height + strideU * chromaHeight);
}

void TestOverriddenGetters() {
    std::vector<std::uint8_t> data(640 * 480 * 3 / 2);
    WebRtcI420Buffer buffer {derivedVtable.slots, 640, 480, 640, 320, 320, data.data()};
    Require(_ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(&buffer) == &overridePlanes[0]);
    Require(_ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(&buffer) == &overridePlanes[1]);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&buffer) == &overridePlanes[2]);
}

void TestNullBufferThrows() {
    RequireInvalidArgument([] { _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(nullptr); });
}

std::vector<std::size_t> allocationSizes;
std::vector<void*> freedPointers;

void* APS5_VABI TestAllocate(std::size_t size) { allocationSizes.push_back(size); return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { freedPointers.push_back(pointer); std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { allocationSizes.push_back(count * size); return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { allocationSizes.push_back(size); return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    allocationSizes.push_back(size);
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

template<typename TFunction> TFunction BufferSlot(const WebRtcRefCountedI420Buffer* buffer, int slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(static_cast<const void* const*>(buffer->buffer.vtable)[slot]));
}

using IntGetter = int (APS5_VABI*)(const void*);
using DataGetter = const std::uint8_t* (APS5_VABI*)(const void*);
using AddRef = void (APS5_VABI*)(const void*);
using Release = std::int32_t (APS5_VABI*)(const void*);
using HasOneRef = bool (APS5_VABI*)(const void*);
using StorageRepresentation = WebRtcGuestString* (APS5_VABI*)(WebRtcGuestString*, const void*);
using ToI420 = void** (APS5_VABI*)(void**, void*);

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

void TestCreate() {
    allocationSizes.clear();
    freedPointers.clear();
    WebRtcRefCountedI420Buffer* buffer = nullptr;
    Require(_ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 640, 480, 640, 320, 328) == &buffer);
    Require(buffer != nullptr && buffer->refCount.load() == 1);
    const std::size_t size = 640 * 480 + (320 + 328) * 240;
    Require(allocationSizes.size() == 2 && allocationSizes[0] == sizeof(WebRtcRefCountedI420Buffer) && allocationSizes[1] == size + sizeof(std::uintptr_t) + 63);
    Require(BufferSlot<IntGetter>(buffer, 4)(buffer) == 1);
    Require(BufferSlot<IntGetter>(buffer, 5)(buffer) == 640 && BufferSlot<IntGetter>(buffer, 6)(buffer) == 480);
    Require(BufferSlot<IntGetter>(buffer, 12)(buffer) == 320 && BufferSlot<IntGetter>(buffer, 13)(buffer) == 240);
    Require(BufferSlot<IntGetter>(buffer, 14)(buffer) == 640 && BufferSlot<IntGetter>(buffer, 15)(buffer) == 320 && BufferSlot<IntGetter>(buffer, 16)(buffer) == 328);
    const std::uint8_t* y = BufferSlot<DataGetter>(buffer, 17)(buffer);
    const std::uint8_t* u = BufferSlot<DataGetter>(buffer, 18)(buffer);
    const std::uint8_t* v = BufferSlot<DataGetter>(buffer, 19)(buffer);
    Require(y != nullptr && reinterpret_cast<std::uintptr_t>(y) % 64 == 0 && y == buffer->buffer.data);
    Require(u == y + 640 * 480 && v == u + 320 * 240);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&buffer->buffer) == v);
    std::memset(buffer->buffer.data, 0x5A, size);
    WebRtcGuestString representation;
    std::memset(&representation, 0xA5, sizeof(representation));
    Require(BufferSlot<StorageRepresentation>(buffer, 11)(&representation, buffer) == &representation);
    Require(representation.size == 1 && representation.capacity == WebRtcGuestStringInlineCapacity && representation.buffer[0] == '?' && representation.buffer[1] == '\0');
    Require(BufferSlot<HasOneRef>(buffer, 20)(buffer));
    void* converted = nullptr;
    BufferSlot<ToI420>(buffer, 7)(&converted, buffer);
    Require(converted == buffer && buffer->refCount.load() == 2 && !BufferSlot<HasOneRef>(buffer, 20)(buffer));
    BufferSlot<AddRef>(buffer, 0)(buffer);
    Require(BufferSlot<Release>(buffer, 1)(buffer) == 1 && BufferSlot<Release>(buffer, 1)(buffer) == 1);
    Require(freedPointers.empty());
    Require(BufferSlot<Release>(buffer, 1)(buffer) == 0);
    Require(freedPointers.size() == 2 && freedPointers[1] == buffer);
}

void TestCreateRejectsInvalidDimensions() {
    allocationSizes.clear();
    WebRtcRefCountedI420Buffer* buffer = nullptr;
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 0, 4, 4, 2, 2); });
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 4, 0, 4, 2, 2); });
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 4, 4, 3, 2, 2); });
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 4, 4, 4, 0, 2); });
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 4, 4, 4, 2, 0); });
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 1, 2, 2147483647, 1, 1); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(nullptr, 4, 4, 4, 2, 2); });
    Require(allocationSizes.empty() && buffer == nullptr);
}

void TestCreateSmallestBuffer() {
    freedPointers.clear();
    WebRtcRefCountedI420Buffer* buffer = nullptr;
    _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 1, 1, 1, 1, 1);
    const std::uint8_t* y = BufferSlot<DataGetter>(buffer, 17)(buffer);
    Require(BufferSlot<DataGetter>(buffer, 18)(buffer) == y + 1 && BufferSlot<DataGetter>(buffer, 19)(buffer) == y + 2);
    Require(BufferSlot<Release>(buffer, 1)(buffer) == 0 && freedPointers.size() == 2);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Check(640, 480, 640, 320, 320);
    Check(641, 361, 641, 321, 321);
    Check(1, 1, 1, 1, 1);
    Check(1920, 1080, 1984, 992, 1000);
    Check(0, 0, 0, 0, 0);
    TestOverriddenGetters();
    TestNullBufferThrows();
    TestCreate();
    TestCreateRejectsInvalidDimensions();
    TestCreateSmallestBuffer();
    return 0;
}

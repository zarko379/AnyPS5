#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtc::IPAddress check failed at line %u\n", location.line());
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

void* lastFreed;
void* APS5_VABI AddressAllocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI AddressFree_nid_no_patch(void* pointer) {
    lastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI AddressCalloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI AddressReallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI AddressAlign_nid_no_patch(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI AddressRealign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI AddressPosixAlign_nid_no_patch(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

WebRtcIpAddress Address(std::int32_t family, std::initializer_list<std::uint8_t> bytes) {
    WebRtcIpAddress address;
    std::memset(&address, 0x5A, sizeof(address));
    address.vtable = &address;
    address.family = family;
    std::memset(&address.address, 0, sizeof(address.address));
    std::size_t index = 0;
    for (std::uint8_t byte : bytes)
        address.address.ip6.bytes[index++] = byte;
    return address;
}

WebRtcIpAddress V4(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    return Address(WebRtcAfInet, {a, b, c, d});
}

WebRtcIpAddress V6Prefix(int ones) {
    WebRtcIpAddress address = Address(WebRtcAfInet6, {});
    for (int bit = 0; bit < ones; ++bit)
        address.address.ip6.bytes[bit / 8] |= static_cast<std::uint8_t>(0x80u >> (bit % 8));
    return address;
}

bool Equal(const WebRtcIpAddress& left, const WebRtcIpAddress& right) {
    return _ZNK3rtc9IPAddresseqERKS0__nid_postfix(&left, &right);
}

int MaskBits(const WebRtcIpAddress& mask) {
    return _ZN3rtc15CountIPMaskBitsERKNS_9IPAddressE_nid_postfix(&mask);
}

void TestAccessors() {
    WebRtcIpAddress v4 = V4(192, 168, 1, 10);
    WebRtcInAddr ip4 = _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(&v4);
    Require(std::memcmp(&ip4, v4.address.ip6.bytes, 4) == 0);
    Require(_ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(&v4) == 0xC0A8010Au);

    WebRtcIpAddress v6 = Address(WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01});
    WebRtcIn6Addr ip6 = _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(&v6);
    Require(std::memcmp(ip6.bytes, v6.address.ip6.bytes, 16) == 0);
    WebRtcInAddr v6AsV4 = _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(&v6);
    Require(std::memcmp(&v6AsV4, v6.address.ip6.bytes, 4) == 0);
    WebRtcIn6Addr v4AsV6 = _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(&v4);
    Require(std::memcmp(v4AsV6.bytes, v4.address.ip6.bytes, 16) == 0);
    Require(_ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(&v6) == 0);

    WebRtcIpAddress unspec = Address(WebRtcAfUnspec, {1, 2, 3, 4});
    Require(_ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(&unspec) == 0);
}

void TestEquality() {
    Require(Equal(V4(10, 0, 0, 1), V4(10, 0, 0, 1)));
    Require(!Equal(V4(10, 0, 0, 1), V4(10, 0, 0, 2)));

    WebRtcIpAddress v4 = V4(10, 0, 0, 1);
    WebRtcIpAddress v4Tail = V4(10, 0, 0, 1);
    v4Tail.address.ip6.bytes[15] = 0xFF;
    Require(Equal(v4, v4Tail));

    WebRtcIpAddress v6 = Address(WebRtcAfInet6, {10, 0, 0, 1});
    Require(!Equal(v4, v6));
    Require(Equal(v6, Address(WebRtcAfInet6, {10, 0, 0, 1})));
    WebRtcIpAddress v6Tail = v6;
    v6Tail.address.ip6.bytes[15] = 1;
    Require(!Equal(v6, v6Tail));

    Require(Equal(Address(WebRtcAfUnspec, {1}), Address(WebRtcAfUnspec, {2})));
    WebRtcIpAddress other = Address(1, {});
    Require(!Equal(other, other));
}

void TestMaskBits() {
    Require(MaskBits(V4(255, 255, 255, 0)) == 24);
    Require(MaskBits(V4(255, 255, 255, 255)) == 32);
    Require(MaskBits(V4(128, 0, 0, 0)) == 1);
    Require(MaskBits(V4(0, 0, 0, 0)) == 0);
    Require(MaskBits(V4(255, 0, 255, 0)) == 24);
    Require(MaskBits(V4(0, 0, 0, 1)) == 32);

    for (int ones : {0, 1, 31, 32, 33, 60, 64, 96, 127, 128})
        Require(MaskBits(V6Prefix(ones)) == ones);
    WebRtcIpAddress gap = V6Prefix(64);
    gap.address.ip6.bytes[9] = 0x01;
    Require(MaskBits(gap) == 80);
    WebRtcIpAddress lateBit = Address(WebRtcAfInet6, {0, 0, 0, 0, 0, 0, 0, 0x01});
    Require(MaskBits(lateBit) == 0);

    Require(MaskBits(Address(WebRtcAfUnspec, {255, 255, 255, 0})) == 0);
    Require(MaskBits(Address(1, {255, 255, 255, 0})) == 0);
}

struct Truncated {
    WebRtcIpAddress address;
    unsigned char padding[4];
};

Truncated Truncate(const WebRtcIpAddress& ip, int length) {
    Truncated result;
    std::memset(&result, 0xC3, sizeof(result));
    Require(_ZN3rtc10TruncateIPERKNS_9IPAddressEi_nid_postfix(&result.address, &ip, length) == &result.address);
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(&result.address);
    for (std::size_t offset = 28; offset < sizeof(WebRtcIpAddress); ++offset)
        Require(raw[offset] == 0xC3);
    for (unsigned char byte : result.padding)
        Require(byte == 0xC3);
    Require(result.address.vtable != nullptr && result.address.vtable != ip.vtable);
    return result;
}

void RequireBytes(const WebRtcIpAddress& address, std::int32_t family, std::initializer_list<std::uint8_t> bytes) {
    Require(address.family == family);
    std::uint8_t expected[16] = {};
    std::size_t index = 0;
    for (std::uint8_t byte : bytes)
        expected[index++] = byte;
    Require(std::memcmp(address.address.ip6.bytes, expected, sizeof(expected)) == 0);
}

void TestTruncateIpv4() {
    WebRtcIpAddress v4 = V4(10, 11, 12, 13);
    v4.address.ip6.bytes[4] = 0x77;
    v4.address.ip6.bytes[15] = 0x99;
    RequireBytes(Truncate(v4, 8).address, WebRtcAfInet, {10});
    RequireBytes(Truncate(v4, 24).address, WebRtcAfInet, {10, 11, 12});
    RequireBytes(Truncate(v4, 1).address, WebRtcAfInet, {0});
    RequireBytes(Truncate(V4(255, 255, 255, 255), 1).address, WebRtcAfInet, {0x80});
    RequireBytes(Truncate(V4(255, 255, 255, 255), 13).address, WebRtcAfInet, {0xFF, 0xF8});
    RequireBytes(Truncate(v4, 31).address, WebRtcAfInet, {10, 11, 12, 12});
    RequireBytes(Truncate(v4, 0).address, WebRtcAfInet, {});
    RequireBytes(Truncate(v4, 32).address, WebRtcAfInet, {10, 11, 12, 13, 0x77, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x99});
    RequireBytes(Truncate(v4, 1000).address, WebRtcAfInet, {10, 11, 12, 13, 0x77, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x99});
    RequireBytes(Truncate(v4, -1).address, WebRtcAfUnspec, {});
}

void TestTruncateIpv6() {
    WebRtcIpAddress v6 = Address(WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x85, 0xA3, 0x08, 0xD3, 0x13, 0x19, 0x8A, 0x2E, 0x03, 0x70, 0x73, 0x34});
    RequireBytes(Truncate(v6, 0).address, WebRtcAfInet6, {});
    RequireBytes(Truncate(v6, 1).address, WebRtcAfInet6, {0x00});
    RequireBytes(Truncate(v6, 3).address, WebRtcAfInet6, {0x20});
    RequireBytes(Truncate(v6, 16).address, WebRtcAfInet6, {0x20, 0x01});
    RequireBytes(Truncate(v6, 31).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8});
    RequireBytes(Truncate(v6, 32).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8});
    RequireBytes(Truncate(v6, 33).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x80});
    RequireBytes(Truncate(v6, 64).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x85, 0xA3, 0x08, 0xD3});
    RequireBytes(Truncate(v6, 100).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x85, 0xA3, 0x08, 0xD3, 0x13, 0x19, 0x8A, 0x2E, 0x00});
    RequireBytes(Truncate(v6, 127).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x85, 0xA3, 0x08, 0xD3, 0x13, 0x19, 0x8A, 0x2E, 0x03, 0x70, 0x73, 0x34});
    WebRtcIpAddress ones = V6Prefix(128);
    RequireBytes(Truncate(ones, 127).address, WebRtcAfInet6, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE});
    RequireBytes(Truncate(ones, 96).address, WebRtcAfInet6, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
    RequireBytes(Truncate(ones, 65).address, WebRtcAfInet6, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80});
    RequireBytes(Truncate(v6, 128).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x85, 0xA3, 0x08, 0xD3, 0x13, 0x19, 0x8A, 0x2E, 0x03, 0x70, 0x73, 0x34});
    RequireBytes(Truncate(v6, 4096).address, WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0x85, 0xA3, 0x08, 0xD3, 0x13, 0x19, 0x8A, 0x2E, 0x03, 0x70, 0x73, 0x34});
    RequireBytes(Truncate(v6, -5).address, WebRtcAfUnspec, {});
}

void TestTruncateOtherFamilies() {
    RequireBytes(Truncate(Address(WebRtcAfUnspec, {1, 2, 3, 4}), 8).address, WebRtcAfUnspec, {});
    RequireBytes(Truncate(Address(1, {1, 2, 3, 4}), 8).address, WebRtcAfUnspec, {});
    RequireBytes(Truncate(Address(1, {1, 2, 3, 4}), 200).address, WebRtcAfUnspec, {});
}

void TestVtable() {
    Truncated first = Truncate(V4(1, 2, 3, 4), 8);
    Truncated second = Truncate(Address(WebRtcAfInet6, {1}), 8);
    Require(first.address.vtable == second.address.vtable);
    Require(first.address.vtable == WebRtcIpAddressVtable());
    using Destructor = void(APS5_VABI*)(WebRtcIpAddress*);
    const auto* slots = static_cast<void* const*>(first.address.vtable);
    Require(slots[-1] == nullptr && slots[-2] == nullptr);
    const Truncated before = first;
    first.address.vtable = &first;
    reinterpret_cast<Destructor>(slots[0])(&first.address);
    Require(std::memcmp(&first, &before, sizeof(first)) == 0);

    auto* heapObject = static_cast<WebRtcIpAddress*>(_Znwm_nid_postfix(sizeof(WebRtcIpAddress)));
    _ZN3rtc10TruncateIPERKNS_9IPAddressEi_nid_postfix(heapObject, &first.address, 32);
    lastFreed = nullptr;
    reinterpret_cast<Destructor>(static_cast<void* const*>(heapObject->vtable)[1])(heapObject);
    Require(lastFreed == heapObject);

    WebRtcIpAddress constructed;
    std::memset(&constructed, 0xC3, sizeof(constructed));
    WebRtcConstructIpAddress(&constructed);
    Require(constructed.vtable == WebRtcIpAddressVtable());
    RequireBytes(constructed, WebRtcAfUnspec, {});
    Require(reinterpret_cast<const unsigned char*>(&constructed)[28] == 0xC3);
}

void TestNullAddress() {
    WebRtcIpAddress v4 = V4(10, 0, 0, 1);
    RequireInvalidArgument([] { _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { _ZNK3rtc9IPAddresseqERKS0__nid_postfix(nullptr, &v4); });
    RequireInvalidArgument([&] { _ZNK3rtc9IPAddresseqERKS0__nid_postfix(&v4, nullptr); });
    RequireInvalidArgument([] { _ZN3rtc15CountIPMaskBitsERKNS_9IPAddressE_nid_postfix(nullptr); });
    WebRtcIpAddress result;
    RequireInvalidArgument([&] { _ZN3rtc10TruncateIPERKNS_9IPAddressEi_nid_postfix(&result, nullptr, 8); });
    RequireInvalidArgument([&] { _ZN3rtc10TruncateIPERKNS_9IPAddressEi_nid_postfix(nullptr, &v4, 8); });
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&AddressAllocate_nid_no_patch), reinterpret_cast<void*>(&AddressFree_nid_no_patch),
        reinterpret_cast<void*>(&AddressCalloc_nid_no_patch), reinterpret_cast<void*>(&AddressReallocate_nid_no_patch),
        reinterpret_cast<void*>(&AddressAlign_nid_no_patch), reinterpret_cast<void*>(&AddressRealign_nid_no_patch), reinterpret_cast<void*>(&AddressPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestAccessors();
    TestEquality();
    TestMaskBits();
    TestTruncateIpv4();
    TestTruncateIpv6();
    TestTruncateOtherFamilies();
    TestVtable();
    TestNullAddress();
    return 0;
}

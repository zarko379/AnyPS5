#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SocketAddress check failed at line %u\n", location.line());
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

void* lastAllocated;
void* lastFreed;
std::size_t lastSize;
void* APS5_VABI AddressAllocate_nid_no_patch(std::size_t size) {
    lastSize = size;
    return lastAllocated = std::malloc(size);
}
void APS5_VABI AddressFree_nid_no_patch(void* pointer) {
    lastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI AddressCalloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI AddressReallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI AddressAlign_nid_no_patch(std::size_t, std::size_t size) { return AddressAllocate_nid_no_patch(size); }
void* APS5_VABI AddressRealign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI AddressPosixAlign_nid_no_patch(void** pointer, std::size_t, std::size_t size) {
    *pointer = AddressAllocate_nid_no_patch(size);
    return *pointer ? 0 : 12;
}

WebRtcSocketAddress Address(std::int32_t family, std::uint16_t port) {
    WebRtcSocketAddress address;
    std::memset(&address, 0, sizeof(address));
    address.ip.family = family;
    address.port = port;
    return address;
}

void SetIpv4(WebRtcSocketAddress& address, std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    const std::uint8_t bytes[4] = {a, b, c, d};
    std::memcpy(&address.ip.address.ip4, bytes, sizeof(bytes));
}

void SetHostname(WebRtcSocketAddress& address, const char* name) {
    std::size_t length = std::strlen(name);
    Require(length <= 15);
    std::memcpy(address.hostname.buffer, name, length + 1);
    address.hostname.size = length;
    address.hostname.capacity = 15;
}

bool IsAnyIP(const WebRtcSocketAddress& address) {
    return _ZNK3rtc13SocketAddress7IsAnyIPEv_nid_postfix(&address);
}

bool IsComplete(const WebRtcSocketAddress& address) {
    return _ZNK3rtc13SocketAddress10IsCompleteEv_nid_postfix(&address);
}

bool IsNil(const WebRtcSocketAddress& address) {
    return _ZNK3rtc13SocketAddress5IsNilEv_nid_postfix(&address);
}

void TestPortAndIpaddr() {
    WebRtcSocketAddress address = Address(WebRtcAfInet, 3478);
    SetIpv4(address, 192, 168, 1, 7);
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(&address) == 3478);
    Require(_ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(&address) == &address.ip);
    address.port = 0xFFFF;
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(&address) == 0xFFFF);
}

void TestIpv4() {
    WebRtcSocketAddress address = Address(WebRtcAfInet, 0);
    Require(IsAnyIP(address) && !IsComplete(address) && !IsNil(address));
    address.port = 80;
    Require(IsAnyIP(address) && !IsComplete(address));
    SetIpv4(address, 127, 0, 0, 1);
    Require(!IsAnyIP(address) && IsComplete(address));
    address.port = 0;
    Require(!IsComplete(address) && !IsNil(address));
}

void TestIpv6() {
    WebRtcSocketAddress address = Address(WebRtcAfInet6, 443);
    Require(IsAnyIP(address) && !IsComplete(address));
    address.ip.address.ip6.bytes[10] = 0xFF;
    address.ip.address.ip6.bytes[11] = 0xFF;
    Require(IsAnyIP(address) && !IsComplete(address));
    address.ip.address.ip6.bytes[15] = 1;
    Require(!IsAnyIP(address) && IsComplete(address));
    WebRtcSocketAddress loopback = Address(WebRtcAfInet6, 443);
    loopback.ip.address.ip6.bytes[15] = 1;
    Require(!IsAnyIP(loopback) && IsComplete(loopback));
    WebRtcSocketAddress halfMapped = Address(WebRtcAfInet6, 443);
    halfMapped.ip.address.ip6.bytes[10] = 0xFF;
    Require(!IsAnyIP(halfMapped));
}

void TestUnspecified() {
    WebRtcSocketAddress address = Address(WebRtcAfUnspec, 0);
    Require(!IsAnyIP(address) && !IsComplete(address) && IsNil(address));
    address.port = 1;
    Require(!IsNil(address) && IsComplete(address));
    address.port = 0;
    SetHostname(address, "example.org");
    Require(!IsNil(address));
    address.hostname.size = 0;
    address.hostname.buffer[0] = '\0';
    Require(IsNil(address));
    char longName[] = "stun.long-hostname.example.org";
    address.hostname.pointer = longName;
    address.hostname.size = sizeof(longName) - 1;
    address.hostname.capacity = 31;
    Require(!IsNil(address));
}

struct Guarded {
    WebRtcSocketAddress address;
    unsigned char after[8];
};

void Fill(Guarded& guarded) {
    std::memset(&guarded, 0xC3, sizeof(guarded));
}

void RequireGuards(const Guarded& guarded) {
    const auto* raw = reinterpret_cast<const unsigned char*>(&guarded.address);
    for (std::size_t offset : {std::size_t{28}, std::size_t{29}, std::size_t{30}, std::size_t{31}})
        Require(raw[40 + offset] == 0xC3);
    Require(raw[74] == 0xC3 && raw[75] == 0xC3);
    for (std::size_t offset = 81; offset < sizeof(WebRtcSocketAddress); ++offset)
        Require(raw[offset] == 0xC3);
    for (unsigned char byte : guarded.after)
        Require(byte == 0xC3);
}

const char* HostnameData(const WebRtcSocketAddress& address) {
    return WebRtcGuestStringData(address.hostname);
}

WebRtcSocketAddress Source(const char* hostname, char* storage) {
    WebRtcSocketAddress address;
    std::memset(&address, 0x5A, sizeof(address));
    const std::size_t length = std::strlen(hostname);
    address.hostname.reserved = 0;
    if (length > 15) {
        std::memcpy(storage, hostname, length + 1);
        address.hostname.pointer = storage;
        address.hostname.capacity = length | 15;
    } else {
        std::memcpy(address.hostname.buffer, hostname, length + 1);
        address.hostname.capacity = 15;
    }
    address.hostname.size = length;
    address.ip.vtable = &address;
    address.ip.family = WebRtcAfInet6;
    for (std::uint8_t index = 0; index < 16; ++index)
        address.ip.address.ip6.bytes[index] = static_cast<std::uint8_t>(0xA0 + index);
    address.port = 5349;
    address.scopeId = -7;
    address.literal = true;
    return address;
}

void RequireCopied(const WebRtcSocketAddress& address, const WebRtcSocketAddress& source) {
    Require(address.hostname.size == source.hostname.size);
    Require(std::memcmp(HostnameData(address), HostnameData(source), source.hostname.size) == 0);
    Require(HostnameData(address)[address.hostname.size] == '\0');
    Require(address.ip.family == source.ip.family);
    Require(std::memcmp(&address.ip.address, &source.ip.address, 16) == 0);
    Require(address.port == source.port && address.scopeId == source.scopeId && address.literal == source.literal);
}

void TestCopyConstructor() {
    char shortStorage[1];
    const WebRtcSocketAddress shortSource = Source("stun.example", shortStorage);
    const WebRtcSocketAddress shortBefore = shortSource;
    Guarded guarded;
    Fill(guarded);
    _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&guarded.address, &shortSource);
    RequireCopied(guarded.address, shortSource);
    Require(guarded.address.ip.vtable == WebRtcIpAddressVtable());
    Require(guarded.address.hostname.capacity == 15 && guarded.address.hostname.reserved == 0);
    Require(std::memcmp(&shortSource, &shortBefore, sizeof(shortSource)) == 0);
    RequireGuards(guarded);

    char longStorage[64];
    const WebRtcSocketAddress longSource = Source("turn.long-hostname.example.org", longStorage);
    Fill(guarded);
    lastAllocated = nullptr;
    _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&guarded.address, &longSource);
    RequireCopied(guarded.address, longSource);
    Require(guarded.address.ip.vtable == WebRtcIpAddressVtable());
    Require(guarded.address.hostname.pointer != longStorage && guarded.address.hostname.pointer == lastAllocated);
    Require(guarded.address.hostname.capacity == 31 && lastSize == 32);
    RequireGuards(guarded);
}

void TestAssignment() {
    char longStorage[64];
    const WebRtcSocketAddress longSource = Source("turn.long-hostname.example.org", longStorage);
    Guarded guarded;
    Fill(guarded);
    _ZN3rtc13SocketAddressC1Ev_nid_postfix(&guarded.address);
    const void* const vtable = guarded.address.ip.vtable;
    Require(_ZN3rtc13SocketAddressaSERKS0__nid_postfix(&guarded.address, &longSource) == &guarded.address);
    RequireCopied(guarded.address, longSource);
    Require(guarded.address.ip.vtable == vtable);
    char* heap = guarded.address.hostname.pointer;
    Require(heap == lastAllocated && guarded.address.hostname.capacity == 31);

    char shortStorage[1];
    WebRtcSocketAddress shortSource = Source("a.b", shortStorage);
    shortSource.ip.family = WebRtcAfInet;
    shortSource.port = 1;
    shortSource.scopeId = 0;
    shortSource.literal = false;
    lastFreed = nullptr;
    _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&guarded.address, &shortSource);
    RequireCopied(guarded.address, shortSource);
    Require(guarded.address.hostname.pointer == heap && guarded.address.hostname.capacity == 31 && lastFreed == nullptr);

    char longerStorage[64];
    const WebRtcSocketAddress longerSource = Source("relay.much-longer-hostname.example.org", longerStorage);
    _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&guarded.address, &longerSource);
    RequireCopied(guarded.address, longerSource);
    Require(lastFreed == heap && guarded.address.hostname.pointer == lastAllocated);
    Require(guarded.address.hostname.capacity == (longerSource.hostname.size | 15));

    const Guarded before = guarded;
    Require(_ZN3rtc13SocketAddressaSERKS0__nid_postfix(&guarded.address, &guarded.address) == &guarded.address);
    Require(std::memcmp(&guarded, &before, sizeof(guarded)) == 0);
    Require(std::memcmp(HostnameData(guarded.address), "relay.much-longer-hostname.example.org", 39) == 0);
    Require(guarded.address.ip.vtable == vtable);
}

void TestConstructorNullArguments() {
    WebRtcSocketAddress address;
    RequireInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(nullptr, &address); });
    RequireInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&address, nullptr); });
    RequireInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(nullptr, &address); });
    RequireInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&address, nullptr); });
}

void TestConstructors() {
    WebRtcSocketAddress defaulted;
    std::memset(&defaulted, 0x5A, sizeof(defaulted));
    _ZN3rtc13SocketAddressC1Ev_nid_postfix(&defaulted);
    Require(defaulted.hostname.size == 0);
    Require(defaulted.hostname.capacity == 15);
    Require(defaulted.hostname.buffer[0] == '\0');
    Require(defaulted.ip.family == WebRtcAfUnspec);
    Require(defaulted.port == 0);
    Require(defaulted.scopeId == 0);
    Require(!defaulted.literal);
    Require(IsNil(defaulted));

    WebRtcIpAddress ip;
    std::memset(&ip, 0, sizeof(ip));
    ip.family = WebRtcAfInet;
    const std::uint8_t bytes[4] = {10, 1, 2, 3};
    std::memcpy(&ip.address.ip4, bytes, sizeof(bytes));

    WebRtcSocketAddress address;
    std::memset(&address, 0x5A, sizeof(address));
    _ZN3rtc13SocketAddressC1ERKNS_9IPAddressEi_nid_postfix(&address, &ip, 8080);
    Require(address.hostname.size == 0);
    Require(address.hostname.capacity == 15);
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(&address) == 8080);
    Require(_ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(&address) != nullptr);
    Require(_ZNK3rtc9IPAddresseqERKS0__nid_postfix(&address.ip, &ip));
    Require(address.scopeId == 0);
    Require(!address.literal);
    Require(IsComplete(address));
    Require(!IsNil(address));
}

void TestNullAddressThrows() {
    try {
        _ZNK3rtc13SocketAddress4portEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&AddressAllocate_nid_no_patch), reinterpret_cast<void*>(&AddressFree_nid_no_patch),
        reinterpret_cast<void*>(&AddressCalloc_nid_no_patch), reinterpret_cast<void*>(&AddressReallocate_nid_no_patch),
        reinterpret_cast<void*>(&AddressAlign_nid_no_patch), reinterpret_cast<void*>(&AddressRealign_nid_no_patch), reinterpret_cast<void*>(&AddressPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestPortAndIpaddr();
    TestIpv4();
    TestIpv6();
    TestUnspecified();
    TestConstructors();
    TestNullAddressThrows();
    TestCopyConstructor();
    TestAssignment();
    TestConstructorNullArguments();
    return 0;
}

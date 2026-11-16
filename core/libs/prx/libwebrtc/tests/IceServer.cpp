#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
    std::fprintf(stderr, "libwebrtc IceServer check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

void SetString(WebRtcGuestString& target, std::string_view text) {
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

void SetVector(WebRtcGuestStringVector& vector, const std::vector<std::string_view>& values, std::size_t capacity) {
    auto* storage = static_cast<WebRtcGuestString*>(_Znwm_nid_postfix(capacity * sizeof(WebRtcGuestString)));
    for (std::size_t index = 0; index < values.size(); ++index)
        SetString(storage[index], values[index]);
    vector = {storage, storage + values.size(), storage + capacity};
}

bool IsEmpty(const WebRtcGuestString& string) {
    return string.size == 0 && string.capacity == WebRtcGuestStringInlineCapacity && string.buffer[0] == '\0';
}

void TestDefaultConstructor() {
    WebRtcIceServer server;
    std::memset(&server, 0xA5, sizeof(server));
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&server);
    Require(IsEmpty(server.uri) && IsEmpty(server.username) && IsEmpty(server.password) && IsEmpty(server.hostname));
    Require(server.urls.begin == nullptr && server.urls.end == nullptr && server.urls.capacity == nullptr);
    Require(server.tlsAlpnProtocols.begin == nullptr && server.tlsEllipticCurves.begin == nullptr);
    Require(server.tlsCertPolicy == 0);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&server);
    Require(live.empty());
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(nullptr); });
}

void TestCopyConstructor() {
    WebRtcIceServer source;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&source);
    SetString(source.uri, "stun:stun.example.org:19302");
    SetVector(source.urls, {"turn:a.example.org", "turns:a-much-longer-relay.example.org:443?transport=tcp"}, 3);
    SetString(source.username, "user");
    SetString(source.password, "a-password-longer-than-fifteen");
    source.tlsCertPolicy = 1;
    SetString(source.hostname, "host");
    SetVector(source.tlsAlpnProtocols, {"h2"}, 1);
    const std::size_t before = live.size();
    WebRtcIceServer copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source);
    Require(live.size() == before + 6);
    Require(Read(copy.uri) == "stun:stun.example.org:19302" && copy.uri.pointer != source.uri.pointer);
    Require(copy.urls.end - copy.urls.begin == 2 && copy.urls.capacity == copy.urls.end && copy.urls.begin != source.urls.begin);
    Require(Read(copy.urls.begin[0]) == "turn:a.example.org" && Read(copy.urls.begin[1]) == "turns:a-much-longer-relay.example.org:443?transport=tcp");
    Require(Read(copy.username) == "user" && Read(copy.password) == "a-password-longer-than-fifteen");
    Require(copy.tlsCertPolicy == 1 && Read(copy.hostname) == "host");
    Require(copy.tlsAlpnProtocols.end - copy.tlsAlpnProtocols.begin == 1 && Read(copy.tlsAlpnProtocols.begin[0]) == "h2");
    Require(copy.tlsEllipticCurves.begin == nullptr && copy.tlsEllipticCurves.capacity == nullptr);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&copy);
    Require(live.size() == before);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestInvalidServersThrow() {
    WebRtcIceServer source;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&source);
    WebRtcIceServer copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    const WebRtcIceServer untouched = copy;
    source.hostname.capacity = 3;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source); });
    source.hostname.capacity = WebRtcGuestStringInlineCapacity;
    SetVector(source.urls, {"a", "b"}, 2);
    WebRtcGuestString* const end = source.urls.end;
    source.urls.end = reinterpret_cast<WebRtcGuestString*>(reinterpret_cast<unsigned char*>(source.urls.begin) + 8);
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source); });
    Require(std::memcmp(&copy, &untouched, sizeof(copy)) == 0);
    source.urls.end = end;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(nullptr, &source); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(nullptr); });
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestRtpTransceiverInit() {
    WebRtcRtpTransceiverInit init;
    std::memset(&init, 0xA5, sizeof(init));
    _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(&init);
    Require(init.direction == 0 && init.streamIds.begin == nullptr && init.streamIds.end == nullptr && init.streamIds.capacity == nullptr);
    Require(init.sendEncodingsBegin == nullptr && init.sendEncodingsEnd == nullptr && init.sendEncodingsCapacity == nullptr);
    _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init);
    SetVector(init.streamIds, {"stream", "a-stream-id-longer-than-fifteen"}, 4);
    init.sendEncodingsBegin = _Znwm_nid_postfix(64);
    init.sendEncodingsEnd = init.sendEncodingsBegin;
    init.sendEncodingsCapacity = static_cast<unsigned char*>(init.sendEncodingsBegin) + 64;
    _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init);
    Require(live.empty());
    _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(&init);
    init.sendEncodingsBegin = _Znwm_nid_postfix(64);
    init.sendEncodingsEnd = static_cast<unsigned char*>(init.sendEncodingsBegin) + 32;
    init.sendEncodingsCapacity = init.sendEncodingsEnd;
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init); });
    Require(live.size() == 1);
    init.sendEncodingsEnd = init.sendEncodingsBegin;
    init.sendEncodingsCapacity = static_cast<unsigned char*>(init.sendEncodingsBegin) - 1;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init); });
    init.sendEncodingsCapacity = init.sendEncodingsBegin;
    _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init);
    Require(live.empty());
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(nullptr); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestDefaultConstructor();
    TestCopyConstructor();
    TestInvalidServersThrow();
    TestRtpTransceiverInit();
    return 0;
}

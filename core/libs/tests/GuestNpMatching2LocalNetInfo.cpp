#include "prx/libSceNpMatching2/include/Api.hpp"
#include "prx/libSceNetCtl/include/NetCtl.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <thread>

extern "C" void NetCtlSetStunServers_nid_no_patch(const sockaddr_in* servers, std::size_t count, int timeoutMs);

namespace {

constexpr int NotInitialized = static_cast<int>(0x80550C03u);
constexpr int InvalidArgument = static_cast<int>(0x80550C0Au);
constexpr int NetCtlNotConnected = static_cast<int>(0x80412108u);
constexpr int NetCtlInfoIpAddress = 14;
constexpr std::uint64_t NetInfoSize = 0x18;
constexpr std::uint8_t Fill = 0xA5;
constexpr std::uint32_t StunCookie = 0x2112A442u;
constexpr std::uint16_t MappedPort = 54321;

#ifdef _WIN32
using TestSocket = SOCKET;
using AddressLength = int;
constexpr TestSocket InvalidSocket = INVALID_SOCKET;
void CloseSocket(TestSocket descriptor) { closesocket(descriptor); }
#else
using TestSocket = int;
using AddressLength = socklen_t;
constexpr TestSocket InvalidSocket = -1;
void CloseSocket(TestSocket descriptor) { close(descriptor); }
#endif

int failures = 0;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "sceNpMatching2SignalingGetLocalNetInfo: %s\n", what);
    ++failures;
}

NpMatching2SignalingNetInfo Filled(std::uint64_t size) {
    NpMatching2SignalingNetInfo info;
    std::memset(&info, Fill, sizeof(info));
    info.size = size;
    return info;
}

bool Untouched(const NpMatching2SignalingNetInfo& info, std::uint64_t size) {
    const NpMatching2SignalingNetInfo expected = Filled(size);
    return std::memcmp(&info, &expected, sizeof(info)) == 0;
}

bool PaddingCleared(const NpMatching2SignalingNetInfo& info) {
    for (const std::uint8_t byte : info.padding) {
        if (byte != 0) return false;
    }
    return true;
}

bool HostAddress(std::uint32_t& address) {
    NetCtlInfo info{};
    const int result = sceNetCtlGetInfo(NetCtlInfoIpAddress, &info);
    Require(result == 0 || result == NetCtlNotConnected, "sceNetCtlGetInfo did not answer the IPv4 address or NOT_CONNECTED");
    address = htonl(INADDR_LOOPBACK);
    if (result != 0) return false;
    in_addr parsed{};
    Require(inet_pton(AF_INET, reinterpret_cast<const char*>(info.opaque), &parsed) == 1, "the NetCtl IPv4 address does not parse");
    address = parsed.s_addr;
    return true;
}

TestSocket OpenResponder(sockaddr_in& bound) {
    const TestSocket descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor == InvalidSocket) return InvalidSocket;
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    AddressLength length = sizeof(bound);
    if (bind(descriptor, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0 ||
        getsockname(descriptor, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        CloseSocket(descriptor);
        return InvalidSocket;
    }
    return descriptor;
}

bool AnswerBindingRequest(TestSocket responder, std::uint32_t mappedAddress) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(responder, &readable);
    timeval timeout{};
    timeout.tv_sec = 2;
#ifdef _WIN32
    if (select(0, &readable, nullptr, nullptr, &timeout) <= 0) return false;
#else
    if (select(responder + 1, &readable, nullptr, nullptr, &timeout) <= 0) return false;
#endif
    std::uint8_t packet[32] = {};
    sockaddr_in client{};
    AddressLength clientLength = sizeof(client);
    const auto received = recvfrom(responder, reinterpret_cast<char*>(packet), sizeof(packet), 0, reinterpret_cast<sockaddr*>(&client), &clientLength);
    if (received != 20 || packet[0] != 0x00 || packet[1] != 0x01) return false;
    const auto port = static_cast<std::uint16_t>(MappedPort ^ (StunCookie >> 16));
    const std::uint32_t address = mappedAddress ^ htonl(StunCookie);
    const std::uint8_t attribute[8] = {0x00, 0x20, 0x00, 0x08, 0x00, 0x01, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port & 0xFF)};
    packet[0] = 0x01;
    packet[1] = 0x01;
    packet[2] = 0x00;
    packet[3] = 12;
    std::memcpy(packet + 20, attribute, sizeof(attribute));
    std::memcpy(packet + 28, &address, sizeof(address));
    const auto sent = sendto(responder, reinterpret_cast<const char*>(packet), sizeof(packet), 0, reinterpret_cast<const sockaddr*>(&client), clientLength);
    return sent == static_cast<int>(sizeof(packet));
}

void CheckStunAnswer(std::uint32_t local) {
    sockaddr_in servers[2]{};
    const TestSocket first = OpenResponder(servers[0]);
    const TestSocket second = OpenResponder(servers[1]);
    Require(first != InvalidSocket && second != InvalidSocket, "no loopback UDP socket for the STUN responders");
    if (first == InvalidSocket || second == InvalidSocket) {
        if (first != InvalidSocket) CloseSocket(first);
        if (second != InvalidSocket) CloseSocket(second);
        return;
    }
    const std::uint8_t mappedBytes[4] = {203, 0, 113, 7};
    std::uint32_t mapped = 0;
    std::memcpy(&mapped, mappedBytes, sizeof(mapped));
    if (mapped == local) mapped ^= htonl(1);
    NetCtlSetStunServers_nid_no_patch(servers, 2, 1000);
    bool answered = false;
    std::thread responder([&] { answered = AnswerBindingRequest(first, mapped) && AnswerBindingRequest(second, mapped); });
    NpMatching2SignalingNetInfo info = Filled(NetInfoSize);
    const int result = sceNpMatching2SignalingGetLocalNetInfo(&info);
    responder.join();
    CloseSocket(first);
    CloseSocket(second);
    Require(answered, "the STUN responders did not get both binding requests");
    Require(result == 0, "a call with a STUN answer failed");
    Require(info.size == NetInfoSize, "the size field was changed");
    Require(info.localAddress == local, "the local address changed with a STUN answer");
    Require(info.mappedAddress == mapped, "the mapped address is not the address the STUN servers saw");
    Require(info.natStatus == 2, "the NAT status is not the NAT type 2 NetCtl found");
    Require(PaddingCleared(info), "the padding was not cleared");
}

}

int main() {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;
#endif
    NetCtlSetStunServers_nid_no_patch(nullptr, 0, 1);

    NpMatching2SignalingNetInfo info = Filled(NetInfoSize);
    Require(sceNpMatching2SignalingGetLocalNetInfo(nullptr) == NotInitialized, "a null pointer before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2SignalingGetLocalNetInfo(&info) == NotInitialized && Untouched(info, NetInfoSize), "a call before initialization did not return NOT_INITIALIZED or wrote the answer");

    NpMatching2InitializeParameter init{};
    init.size = 0x30;
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize failed");

    Require(sceNpMatching2SignalingGetLocalNetInfo(nullptr) == InvalidArgument, "a null pointer was taken");
    for (const std::uint64_t size : {0x0ull, 0x10ull, 0x14ull, 0x17ull, 0x19ull, 0x20ull, 0x100000018ull}) {
        info = Filled(size);
        Require(sceNpMatching2SignalingGetLocalNetInfo(&info) == InvalidArgument && Untouched(info, size), "a size other than 0x18 was taken or the answer was written");
    }

    std::uint32_t local = 0;
    const bool hasAddress = HostAddress(local);
    info = Filled(NetInfoSize);
    Require(sceNpMatching2SignalingGetLocalNetInfo(&info) == 0, "a call without a STUN server failed");
    Require(info.size == NetInfoSize, "the size field was changed");
    Require(info.localAddress == local, "the local address is not the NetCtl IPv4 address, or 127.0.0.1 without one");
    Require(info.mappedAddress == local, "without a STUN answer the mapped address is not the local address");
    Require(info.natStatus == 0, "without a STUN answer the NAT status is not 0");
    Require(PaddingCleared(info), "the padding was not cleared");

    if (hasAddress) CheckStunAnswer(local);

    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate failed");
    info = Filled(NetInfoSize);
    Require(sceNpMatching2SignalingGetLocalNetInfo(&info) == NotInitialized && Untouched(info, NetInfoSize), "a call after termination did not return NOT_INITIALIZED or wrote the answer");
    NetCtlSetStunServers_nid_no_patch(nullptr, 0, 0);

    if (failures != 0) return 1;
    std::puts("NpMatching2 local net info tests passed");
    return 0;
}

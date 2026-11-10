// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from shadPS4 src/core/libraries/np/np_matching2/np_matching2.cpp@2183807e592b36d4440384878165b43d930c1dee and src/core/libraries/np/np_matching2/np_matching2_signaling.cpp@2183807e592b36d4440384878165b43d930c1dee

#include "prx/libSceNpMatching2/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceNetCtl/include/NetCtl.hpp"
#include "prx/libc/include/General.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <system_error>

namespace {

constexpr int SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80550C03);
constexpr int SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550C0A);
constexpr std::uint64_t SIGNALING_NET_INFO_SIZE = 0x18;
constexpr int NET_CTL_INFO_IP_ADDRESS = 14;
constexpr std::uint8_t LOOPBACK_ADDRESS[4] = {127, 0, 0, 1};

static_assert(sizeof(NpMatching2SignalingNetInfo) == SIGNALING_NET_INFO_SIZE);
static_assert(offsetof(NpMatching2SignalingNetInfo, size) == 0x00);
static_assert(offsetof(NpMatching2SignalingNetInfo, localAddr) == 0x08);
static_assert(offsetof(NpMatching2SignalingNetInfo, mappedAddr) == 0x0C);
static_assert(offsetof(NpMatching2SignalingNetInfo, natStatus) == 0x10);
static_assert(offsetof(NpMatching2SignalingNetInfo, padding) == 0x14);

std::uint32_t ParseIpv4Address(const NetCtlInfo& info) {
    const char* text = reinterpret_cast<const char*>(info.opaque);
    const char* end = static_cast<const char*>(std::memchr(text, '\0', sizeof(info.opaque)));
    if (!end) return 0;
    std::uint8_t octets[4] = {};
    for (std::size_t index = 0; index < sizeof(octets); ++index) {
        if (index != 0) {
            if (text == end || *text != '.') return 0;
            ++text;
        }
        unsigned value = 0;
        const auto [next, error] = std::from_chars(text, end, value);
        if (error != std::errc{} || value > 0xFF) return 0;
        octets[index] = static_cast<std::uint8_t>(value);
        text = next;
    }
    if (text != end) return 0;
    std::uint32_t address = 0;
    std::memcpy(&address, octets, sizeof(address));
    return address;
}

std::uint32_t LocalIpv4Address() {
    NetCtlInfo info{};
    const std::uint32_t address = sceNetCtlGetInfo(NET_CTL_INFO_IP_ADDRESS, &info) == 0 ? ParseIpv4Address(info) : 0;
    if (address != 0) return address;
    std::uint32_t loopback = 0;
    std::memcpy(&loopback, LOOPBACK_ADDRESS, sizeof(loopback));
    return loopback;
}

}

extern "C" bool NpMatching2IsInitialized_nid_no_patch();

extern "C" {

int APS5_VABI sceNpMatching2SignalingGetLocalNetInfo(NpMatching2SignalingNetInfo* info) {
    if (!NpMatching2IsInitialized_nid_no_patch()) {
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    }
    if (!info || info->size != SIGNALING_NET_INFO_SIZE) {
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    }
    const std::uint32_t localAddress = LocalIpv4Address();
    std::uint32_t mappedAddress = 0;
    std::int32_t natStatus = 0;
    NetCtlNatInfo nat{};
    nat.size = sizeof(nat);
    if (sceNetCtlGetNatInfo(&nat) >= 0) {
        mappedAddress = nat.mapped_addr;
        natStatus = nat.nat_type;
    }
    info->localAddr = localAddress;
    info->mappedAddr = mappedAddress != 0 ? mappedAddress : localAddress;
    info->natStatus = natStatus;
    info->padding = 0;
    return 0;
}

}

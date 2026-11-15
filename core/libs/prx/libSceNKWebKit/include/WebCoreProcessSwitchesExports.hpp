#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct ResourceLoadObserver;

struct OptionalUInt64 {
    bool hasValue{};
    std::uint64_t value{};

    OptionalUInt64() = default;
    OptionalUInt64(bool engaged, std::uint64_t contents) : hasValue(engaged), value(contents) {}
    OptionalUInt64(const OptionalUInt64& other) : hasValue(other.hasValue), value(other.value) {}
    OptionalUInt64(OptionalUInt64&& other) noexcept : OptionalUInt64(other) {}
};

static_assert(sizeof(OptionalUInt64) == 16);
static_assert(alignof(OptionalUInt64) == 8);
static_assert(offsetof(OptionalUInt64, hasValue) == 0);
static_assert(offsetof(OptionalUInt64, value) == 8);
static_assert(!std::is_trivially_copy_constructible_v<OptionalUInt64>);
static_assert(!std::is_trivially_move_constructible_v<OptionalUInt64>);
static_assert(std::is_trivially_destructible_v<OptionalUInt64>);

bool MediaEngineConfigurationMockEnabled();

struct DeprecatedGlobalSettingsState {
    bool mockScrollbarsEnabled;
    bool usesOverlayScrollbars;
    bool mockScrollAnimatorEnabled;
    bool shouldRespectPriorityInCSSAttributeSetters;
    bool lowPowerVideoAudioBufferSizeEnabled;
    bool resourceLoadStatisticsEnabled;
    bool allowsAnySSLCertificate;
};

DeprecatedGlobalSettingsState CurrentDeprecatedGlobalSettings();

}

extern "C" {

bool APS5_VABI _ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv();
void APS5_VABI _ZN7WebCore21NetworkStorageSession27permitProcessToUseCookieAPIEb(bool value);
void APS5_VABI _ZN7WebCore20ResourceLoadObserver9setSharedERS0_(NKWebKit::WebCoreAbi::ResourceLoadObserver& observer);
NKWebKit::WebCoreAbi::ResourceLoadObserver* APS5_VABI _ZN7WebCore20ResourceLoadObserver14sharedIfExistsEv();
void APS5_VABI _ZN7WebCore31MediaEngineConfigurationFactory10enableMockEv();
void APS5_VABI _ZN7WebCore31MediaEngineConfigurationFactory11disableMockEv();
NKWebKit::WebCoreAbi::OptionalUInt64 APS5_VABI _ZN7WebCore18PerformanceLogging17physicalFootprintEv();
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(bool flag);
bool APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv();
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb(bool flag);
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings25setUsesMockScrollAnimatorEb(bool flag);
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings45setShouldRespectPriorityInCSSAttributeSettersEb(bool flag);
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings38setLowPowerVideoAudioBufferSizeEnabledEb(bool flag);
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings32setResourceLoadStatisticsEnabledEb(bool flag);
void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb(bool allowAnySSLCertificate);
void APS5_VABI _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(bool shouldUseSmoothing);
bool APS5_VABI _ZN7WebCore11FontCascade18shouldUseSmoothingEv();

}

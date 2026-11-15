#include "prx/libSceNKWebKit/include/WebCoreProcessSwitchesExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::CurrentDeprecatedGlobalSettings;
using NKWebKit::WebCoreAbi::DeprecatedGlobalSettingsState;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore global settings check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Same(const DeprecatedGlobalSettingsState& a, const DeprecatedGlobalSettingsState& b) {
    return a.mockScrollbarsEnabled == b.mockScrollbarsEnabled && a.usesOverlayScrollbars == b.usesOverlayScrollbars
        && a.mockScrollAnimatorEnabled == b.mockScrollAnimatorEnabled
        && a.shouldRespectPriorityInCSSAttributeSetters == b.shouldRespectPriorityInCSSAttributeSetters
        && a.lowPowerVideoAudioBufferSizeEnabled == b.lowPowerVideoAudioBufferSizeEnabled
        && a.resourceLoadStatisticsEnabled == b.resourceLoadStatisticsEnabled
        && a.allowsAnySSLCertificate == b.allowsAnySSLCertificate;
}

using Setter = void (APS5_VABI*)(bool);

void CheckSetter(Setter set, bool DeprecatedGlobalSettingsState::*field, std::source_location location = std::source_location::current()) {
    DeprecatedGlobalSettingsState before = CurrentDeprecatedGlobalSettings();
    Require(!(before.*field), location);
    set(true);
    DeprecatedGlobalSettingsState expected = before;
    expected.*field = true;
    Require(Same(CurrentDeprecatedGlobalSettings(), expected), location);
    set(true);
    Require(Same(CurrentDeprecatedGlobalSettings(), expected), location);
    set(false);
    Require(Same(CurrentDeprecatedGlobalSettings(), before), location);
    set(false);
    Require(Same(CurrentDeprecatedGlobalSettings(), before), location);
    set(true);
    Require(Same(CurrentDeprecatedGlobalSettings(), expected), location);
}

void CheckDeprecatedGlobalSettings() {
    DeprecatedGlobalSettingsState initial = CurrentDeprecatedGlobalSettings();
    Require(Same(initial, DeprecatedGlobalSettingsState{false, false, false, false, false, false, false}));
    Require(!_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());

    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb, &DeprecatedGlobalSettingsState::mockScrollbarsEnabled);
    Require(_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());
    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb, &DeprecatedGlobalSettingsState::usesOverlayScrollbars);
    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings25setUsesMockScrollAnimatorEb, &DeprecatedGlobalSettingsState::mockScrollAnimatorEnabled);
    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings45setShouldRespectPriorityInCSSAttributeSettersEb,
        &DeprecatedGlobalSettingsState::shouldRespectPriorityInCSSAttributeSetters);
    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings38setLowPowerVideoAudioBufferSizeEnabledEb,
        &DeprecatedGlobalSettingsState::lowPowerVideoAudioBufferSizeEnabled);
    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings32setResourceLoadStatisticsEnabledEb, &DeprecatedGlobalSettingsState::resourceLoadStatisticsEnabled);
    CheckSetter(_ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb, &DeprecatedGlobalSettingsState::allowsAnySSLCertificate);
    Require(Same(CurrentDeprecatedGlobalSettings(), DeprecatedGlobalSettingsState{true, true, true, true, true, true, true}));
    Require(_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());

    _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(false);
    Require(!_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());
    Require(Same(CurrentDeprecatedGlobalSettings(), DeprecatedGlobalSettingsState{false, true, true, true, true, true, true}));
}

void CheckFontSmoothing() {
    Require(_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(false);
    Require(!_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(false);
    Require(!_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
    Require(!_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());
    _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(true);
    Require(_ZN7WebCore11FontCascade18shouldUseSmoothingEv());
}

}

int main() {
    CheckFontSmoothing();
    CheckDeprecatedGlobalSettings();
    CheckFontSmoothing();
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage scrolling check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, location);
}

using Getter = bool (APS5_VABI*)(const void*);
using Setter = void (APS5_VABI*)(const void*, bool);

void CheckBoolean(Getter get, Setter set, bool initial, std::source_location location = std::source_location::current()) {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(get(pageRef) == initial, location);
    set(pageRef, !initial);
    Require(get(pageRef) == !initial, location);
    set(pageRef, !initial);
    Require(get(pageRef) == !initial, location);
    set(pageRef, initial);
    Require(get(pageRef) == initial, location);
    RequireInvalidArgument([get] { get(nullptr); }, location);
    RequireInvalidArgument([set] { set(nullptr, true); }, location);
    RequireInvalidArgument([set] { set(nullptr, false); }, location);
    WKRelease(pageRef);
}

void CheckEdgesAreIndependent() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    WKPageSetRubberBandsAtLeft(pageRef, false);
    Require(!WKPageRubberBandsAtLeft(pageRef));
    Require(WKPageRubberBandsAtRight(pageRef));
    Require(WKPageRubberBandsAtTop(pageRef));
    Require(WKPageRubberBandsAtBottom(pageRef));
    WKPageSetRubberBandsAtBottom(pageRef, false);
    Require(!WKPageRubberBandsAtBottom(pageRef));
    Require(WKPageRubberBandsAtTop(pageRef));
    Require(WKPageVerticalRubberBandingIsEnabled(pageRef));
    Require(WKPageHorizontalRubberBandingIsEnabled(pageRef));
    WKPageSetEnableVerticalRubberBanding(pageRef, false);
    Require(!WKPageVerticalRubberBandingIsEnabled(pageRef));
    Require(WKPageHorizontalRubberBandingIsEnabled(pageRef));
    Require(!WKPageRubberBandsAtLeft(pageRef));
    Require(WKPageRubberBandsAtRight(pageRef));
    Require(WKPageIsPinnedToLeftSide(pageRef));
    WKRelease(pageRef);
}

void CheckPaginationMode() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(WKPageGetPaginationMode(pageRef) == 0);
    for (std::uint32_t mode = 0; mode <= 4; ++mode) {
        WKPageSetPaginationMode(pageRef, mode);
        Require(WKPageGetPaginationMode(pageRef) == mode);
    }
    WKPageSetPaginationMode(pageRef, 2);
    WKPageSetPaginationMode(pageRef, 5);
    Require(WKPageGetPaginationMode(pageRef) == 2);
    WKPageSetPaginationMode(pageRef, 0xFFFFFFFFu);
    Require(WKPageGetPaginationMode(pageRef) == 2);
    WKPageSetPaginationMode(nullptr, 5);
    WKPageSetPaginationMode(nullptr, 0x80000000u);
    RequireInvalidArgument([] { WKPageSetPaginationMode(nullptr, 0); });
    RequireInvalidArgument([] { WKPageSetPaginationMode(nullptr, 4); });
    RequireInvalidArgument([] { WKPageGetPaginationMode(nullptr); });
    WKRelease(pageRef);
}

void CheckPageDimensions() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(WKPageGetPageLength(pageRef) == 0);
    Require(WKPageGetGapBetweenPages(pageRef) == 0);
    WKPageSetPageLength(pageRef, 768.5);
    Require(WKPageGetPageLength(pageRef) == 768.5);
    Require(WKPageGetGapBetweenPages(pageRef) == 0);
    WKPageSetGapBetweenPages(pageRef, -12.25);
    Require(WKPageGetGapBetweenPages(pageRef) == -12.25);
    Require(WKPageGetPageLength(pageRef) == 768.5);
    WKPageSetPageLength(pageRef, std::numeric_limits<double>::infinity());
    Require(std::isinf(WKPageGetPageLength(pageRef)));
    WKPageSetGapBetweenPages(pageRef, std::numeric_limits<double>::quiet_NaN());
    Require(std::isnan(WKPageGetGapBetweenPages(pageRef)));
    WKPageSetPageLength(pageRef, -0.0);
    Require(WKPageGetPageLength(pageRef) == 0 && std::signbit(WKPageGetPageLength(pageRef)));
    RequireInvalidArgument([] { WKPageSetPageLength(nullptr, 1); });
    RequireInvalidArgument([] { WKPageGetPageLength(nullptr); });
    RequireInvalidArgument([] { WKPageSetGapBetweenPages(nullptr, 1); });
    RequireInvalidArgument([] { WKPageGetGapBetweenPages(nullptr); });
    WKRelease(pageRef);
}

void CheckScrollPinningBehavior() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);
    WKPageSetScrollPinningBehavior(pageRef, 1);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 1);
    WKPageSetScrollPinningBehavior(pageRef, 2);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 2);
    WKPageSetScrollPinningBehavior(pageRef, 3);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);
    WKPageSetScrollPinningBehavior(pageRef, 1);
    WKPageSetScrollPinningBehavior(pageRef, 0xFFFFFFFFu);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);
    WKPageSetScrollPinningBehavior(pageRef, 2);
    WKPageSetScrollPinningBehavior(pageRef, 0);
    Require(WKPageGetScrollPinningBehavior(pageRef) == 0);
    RequireInvalidArgument([] { WKPageSetScrollPinningBehavior(nullptr, 1); });
    RequireInvalidArgument([] { WKPageSetScrollPinningBehavior(nullptr, 7); });
    RequireInvalidArgument([] { WKPageGetScrollPinningBehavior(nullptr); });
    WKRelease(pageRef);
}

void CheckWrongObjectType() {
    auto preferences = NKWebKit::Make<NKWebKit::Preferences>();
    const void* preferencesRef = NKWebKit::ToRef(preferences);
    RequireInvalidArgument([preferencesRef] { WKPageRubberBandsAtTop(preferencesRef); });
    RequireInvalidArgument([preferencesRef] { WKPageSetPaginationMode(preferencesRef, 1); });
    RequireInvalidArgument([preferencesRef] { WKPageSetScrollPinningBehavior(preferencesRef, 1); });
    WKPageSetPaginationMode(preferencesRef, 9);
    WKRelease(preferencesRef);
}

}

int main() {
    CheckBoolean(WKPageRubberBandsAtLeft, WKPageSetRubberBandsAtLeft, true);
    CheckBoolean(WKPageRubberBandsAtRight, WKPageSetRubberBandsAtRight, true);
    CheckBoolean(WKPageRubberBandsAtTop, WKPageSetRubberBandsAtTop, true);
    CheckBoolean(WKPageRubberBandsAtBottom, WKPageSetRubberBandsAtBottom, true);
    CheckBoolean(WKPageVerticalRubberBandingIsEnabled, WKPageSetEnableVerticalRubberBanding, true);
    CheckBoolean(WKPageHorizontalRubberBandingIsEnabled, WKPageSetEnableHorizontalRubberBanding, true);
    CheckBoolean(WKPageBackgroundExtendsBeyondPage, WKPageSetBackgroundExtendsBeyondPage, true);
    CheckBoolean(WKPageGetPaginationBehavesLikeColumns, WKPageSetPaginationBehavesLikeColumns, false);
    CheckBoolean(WKPageAreScrollbarAnimationsSuppressed, WKPageSetSuppressScrollbarAnimations, false);
    CheckEdgesAreIndependent();
    CheckPaginationMode();
    CheckPageDimensions();
    CheckScrollPinningBehavior();
    CheckWrongObjectType();
    return 0;
}

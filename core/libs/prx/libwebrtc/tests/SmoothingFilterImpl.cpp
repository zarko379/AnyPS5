#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <cmath>
#include <math.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc webrtc::SmoothingFilterImpl check failed at line %u\n", location.line());
    std::abort();
}

void* lastFreed;
void* APS5_VABI FilterAllocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI FilterFree_nid_no_patch(void* pointer) {
    lastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI FilterCalloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI FilterReallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI FilterAlign_nid_no_patch(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI FilterRealign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI FilterPosixAlign_nid_no_patch(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

using Destructor = void(APS5_VABI*)(WebRtcSmoothingFilterImpl*);
using AddSample = void(APS5_VABI*)(WebRtcSmoothingFilterImpl*, float);
using GetAverage = WebRtcOptionalFloat(APS5_VABI*)(WebRtcSmoothingFilterImpl*);
using SetTimeConstantMs = bool(APS5_VABI*)(WebRtcSmoothingFilterImpl*, int);

std::int64_t Now() {
    return _ZN3rtc10TimeMillisEv_nid_postfix();
}

bool Near(double actual, double expected) {
    return std::fabs(actual - expected) <= 1e-5 * std::max(1.0, std::fabs(expected));
}

bool Within(float value, float first, float second) {
    const float low = std::min(first, second);
    const float high = std::max(first, second);
    return value >= low - 1e-5f * std::max(1.0f, std::fabs(low)) && value <= high + 1e-5f * std::max(1.0f, std::fabs(high));
}

WebRtcSmoothingFilterImpl Construct(int initTimeMs) {
    WebRtcSmoothingFilterImpl filter;
    std::memset(&filter, 0xA5, sizeof(filter));
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&filter, initTimeMs);
    return filter;
}

void Add(WebRtcSmoothingFilterImpl& filter, float sample) {
    reinterpret_cast<AddSample>(filter.vtable[2])(&filter, sample);
}

WebRtcOptionalFloat Average(WebRtcSmoothingFilterImpl& filter) {
    return reinterpret_cast<GetAverage>(filter.vtable[3])(&filter);
}

bool SetTimeConstant(WebRtcSmoothingFilterImpl& filter, int timeConstantMs) {
    return reinterpret_cast<SetTimeConstantMs>(filter.vtable[4])(&filter, timeConstantMs);
}

void Engage(WebRtcSmoothingFilterImpl& filter, std::int64_t initEndTimeMs, std::int64_t lastStateTimeMs, float state, float lastSample) {
    filter.initEndTimeMs.value = initEndTimeMs;
    filter.initEndTimeMs.engaged = true;
    filter.lastStateTimeMs = lastStateTimeMs;
    filter.state = state;
    filter.lastSample = lastSample;
}

float Mix(float multiplier, float state, float lastSample) {
    return multiplier * state + (1.0f - multiplier) * lastSample;
}

void TestConstructor() {
    WebRtcSmoothingFilterImpl zero = Construct(0);
    Require(zero.vtable != nullptr);
    Require(zero.initTimeMs == 0);
    Require(zero.initFactor == 0.0f && zero.initConst == 0.0f && zero.alpha == 0.0f);
    Require(!zero.initEndTimeMs.engaged);
    unsigned char untouched[4];
    std::memset(untouched, 0xA5, sizeof(untouched));
    Require(std::memcmp(&zero.lastSample, untouched, 4) == 0);
    Require(std::memcmp(&zero.state, untouched, 4) == 0);

    WebRtcSmoothingFilterImpl one = Construct(1);
    Require(one.initTimeMs == 1);
    Require(one.initFactor == 1.0f);
    Require(one.initConst == 0.0f);
    Require(Near(one.alpha, std::exp(-1.0)));

    for (int initTimeMs : {2, 100, 5000, -4}) {
        WebRtcSmoothingFilterImpl filter = Construct(initTimeMs);
        Require(filter.initTimeMs == initTimeMs);
        Require(!filter.initEndTimeMs.engaged);
        Require(Near(filter.alpha, std::exp(-1.0 / initTimeMs)));
        if (initTimeMs < 0) {
            Require(std::isnan(filter.initFactor) && std::isnan(filter.initConst));
            continue;
        }
        Require(Near(filter.initFactor, std::pow(static_cast<double>(initTimeMs), -1.0 / initTimeMs)));
        Require(std::fabs(filter.initConst - (initTimeMs - std::pow(static_cast<double>(initTimeMs), 1.0 - 1.0 / initTimeMs))) <= 4e-6 * initTimeMs);
        Require(Near(filter.initConst, initTimeMs - powf(static_cast<float>(initTimeMs), 1.0f - 1.0f / initTimeMs)));
    }
    Require(Construct(100).vtable == Construct(5).vtable);

    bool threw = false;
    try {
        _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(nullptr, 100);
    } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void TestAverageBeforeFirstSample() {
    WebRtcSmoothingFilterImpl filter = Construct(100);
    const WebRtcSmoothingFilterImpl before = filter;
    Require(!Average(filter).engaged);
    Require(std::memcmp(&filter, &before, sizeof(filter)) == 0);
    Require(!SetTimeConstant(filter, 10));
    Require(std::memcmp(&filter, &before, sizeof(filter)) == 0);
}

void TestFirstSample() {
    WebRtcSmoothingFilterImpl filter = Construct(250);
    const std::int64_t before = Now();
    Add(filter, 3.5f);
    const std::int64_t after = Now();
    Require(filter.initEndTimeMs.engaged);
    Require(filter.state == 3.5f && filter.lastSample == 3.5f);
    Require(filter.lastStateTimeMs >= before && filter.lastStateTimeMs <= after);
    Require(filter.initEndTimeMs.value == filter.lastStateTimeMs + 250);
}

void TestSecondSample() {
    WebRtcSmoothingFilterImpl filter = Construct(100);
    filter.alpha = 0.999f;
    const std::int64_t before = Now();
    Engage(filter, before - 2000, before - 500, 10.0f, 20.0f);
    Add(filter, -1000.0f);
    const std::int64_t after = Now();
    Require(filter.lastSample == -1000.0f);
    Require(filter.lastStateTimeMs >= before && filter.lastStateTimeMs <= after);
    const float first = Mix(powf(0.999f, 500.0f), 10.0f, 20.0f);
    const float last = Mix(powf(0.999f, static_cast<float>(after - before + 500)), 10.0f, 20.0f);
    Require(Within(filter.state, first, last));
    Require(filter.state > 10.0f && filter.state < 20.0f);
}

void TestAverageAfterInit() {
    WebRtcSmoothingFilterImpl filter = Construct(100);
    filter.alpha = 1.0f;
    std::int64_t before = Now();
    Engage(filter, before - 2000, before - 1000, 10.0f, 20.0f);
    WebRtcOptionalFloat average = Average(filter);
    std::int64_t after = Now();
    Require(average.engaged && average.storage.value == 10.0f && filter.state == 10.0f);
    Require(filter.lastStateTimeMs >= before && filter.lastStateTimeMs <= after);

    filter.alpha = 0.998f;
    before = Now();
    Engage(filter, before - 2000, before - 300, 10.0f, 20.0f);
    average = Average(filter);
    after = Now();
    Require(average.engaged && average.storage.value == filter.state);
    Require(filter.lastStateTimeMs >= before && filter.lastStateTimeMs <= after);
    Require(Within(filter.state, Mix(powf(0.998f, 300.0f), 10.0f, 20.0f), Mix(powf(0.998f, static_cast<float>(after - before + 300)), 10.0f, 20.0f)));
    Require(filter.state > 10.0f && filter.state < 20.0f);
    Require(filter.lastSample == 20.0f);
}

void TestInitPhase() {
    WebRtcSmoothingFilterImpl zero = Construct(0);
    std::int64_t before = Now();
    Engage(zero, before + 100000, before - 50, 10.0f, 20.0f);
    WebRtcOptionalFloat average = Average(zero);
    Require(average.engaged && average.storage.value == 20.0f && zero.state == 20.0f);

    WebRtcSmoothingFilterImpl one = Construct(1);
    before = Now();
    Engage(one, before + 100000, before - 2, 10.0f, 20.0f);
    average = Average(one);
    std::int64_t after = Now();
    Require(one.lastStateTimeMs >= before && one.lastStateTimeMs <= after);
    Require(Within(one.state, Mix(static_cast<float>(std::exp(-2.0)), 10.0f, 20.0f), Mix(static_cast<float>(std::exp(static_cast<double>(before - 2 - after))), 10.0f, 20.0f)));
    Require(one.state > 10.0f && one.state <= 20.0f);

    WebRtcSmoothingFilterImpl general = Construct(5000);
    before = Now();
    const std::int64_t end = before + 1000;
    Engage(general, end, before - 10, 10.0f, 20.0f);
    average = Average(general);
    after = Now();
    Require(after <= end);
    const auto multiplier = [&](std::int64_t time) {
        return std::exp(-(powf(general.initFactor, static_cast<float>(before - 10 - end)) - powf(general.initFactor, static_cast<float>(time - end))) / general.initConst);
    };
    Require(Within(general.state, Mix(multiplier(before), 10.0f, 20.0f), Mix(multiplier(after), 10.0f, 20.0f)));
    Require(general.state > 10.0f && general.state < 20.0f);
    Require(average.engaged && average.storage.value == general.state);
}

void TestCrossingInitEnd() {
    WebRtcSmoothingFilterImpl filter = Construct(100);
    filter.alpha = 1.0f;
    const std::int64_t before = Now();
    const std::int64_t end = before - 500;
    Engage(filter, end, end - 50, 1.0f, 5.0f);
    const WebRtcOptionalFloat average = Average(filter);
    const std::int64_t after = Now();
    const float multiplier = std::exp(-(powf(filter.initFactor, -50.0f) - powf(filter.initFactor, 0.0f)) / filter.initConst);
    Require(multiplier > 0.0f && multiplier < 1.0f);
    Require(Near(filter.state, Mix(multiplier, 1.0f, 5.0f)));
    Require(average.engaged && average.storage.value == filter.state);
    Require(filter.lastStateTimeMs >= before && filter.lastStateTimeMs <= after);
}

void TestSetTimeConstantMs() {
    WebRtcSmoothingFilterImpl filter = Construct(100);
    const std::int64_t now = Now();
    Engage(filter, now, now - 1, 1.0f, 1.0f);
    const float alpha = filter.alpha;
    Require(!SetTimeConstant(filter, 250));
    Require(filter.alpha == alpha);
    filter.lastStateTimeMs = now;
    Require(SetTimeConstant(filter, 250));
    Require(Near(filter.alpha, std::exp(-1.0 / 250)));
    filter.lastStateTimeMs = now + 10;
    Require(SetTimeConstant(filter, 0));
    Require(filter.alpha == 0.0f);
    Require(filter.lastStateTimeMs == now + 10 && filter.state == 1.0f);
}

void TestDestructors() {
    WebRtcSmoothingFilterImpl filter = Construct(100);
    const WebRtcSmoothingFilterImpl before = filter;
    reinterpret_cast<Destructor>(filter.vtable[0])(&filter);
    Require(std::memcmp(&filter, &before, sizeof(filter)) == 0);
    _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(&filter);
    Require(std::memcmp(&filter, &before, sizeof(filter)) == 0);

    auto* heapObject = static_cast<WebRtcSmoothingFilterImpl*>(_Znwm_nid_postfix(sizeof(WebRtcSmoothingFilterImpl)));
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(heapObject, 100);
    lastFreed = nullptr;
    reinterpret_cast<Destructor>(heapObject->vtable[1])(heapObject);
    Require(lastFreed == heapObject);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&FilterAllocate_nid_no_patch), reinterpret_cast<void*>(&FilterFree_nid_no_patch),
        reinterpret_cast<void*>(&FilterCalloc_nid_no_patch), reinterpret_cast<void*>(&FilterReallocate_nid_no_patch),
        reinterpret_cast<void*>(&FilterAlign_nid_no_patch), reinterpret_cast<void*>(&FilterRealign_nid_no_patch), reinterpret_cast<void*>(&FilterPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        TestConstructor();
        TestAverageBeforeFirstSample();
        TestFirstSample();
        TestSecondSample();
        TestAverageAfterInit();
        TestInitPhase();
        TestCrossingInitEnd();
        TestSetTimeConstantMs();
        TestDestructors();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SmoothingFilterImpl exception: %s\n", error.what());
        return 1;
    }
    return 0;
}

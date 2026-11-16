#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <vector>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioState::Config check failed at line %u\n", location.line());
    std::abort();
}

void TestConstructorClearsEveryReference() {
    WebRtcAudioStateConfig config;
    std::memset(&config, 0xA5, sizeof(config));
    _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(&config);
    Require(config.audioMixer == nullptr);
    Require(config.audioProcessing == nullptr);
    Require(config.audioDeviceModule == nullptr);
    Require(config.asyncAudioProcessingFactory == nullptr);
}

void TestNullConfigThrows() {
    try {
        _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

std::vector<int> released;

struct FakeReference {
    const void* vtable;
    int id;
};

void APS5_VABI AddRef_nid_no_patch(const void*) {}
int APS5_VABI Release_nid_no_patch(const void* object) {
    released.push_back(static_cast<const FakeReference*>(object)->id);
    return 1;
}

const void* const CountedVtable[] = {reinterpret_cast<const void*>(&AddRef_nid_no_patch), reinterpret_cast<const void*>(&Release_nid_no_patch)};
const void* const BrokenVtable[] = {reinterpret_cast<const void*>(&AddRef_nid_no_patch), nullptr};

void TestDestructorReleasesInReverseOrder() {
    FakeReference mixer {CountedVtable, 1};
    FakeReference processing {CountedVtable, 2};
    FakeReference module {CountedVtable, 3};
    FakeReference factory {CountedVtable, 4};
    WebRtcAudioStateConfig config {&mixer, &processing, &module, &factory};
    released.clear();
    _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&config);
    Require((released == std::vector<int> {4, 3, 2, 1}));
    WebRtcAudioStateConfig partial {&mixer, nullptr, &module, nullptr};
    released.clear();
    _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&partial);
    Require((released == std::vector<int> {3, 1}));
    WebRtcAudioStateConfig empty {};
    released.clear();
    _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&empty);
    Require(released.empty());
}

void TestDestructorChecksEveryReferenceFirst() {
    FakeReference mixer {BrokenVtable, 1};
    FakeReference factory {CountedVtable, 4};
    WebRtcAudioStateConfig config {&mixer, nullptr, nullptr, &factory};
    released.clear();
    bool threw = false;
    try {
        _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&config);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && released.empty());
    threw = false;
    try {
        _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestConstructorClearsEveryReference();
    TestNullConfigThrows();
    TestDestructorReleasesInReverseOrder();
    TestDestructorChecksEveryReferenceFirst();
    return 0;
}

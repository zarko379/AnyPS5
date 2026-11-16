#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
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
    std::fprintf(stderr, "libwebrtc RTCStatsReport check failed at line %u\n", location.line());
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

struct FakeStats {
    const void* vtable;
    int id;
};

std::vector<int> deleted;

void APS5_VABI FakeDestroy_nid_no_patch(FakeStats*) {}

void APS5_VABI FakeDelete_nid_no_patch(FakeStats* stats) {
    deleted.push_back(stats->id);
    _ZdlPv_nid_postfix(stats);
}

void* const FakeVtable[] = {
    reinterpret_cast<void*>(&FakeDestroy_nid_no_patch),
    reinterpret_cast<void*>(&FakeDelete_nid_no_patch),
};

void* const MissingVtable[] = {nullptr, nullptr};

FakeStats* Stats(int id) {
    auto* stats = static_cast<FakeStats*>(_Znwm_nid_postfix(sizeof(FakeStats)));
    stats->vtable = FakeVtable;
    stats->id = id;
    return stats;
}

WebRtcRtcStatsNode* EndNode(WebRtcRtcStatsReport* report) {
    return reinterpret_cast<WebRtcRtcStatsNode*>(&report->root);
}

WebRtcRtcStatsNode* Node(std::string_view id, const void* stats) {
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcRtcStatsNode))) WebRtcRtcStatsNode{};
    node->id.size = id.size();
    if (id.size() <= WebRtcGuestStringInlineCapacity) {
        std::memcpy(node->id.buffer, id.data(), id.size());
        node->id.capacity = WebRtcGuestStringInlineCapacity;
    } else {
        const std::size_t capacity = id.size() | WebRtcGuestStringInlineCapacity;
        auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
        std::memcpy(data, id.data(), id.size());
        data[id.size()] = '\0';
        node->id.pointer = data;
        node->id.capacity = capacity;
    }
    node->stats = stats;
    return node;
}

WebRtcRtcStatsReport* EmptyReport() {
    auto* report = new (_Znwm_nid_postfix(sizeof(WebRtcRtcStatsReport))) WebRtcRtcStatsReport{};
    report->refCount.store(1);
    report->timestampUs = 1234;
    report->beginNode = EndNode(report);
    report->root = nullptr;
    report->size = 0;
    return report;
}

struct ThreeStats {
    WebRtcRtcStatsReport* report;
    WebRtcRtcStatsNode* first;
    WebRtcRtcStatsNode* second;
    WebRtcRtcStatsNode* third;
};

ThreeStats Report() {
    WebRtcRtcStatsReport* report = EmptyReport();
    auto* first = Node("A", Stats(1));
    auto* second = Node("B", Stats(2));
    auto* third = Node("C-with-a-long-identifier-0123456789", Stats(3));
    second->isBlack = true;
    second->parent = EndNode(report);
    second->left = first;
    second->right = third;
    first->parent = second;
    third->parent = second;
    report->root = second;
    report->beginNode = first;
    report->size = 3;
    return {report, first, second, third};
}

void TestIteration() {
    ThreeStats stats = Report();
    WebRtcRtcStatsReportIterator it {};
    WebRtcRtcStatsReportIterator end {};
    Require(_ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&it, stats.report) == &it);
    Require(_ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&end, stats.report) == &end);
    Require(stats.report->refCount.load() == 3);
    Require(it.report == stats.report && it.node == stats.first && end.node == EndNode(stats.report));
    std::vector<int> seen;
    while (_ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&it, &end)) {
        const auto* value = static_cast<const FakeStats*>(_ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&it));
        Require(_ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&it) == value);
        seen.push_back(value->id);
        Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&it) == &it);
    }
    Require((seen == std::vector<int> {1, 2, 3}));
    Require(it.node == EndNode(stats.report));
    RequireInvalidArgument([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(&it); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&it); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&it); });
    WebRtcRtcStatsReportIterator post {};
    _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&post, stats.report);
    Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(&post, 0) == &post && post.node == stats.second);
    Require(_ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(&post, 0) == &post && post.node == stats.third);
    Require(stats.report->refCount.load() == 4);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&post);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&end);
    Require(stats.report->refCount.load() == 1 && deleted.empty());
    WebRtcRtcStatsReportIterator last {};
    _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&last, stats.report);
    stats.report->refCount.fetch_sub(1);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&last);
    Require((deleted == std::vector<int> {1, 3, 2}));
    Require(live.empty());
    deleted.clear();
}

void TestEmptyReport() {
    WebRtcRtcStatsReport* report = EmptyReport();
    WebRtcRtcStatsReportIterator begin {};
    WebRtcRtcStatsReportIterator end {};
    _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&begin, report);
    _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&end, report);
    Require(!_ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&begin, &end));
    report->refCount.fetch_sub(1);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&begin);
    Require(!live.empty());
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&end);
    Require(live.empty());
}

void TestNullStatsAndMovedIterators() {
    WebRtcRtcStatsReport* report = EmptyReport();
    auto* node = Node("only", nullptr);
    node->parent = EndNode(report);
    report->root = node;
    report->beginNode = node;
    report->size = 1;
    WebRtcRtcStatsReportIterator it {};
    _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&it, report);
    Require(_ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&it) == nullptr);
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(&it); });
    WebRtcRtcStatsReportIterator moved {nullptr, node};
    Require(_ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(&moved) == nullptr);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&moved);
    report->refCount.fetch_sub(1);
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it);
    Require(live.empty() && deleted.empty());
}

void TestInvalidReportKeepsTheLastReference() {
    ThreeStats stats = Report();
    stats.report->size = 2;
    WebRtcRtcStatsReportIterator it {stats.report, stats.first};
    RequireInvalidArgument([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it); });
    Require(stats.report->refCount.load() == 1 && deleted.empty());
    stats.report->size = 3;
    auto* stats3 = const_cast<FakeStats*>(static_cast<const FakeStats*>(stats.third->stats));
    stats3->vtable = MissingVtable;
    RequireInvalidArgument([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it); });
    Require(stats.report->refCount.load() == 1 && deleted.empty());
    stats3->vtable = FakeVtable;
    stats.third->parent = stats.first;
    RequireInvalidArgument([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it); });
    stats.third->parent = stats.second;
    stats.third->id.capacity = 3;
    RequireInvalidArgument([&] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it); });
    stats.third->id.capacity = stats.third->id.size | WebRtcGuestStringInlineCapacity;
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&it);
    Require((deleted == std::vector<int> {1, 3, 2}) && live.empty());
    deleted.clear();
}

void TestNullArgumentsThrow() {
    WebRtcRtcStatsReport* report = EmptyReport();
    WebRtcRtcStatsReportIterator it {};
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(nullptr, report); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(&it, nullptr); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(nullptr, report); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(&it, nullptr); });
    Require(report->refCount.load() == 1);
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(nullptr, &it); });
    RequireInvalidArgument([&] { _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(&it, nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(nullptr, 0); });
    RequireInvalidArgument([] { _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(nullptr); });
    WebRtcRtcStatsReportIterator last {report, EndNode(report)};
    _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(&last);
    Require(live.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestIteration();
    TestEmptyReport();
    TestNullStatsAndMovedIterators();
    TestInvalidReportKeepsTheLastReference();
    TestNullArgumentsThrow();
    return 0;
}

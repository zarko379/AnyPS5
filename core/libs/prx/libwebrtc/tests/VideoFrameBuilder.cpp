#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrame::Builder check failed at line %u\n", location.line());
    std::abort();
}

WebRtcVideoFrameBuilder Filled() {
    WebRtcVideoFrameBuilder builder;
    std::memset(&builder, 0xA5, sizeof(builder));
    return builder;
}

bool OnlyChanged(const WebRtcVideoFrameBuilder& builder, std::size_t offset, std::size_t size) {
    WebRtcVideoFrameBuilder filled = Filled();
    const auto* left = reinterpret_cast<const unsigned char*>(&builder);
    const auto* right = reinterpret_cast<const unsigned char*>(&filled);
    for (std::size_t index = 0; index < sizeof(builder); ++index) {
        if (index >= offset && index < offset + size) continue;
        if (left[index] != right[index]) return false;
    }
    return true;
}

void TestTimestampMs() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, 1234) == &builder);
    Require(builder.timestampUs == 1234000);
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, timestampUs), 8));
    _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, -5);
    Require(builder.timestampUs == -5000);
    _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, 0);
    Require(builder.timestampUs == 0);
}

void TestTimestampRtp() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder17set_timestamp_rtpEj_nid_postfix(&builder, 0xFFFFFFFFu) == &builder);
    Require(builder.timestampRtp == 0xFFFFFFFFu);
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, timestampRtp), 4));
}

void TestNtpTime() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder15set_ntp_time_msEl_nid_postfix(&builder, std::numeric_limits<std::int64_t>::min()) == &builder);
    Require(builder.ntpTimeMs == std::numeric_limits<std::int64_t>::min());
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, ntpTimeMs), 8));
}

void TestId() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(&builder, 0xBEEF) == &builder);
    Require(builder.id == 0xBEEF);
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, id), 2));
}

void TestConstructor() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    Require(builder.id == 0 && builder.videoFrameBuffer == nullptr && builder.timestampUs == 0);
    Require(!builder.presentationTimestamp.engaged && !builder.referenceTime.engaged);
    Require(builder.timestampRtp == 0 && builder.ntpTimeMs == 0 && builder.rotation == 0);
    Require(builder.colorSpace[68] == 0 && !builder.useLowLatencyRendering && !builder.hasMaxCompositionDelayInFrames);
    Require(!builder.hasUpdateRect && builder.packetInfos == nullptr);
}

void TestDestructor() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(&builder, 5);
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(builder.id == 5);
}

struct RefBuffer {
    const void* const* vtable;
    int addRefs = 0;
    int releases = 0;
};

std::vector<int> g_order;
const void* g_refVtable[4];

void APS5_VABI RefAddRef(const void* buffer) {
    ++const_cast<RefBuffer*>(static_cast<const RefBuffer*>(buffer))->addRefs;
    g_order.push_back(1);
}

int APS5_VABI RefRelease(const void* buffer) {
    ++const_cast<RefBuffer*>(static_cast<const RefBuffer*>(buffer))->releases;
    g_order.push_back(2);
    return 1;
}

void TestSetVideoFrameBuffer() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer first{g_refVtable};
    RefBuffer second{g_refVtable};
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    void* firstReference = &first;
    void* secondReference = &second;
    g_order.clear();
    Require(_ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &firstReference) == &builder);
    Require(builder.videoFrameBuffer == &first && first.addRefs == 1 && first.releases == 0);
    Require(g_order == std::vector<int>{1});
    g_order.clear();
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &secondReference);
    Require(builder.videoFrameBuffer == &second && second.addRefs == 1 && first.releases == 1 && second.releases == 0);
    Require(g_order == std::vector<int>({1, 2}));
    g_order.clear();
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &secondReference);
    Require(builder.videoFrameBuffer == &second && second.addRefs == 2 && second.releases == 1);
    Require(g_order == std::vector<int>({1, 2}));
    void* nothing = nullptr;
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &nothing);
    Require(builder.videoFrameBuffer == nullptr && second.releases == 2 && second.addRefs == 2);
}

void TestVideoFrameBuffer() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcVideoFrame frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.videoFrameBuffer = &buffer;
    void* result = nullptr;
    Require(_ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(&result, &frame) == &result);
    Require(result == &buffer && buffer.addRefs == 1 && buffer.releases == 0 && frame.videoFrameBuffer == &buffer);
    frame.videoFrameBuffer = nullptr;
    result = &buffer;
    Require(_ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(&result, &frame) == &result);
    Require(result == nullptr && buffer.addRefs == 1);
}

int releaseCalls = 0;
void* releasedBuffer = nullptr;

int APS5_VABI ReleaseBuffer(const void* buffer) {
    ++releaseCalls;
    releasedBuffer = const_cast<void*>(buffer);
    return 1;
}

const void* bufferVtable[] = {nullptr, reinterpret_cast<const void*>(&ReleaseBuffer)};

const void* emptyVtable[] = {nullptr, nullptr};

void TestDestructorReleasesBuffer() {
    const void* const* buffer = bufferVtable;
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.videoFrameBuffer = &buffer;
    releaseCalls = 0;
    releasedBuffer = nullptr;
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(releaseCalls == 1 && releasedBuffer == &buffer);
    Require(builder.videoFrameBuffer == &buffer);
}

void TestDestructorWithoutBufferReleasesNothing() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    releaseCalls = 0;
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(releaseCalls == 0);
}

void TestDestructorWithoutReleaseThrows() {
    const void* const* buffer = emptyVtable;
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.videoFrameBuffer = &buffer;
    bool threw = false;
    try {
        _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(releaseCalls == 0);
}

int frees = 0;
int addRefCalls = 0;

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void APS5_VABI AddRefBuffer(const void*) { ++addRefCalls; }

const void* countedVtable[] = {reinterpret_cast<const void*>(&AddRefBuffer), reinterpret_cast<const void*>(&ReleaseBuffer)};

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

WebRtcRtpPacketInfosData* PacketInfos(std::int32_t references) {
    auto* data = static_cast<WebRtcRtpPacketInfosData*>(_Znwm_nid_postfix(sizeof(WebRtcRtpPacketInfosData)));
    auto* entries = static_cast<WebRtcRtpPacketInfoEntry*>(_Znwm_nid_postfix(2 * sizeof(WebRtcRtpPacketInfoEntry)));
    std::memset(entries, 0, 2 * sizeof(WebRtcRtpPacketInfoEntry));
    auto* csrcs = static_cast<std::uint32_t*>(_Znwm_nid_postfix(sizeof(std::uint32_t)));
    entries[1].csrcsBegin = csrcs;
    entries[1].csrcsEnd = csrcs + 1;
    entries[1].csrcsCapacity = csrcs + 1;
    new (&data->refCount) std::atomic<std::int32_t>(references);
    data->begin = entries;
    data->end = entries + 2;
    data->capacity = entries + 2;
    return data;
}

void TestDestructorReleasesPacketInfos() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    WebRtcRtpPacketInfosData* infos = PacketInfos(2);
    builder.packetInfos = infos;
    const int before = frees;
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(infos->refCount.load() == 1 && frees == before);
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(frees == before + 3);

    const void* const* buffer = bufferVtable;
    builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.videoFrameBuffer = &buffer;
    infos = PacketInfos(1);
    infos->capacity = infos->end - 1;
    builder.packetInfos = infos;
    releaseCalls = 0;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder); });
    Require(releaseCalls == 0 && infos->refCount.load() == 1 && frees == before + 3);
    infos->capacity = infos->end;
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(releaseCalls == 1 && frees == before + 6);
}

void TestBuild() {
    const void* const* buffer = countedVtable;
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    WebRtcVideoFrame frame;
    std::memset(&frame, 0xA5, sizeof(frame));
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); });
    addRefCalls = 0;
    builder.videoFrameBuffer = &buffer;
    builder.id = 7;
    builder.timestampUs = 1234000;
    builder.presentationTimestamp = {55, true};
    builder.referenceTime = {66, true};
    builder.timestampRtp = 90000;
    builder.ntpTimeMs = -3;
    builder.rotation = 270;
    for (std::size_t index = 0; index < sizeof(builder.colorSpace); ++index)
        builder.colorSpace[index] = static_cast<unsigned char>(index + 1);
    builder.useLowLatencyRendering = true;
    builder.maxCompositionDelayInFrames = 4;
    builder.hasMaxCompositionDelayInFrames = true;
    builder.updateRect[0] = 1;
    builder.updateRect[1] = 2;
    builder.updateRect[2] = 3;
    builder.updateRect[3] = 4;
    builder.hasUpdateRect = true;
    WebRtcRtpPacketInfosData* infos = PacketInfos(1);
    builder.packetInfos = infos;
    Require(_ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder) == &frame);
    Require(addRefCalls == 1 && infos->refCount.load() == 2);
    Require(frame.id == 7 && frame.videoFrameBuffer == &buffer && frame.timestampUs == 1234000);
    Require(frame.presentationTimestamp.engaged && frame.presentationTimestamp.value == 55);
    Require(frame.referenceTime.engaged && frame.referenceTime.value == 66);
    Require(frame.timestampRtp == 90000 && frame.ntpTimeMs == -3 && frame.rotation == 270);
    Require(std::memcmp(frame.colorSpace, builder.colorSpace, sizeof(frame.colorSpace)) == 0);
    Require(frame.useLowLatencyRendering && frame.hasMaxCompositionDelayInFrames && frame.maxCompositionDelayInFrames == 4);
    Require(frame.hasUpdateRect && frame.updateRect[0] == 1 && frame.updateRect[3] == 4);
    Require(frame.packetInfos == infos && !frame.hasProcessingTime);
    Require(builder.videoFrameBuffer == &buffer && builder.packetInfos == infos);
    releaseCalls = 0;
    const int before = frees;
    _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame);
    Require(releaseCalls == 1 && infos->refCount.load() == 1 && frees == before);
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(releaseCalls == 2 && frees == before + 3);
    const void* const* broken = emptyVtable;
    builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.videoFrameBuffer = &broken;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(nullptr, &builder); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, nullptr); });
}

void TestNullDestructorThrows() {
    bool threw = false;
    try {
        _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestRenderTimeMs() {
    WebRtcVideoFrame frame;
    std::memset(&frame, 0xA5, sizeof(frame));
    for (const auto [microseconds, milliseconds] : {std::pair<std::int64_t, std::int64_t>{0, 0}, {999, 0}, {1000, 1}, {123456789, 123456}, {-1, 0}, {-999, 0}, {-1000, -1}, {-1500, -1},
             {std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max() / 1000}}) {
        frame.timestampUs = microseconds;
        Require(_ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(&frame) == milliseconds);
    }
    bool threw = false;
    try {
        _ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestNullBuilderThrows() {
    try {
        _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(nullptr, 1);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestTimestampMs();
    TestTimestampRtp();
    TestNtpTime();
    TestId();
    TestConstructor();
    TestDestructor();
    TestSetVideoFrameBuffer();
    TestVideoFrameBuffer();
    TestDestructorReleasesBuffer();
    TestDestructorWithoutBufferReleasesNothing();
    TestDestructorWithoutReleaseThrows();
    TestDestructorReleasesPacketInfos();
    TestBuild();
    TestNullDestructorThrows();
    TestRenderTimeMs();
    TestNullBuilderThrows();
    return 0;
}

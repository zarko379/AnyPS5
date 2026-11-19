#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AccelerationStructure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBvh.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "SpirvBackend/RtProto.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace AgcDriver::DriverDetail {

namespace {

struct Built {
    std::unique_ptr<Graphics::AccelerationStructure> bottom;
    std::unique_ptr<Graphics::AccelerationStructure> top;
};

struct TableEntry {
    std::uint64_t guest;
    std::uint64_t top;
    std::uint64_t side;
    std::uint32_t opaque;
    std::uint32_t pad;
};

std::unique_ptr<Graphics::Buffer>& tableBuffer() {
    static auto* buffer = new std::unique_ptr<Graphics::Buffer>();
    return *buffer;
}

std::vector<Built>& builtStructures() {
    static auto* built = new std::vector<Built>();
    return *built;
}

std::unique_ptr<Graphics::Buffer>& sideBuffer() {
    static auto* buffer = new std::unique_ptr<Graphics::Buffer>();
    return *buffer;
}

std::vector<std::byte> readGuest(std::uint64_t address, std::size_t bytes) {
    std::vector<std::byte> data(bytes);
    GuestMemory::ReadCommitted(address, data);
    return data;
}

std::vector<std::byte> readStructure(std::uint64_t address) {
    auto header = readGuest(address, Graphics::GuestBvh::HeaderBytes);
    Graphics::GuestBvh::Refusal refusal{};
    const auto parsed = Graphics::GuestBvh::ReadHeader(header, refusal);
    if (!parsed && refusal != Graphics::GuestBvh::Refusal::Truncated) return {};
    const auto bytes = parsed ? parsed->bytes : static_cast<std::uint32_t>(0);
    std::uint32_t size = bytes;
    if (size == 0) std::memcpy(&size, header.data() + 0x10, sizeof(size));
    return readGuest(address, size);
}

}

void RtProtoPrepareTable(const Graphics::Context& context) {
    static const bool enabled = std::getenv("APS5_RT_PROTO_TLAS") != nullptr;
    if (!enabled || tableBuffer() || !context.rayQuery) return;
    tableBuffer() = std::make_unique<Graphics::Buffer>(context, 16u + 512u * sizeof(TableEntry) + 64u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, std::getenv("APS5_RT_PROTO_HOST_TABLES") != nullptr ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memset(tableBuffer()->Bytes().data(), 0, 16u + 512u * sizeof(TableEntry) + 64u);
    ShaderRecompiler::RtProto::tableAddress.store(tableBuffer()->DeviceAddress());
    std::fprintf(stderr, "[rt-proto] lookup table at device address 0x%llx\n", static_cast<unsigned long long>(tableBuffer()->DeviceAddress()));
}

void Driver::RtProtoBuild() {
    const char* tlasText = std::getenv("APS5_RT_PROTO_TLAS");
    if (tlasText != nullptr && tableBuffer() && (!builtStructures().empty() || std::getenv("APS5_RT_PROTO_NO_BUILD") != nullptr)) {
        WaitIdle();
        std::uint32_t counters[8]{};
        std::memcpy(counters, tableBuffer()->Bytes().data() + 16u + 512u * sizeof(TableEntry), sizeof(counters));
        std::fprintf(stderr, "[rt-proto] counters: %u queries, %u found their bottom structure, %u committed a triangle, %u side reads%c", counters[0], counters[1], counters[2], counters[3], 10);
        std::fprintf(stderr, "[rt-proto] last query: instance record 0x%08x 0x%08x bottom 0x%08x%08x; first table entry 0x%llx%c", counters[4], counters[5], counters[7], counters[6], static_cast<unsigned long long>(*reinterpret_cast<const std::uint64_t*>(tableBuffer()->Bytes().data() + 16u)), 10);
        return;
    }
    if (tlasText == nullptr || !tableBuffer()) return;
    WaitIdle();
    const std::shared_ptr<VulkanDevice> local = device;
    if (local == nullptr) return;
    const auto context = local->graphicsContext();
    std::map<std::uint64_t, Graphics::GuestBvh::Bottom> bottoms;
    std::size_t instances = 0;
    for (const char* cursor = tlasText; *cursor != '\0';) {
        char* end = nullptr;
        const auto tlasAddress = std::strtoull(cursor, &end, 0);
        if (end == cursor) break;
        cursor = *end == ',' ? end + 1 : end;
        const auto topBytes = readStructure(tlasAddress);
        const auto top = Graphics::GuestBvh::DecodeTop(topBytes);
        if (!top.top) {
            std::fprintf(stderr, "[rt-proto] top structure at 0x%llx refused: %s%c", static_cast<unsigned long long>(tlasAddress), Graphics::GuestBvh::RefusalName(top.refusal), 10);
            continue;
        }
        instances += top.top->instances.size();
        for (const auto& instance : top.top->instances) {
            if (bottoms.contains(instance.bottomAddress)) continue;
            const auto bytes = readStructure(instance.bottomAddress);
            auto decoded = Graphics::GuestBvh::DecodeBottom(bytes, instance.bottomRoot);
            if (!decoded.bottom) {
                std::fprintf(stderr, "[rt-proto] bottom structure at 0x%llx refused: %s%c", static_cast<unsigned long long>(instance.bottomAddress), Graphics::GuestBvh::RefusalName(decoded.refusal), 10);
                continue;
            }
            bottoms.emplace(instance.bottomAddress, std::move(*decoded.bottom));
        }
    }
    std::size_t primitives = 0;
    for (const auto& [address, bottom] : bottoms) primitives += bottom.opaque.size() + bottom.alphaTested.size();
    sideBuffer() = std::make_unique<Graphics::Buffer>(context, std::max<std::size_t>(primitives, 1u) * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, std::getenv("APS5_RT_PROTO_HOST_TABLES") != nullptr ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto* side = sideBuffer()->Bytes().data();
    std::vector<TableEntry> entries;
    std::size_t cursor = 0;
    Graphics::CommandBatch batch(context);
    const auto commands = batch.Handle();
    for (const auto& [address, bottom] : bottoms) {
        Built built;
        built.bottom = Graphics::AccelerationStructure::Bottom(context, commands, bottom);
        TableEntry entry{address, 0, sideBuffer()->DeviceAddress() + cursor * 16u, static_cast<std::uint32_t>(bottom.opaque.size()), 0};
        for (const auto* set : {&bottom.opaque, &bottom.alphaTested}) {
            for (const auto& triangle : *set) {
                const std::uint32_t words[4]{triangle.nodePointer, triangle.leafInfo, triangle.triangleId, 0u};
                std::memcpy(side + cursor * 16u, words, sizeof(words));
                ++cursor;
            }
        }
        entries.push_back(entry);
        builtStructures().push_back(std::move(built));
    }
    Graphics::AccelerationStructure::BarrierBeforeUse(context, commands, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        Graphics::TopInstance instance;
        instance.objectToWorld = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        instance.bottom = builtStructures()[index].bottom->Address();
        builtStructures()[index].top = Graphics::AccelerationStructure::Top(context, commands, std::span<const Graphics::TopInstance>(&instance, 1));
        entries[index].top = builtStructures()[index].top->Address();
    }
    Graphics::AccelerationStructure::BarrierBeforeUse(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    batch.SubmitAndWait();
    for (auto& built : builtStructures()) {
        built.bottom->ReleaseBuildInputs();
        built.top->ReleaseBuildInputs();
    }
    std::sort(entries.begin(), entries.end(), [](const TableEntry& left, const TableEntry& right) { return left.guest < right.guest; });
    auto* table = tableBuffer()->Bytes().data();
    const std::uint32_t count = static_cast<std::uint32_t>(std::min<std::size_t>(entries.size(), 512u));
    std::memcpy(table + 16u, entries.data(), count * sizeof(TableEntry));
    const std::uint64_t sideBase = sideBuffer()->DeviceAddress();
    std::memcpy(table + 8u, &sideBase, sizeof(sideBase));
    std::memcpy(table, &count, sizeof(count));
    std::fprintf(stderr, "[rt-proto] built %u bottom structures (%zu triangles) from %zu instances%c", count, primitives, instances, 10);
}

}

extern "C" void AgcDriverRtProtoBuild_nid_postfix() {
    AgcDriver::DriverDetail::Driver::Get().RtProtoBuild();
}

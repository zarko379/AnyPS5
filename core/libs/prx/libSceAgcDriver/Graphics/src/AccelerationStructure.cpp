#include "prx/libSceAgcDriver/Graphics/include/AccelerationStructure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

constexpr VkBufferUsageFlags InputUsage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
constexpr VkDeviceSize InstanceAlignment = 16;

VkDeviceAddress alignUp(VkDeviceAddress value, VkDeviceAddress alignment) {
    return (value + alignment - 1u) / alignment * alignment;
}

void copyVertices(std::byte* destination, std::span<const GuestBvh::Triangle> triangles) {
    for (const auto& triangle : triangles) {
        for (const auto& vertex : triangle.vertices) {
            std::memcpy(destination, vertex.data(), sizeof(float) * 3u);
            destination += sizeof(float) * 3u;
        }
    }
}

}

AccelerationStructure::AccelerationStructure(const Context& context) : context(context) {
    Require(context.rayQuery, "acceleration structures need APS5_NATIVE_RT=1 on a device with VK_KHR_acceleration_structure and VK_KHR_ray_query");
}

AccelerationStructure::~AccelerationStructure() {
    release();
}

void AccelerationStructure::release() noexcept {
    if (handle != VK_NULL_HANDLE) context.Function<PFN_vkDestroyAccelerationStructureKHR>("vkDestroyAccelerationStructureKHR")(context.device, handle, nullptr);
    handle = VK_NULL_HANDLE;
    address = 0;
    scratch.reset();
    inputs.reset();
    storage.reset();
}

void AccelerationStructure::ReleaseBuildInputs() {
    scratch.reset();
    inputs.reset();
}

void AccelerationStructure::build(VkCommandBuffer commands, VkAccelerationStructureTypeKHR type, std::span<const VkAccelerationStructureGeometryKHR> geometries, std::span<const std::uint32_t> counts) {
    Require(geometries.size() == counts.size(), "every acceleration structure geometry needs a primitive count");
    VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    info.type = type;
    info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.geometryCount = static_cast<std::uint32_t>(geometries.size());
    info.pGeometries = geometries.data();
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    context.Function<PFN_vkGetAccelerationStructureBuildSizesKHR>("vkGetAccelerationStructureBuildSizesKHR")(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, counts.data(), &sizes);
    storage = std::make_unique<Buffer>(context, static_cast<std::size_t>(sizes.accelerationStructureSize), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkAccelerationStructureCreateInfoKHR create{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    create.buffer = storage->Handle();
    create.size = sizes.accelerationStructureSize;
    create.type = type;
    Check(context.Function<PFN_vkCreateAccelerationStructureKHR>("vkCreateAccelerationStructureKHR")(context.device, &create, nullptr, &handle), "vkCreateAccelerationStructureKHR");
    const VkDeviceAddress scratchAlignment = context.accelerationScratchAlignment;
    Require(scratchAlignment != 0u && (scratchAlignment & (scratchAlignment - 1u)) == 0u, "the device reported no power-of-two acceleration structure scratch alignment");
    scratch = std::make_unique<Buffer>(context, static_cast<std::size_t>(sizes.buildScratchSize + scratchAlignment), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    info.dstAccelerationStructure = handle;
    info.scratchData.deviceAddress = alignUp(scratch->DeviceAddress(), scratchAlignment);
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(counts.size());
    for (std::size_t index = 0; index < counts.size(); ++index) ranges[index].primitiveCount = counts[index];
    const VkAccelerationStructureBuildRangeInfoKHR* rangeList = ranges.data();
    context.Function<PFN_vkCmdBuildAccelerationStructuresKHR>("vkCmdBuildAccelerationStructuresKHR")(commands, 1, &info, &rangeList);
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = handle;
    address = context.Function<PFN_vkGetAccelerationStructureDeviceAddressKHR>("vkGetAccelerationStructureDeviceAddressKHR")(context.device, &addressInfo);
}

std::unique_ptr<AccelerationStructure> AccelerationStructure::Bottom(const Context& context, VkCommandBuffer commands, const GuestBvh::Bottom& bottom) {
    std::unique_ptr<AccelerationStructure> structure(new AccelerationStructure(context));
    const std::array<std::span<const GuestBvh::Triangle>, 2> sets{std::span<const GuestBvh::Triangle>(bottom.opaque), std::span<const GuestBvh::Triangle>(bottom.alphaTested)};
    constexpr std::size_t TriangleBytes = sizeof(float) * 9u;
    const auto opaqueBytes = bottom.opaque.size() * TriangleBytes;
    const auto totalBytes = std::max<std::size_t>(opaqueBytes + bottom.alphaTested.size() * TriangleBytes, TriangleBytes);
    structure->inputs = std::make_unique<Buffer>(context, totalBytes, InputUsage);
    auto bytes = structure->inputs->Bytes();
    copyVertices(bytes.data(), sets[OpaqueGeometry]);
    copyVertices(bytes.data() + opaqueBytes, sets[AlphaTestedGeometry]);
    const auto base = structure->inputs->DeviceAddress();
    std::array<VkAccelerationStructureGeometryKHR, 2> geometries{};
    std::array<std::uint32_t, 2> counts{};
    for (std::uint32_t index = 0; index < 2u; ++index) {
        auto& geometry = geometries[index];
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = index == OpaqueGeometry ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0u;
        auto& triangles = geometry.geometry.triangles;
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress = base + (index == OpaqueGeometry ? 0u : opaqueBytes);
        triangles.vertexStride = sizeof(float) * 3u;
        counts[index] = static_cast<std::uint32_t>(sets[index].size());
        triangles.maxVertex = counts[index] == 0u ? 0u : counts[index] * 3u - 1u;
        triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
    }
    structure->build(commands, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, geometries, counts);
    return structure;
}

std::unique_ptr<AccelerationStructure> AccelerationStructure::Top(const Context& context, VkCommandBuffer commands, std::span<const TopInstance> instances) {
    std::unique_ptr<AccelerationStructure> structure(new AccelerationStructure(context));
    const auto bytes = std::max<std::size_t>(instances.size(), 1u) * sizeof(VkAccelerationStructureInstanceKHR) + InstanceAlignment;
    structure->inputs = std::make_unique<Buffer>(context, bytes, InputUsage);
    const auto base = structure->inputs->DeviceAddress();
    const auto aligned = alignUp(base, InstanceAlignment);
    auto* records = structure->inputs->Bytes().data() + (aligned - base);
    for (std::size_t index = 0; index < instances.size(); ++index) {
        const auto& source = instances[index];
        Require(source.customIndex < (1u << 24u), "an instance custom index must fit in 24 bits");
        Require(source.mask <= 0xffu, "an instance mask must fit in 8 bits");
        Require(source.bottom != 0u, "an instance needs a bottom-level acceleration structure");
        VkAccelerationStructureInstanceKHR record{};
        std::memcpy(&record.transform, source.objectToWorld.data(), sizeof(record.transform));
        record.instanceCustomIndex = source.customIndex;
        record.mask = source.mask;
        record.instanceShaderBindingTableRecordOffset = 0;
        record.flags = 0;
        record.accelerationStructureReference = source.bottom;
        std::memcpy(records + index * sizeof(record), &record, sizeof(record));
    }
    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = aligned;
    const std::array<std::uint32_t, 1> counts{static_cast<std::uint32_t>(instances.size())};
    structure->build(commands, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, std::span<const VkAccelerationStructureGeometryKHR>(&geometry, 1), counts);
    return structure;
}

void AccelerationStructure::BarrierBeforeUse(const Context& context, VkCommandBuffer commands, VkPipelineStageFlags users) {
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    const auto destination = users | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, destination, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

}

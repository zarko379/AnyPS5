#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AccelerationStructure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "RayQueryProbe_spv.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

constexpr int VulkanTestSkipped = 77;

struct Ray {
    std::array<float, 4> origin;
    std::array<float, 4> direction;
};

struct Hit {
    std::uint32_t kind;
    std::uint32_t geometry;
    std::uint32_t primitive;
    std::uint32_t custom;
    float t;
    float u;
    float v;
    std::uint32_t candidates;
};

GuestBvh::Triangle triangleAt(float x, std::uint32_t node, std::uint32_t info) {
    GuestBvh::Triangle triangle;
    triangle.vertices = {{{x, 0.0f, 5.0f}, {x + 1.0f, 0.0f, 5.0f}, {x, 1.0f, 5.0f}}};
    triangle.nodePointer = node;
    triangle.leafInfo = info;
    return triangle;
}

Ray downward(float x, float y) {
    return {{x, y, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 100.0f}};
}

class Probe {
public:
    explicit Probe(const Context& context) : context(context) {
        const std::array<VkDescriptorSetLayoutBinding, 3> bindings{{
            {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        }};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &setLayout;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = sizeof(RAY_QUERY_PROBE_SPV);
        moduleInfo.pCode = RAY_QUERY_PROBE_SPV;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
        pipelineInfo.layout = pipelineLayout;
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        const std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
        VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = pool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &setLayout;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocateInfo, &set), "vkAllocateDescriptorSets");
    }

    ~Probe() {
        context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    void Record(VkCommandBuffer commands, VkAccelerationStructureKHR scene, Buffer& rays, Buffer& hits, std::uint32_t count) {
        VkWriteDescriptorSetAccelerationStructureKHR sceneInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        sceneInfo.accelerationStructureCount = 1;
        sceneInfo.pAccelerationStructures = &scene;
        const VkDescriptorBufferInfo rayInfo{rays.Handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo hitInfo{hits.Handle(), 0, VK_WHOLE_SIZE};
        std::array<VkWriteDescriptorSet, 3> writes{};
        for (std::uint32_t index = 0; index < 3u; ++index) {
            writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[index].dstSet = set;
            writes[index].dstBinding = index;
            writes[index].descriptorCount = 1;
        }
        writes[0].pNext = &sceneInfo;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &rayInfo;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo = &hitInfo;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        context.BindComputePipeline(commands, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, count, 1, 1);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

private:
    Context context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};

bool near(float value, float expected) {
    return std::fabs(value - expected) <= 1e-4f;
}

void tracesDecodedTriangles(const Context& context) {
    GuestBvh::Bottom bottom;
    bottom.opaque.push_back(triangleAt(0.0f, 0x30u, 0x40000000u));
    bottom.opaque.push_back(triangleAt(10.0f, 0x31u, 0x40000001u));
    bottom.alphaTested.push_back(triangleAt(20.0f, 0x32u, 2u));
    const std::vector<Ray> rays{downward(0.5f, 0.25f), downward(10.25f, 0.5f), downward(20.25f, 0.25f), downward(30.5f, 0.25f), downward(0.5f, 100.25f)};
    Buffer rayBuffer(context, rays.size() * sizeof(Ray), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memcpy(rayBuffer.Bytes().data(), rays.data(), rays.size() * sizeof(Ray));
    Buffer hitBuffer(context, rays.size() * sizeof(Hit), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Probe probe(context);
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    const auto blas = AccelerationStructure::Bottom(context, commands, bottom);
    AccelerationStructure::BarrierBeforeUse(context, commands, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
    TopInstance first;
    first.objectToWorld = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    first.bottom = blas->Address();
    first.customIndex = 7;
    TopInstance moved = first;
    moved.objectToWorld[7] = 100.0f;
    moved.customIndex = 9;
    const std::array<TopInstance, 2> instances{first, moved};
    const auto tlas = AccelerationStructure::Top(context, commands, instances);
    AccelerationStructure::BarrierBeforeUse(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    probe.Record(commands, tlas->Handle(), rayBuffer, hitBuffer, static_cast<std::uint32_t>(rays.size()));
    batch.SubmitAndWait();
    hitBuffer.Invalidate();
    std::vector<Hit> hits(rays.size());
    std::memcpy(hits.data(), hitBuffer.Bytes().data(), hits.size() * sizeof(Hit));
    const auto describe = [&](std::size_t index) {
        const auto& hit = hits[index];
        return "ray " + std::to_string(index) + ": kind " + std::to_string(hit.kind) + " geometry " + std::to_string(hit.geometry) + " primitive " + std::to_string(hit.primitive) + " custom " + std::to_string(hit.custom) + " t " + std::to_string(hit.t) + " u " + std::to_string(hit.u) + " v " + std::to_string(hit.v) + " candidates " + std::to_string(hit.candidates);
    };
    constexpr std::uint32_t Triangle = 1;
    Require(hits[0].kind == Triangle && hits[0].geometry == OpaqueGeometry && hits[0].primitive == 0u && hits[0].custom == 7u && near(hits[0].t, 5.0f) && near(hits[0].u, 0.5f) && near(hits[0].v, 0.25f) && hits[0].candidates == 0u, "an opaque triangle keeps its geometry, primitive order and vertex order (" + describe(0) + ")");
    Require(hits[1].kind == Triangle && hits[1].geometry == OpaqueGeometry && hits[1].primitive == 1u && near(hits[1].u, 0.25f) && near(hits[1].v, 0.5f), "the second opaque triangle is primitive 1 (" + describe(1) + ")");
    Require(hits[2].kind == Triangle && hits[2].geometry == AlphaTestedGeometry && hits[2].primitive == 0u && hits[2].candidates == 1u, "an alpha-tested triangle is a candidate for the shader to accept (" + describe(2) + ")");
    Require(hits[3].kind == 0u, "a ray past every triangle misses (" + describe(3) + ")");
    Require(hits[4].kind == Triangle && hits[4].custom == 9u && hits[4].primitive == 0u && near(hits[4].t, 5.0f), "a translated instance is hit through its object-to-world transform (" + describe(4) + ")");
    std::printf("  acceleration structures: %s\n", describe(0).c_str());
}

}

int main() {
    try {
        std::unique_ptr<AgcDriver::VulkanDevice> device;
        try {
            device = std::make_unique<AgcDriver::VulkanDevice>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        const auto context = device->graphicsContext();
        if (!context.rayQuery) {
            std::printf("skipped, the device offers no acceleration structures or ray queries\n");
            return VulkanTestSkipped;
        }
        tracesDecodedTriangles(context);
        device->WaitIdle();
        std::cout << "acceleration structure tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

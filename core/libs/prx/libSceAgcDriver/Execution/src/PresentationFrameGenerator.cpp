#include "prx/libSceAgcDriver/Execution/include/PresentationFrameGenerator.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/FrameGenLuma_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/FrameGenMotion_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/FrameGenSmooth_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/FrameGenWarp_spv.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace AgcDriver {
namespace {

enum Stage : std::size_t { LumaStage, MotionStage, SmoothStage, WarpStage };

struct LumaParameters {
    std::int32_t width;
    std::int32_t height;
    std::int32_t finerWidth;
    std::int32_t finerHeight;
    std::uint32_t fromColor;
};

struct MotionParameters {
    std::int32_t width;
    std::int32_t height;
    std::int32_t blocksX;
    std::int32_t blocksY;
    std::int32_t guideBlocksX;
    std::int32_t guideBlocksY;
    std::int32_t radius;
    std::uint32_t hasGuide;
    float motionBias;
    std::uint32_t bilateral;
};

struct SmoothParameters {
    std::int32_t blocksX;
    std::int32_t blocksY;
};

struct WarpParameters {
    std::int32_t width;
    std::int32_t height;
    std::int32_t fieldBlocksX;
    std::int32_t fieldBlocksY;
    float pixelsPerBlock;
    float pixelsPerUnit;
    float cutCost;
    float mismatch;
};

constexpr std::array<std::uint32_t, 4> ParameterBytes{sizeof(LumaParameters), sizeof(MotionParameters), sizeof(SmoothParameters), sizeof(WarpParameters)};

std::uint32_t groups(std::uint32_t count) {
    return (count + 7u) / 8u;
}

}

PresentationFrameGenerator::PresentationFrameGenerator(const Graphics::Context& context) : context(context) {
    try {
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Graphics::Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler frame generation");
        const auto sampled = [&](std::uint32_t binding) { return VkDescriptorSetLayoutBinding{binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &sampler}; };
        const auto storage = [](std::uint32_t binding) { return VkDescriptorSetLayoutBinding{binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; };
        const auto buffer = [](std::uint32_t binding) { return VkDescriptorSetLayoutBinding{binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; };
        const std::array<std::vector<VkDescriptorSetLayoutBinding>, 4> bindings{{
            {sampled(0), storage(1), storage(2)},
            {storage(0), storage(1), storage(2), storage(3), buffer(4)},
            {storage(0), storage(1)},
            {sampled(0), sampled(1), storage(2), storage(3), buffer(4)},
        }};
        const std::array<const std::uint32_t*, 4> code{FRAME_GEN_LUMA_SPV, FRAME_GEN_MOTION_SPV, FRAME_GEN_SMOOTH_SPV, FRAME_GEN_WARP_SPV};
        const std::array<std::size_t, 4> codeBytes{sizeof(FRAME_GEN_LUMA_SPV), sizeof(FRAME_GEN_MOTION_SPV), sizeof(FRAME_GEN_SMOOTH_SPV), sizeof(FRAME_GEN_WARP_SPV)};
        for (std::size_t stage = 0; stage < bindings.size(); ++stage) {
            VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings[stage].size());
            layoutInfo.pBindings = bindings[stage].data();
            Graphics::Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &setLayouts[stage]), "vkCreateDescriptorSetLayout frame generation");
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, ParameterBytes[stage]};
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &setLayouts[stage];
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &push;
            Graphics::Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayouts[stage]), "vkCreatePipelineLayout frame generation");
            pipelines[stage] = createPipeline(code[stage], codeBytes[stage], pipelineLayouts[stage], "vkCreateComputePipelines frame generation");
        }
        const std::uint32_t sets = 2 * Levels * 2 + 2 + 1 + 2;
        const std::array<VkDescriptorPoolSize, 3> sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets * 2}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, sets * 4}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sets}}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = sets;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        Graphics::Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool frame generation");
        std::vector<VkDescriptorSetLayout> layouts;
        for (std::uint32_t i = 0; i < 2 * Levels; ++i) layouts.push_back(setLayouts[LumaStage]);
        for (std::uint32_t i = 0; i < 2 * Levels + 2; ++i) layouts.push_back(setLayouts[MotionStage]);
        layouts.push_back(setLayouts[SmoothStage]);
        layouts.push_back(setLayouts[WarpStage]);
        layouts.push_back(setLayouts[WarpStage]);
        std::vector<VkDescriptorSet> allocated(layouts.size());
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool;
        allocation.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
        allocation.pSetLayouts = layouts.data();
        Graphics::Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, allocated.data()), "vkAllocateDescriptorSets frame generation");
        std::size_t next = 0;
        for (auto& slot : lumaSets) for (auto& set : slot) set = allocated[next++];
        for (auto& slot : motionSets) for (auto& set : slot) set = allocated[next++];
        for (auto& set : bilateralSets) set = allocated[next++];
        smoothSet = allocated[next++];
        for (auto& set : warpSets) set = allocated[next++];
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = 2 * sizeof(std::uint32_t);
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Graphics::Check(context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &bufferInfo, nullptr, &statistics), "vkCreateBuffer frame generation statistics");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, statistics, &requirements);
        VkMemoryAllocateInfo memoryInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memoryInfo.allocationSize = requirements.size;
        memoryInfo.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &memoryInfo, nullptr, &statisticsMemory), "vkAllocateMemory frame generation statistics");
        Graphics::Check(context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, statistics, statisticsMemory, 0), "vkBindBufferMemory frame generation statistics");
    } catch (...) {
        release();
        throw;
    }
}

PresentationFrameGenerator::~PresentationFrameGenerator() {
    release();
}

bool PresentationFrameGenerator::Supports(const Graphics::Context& context, VkFormat format) {
    VkFormatProperties frame{};
    context.formatProperties(context.physical, format, &frame);
    const VkFormatFeatureFlags frameNeeded = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    VkFormatProperties result{};
    context.formatProperties(context.physical, VK_FORMAT_R16G16B16A16_SFLOAT, &result);
    const VkFormatFeatureFlags resultNeeded = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT;
    return (frame.optimalTilingFeatures & frameNeeded) == frameNeeded && (result.optimalTilingFeatures & resultNeeded) == resultNeeded;
}

VkPipeline PresentationFrameGenerator::createPipeline(const std::uint32_t* code, std::size_t bytes, VkPipelineLayout layout, const char* what) {
    const auto module = Graphics::CreateShaderModule(context, code, bytes, "vkCreateShaderModule frame generation");
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
    info.layout = layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    const auto result = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    Graphics::Check(result, what);
    return pipeline;
}

VkExtent2D PresentationFrameGenerator::levelExtent(VkExtent2D extent, std::uint32_t level) {
    for (std::uint32_t i = 0; i <= level; ++i) extent = {std::max(1u, (extent.width + 1u) / 2u), std::max(1u, (extent.height + 1u) / 2u)};
    return extent;
}

VkExtent2D PresentationFrameGenerator::blocksOf(VkExtent2D extent) {
    return {(extent.width + BlockSize - 1u) / BlockSize, (extent.height + BlockSize - 1u) / BlockSize};
}

void PresentationFrameGenerator::createSurface(Surface& surface, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage) {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {extent.width, extent.height, 1u};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &surface.image), "vkCreateImage frame generation");
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, surface.image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &surface.memory), "vkAllocateMemory frame generation");
    Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, surface.image, surface.memory, 0), "vkBindImageMemory frame generation");
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = surface.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    Graphics::Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &surface.view), "vkCreateImageView frame generation");
    surface.format = format;
    surface.extent = extent;
}

void PresentationFrameGenerator::releaseSurface(Surface& surface) noexcept {
    if (surface.view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, surface.view, nullptr);
    if (surface.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, surface.image, nullptr);
    if (surface.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, surface.memory, nullptr);
    surface = Surface{};
}

void PresentationFrameGenerator::releaseFrames() noexcept {
    for (auto& slot : history) {
        releaseSurface(slot.color);
        for (auto& level : slot.luma) releaseSurface(level);
    }
    for (auto& level : fields) releaseSurface(level);
    releaseSurface(bilateral);
    releaseSurface(smoothed);
    releaseSurface(output);
    frameFormat = VK_FORMAT_UNDEFINED;
    frameExtent = {};
    previousValid = false;
    held = false;
}

void PresentationFrameGenerator::release() noexcept {
    releaseFrames();
    if (statistics) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, statistics, nullptr);
    if (statisticsMemory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, statisticsMemory, nullptr);
    statistics = VK_NULL_HANDLE;
    statisticsMemory = VK_NULL_HANDLE;
    for (auto& pipeline : pipelines) {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    for (auto& layout : pipelineLayouts) {
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
    if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    pool = VK_NULL_HANDLE;
    for (auto& layout : setLayouts) {
        if (layout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
    if (sampler) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
}

void PresentationFrameGenerator::ensure(VkFormat format, VkExtent2D extent) {
    if (frameFormat == format && frameExtent.width == extent.width && frameExtent.height == extent.height) return;
    releaseFrames();
    try {
        for (auto& slot : history) {
            createSurface(slot.color, format, extent, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            for (std::uint32_t level = 0; level < Levels; ++level) createSurface(slot.luma[level], VK_FORMAT_R32_SFLOAT, levelExtent(extent, level), VK_IMAGE_USAGE_STORAGE_BIT);
        }
        for (std::uint32_t level = 0; level < Levels; ++level) createSurface(fields[level], VK_FORMAT_R32G32B32A32_SFLOAT, blocksOf(levelExtent(extent, level)), VK_IMAGE_USAGE_STORAGE_BIT);
        createSurface(bilateral, VK_FORMAT_R32G32B32A32_SFLOAT, blocksOf(levelExtent(extent, 0)), VK_IMAGE_USAGE_STORAGE_BIT);
        createSurface(smoothed, VK_FORMAT_R32G32B32A32_SFLOAT, blocksOf(levelExtent(extent, 0)), VK_IMAGE_USAGE_STORAGE_BIT);
        createSurface(output, VK_FORMAT_R16G16B16A16_SFLOAT, extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        frameFormat = format;
        frameExtent = extent;
        writeSets();
    } catch (...) {
        releaseFrames();
        throw;
    }
}

void PresentationFrameGenerator::writeSets() {
    std::vector<VkDescriptorImageInfo> images;
    images.reserve(64);
    std::vector<VkWriteDescriptorSet> writes;
    const VkDescriptorBufferInfo statisticsInfo{statistics, 0, VK_WHOLE_SIZE};
    const auto sampledAt = [&](VkDescriptorSet set, std::uint32_t binding, const Surface& surface) {
        images.push_back({sampler, surface.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &images.back();
        writes.push_back(write);
    };
    const auto storageAt = [&](VkDescriptorSet set, std::uint32_t binding, const Surface& surface) {
        images.push_back({VK_NULL_HANDLE, surface.view, VK_IMAGE_LAYOUT_GENERAL});
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &images.back();
        writes.push_back(write);
    };
    const auto bufferAt = [&](VkDescriptorSet set, std::uint32_t binding) {
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &statisticsInfo;
        writes.push_back(write);
    };
    for (std::uint32_t slot = 0; slot < 2; ++slot) {
        const auto& own = history[slot];
        const auto& other = history[1u - slot];
        for (std::uint32_t level = 0; level < Levels; ++level) {
            const auto set = lumaSets[slot][level];
            sampledAt(set, 0, own.color);
            storageAt(set, 1, level == 0 ? own.luma[0] : own.luma[level - 1]);
            storageAt(set, 2, own.luma[level]);
            const auto motion = motionSets[slot][level];
            storageAt(motion, 0, other.luma[level]);
            storageAt(motion, 1, own.luma[level]);
            storageAt(motion, 2, level + 1 < Levels ? fields[level + 1] : fields[level]);
            storageAt(motion, 3, fields[level]);
            bufferAt(motion, 4);
        }
        storageAt(bilateralSets[slot], 0, other.luma[0]);
        storageAt(bilateralSets[slot], 1, own.luma[0]);
        storageAt(bilateralSets[slot], 2, fields[0]);
        storageAt(bilateralSets[slot], 3, bilateral);
        bufferAt(bilateralSets[slot], 4);
        sampledAt(warpSets[slot], 0, other.color);
        sampledAt(warpSets[slot], 1, own.color);
        storageAt(warpSets[slot], 2, smoothed);
        storageAt(warpSets[slot], 3, output);
        bufferAt(warpSets[slot], 4);
    }
    storageAt(smoothSet, 0, bilateral);
    storageAt(smoothSet, 1, smoothed);
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void PresentationFrameGenerator::barrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess, VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess) const {
    VkImageMemoryBarrier imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageBarrier.srcAccessMask = sourceAccess;
    imageBarrier.dstAccessMask = destinationAccess;
    imageBarrier.oldLayout = from;
    imageBarrier.newLayout = to;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = image;
    imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
}

void PresentationFrameGenerator::computeBarrier(VkCommandBuffer commands) const {
    VkMemoryBarrier memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}

void PresentationFrameGenerator::recordPyramid(VkCommandBuffer commands, std::uint32_t slot) {
    const auto bindSets = context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets");
    const auto push = context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants");
    const auto dispatch = context.Function<PFN_vkCmdDispatch>("vkCmdDispatch");
    context.BindComputePipeline(commands, pipelines[LumaStage]);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto extent = levelExtent(frameExtent, level);
        const auto finer = level == 0 ? extent : levelExtent(frameExtent, level - 1);
        const LumaParameters parameters{static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), static_cast<std::int32_t>(finer.width), static_cast<std::int32_t>(finer.height), level == 0 ? 1u : 0u};
        bindSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayouts[LumaStage], 0, 1, &lumaSets[slot][level], 0, nullptr);
        push(commands, pipelineLayouts[LumaStage], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(parameters), &parameters);
        dispatch(commands, groups(extent.width), groups(extent.height), 1);
        computeBarrier(commands);
    }
}

void PresentationFrameGenerator::recordInterpolation(VkCommandBuffer commands, std::uint32_t slot) {
    const auto bindSets = context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets");
    const auto push = context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants");
    const auto dispatch = context.Function<PFN_vkCmdDispatch>("vkCmdDispatch");
    context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, statistics, 0, VK_WHOLE_SIZE, 0);
    VkMemoryBarrier filled{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    filled.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    filled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &filled, 0, nullptr, 0, nullptr);
    context.BindComputePipeline(commands, pipelines[MotionStage]);
    for (std::uint32_t step = 0; step < Levels; ++step) {
        const auto level = Levels - 1u - step;
        const auto extent = levelExtent(frameExtent, level);
        const auto blocks = blocksOf(extent);
        const auto coarser = level + 1 < Levels ? blocksOf(levelExtent(frameExtent, level + 1)) : blocks;
        const MotionParameters parameters{static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), static_cast<std::int32_t>(blocks.width), static_cast<std::int32_t>(blocks.height), static_cast<std::int32_t>(coarser.width), static_cast<std::int32_t>(coarser.height), level + 1 < Levels ? RefineRadius : CoarsestRadius, level + 1 < Levels ? 1u : 0u, MotionBias, 0u};
        bindSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayouts[MotionStage], 0, 1, &motionSets[slot][level], 0, nullptr);
        push(commands, pipelineLayouts[MotionStage], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(parameters), &parameters);
        dispatch(commands, groups(blocks.width), groups(blocks.height), 1);
        computeBarrier(commands);
    }
    const auto finest = blocksOf(levelExtent(frameExtent, 0));
    const auto finestExtent = levelExtent(frameExtent, 0);
    const MotionParameters selection{static_cast<std::int32_t>(finestExtent.width), static_cast<std::int32_t>(finestExtent.height), static_cast<std::int32_t>(finest.width), static_cast<std::int32_t>(finest.height), static_cast<std::int32_t>(finest.width), static_cast<std::int32_t>(finest.height), RefineRadius, 1u, MotionBias, 1u};
    bindSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayouts[MotionStage], 0, 1, &bilateralSets[slot], 0, nullptr);
    push(commands, pipelineLayouts[MotionStage], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(selection), &selection);
    dispatch(commands, groups(finest.width), groups(finest.height), 1);
    computeBarrier(commands);
    const SmoothParameters smoothing{static_cast<std::int32_t>(finest.width), static_cast<std::int32_t>(finest.height)};
    context.BindComputePipeline(commands, pipelines[SmoothStage]);
    bindSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayouts[SmoothStage], 0, 1, &smoothSet, 0, nullptr);
    push(commands, pipelineLayouts[SmoothStage], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(smoothing), &smoothing);
    dispatch(commands, groups(finest.width), groups(finest.height), 1);
    computeBarrier(commands);
    barrier(commands, output.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
    const WarpParameters warp{static_cast<std::int32_t>(frameExtent.width), static_cast<std::int32_t>(frameExtent.height), static_cast<std::int32_t>(finest.width), static_cast<std::int32_t>(finest.height), static_cast<float>(BlockSize * 2u), 2.0f, CutCost, Mismatch};
    context.BindComputePipeline(commands, pipelines[WarpStage]);
    bindSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayouts[WarpStage], 0, 1, &warpSets[slot], 0, nullptr);
    push(commands, pipelineLayouts[WarpStage], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(warp), &warp);
    dispatch(commands, groups(frameExtent.width), groups(frameExtent.height), 1);
}

bool PresentationFrameGenerator::RecordCapture(VkCommandBuffer commands, VkImage image, VkFormat format, VkExtent2D extent, VkImageLayout layout, bool generate) {
    Graphics::Require(extent.width != 0 && extent.height != 0, "frame generation needs a non-empty frame");
    const bool fresh = frameFormat != format || frameExtent.width != extent.width || frameExtent.height != extent.height;
    ensure(format, extent);
    if (fresh) {
        for (auto& slot : history) for (auto& level : slot.luma) barrier(commands, level.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        for (auto& level : fields) barrier(commands, level.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        barrier(commands, bilateral.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        barrier(commands, smoothed.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
    const auto slot = previousValid ? 1u - newest : newest;
    const auto& target = history[slot].color;
    barrier(commands, image, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    barrier(commands, target.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.extent = {extent.width, extent.height, 1u};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(commands, target.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    recordPyramid(commands, slot);
    const bool interpolate = generate && previousValid;
    if (interpolate) {
        recordInterpolation(commands, slot);
        barrier(commands, output.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        barrier(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1] = {static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), 1};
        blit.dstOffsets[1] = blit.srcOffsets[1];
        context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        barrier(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
        ++generated;
    } else {
        barrier(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
    }
    newest = slot;
    previousValid = true;
    held = interpolate;
    return interpolate;
}

void PresentationFrameGenerator::RecordHeld(VkCommandBuffer commands, VkImage image, VkImageLayout finalLayout) {
    Graphics::Require(held, "frame generation holds no real frame to present");
    const auto& source = history[newest].color;
    barrier(commands, source.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    barrier(commands, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.extent = {frameExtent.width, frameExtent.height, 1u};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
    barrier(commands, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    held = false;
}

void PresentationFrameGenerator::Forget() {
    previousValid = false;
    held = false;
}

}

#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONFRAMEGENERATOR_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATIONFRAMEGENERATOR_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <cstdint>

namespace AgcDriver {

class PresentationFrameGenerator {
public:
    static constexpr std::uint32_t Levels = 3;
    static constexpr std::uint32_t BlockSize = 8;
    static constexpr std::int32_t CoarsestRadius = 8;
    static constexpr std::int32_t RefineRadius = 2;
    static constexpr float MotionBias = 0.002f;
    static constexpr float CutCost = 0.12f;
    static constexpr float Mismatch = 0.04f;

    explicit PresentationFrameGenerator(const Graphics::Context& context);
    ~PresentationFrameGenerator();
    PresentationFrameGenerator(const PresentationFrameGenerator&) = delete;
    PresentationFrameGenerator& operator=(const PresentationFrameGenerator&) = delete;

    static bool Supports(const Graphics::Context& context, VkFormat format);
    bool RecordCapture(VkCommandBuffer commands, VkImage image, VkFormat format, VkExtent2D extent, VkImageLayout layout, bool generate);
    bool HoldsFrame() const { return held; }
    void RecordHeld(VkCommandBuffer commands, VkImage image, VkImageLayout finalLayout);
    void Forget();
    std::uint64_t Generated() const { return generated; }

private:
    struct Surface {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
    };
    struct History {
        Surface color;
        std::array<Surface, Levels> luma;
    };

    void ensure(VkFormat format, VkExtent2D extent);
    void createSurface(Surface& surface, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage);
    void releaseSurface(Surface& surface) noexcept;
    void releaseFrames() noexcept;
    void release() noexcept;
    void writeSets();
    VkPipeline createPipeline(const std::uint32_t* code, std::size_t bytes, VkPipelineLayout layout, const char* what);
    static VkExtent2D levelExtent(VkExtent2D extent, std::uint32_t level);
    static VkExtent2D blocksOf(VkExtent2D extent);
    void barrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess, VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess) const;
    void computeBarrier(VkCommandBuffer commands) const;
    void recordPyramid(VkCommandBuffer commands, std::uint32_t slot);
    void recordInterpolation(VkCommandBuffer commands, std::uint32_t newest);

    Graphics::Context context;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSetLayout, 4> setLayouts{};
    std::array<VkPipelineLayout, 4> pipelineLayouts{};
    std::array<VkPipeline, 4> pipelines{};
    std::array<std::array<VkDescriptorSet, Levels>, 2> lumaSets{};
    std::array<std::array<VkDescriptorSet, Levels>, 2> motionSets{};
    std::array<VkDescriptorSet, 2> bilateralSets{};
    VkDescriptorSet smoothSet = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> warpSets{};
    std::array<History, 2> history;
    std::array<Surface, Levels> fields;
    Surface bilateral;
    Surface smoothed;
    Surface output;
    VkBuffer statistics = VK_NULL_HANDLE;
    VkDeviceMemory statisticsMemory = VK_NULL_HANDLE;
    VkFormat frameFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D frameExtent{};
    std::uint32_t newest = 0;
    bool previousValid = false;
    bool held = false;
    std::uint64_t generated = 0;
};

}

#endif

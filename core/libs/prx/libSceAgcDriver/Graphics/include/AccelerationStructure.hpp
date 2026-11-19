#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ACCELERATIONSTRUCTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ACCELERATIONSTRUCTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBvh.hpp"
#include <array>
#include <memory>
#include <span>

namespace AgcDriver::Graphics {

class Buffer;

inline constexpr std::uint32_t OpaqueGeometry = 0;
inline constexpr std::uint32_t AlphaTestedGeometry = 1;

struct TopInstance {
    std::array<float, 12> objectToWorld{};
    VkDeviceAddress bottom = 0;
    std::uint32_t customIndex = 0;
    std::uint32_t mask = 0xffu;
};

class AccelerationStructure {
public:
    ~AccelerationStructure();
    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;

    static std::unique_ptr<AccelerationStructure> Bottom(const Context& context, VkCommandBuffer commands, const GuestBvh::Bottom& bottom);
    static std::unique_ptr<AccelerationStructure> Top(const Context& context, VkCommandBuffer commands, std::span<const TopInstance> instances);
    static void BarrierBeforeUse(const Context& context, VkCommandBuffer commands, VkPipelineStageFlags users);

    VkAccelerationStructureKHR Handle() const { return handle; }
    VkDeviceAddress Address() const { return address; }
    void ReleaseBuildInputs();

private:
    explicit AccelerationStructure(const Context& context);
    void build(VkCommandBuffer commands, VkAccelerationStructureTypeKHR type, std::span<const VkAccelerationStructureGeometryKHR> geometries, std::span<const std::uint32_t> counts);
    void release() noexcept;

    Context context;
    std::unique_ptr<Buffer> storage;
    std::unique_ptr<Buffer> inputs;
    std::unique_ptr<Buffer> scratch;
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkDeviceAddress address = 0;
};

}

#endif

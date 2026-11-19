#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBVH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBVH_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver::Graphics::GuestBvh {

inline constexpr std::size_t HeaderBytes = 0x100;
inline constexpr std::uint32_t RootPointer = 37;

enum class Refusal {
    None,
    Truncated,
    Magic,
    Header,
    Kind,
    NodeRange,
    LeafRange,
    NodeType,
    LeafType,
    RecordType,
    Depth,
    DuplicateRecord,
    InstanceRange,
};

const char* RefusalName(Refusal refusal);

struct Header {
    std::uint32_t bytes = 0;
    std::uint32_t nodeEnd = 0;
    std::uint32_t recordEnd = 0;
    std::uint32_t instanceOffset = 0;
    std::uint32_t instanceCount = 0;
    bool top = false;
};

std::optional<Header> ReadHeader(std::span<const std::byte> bytes, Refusal& refusal);

struct Triangle {
    std::array<std::array<float, 3>, 3> vertices{};
    std::uint32_t nodePointer = 0;
    std::uint32_t leafInfo = 0;
    std::uint32_t triangleId = 0;
};

struct Bottom {
    std::vector<Triangle> opaque;
    std::vector<Triangle> alphaTested;
    std::uint32_t records = 0;
    std::uint32_t boxes = 0;
    std::uint32_t leaves = 0;
    std::uint32_t emptyChildren = 0;
    std::uint32_t invalidChildren = 0;
};

struct Instance {
    std::uint32_t index = 0;
    std::array<float, 12> worldToObject{};
    std::array<float, 12> objectToWorld{};
    std::uint64_t bottomAddress = 0;
    std::uint32_t bottomFlags = 0;
    std::uint32_t bottomRoot = 0;
    std::uint32_t maskAndFlags = 0;
    std::uint32_t instanceIndex = 0;
    std::uint32_t word29 = 0;
    std::uint32_t customIndex = 0;
};

struct Top {
    std::vector<Instance> instances;
    std::uint32_t boxes = 0;
    std::uint32_t emptyChildren = 0;
    std::uint32_t invalidChildren = 0;
};

struct BottomResult {
    std::optional<Bottom> bottom;
    Refusal refusal = Refusal::None;
};

struct TopResult {
    std::optional<Top> top;
    Refusal refusal = Refusal::None;
};

BottomResult DecodeBottom(std::span<const std::byte> bytes, std::uint32_t root = RootPointer);
TopResult DecodeTop(std::span<const std::byte> bytes);

}

#endif

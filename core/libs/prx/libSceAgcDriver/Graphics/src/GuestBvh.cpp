#include "prx/libSceAgcDriver/Graphics/include/GuestBvh.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace AgcDriver::Graphics::GuestBvh {

namespace {

constexpr std::array<std::byte, 8> Magic{std::byte{'P'}, std::byte{'S'}, std::byte{'R'}, std::byte{'_'}, std::byte{'B'}, std::byte{'V'}, std::byte{'H'}, std::byte{'L'}};
constexpr std::uint32_t FormatWord = 0x5d1cba8cu;
constexpr std::uint32_t NoChild = 0xffffffffu;
constexpr std::uint32_t TypeBox16 = 4u;
constexpr std::uint32_t TypeBox32 = 5u;
constexpr std::uint32_t Box16Bytes = 64u;
constexpr std::uint32_t Box32Bytes = 128u;
constexpr std::uint32_t TriangleBytes = 64u;
constexpr std::uint32_t RecordBytes = 8u;
constexpr std::uint32_t InstanceBytes = 128u;
constexpr std::uint32_t LastRecord = 0x80000000u;
constexpr std::uint32_t OpaqueRecord = 0x40000000u;
constexpr std::uint32_t MaxDepth = 64u;
constexpr std::array<std::array<std::uint32_t, 3>, 4> Corners{{{0u, 1u, 2u}, {1u, 3u, 2u}, {2u, 3u, 4u}, {2u, 4u, 0u}}};

std::uint32_t word(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float real(std::span<const std::byte> bytes, std::size_t offset) {
    return std::bit_cast<float>(word(bytes, offset));
}

float half(std::uint32_t bits) {
    const auto sign = (bits >> 15u) & 1u;
    const auto exponent = (bits >> 10u) & 0x1fu;
    const auto mantissa = bits & 0x3ffu;
    float value = 0.0f;
    if (exponent == 0u) value = std::ldexp(static_cast<float>(mantissa), -24);
    else if (exponent == 0x1fu) value = mantissa == 0u ? INFINITY : NAN;
    else value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    return sign != 0u ? -value : value;
}

std::uint64_t nodeOffset(std::uint32_t pointer) {
    return std::uint64_t{pointer & ~7u} * 8u;
}

bool live(const std::array<float, 6>& box) {
    return box[0] <= box[3] && box[1] <= box[4] && box[2] <= box[5];
}

std::array<float, 6> childBox(std::span<const std::byte> bytes, std::uint64_t offset, std::uint32_t type, std::uint32_t child) {
    std::array<float, 6> box{};
    if (type == TypeBox32) {
        for (std::uint32_t i = 0; i < 6u; ++i) box[i] = real(bytes, offset + 16u + (child * 6u + i) * 4u);
        return box;
    }
    for (std::uint32_t i = 0; i < 6u; ++i) {
        const auto index = child * 6u + i;
        const auto packed = word(bytes, offset + 16u + (index / 2u) * 4u);
        box[i] = half(index % 2u == 0u ? packed & 0xffffu : packed >> 16u);
    }
    return box;
}

struct Walk {
    std::span<const std::byte> bytes;
    Header header;
    Refusal refusal = Refusal::None;
    std::uint32_t boxes = 0;
    std::uint32_t emptyChildren = 0;
    std::uint32_t invalidChildren = 0;
};

template<typename TLeaf>
bool walk(Walk& state, std::uint32_t root, const TLeaf& leaf) {
    struct Pending {
        std::uint32_t pointer;
        std::uint32_t depth;
    };
    std::vector<Pending> stack{{root, 0u}};
    std::unordered_set<std::uint32_t> seen;
    while (!stack.empty()) {
        const auto [pointer, depth] = stack.back();
        stack.pop_back();
        if (!seen.insert(pointer).second) continue;
        if (depth > MaxDepth) {
            state.refusal = Refusal::Depth;
            return false;
        }
        const auto type = pointer & 7u;
        if (type != TypeBox16 && type != TypeBox32) {
            if (type != 0u) {
                state.refusal = Refusal::NodeType;
                return false;
            }
            if (!leaf(pointer)) return false;
            continue;
        }
        const auto offset = nodeOffset(pointer);
        const auto size = type == TypeBox32 ? Box32Bytes : Box16Bytes;
        if (offset < HeaderBytes || offset + size > state.header.nodeEnd) {
            state.refusal = Refusal::NodeRange;
            return false;
        }
        ++state.boxes;
        for (std::uint32_t child = 0; child < 4u; ++child) {
            const auto next = word(state.bytes, offset + child * 4u);
            if (next == NoChild) continue;
            if ((next & 7u) > TypeBox32) {
                ++state.invalidChildren;
                continue;
            }
            if (!live(childBox(state.bytes, offset, type, child))) {
                ++state.emptyChildren;
                continue;
            }
            stack.push_back({next, depth + 1u});
        }
    }
    return true;
}

}

const char* RefusalName(Refusal refusal) {
    switch (refusal) {
    case Refusal::None: return "none";
    case Refusal::Truncated: return "truncated";
    case Refusal::Magic: return "magic";
    case Refusal::Header: return "header";
    case Refusal::Kind: return "kind";
    case Refusal::NodeRange: return "node range";
    case Refusal::LeafRange: return "leaf range";
    case Refusal::NodeType: return "node type";
    case Refusal::LeafType: return "leaf type";
    case Refusal::RecordType: return "record type";
    case Refusal::Depth: return "depth";
    case Refusal::DuplicateRecord: return "duplicate record";
    case Refusal::InstanceRange: return "instance range";
    }
    return "unknown";
}

std::optional<Header> ReadHeader(std::span<const std::byte> bytes, Refusal& refusal) {
    refusal = Refusal::None;
    if (bytes.size() < HeaderBytes) {
        refusal = Refusal::Truncated;
        return std::nullopt;
    }
    if (std::memcmp(bytes.data(), Magic.data(), Magic.size()) != 0) {
        refusal = Refusal::Magic;
        return std::nullopt;
    }
    if (word(bytes, 0x08) != FormatWord || word(bytes, 0x0c) != 0u || word(bytes, 0x14) != 0u || word(bytes, 0x1c) != 0u) {
        refusal = Refusal::Header;
        return std::nullopt;
    }
    Header header;
    header.bytes = word(bytes, 0x10);
    header.nodeEnd = word(bytes, 0x58) * 64u;
    header.instanceOffset = word(bytes, 0x30);
    header.instanceCount = word(bytes, 0x70);
    header.top = header.instanceOffset != 0u || header.instanceCount != 0u;
    const auto recordEnd = word(bytes, 0x28);
    const auto recordUnits = word(bytes, 0x60);
    if (header.bytes < HeaderBytes || word(bytes, 0x58) > header.bytes / 64u || header.nodeEnd < HeaderBytes) {
        refusal = Refusal::Header;
        return std::nullopt;
    }
    if (header.top) {
        if (header.instanceOffset != header.nodeEnd || std::uint64_t{header.instanceOffset} + std::uint64_t{header.instanceCount} * InstanceBytes != header.bytes || recordEnd != 0u || recordUnits != 0u) {
            refusal = Refusal::Header;
            return std::nullopt;
        }
    } else {
        if (std::uint64_t{recordUnits} * RecordBytes != recordEnd || recordEnd < header.nodeEnd || recordEnd > header.bytes) {
            refusal = Refusal::Header;
            return std::nullopt;
        }
        header.recordEnd = recordEnd;
    }
    if (bytes.size() < header.bytes) {
        refusal = Refusal::Truncated;
        return std::nullopt;
    }
    return header;
}

BottomResult DecodeBottom(std::span<const std::byte> bytes, std::uint32_t root) {
    BottomResult result;
    const auto header = ReadHeader(bytes, result.refusal);
    if (!header) return result;
    if (header->top) {
        result.refusal = Refusal::Kind;
        return result;
    }
    Bottom bottom;
    Walk state{bytes, *header};
    std::unordered_map<std::uint32_t, std::uint32_t> records;
    const auto leaf = [&](std::uint32_t pointer) {
        ++bottom.leaves;
        for (std::uint64_t index = pointer >> 3u;; ++index) {
            const auto offset = index * RecordBytes;
            if (offset < header->nodeEnd || offset + RecordBytes > header->recordEnd) {
                state.refusal = Refusal::LeafRange;
                return false;
            }
            const auto node = word(bytes, offset);
            const auto info = word(bytes, offset + 4u);
            ++bottom.records;
            const auto type = node & 7u;
            if (type > 3u) {
                state.refusal = Refusal::RecordType;
                return false;
            }
            const auto nodeAt = nodeOffset(node);
            if (nodeAt < HeaderBytes || nodeAt + TriangleBytes > header->nodeEnd) {
                state.refusal = Refusal::NodeRange;
                return false;
            }
            const auto kept = info & ~LastRecord;
            const auto [found, inserted] = records.emplace(node, kept);
            if (!inserted && found->second != kept) {
                state.refusal = Refusal::DuplicateRecord;
                return false;
            }
            if (inserted) {
                Triangle triangle;
                triangle.nodePointer = node;
                triangle.leafInfo = kept;
                triangle.triangleId = word(bytes, nodeAt + 60u);
                for (std::uint32_t corner = 0; corner < 3u; ++corner) {
                    for (std::uint32_t axis = 0; axis < 3u; ++axis) triangle.vertices[corner][axis] = real(bytes, nodeAt + (Corners[type][corner] * 3u + axis) * 4u);
                }
                ((kept & OpaqueRecord) != 0u ? bottom.opaque : bottom.alphaTested).push_back(triangle);
            }
            if ((info & LastRecord) != 0u) return true;
        }
    };
    if (!walk(state, root, leaf)) {
        result.refusal = state.refusal;
        return result;
    }
    bottom.boxes = state.boxes;
    bottom.emptyChildren = state.emptyChildren;
    bottom.invalidChildren = state.invalidChildren;
    result.bottom = std::move(bottom);
    return result;
}

TopResult DecodeTop(std::span<const std::byte> bytes) {
    TopResult result;
    const auto header = ReadHeader(bytes, result.refusal);
    if (!header) return result;
    if (!header->top) {
        result.refusal = Refusal::Kind;
        return result;
    }
    Top top;
    Walk state{bytes, *header};
    std::unordered_set<std::uint32_t> reached;
    const auto leaf = [&](std::uint32_t pointer) {
        const auto index = pointer >> 3u;
        if (index >= header->instanceCount) {
            state.refusal = Refusal::InstanceRange;
            return false;
        }
        reached.insert(index);
        return true;
    };
    if (!walk(state, RootPointer, leaf)) {
        result.refusal = state.refusal;
        return result;
    }
    std::vector<std::uint32_t> order(reached.begin(), reached.end());
    std::sort(order.begin(), order.end());
    for (const auto index : order) {
        const auto offset = std::uint64_t{header->instanceOffset} + std::uint64_t{index} * InstanceBytes;
        Instance instance;
        instance.index = index;
        for (std::uint32_t i = 0; i < 12u; ++i) {
            instance.worldToObject[i] = real(bytes, offset + i * 4u);
            instance.objectToWorld[i] = real(bytes, offset + (16u + i) * 4u);
        }
        const auto low = word(bytes, offset + 48u);
        const auto high = word(bytes, offset + 52u);
        const auto packed = (std::uint64_t{high} << 32u) | low;
        instance.bottomAddress = (packed & 0x003fffffffffffffull) << 3u;
        instance.bottomFlags = high >> 22u;
        instance.bottomRoot = word(bytes, offset + 56u);
        instance.maskAndFlags = word(bytes, offset + 60u);
        instance.instanceIndex = word(bytes, offset + 112u);
        instance.word29 = word(bytes, offset + 116u);
        instance.customIndex = word(bytes, offset + 120u);
        if (instance.bottomAddress == 0u || (instance.bottomRoot & 7u) != TypeBox32) {
            result.refusal = Refusal::InstanceRange;
            return result;
        }
        top.instances.push_back(instance);
    }
    top.boxes = state.boxes;
    top.emptyChildren = state.emptyChildren;
    top.invalidChildren = state.invalidChildren;
    result.top = std::move(top);
    return result;
}

}

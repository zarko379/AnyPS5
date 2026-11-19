#include "prx/libSceAgcDriver/Graphics/include/GuestBvh.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace AgcDriver::Graphics::GuestBvh;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

struct Image {
    std::vector<std::byte> bytes;

    explicit Image(std::size_t size) : bytes(size) {}

    void Put(std::size_t offset, std::uint32_t value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    void PutFloat(std::size_t offset, float value) {
        Put(offset, std::bit_cast<std::uint32_t>(value));
    }

    void Header(std::uint32_t total, std::uint32_t nodeEnd) {
        std::memcpy(bytes.data(), "PSR_BVHL", 8);
        Put(0x08, 0x5d1cba8cu);
        Put(0x10, total);
        Put(0x58, nodeEnd / 64u);
    }

    void Records(std::uint32_t recordEnd) {
        Put(0x28, recordEnd);
        Put(0x60, recordEnd / 8u);
    }

    void Box32(std::size_t offset, std::array<std::uint32_t, 4> children, std::array<std::array<float, 6>, 4> boxes) {
        for (std::uint32_t i = 0; i < 4u; ++i) {
            Put(offset + i * 4u, children[i]);
            for (std::uint32_t k = 0; k < 6u; ++k) PutFloat(offset + 16u + (i * 6u + k) * 4u, boxes[i][k]);
        }
    }

    void Triangles(std::size_t offset, std::uint32_t triangleId) {
        for (std::uint32_t vertex = 0; vertex < 5u; ++vertex) {
            for (std::uint32_t axis = 0; axis < 3u; ++axis) PutFloat(offset + (vertex * 3u + axis) * 4u, static_cast<float>(vertex * 10u + axis));
        }
        Put(offset + 60u, triangleId);
    }

    void Record(std::size_t offset, std::uint32_t node, std::uint32_t info) {
        Put(offset, node);
        Put(offset + 4u, info);
    }
};

constexpr std::array<float, 6> Unit{0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 50.0f};
constexpr std::array<float, 6> Inverted{1.0f, 0.0f, 0.0f, 0.0f, 50.0f, 50.0f};
constexpr std::uint32_t None = 0xffffffffu;
constexpr std::uint32_t TriangleNode = 0x180u / 8u;
constexpr std::uint32_t LeafPointer = (0x1c0u / 8u) << 3u;

Image TwoTriangleBottom() {
    Image image(0x1d0);
    image.Header(0x1d0, 0x1c0);
    image.Records(0x1d0);
    image.Box32(0x100, {LeafPointer, None, None, None}, {Unit, Unit, Unit, Unit});
    image.Triangles(0x180, 0x0b0a0908u);
    image.Record(0x1c0, TriangleNode | 0u, 0x40000000u | 7u);
    image.Record(0x1c8, TriangleNode | 2u, 0x80000000u | 9u);
    return image;
}

void DecodesTriangles() {
    const auto image = TwoTriangleBottom();
    const auto result = DecodeBottom(image.bytes);
    Expect(result.bottom.has_value(), std::string("two-triangle bottom refused: ") + RefusalName(result.refusal));
    if (!result.bottom) return;
    const auto& bottom = *result.bottom;
    Expect(bottom.opaque.size() == 1u && bottom.alphaTested.size() == 1u, "opaque and alpha-tested records are split by bit 30");
    Expect(bottom.records == 2u && bottom.leaves == 1u && bottom.boxes == 1u, "record, leaf and box counts");
    if (bottom.opaque.size() != 1u || bottom.alphaTested.size() != 1u) return;
    const auto& first = bottom.opaque[0];
    Expect(first.nodePointer == (TriangleNode | 0u) && first.leafInfo == (0x40000000u | 7u) && first.triangleId == 0x0b0a0908u, "type 0 keeps its node pointer, leaf info and triangle id");
    Expect(first.vertices[0][0] == 0.0f && first.vertices[1][0] == 10.0f && first.vertices[2][0] == 20.0f && first.vertices[2][2] == 22.0f, "type 0 is (v0, v1, v2)");
    const auto& second = bottom.alphaTested[0];
    Expect(second.leafInfo == 9u, "the end-of-list bit is not kept in the leaf info");
    Expect(second.vertices[0][0] == 20.0f && second.vertices[1][0] == 30.0f && second.vertices[2][0] == 40.0f, "type 2 is (v2, v3, v4)");
}

void DecodesEveryTriangleType() {
    Image image(0x1e0);
    image.Header(0x1e0, 0x1c0);
    image.Records(0x1e0);
    image.Box32(0x100, {LeafPointer, None, None, None}, {Unit, Unit, Unit, Unit});
    image.Triangles(0x180, 0u);
    for (std::uint32_t type = 0; type < 4u; ++type) image.Record(0x1c0 + type * 8u, TriangleNode | type, 0x40000000u | type | (type == 3u ? 0x80000000u : 0u));
    const auto result = DecodeBottom(image.bytes);
    Expect(result.bottom.has_value() && result.bottom->opaque.size() == 4u, "four triangle types decode");
    if (!result.bottom || result.bottom->opaque.size() != 4u) return;
    constexpr std::array<std::array<float, 3>, 4> expected{{{0.0f, 10.0f, 20.0f}, {10.0f, 30.0f, 20.0f}, {20.0f, 30.0f, 40.0f}, {20.0f, 40.0f, 0.0f}}};
    for (std::uint32_t type = 0; type < 4u; ++type) {
        const auto& triangle = result.bottom->opaque[type];
        Expect(triangle.vertices[0][0] == expected[type][0] && triangle.vertices[1][0] == expected[type][1] && triangle.vertices[2][0] == expected[type][2], "triangle type " + std::to_string(type) + " picks its fan corners in order");
    }
}

void SharesDuplicateRecords() {
    Image image(0x1f0);
    image.Header(0x1f0, 0x1c0);
    image.Records(0x1f0);
    const std::uint32_t secondLeaf = (0x1d0u / 8u) << 3u;
    image.Box32(0x100, {LeafPointer, secondLeaf, None, None}, {Unit, Unit, Unit, Unit});
    image.Triangles(0x180, 0u);
    image.Record(0x1c0, TriangleNode | 0u, 0x40000000u | 0u);
    image.Record(0x1c8, TriangleNode | 1u, 0x80000000u | 0x40000000u | 1u);
    image.Record(0x1d0, TriangleNode | 1u, 0x40000000u | 1u);
    image.Record(0x1d8, TriangleNode | 3u, 0x80000000u | 3u);
    auto result = DecodeBottom(image.bytes);
    Expect(result.bottom.has_value() && result.bottom->records == 4u && result.bottom->opaque.size() == 2u && result.bottom->alphaTested.size() == 1u, "a triangle referenced from two leaves is kept once");
    image.Record(0x1d0, TriangleNode | 1u, 1u);
    result = DecodeBottom(image.bytes);
    Expect(!result.bottom && result.refusal == Refusal::DuplicateRecord, "a triangle referenced twice with different leaf info is refused");
}

void SkipsUnreachableChildren() {
    auto image = TwoTriangleBottom();
    image.Box32(0x100, {LeafPointer, 0x1eu, (0x1c8u / 8u) << 3u, None}, {Unit, Unit, Inverted, Unit});
    const auto result = DecodeBottom(image.bytes);
    Expect(result.bottom.has_value(), std::string("bottom with skipped children refused: ") + (result.bottom ? "" : RefusalName(result.refusal)));
    if (!result.bottom) return;
    Expect(result.bottom->invalidChildren == 1u, "a type 6 child, which the node test reports invalid, reaches nothing");
    Expect(result.bottom->emptyChildren == 1u && result.bottom->leaves == 1u, "a child whose box is inverted is never reached");
    image.Box32(0x100, {LeafPointer, None, None, None}, {std::array<float, 6>{0.0f, NAN, 0.0f, 1.0f, 1.0f, 1.0f}, Unit, Unit, Unit});
    const auto unordered = DecodeBottom(image.bytes);
    Expect(unordered.bottom.has_value() && unordered.bottom->emptyChildren == 1u && unordered.bottom->leaves == 0u, "a child whose box has a NaN bound is never reached");
}

void DecodesBox16() {
    auto image = TwoTriangleBottom();
    std::fill(image.bytes.begin() + 0x100, image.bytes.begin() + 0x180, std::byte{0});
    const std::uint32_t box16 = (0x100u / 8u) | 4u;
    image.Put(0x100, LeafPointer);
    image.Put(0x104, (0x1c8u / 8u) << 3u);
    image.Put(0x108, None);
    image.Put(0x10c, None);
    image.Put(0x110, 0x00000000u);
    image.Put(0x114, 0x3c000000u);
    image.Put(0x118, 0x3c003c00u);
    image.Put(0x11c, 0x3c003c00u);
    image.Put(0x120, 0x00003c00u);
    image.Put(0x124, 0x00000000u);
    const auto result = DecodeBottom(image.bytes, box16);
    Expect(result.bottom.has_value() && result.bottom->leaves == 1u && result.bottom->emptyChildren == 1u, "box16 children are read from packed half bounds");
}

void RefusesMalformedBottoms() {
    const auto check = [](Image image, Refusal expected, const std::string& what) {
        const auto result = DecodeBottom(image.bytes);
        Expect(!result.bottom && result.refusal == expected, what + " (got " + RefusalName(result.refusal) + ")");
    };
    auto magic = TwoTriangleBottom();
    magic.bytes[0] = std::byte{'X'};
    check(magic, Refusal::Magic, "a wrong magic is refused");
    auto format = TwoTriangleBottom();
    format.Put(0x08, 0x12345678u);
    check(format, Refusal::Header, "an unknown format word is refused");
    auto truncated = TwoTriangleBottom();
    truncated.bytes.resize(0x1c8);
    check(truncated, Refusal::Truncated, "a structure shorter than its header size is refused");
    auto records = TwoTriangleBottom();
    records.Put(0x60, 0x1d0u / 8u + 1u);
    check(records, Refusal::Header, "a record end that disagrees with the record count is refused");
    auto node = TwoTriangleBottom();
    node.Record(0x1c0, (0x1c0u / 8u) | 0u, 0x40000000u);
    check(node, Refusal::NodeRange, "a triangle node past the node region is refused");
    auto leaf = TwoTriangleBottom();
    leaf.Record(0x1c8, TriangleNode | 2u, 9u);
    check(leaf, Refusal::LeafRange, "a leaf list that runs past the record region is refused");
    auto type = TwoTriangleBottom();
    type.Record(0x1c0, TriangleNode | 4u, 0x40000000u);
    check(type, Refusal::RecordType, "a leaf record that is not a triangle is refused");
    auto leafType = TwoTriangleBottom();
    leafType.Put(0x100, LeafPointer | 1u);
    check(leafType, Refusal::NodeType, "a leaf pointer type that was never seen is refused");
    auto counted = TwoTriangleBottom();
    counted.Put(0x70, 1u);
    check(counted, Refusal::Header, "a bottom header with an instance count is refused");
    const auto asTop = DecodeTop(TwoTriangleBottom().bytes);
    Expect(!asTop.top && asTop.refusal == Refusal::Kind, "a bottom structure decoded as a top one is refused");
}

void RefusesDeepChains() {
    constexpr std::uint32_t depth = 70u;
    const std::uint32_t nodeEnd = 0x100u + depth * 128u;
    Image image(nodeEnd + 8u);
    image.Header(nodeEnd + 8u, nodeEnd);
    image.Records(nodeEnd + 8u);
    for (std::uint32_t level = 0; level < depth; ++level) {
        const auto offset = 0x100u + level * 128u;
        const auto next = level + 1u < depth ? ((offset + 128u) / 8u) | 5u : None;
        image.Box32(offset, {next, None, None, None}, {Unit, Unit, Unit, Unit});
    }
    const auto result = DecodeBottom(image.bytes);
    Expect(!result.bottom && result.refusal == Refusal::Depth, "a chain deeper than the walk limit is refused");
}

Image TwoInstanceTop() {
    constexpr std::uint32_t nodeEnd = 0x180u;
    constexpr std::uint32_t total = nodeEnd + 3u * 128u;
    Image image(total);
    image.Header(total, nodeEnd);
    image.Put(0x30, nodeEnd);
    image.Put(0x70, 3u);
    image.Box32(0x100, {(0u << 3u), (2u << 3u), None, None}, {Unit, Unit, Unit, Unit});
    for (std::uint32_t index = 0; index < 3u; ++index) {
        const auto offset = nodeEnd + index * 128u;
        for (std::uint32_t i = 0; i < 12u; ++i) {
            image.PutFloat(offset + i * 4u, static_cast<float>(index * 100u + i));
            image.PutFloat(offset + (16u + i) * 4u, -static_cast<float>(index * 100u + i));
        }
        const std::uint64_t packed = (std::uint64_t{0x81u} << 54u) | ((0x2a0000000ull + index * 0x1000u) >> 3u);
        image.Put(offset + 48u, static_cast<std::uint32_t>(packed));
        image.Put(offset + 52u, static_cast<std::uint32_t>(packed >> 32u));
        image.Put(offset + 56u, 37u);
        image.Put(offset + 60u, 0x3f000000u);
        image.Put(offset + 112u, index);
        image.Put(offset + 116u, 0x4fc000u | index);
        image.Put(offset + 120u, 40u + index);
    }
    return image;
}

void DecodesInstances() {
    auto image = TwoInstanceTop();
    const auto result = DecodeTop(image.bytes);
    Expect(result.top.has_value(), std::string("two-instance top refused: ") + RefusalName(result.refusal));
    if (!result.top) return;
    Expect(result.top->instances.size() == 2u, "only reachable instances are listed");
    if (result.top->instances.size() != 2u) return;
    const auto& second = result.top->instances[1];
    Expect(second.index == 2u && second.bottomAddress == 0x2a0002000ull && second.bottomFlags == 0x81u && second.bottomRoot == 37u, "bottom address, flags and root");
    Expect(second.worldToObject[3] == 203.0f && second.objectToWorld[11] == -211.0f, "both transforms");
    Expect(second.maskAndFlags == 0x3f000000u && second.instanceIndex == 2u && second.word29 == (0x4fc000u | 2u) && second.customIndex == 42u, "mask, index and custom index words");
    image.Put(0x100, 3u << 3u);
    const auto past = DecodeTop(image.bytes);
    Expect(!past.top && past.refusal == Refusal::InstanceRange, "an instance leaf past the instance array is refused");
    image = TwoInstanceTop();
    image.Put(0x180 + 56u, 36u);
    const auto root = DecodeTop(image.bytes);
    Expect(!root.top && root.refusal == Refusal::InstanceRange, "an instance whose bottom root is not a box32 node is refused");
    image = TwoInstanceTop();
    image.Put(0x70, 4u);
    const auto size = DecodeTop(image.bytes);
    Expect(!size.top && size.refusal == Refusal::Header, "an instance count that disagrees with the structure size is refused");
    const auto asBottom = DecodeBottom(TwoInstanceTop().bytes);
    Expect(!asBottom.bottom && asBottom.refusal == Refusal::Kind, "a top structure decoded as a bottom one is refused");
}

}

int main() {
    DecodesTriangles();
    DecodesEveryTriangleType();
    SharesDuplicateRecords();
    SkipsUnreachableChildren();
    DecodesBox16();
    RefusesMalformedBottoms();
    RefusesDeepChains();
    DecodesInstances();
    if (failures != 0) return 1;
    std::puts("guest BVH decoder tests passed");
    return 0;
}

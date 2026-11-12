#include "prx/libSceFreeTypeOt/include/Trigonometry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>

namespace {

using FreeType::GuestVector;

constexpr std::int64_t AnglePi = std::int64_t{180} << 16;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceFreeTypeOt trigonometry: %s\n", message);
    std::exit(1);
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

bool Same(const GuestVector& vector, std::int64_t x, std::int64_t y) {
    return vector.x == x && vector.y == y;
}

GuestVector Rotate(GuestVector vector, std::int64_t angle) {
    sceFreeTypeOtVectorRotate_nid_no_patch(&vector, angle);
    return vector;
}

GuestVector FromPolar(std::int64_t length, std::int64_t angle) {
    GuestVector vector{-7, -7};
    sceFreeTypeOtVectorFromPolar_nid_no_patch(&vector, length, angle);
    return vector;
}

bool Polarize(GuestVector vector, std::int64_t length, std::int64_t angle) {
    std::int64_t outLength = -7;
    std::int64_t outAngle = -7;
    sceFreeTypeOtVectorPolarize_nid_no_patch(&vector, &outLength, &outAngle);
    return outLength == length && outAngle == angle;
}

std::int64_t Length(GuestVector vector) {
    return sceFreeTypeOtVectorLength_nid_no_patch(&vector);
}

void VectorLength() {
    Require(Length({3, 4}) == 5 && Length({-3, -4}) == 5 && Length({1, 1}) == 1, "small lengths");
    Require(Length({1000, -2500}) == 2693, "the length rounds the CORDIC magnitude");
    Require(Length({0, -std::numeric_limits<std::int64_t>::max()}) == std::numeric_limits<std::int64_t>::max(), "a vertical vector returns |y| in full");
    Require(Length({-std::numeric_limits<std::int64_t>::max(), 0}) == std::numeric_limits<std::int64_t>::max(), "a horizontal vector returns |x| in full");
    Require(Length({0, 0}) == 0, "the zero vector has length 0");
    Require(Throws([] { Length({0, std::numeric_limits<std::int64_t>::min()}); }), "an axis value whose magnitude overflows throws");
    Require(Throws([] { Length({300000000, 400000000}); }), "a length the two FT_LONG64 builds round differently throws");
    Require(Throws([] { Length({761958, -938825}); }), "a font-range length the builds disagree on throws");
    Require(Throws([] { Length({0x100000000, 1}); }), "a component the 2.4.11 normalisation cannot resolve throws");
    Require(Throws([] { sceFreeTypeOtVectorLength_nid_no_patch(nullptr); }), "a null vector throws");
}

void VectorPolarize() {
    Require(Polarize({0, 65536}, 65536, 5898240), "the positive y axis is 90 degrees");
    Require(Polarize({-65536, 0}, 65536, 11796480), "the negative x axis is 180 degrees");
    Require(Polarize({3, -4}, 5, -3481920), "a fourth-quadrant vector has a negative angle");
    Require(Polarize({-1000, -2500}, 2692, -7327008), "the polar length is truncated, not rounded");
    Require(Polarize({-5, 7}, 8, 8227232), "a second-quadrant vector");
    Require(Polarize({1, 0}, 1, 0), "the unit x vector");
    std::int64_t length = -7;
    std::int64_t angle = -7;
    GuestVector zero{0, 0};
    sceFreeTypeOtVectorPolarize_nid_no_patch(&zero, &length, &angle);
    Require(length == -7 && angle == -7, "the zero vector leaves both outputs");
    sceFreeTypeOtVectorPolarize_nid_no_patch(&zero, nullptr, nullptr);
    GuestVector vector{3, 4};
    Require(Throws([&] { sceFreeTypeOtVectorPolarize_nid_no_patch(&vector, nullptr, &angle); }), "a null length throws");
    Require(Throws([&] { sceFreeTypeOtVectorPolarize_nid_no_patch(&vector, &length, nullptr); }), "a null angle throws");
    Require(Throws([&] { sceFreeTypeOtVectorPolarize_nid_no_patch(nullptr, &length, &angle); }), "a null vector throws");
    Require(Throws([] { Polarize({690903, -468124}, 0, 0); }), "a polar length the builds disagree on throws");
}

void VectorRotate() {
    Require(Same(Rotate({65536, 0}, 5898240), 0, 65536) && Same(Rotate({65536, 0}, -5898240), 0, -65536), "quarter turns");
    Require(Same(Rotate({65536, 0}, 11796480), -65536, 0) && Same(Rotate({65536, 0}, -11796480), -65536, 0), "half turns");
    Require(Same(Rotate({640, -1280}, 1234567), 1019, -1005), "a positive angle");
    Require(Same(Rotate({-640, 1280}, -7654321), 1431, -6), "a negative angle beyond a quarter turn");
    Require(Same(Rotate({1, 1}, 2949120), 0, 1), "an eighth turn of a tiny vector");
    Require(Same(Rotate({100000, 200000}, -100), 100005, 199997), "a small angle changes a large vector");
    Require(Same(Rotate({123, 456}, 0), 123, 456) && Same(Rotate({0, 0}, 1234567), 0, 0), "a zero angle or vector is left as is");
    const std::int64_t turns = (std::numeric_limits<std::int64_t>::max() - 2 * AnglePi) / (2 * AnglePi);
    Require(Same(Rotate({640, -1280}, 1234567 + turns * 2 * AnglePi), 1019, -1005), "full turns are reduced without iterating");
    Require(Same(Rotate({640, -1280}, 1234567 - turns * 2 * AnglePi), 1019, -1005), "negative full turns are reduced");
    Require(Same(Rotate({640, -1280}, 1234567 + AnglePi), -1019, 1005), "a half turn negates the vector");
    Require(Same(Rotate({640, -1280}, 1234567 - (2 * turns + 1) * AnglePi), -1019, 1005), "an odd number of large half turns negates the vector");
    Require(Throws([] { Rotate({300000000, 400000000}, 3000000); }), "a rotation the builds disagree on throws");
    Require(Throws([] { Rotate({-106526, 266498}, 85432276); }), "a font-range rotation the builds disagree on throws");
    Require(Throws([] { Rotate({0x100000000, 0}, 1); }), "a component above 32 bits throws");
    GuestVector vector{-106526, 266498};
    Require(Throws([&] { sceFreeTypeOtVectorRotate_nid_no_patch(&vector, 85432276); }) && Same(vector, -106526, 266498), "a throwing rotation leaves the vector");
    Require(Throws([] { sceFreeTypeOtVectorRotate_nid_no_patch(nullptr, 0); }), "a null vector throws");
}

void VectorFromPolar() {
    Require(Same(FromPolar(65536, 2949120), 46341, 46341), "an eighth turn");
    Require(Same(FromPolar(-1000, -9000000), 735, 678), "a negative length and angle");
    Require(Same(FromPolar(7, 23592960), 7, 0), "a full turn");
    Require(Same(FromPolar(0x100000000, 0), 0x100000000, 0) && Same(FromPolar(0, 1234567), 0, 0), "a zero angle or length writes (length, 0)");
    Require(Same(FromPolar(-std::numeric_limits<std::int64_t>::max(), 0), -std::numeric_limits<std::int64_t>::max(), 0), "a zero angle keeps any length");
    Require(Throws([] { FromPolar(0x100000000, 1); }), "a length above 32 bits throws");
    Require(Throws([] { sceFreeTypeOtVectorFromPolar_nid_no_patch(nullptr, 1, 1); }), "a null vector throws");
}

}

int main() {
    VectorLength();
    VectorPolarize();
    VectorRotate();
    VectorFromPolar();
    std::puts("libSceFreeTypeOt: the vector calls match FreeType 2.4.11");
    return 0;
}

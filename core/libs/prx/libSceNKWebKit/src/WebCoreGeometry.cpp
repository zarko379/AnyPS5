#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace WebCore {

namespace {

int ClampToInteger(float value) {
    if (value >= static_cast<float>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<float>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

int TruncateToIntegerOrMin(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

int ClampToInteger(double value) {
    if (value >= static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<double>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

bool AreEssentiallyEqual(float u, float v) {
    if (u == v) {
        return true;
    }
    float delta = std::abs(u - v);
    float eps = std::numeric_limits<float>::epsilon();
    auto safeDiv = [](float num, float den) {
        if (std::abs(num) < std::abs(den) * std::numeric_limits<float>::max()) {
            return num / den;
        }
        return std::numeric_limits<float>::max();
    };
    return safeDiv(delta, std::abs(u)) <= eps && safeDiv(delta, std::abs(v)) <= eps;
}

bool IsPointInTriangle(const FloatPoint& p, const FloatPoint& t1, const FloatPoint& t2, const FloatPoint& t3) {
    float v0x = t3.m_x - t1.m_x;
    float v0y = t3.m_y - t1.m_y;
    float v1x = t2.m_x - t1.m_x;
    float v1y = t2.m_y - t1.m_y;
    float v2x = p.m_x - t1.m_x;
    float v2y = p.m_y - t1.m_y;

    float dot00 = v0x * v0x + v0y * v0y;
    float dot01 = v0x * v1x + v0y * v1y;
    float dot02 = v0x * v2x + v0y * v2y;
    float dot11 = v1x * v1x + v1y * v1y;
    float dot12 = v1x * v2x + v1y * v2y;

    float denom = dot00 * dot11 - dot01 * dot01;
    if (denom == 0.0f) {
        return false;
    }
    float invDenom = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

bool EllipseContainsPoint(const FloatPoint& center, float rx, float ry, const FloatPoint& point) {
    if (rx <= 0.0f || ry <= 0.0f) {
        return false;
    }
    FloatPoint transformed{point.m_x - center.m_x, point.m_y - center.m_y};
    if (transformed.m_x < -rx || transformed.m_x > rx || transformed.m_y < -ry || transformed.m_y > ry) {
        return false;
    }
    transformed.m_x *= ry;
    transformed.m_y *= rx;
    float radius = rx * ry;
    if (std::abs(transformed.m_x) + std::abs(transformed.m_y) <= radius) {
        return true;
    }
    return (transformed.m_x * transformed.m_x + transformed.m_y * transformed.m_y) <= (radius * radius);
}

bool IsRoundedRectRenderable(const RoundedRect& rr) {
    int w = rr.m_rect.m_size.m_width.m_value;
    int h = rr.m_rect.m_size.m_height.m_value;
    int tlw = rr.m_radii.m_topLeft.m_width.m_value;
    int tlh = rr.m_radii.m_topLeft.m_height.m_value;
    int trw = rr.m_radii.m_topRight.m_width.m_value;
    int trh = rr.m_radii.m_topRight.m_height.m_value;
    int blw = rr.m_radii.m_bottomLeft.m_width.m_value;
    int blh = rr.m_radii.m_bottomLeft.m_height.m_value;
    int brw = rr.m_radii.m_bottomRight.m_width.m_value;
    int brh = rr.m_radii.m_bottomRight.m_height.m_value;
    auto saturatedSum = [](int a, int b) {
        return static_cast<int>(std::min<std::int64_t>(static_cast<std::int64_t>(a) + b, std::numeric_limits<int>::max()));
    };
    return tlw >= 0 && tlh >= 0 && blw >= 0 && blh >= 0
        && trw >= 0 && trh >= 0 && brw >= 0 && brh >= 0
        && saturatedSum(tlw, trw) <= w && saturatedSum(blw, brw) <= w
        && saturatedSum(tlh, blh) <= h && saturatedSum(trh, brh) <= h;
}

constexpr int kFixedPointDenominator = 64;
constexpr int kIntMaxForLayoutUnit = std::numeric_limits<int>::max() / kFixedPointDenominator;
constexpr int kIntMinForLayoutUnit = std::numeric_limits<int>::min() / kFixedPointDenominator;

int WrappingAdd(int a, int b) {
    return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

int WrappingSub(int a, int b) {
    return static_cast<int>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

int SaturatedAdd(int a, int b) {
    std::uint32_t ua = static_cast<std::uint32_t>(a);
    std::uint32_t ub = static_cast<std::uint32_t>(b);
    std::uint32_t result = ua + ub;
    if (!((ua ^ ub) >> 31) && ((result ^ ua) >> 31)) {
        return static_cast<int>(static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + (ua >> 31));
    }
    return static_cast<int>(result);
}

int SaturatedSub(int a, int b) {
    std::uint32_t ua = static_cast<std::uint32_t>(a);
    std::uint32_t ub = static_cast<std::uint32_t>(b);
    std::uint32_t result = ua - ub;
    if (((ua ^ ub) >> 31) && ((result ^ ua) >> 31)) {
        return static_cast<int>(static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + (ua >> 31));
    }
    return static_cast<int>(result);
}

LayoutUnit LayoutFromInt(int value) {
    if (value > kIntMaxForLayoutUnit) {
        return LayoutUnit{std::numeric_limits<int>::max()};
    }
    if (value < kIntMinForLayoutUnit) {
        return LayoutUnit{std::numeric_limits<int>::min()};
    }
    return LayoutUnit{value * kFixedPointDenominator};
}

LayoutUnit LayoutFromFloat(float value) {
    return LayoutUnit{ClampToInteger(value * kFixedPointDenominator)};
}

LayoutUnit LayoutFromFloatFloor(float value) {
    return LayoutUnit{ClampToInteger(std::floor(value * kFixedPointDenominator))};
}

LayoutUnit LayoutFromFloatCeil(float value) {
    return LayoutUnit{ClampToInteger(std::ceil(value * kFixedPointDenominator))};
}

float LayoutToFloat(LayoutUnit value) {
    return static_cast<float>(value.m_value) / kFixedPointDenominator;
}

int LayoutFloor(LayoutUnit value) {
    if (value.m_value <= std::numeric_limits<int>::min() + kFixedPointDenominator - 1) {
        return kIntMinForLayoutUnit;
    }
    if (value.m_value >= 0) {
        return value.m_value / kFixedPointDenominator;
    }
    return (value.m_value - kFixedPointDenominator + 1) / kFixedPointDenominator;
}

int LayoutCeil(LayoutUnit value) {
    if (value.m_value >= std::numeric_limits<int>::max() - kFixedPointDenominator + 1) {
        return kIntMaxForLayoutUnit;
    }
    if (value.m_value >= 0) {
        return (value.m_value + kFixedPointDenominator - 1) / kFixedPointDenominator;
    }
    return value.m_value / kFixedPointDenominator;
}

LayoutUnit LayoutAdd(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{SaturatedAdd(a.m_value, b.m_value)};
}

LayoutUnit LayoutSub(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{SaturatedSub(a.m_value, b.m_value)};
}

LayoutUnit LayoutNegate(LayoutUnit a) {
    return LayoutUnit{WrappingSub(0, a.m_value)};
}

LayoutUnit LayoutMin(LayoutUnit a, LayoutUnit b) {
    return b.m_value < a.m_value ? b : a;
}

LayoutUnit LayoutMax(LayoutUnit a, LayoutUnit b) {
    return a.m_value < b.m_value ? b : a;
}

void LayoutRectIntersect(LayoutRect& rect, const LayoutRect& other) {
    LayoutPoint newLocation{LayoutMax(rect.m_location.m_x, other.m_location.m_x), LayoutMax(rect.m_location.m_y, other.m_location.m_y)};
    LayoutPoint newMaxPoint{
        LayoutMin(LayoutAdd(rect.m_location.m_x, rect.m_size.m_width), LayoutAdd(other.m_location.m_x, other.m_size.m_width)),
        LayoutMin(LayoutAdd(rect.m_location.m_y, rect.m_size.m_height), LayoutAdd(other.m_location.m_y, other.m_size.m_height))};
    if (newLocation.m_x.m_value >= newMaxPoint.m_x.m_value || newLocation.m_y.m_value >= newMaxPoint.m_y.m_value) {
        newLocation = LayoutPoint{};
        newMaxPoint = LayoutPoint{};
    }
    rect.m_location = newLocation;
    rect.m_size = LayoutSize{LayoutSub(newMaxPoint.m_x, newLocation.m_x), LayoutSub(newMaxPoint.m_y, newLocation.m_y)};
}

float Min3(float v1, float v2, float v3) {
    return std::min(std::min(v1, v2), v3);
}

float Max3(float v1, float v2, float v3) {
    return std::max(std::max(v1, v2), v3);
}

float Min4(float v1, float v2, float v3, float v4) {
    return std::min(std::min(v1, v2), std::min(v3, v4));
}

float Max4(float v1, float v2, float v3, float v4) {
    return std::max(std::max(v1, v2), std::max(v3, v4));
}

void SetLocationAndSizeFromEdges(FloatRect& rect, float left, float top, float right, float bottom) {
    rect.m_location = FloatPoint{left, top};
    rect.m_size = FloatSize{right - left, bottom - top};
}

FloatRect QuadBoundingBox(const FloatQuad& quad) {
    float left = Min4(quad.m_p1.m_x, quad.m_p2.m_x, quad.m_p3.m_x, quad.m_p4.m_x);
    float top = Min4(quad.m_p1.m_y, quad.m_p2.m_y, quad.m_p3.m_y, quad.m_p4.m_y);
    float right = Max4(quad.m_p1.m_x, quad.m_p2.m_x, quad.m_p3.m_x, quad.m_p4.m_x);
    float bottom = Max4(quad.m_p1.m_y, quad.m_p2.m_y, quad.m_p3.m_y, quad.m_p4.m_y);
    return FloatRect{FloatPoint{left, top}, FloatSize{right - left, bottom - top}};
}

void UniteEvenIfEmpty(FloatRect& rect, const FloatRect& other) {
    float minX = std::min(rect.m_location.m_x, other.m_location.m_x);
    float minY = std::min(rect.m_location.m_y, other.m_location.m_y);
    float maxX = std::max(rect.m_location.m_x + rect.m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    float maxY = std::max(rect.m_location.m_y + rect.m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    SetLocationAndSizeFromEdges(rect, minX, minY, maxX, maxY);
}

template <typename T>
const T* VectorElements(const NKWebKit::WtfAbi::Vector<T>& vector) {
    if (!vector.buffer && vector.size) {
        throw std::invalid_argument("WTF::Vector with a null buffer and a nonzero size");
    }
    if (vector.size > vector.capacity) {
        throw std::invalid_argument("WTF::Vector size exceeds its capacity");
    }
    return vector.buffer;
}

IntRect RectForPoint(const LayoutPoint& point, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding) {
    int x = WrappingSub(LayoutFloor(point.m_x), static_cast<int>(leftPadding));
    int y = WrappingSub(LayoutFloor(point.m_y), static_cast<int>(topPadding));
    int width = static_cast<int>(leftPadding + rightPadding + 1u);
    int height = static_cast<int>(topPadding + bottomPadding + 1u);
    return IntRect{IntPoint{x, y}, IntSize{width, height}};
}

FloatQuad QuadFromIntRect(const IntRect& rect) {
    float x = static_cast<float>(rect.m_location.m_x);
    float y = static_cast<float>(rect.m_location.m_y);
    float maxX = x + static_cast<float>(rect.m_size.m_width);
    float maxY = y + static_cast<float>(rect.m_size.m_height);
    return FloatQuad{FloatPoint{x, y}, FloatPoint{maxX, y}, FloatPoint{maxX, maxY}, FloatPoint{x, maxY}};
}

void ConstructHitTestLocation(HitTestLocation* self, const LayoutPoint& point) {
    self->m_point = point;
    self->m_boundingBox = RectForPoint(point, 0, 0, 0, 0);
    self->m_transformedPoint = FloatPoint{LayoutToFloat(point.m_x), LayoutToFloat(point.m_y)};
    self->m_transformedRect = QuadFromIntRect(self->m_boundingBox);
    self->m_isRectBased = false;
    self->m_isRectilinear = true;
}

enum class LengthKind : std::uint8_t {
    Auto,
    Relative,
    Percent,
    Fixed,
    Intrinsic,
    MinIntrinsic,
    MinContent,
    MaxContent,
    FillAvailable,
    FitContent,
    Calculated,
    Undefined
};

LengthKind CheckedLengthKind(const LengthAbi& length) {
    if (length.m_type == static_cast<std::uint8_t>(LengthKind::Calculated)) {
        throw std::invalid_argument("Calculated Length needs the CalculationValue map");
    }
    if (length.m_type > static_cast<std::uint8_t>(LengthKind::Undefined)) {
        throw std::invalid_argument("Length type out of range");
    }
    return static_cast<LengthKind>(length.m_type);
}

float LengthValue(const LengthAbi& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

}  // namespace
}  // namespace WebCore

extern "C" {
bool APS5_VABI _ZN7WebCore16findIntersectionERKNS_10FloatPointES2_S2_S2_RS0_(const WebCore::FloatPoint* p1, const WebCore::FloatPoint* p2, const WebCore::FloatPoint* d1, const WebCore::FloatPoint* d2, WebCore::FloatPoint* intersection) {
    if (!p1 || !p2 || !d1 || !d2 || !intersection) {
        throw std::invalid_argument("findIntersection: null point");
    }
    float pxLength = p2->m_x - p1->m_x;
    float pyLength = p2->m_y - p1->m_y;
    float dxLength = d2->m_x - d1->m_x;
    float dyLength = d2->m_y - d1->m_y;
    float denom = pxLength * dyLength - pyLength * dxLength;
    if (!denom) {
        return false;
    }
    float param = ((d1->m_x - p1->m_x) * dyLength - (d1->m_y - p1->m_y) * dxLength) / denom;
    intersection->m_x = p1->m_x + param * pxLength;
    intersection->m_y = p1->m_y + param * pyLength;
    return true;
}

WebCore::FloatPoint APS5_VABI _ZN7WebCore10FloatPoint15narrowPrecisionEdd(double x, double y) {
    return WebCore::FloatPoint{static_cast<float>(x), static_cast<float>(y)};
}

void APS5_VABI _ZN7WebCore10FloatPoint9normalizeEv(WebCore::FloatPoint* self) {
    float length = std::hypot(self->m_x, self->m_y);
    if (length != 0.0f) {
        self->m_x /= length;
        self->m_y /= length;
    }
}

void APS5_VABI _ZN7WebCore10FloatPointC1ERKNS_8IntPointE(WebCore::FloatPoint* self, const WebCore::IntPoint& p) {
    self->m_x = static_cast<float>(p.m_x);
    self->m_y = static_cast<float>(p.m_y);
}

void APS5_VABI _ZN7WebCore10FloatPointC2ERKNS_8IntPointE(WebCore::FloatPoint* self, const WebCore::IntPoint& p) {
    self->m_x = static_cast<float>(p.m_x);
    self->m_y = static_cast<float>(p.m_y);
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE(
    const WebCore::FloatPoint* self, const WebCore::AffineTransform& t) {
    double x = static_cast<double>(self->m_x);
    double y = static_cast<double>(self->m_y);
    double newX = t.m_transform[0] * x + t.m_transform[2] * y + t.m_transform[4];
    double newY = t.m_transform[1] * x + t.m_transform[3] * y + t.m_transform[5];
    return WebCore::FloatPoint{static_cast<float>(newX), static_cast<float>(newY)};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE(
    const WebCore::FloatPoint* self, const WebCore::TransformationMatrix& t) {
    double x = static_cast<double>(self->m_x);
    double y = static_cast<double>(self->m_y);
    double resultX = t.m_matrix[3][0] + x * t.m_matrix[0][0] + y * t.m_matrix[1][0];
    double resultY = t.m_matrix[3][1] + x * t.m_matrix[0][1] + y * t.m_matrix[1][1];
    double w = t.m_matrix[3][3] + x * t.m_matrix[0][3] + y * t.m_matrix[1][3];
    if (w != 1.0 && w != 0.0) {
        resultX /= w;
        resultY /= w;
    }
    return WebCore::FloatPoint{static_cast<float>(resultX), static_cast<float>(resultY)};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_(
    const WebCore::FloatPoint* self, const WebCore::FloatPoint& min, const WebCore::FloatPoint& max) {
    return WebCore::FloatPoint{
        std::max(min.m_x, std::min(max.m_x, self->m_x)),
        std::max(min.m_y, std::min(max.m_y, self->m_y))
    };
}

WebCore::FloatSize APS5_VABI _ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_(
    const WebCore::FloatSize* self, const WebCore::FloatSize& min, const WebCore::FloatSize& max) {
    return WebCore::FloatSize{
        std::max(min.m_width, std::min(max.m_width, self->m_width)),
        std::max(min.m_height, std::min(max.m_height, self->m_height))
    };
}

bool APS5_VABI _ZNK7WebCore9FloatSize6isZeroEv(const WebCore::FloatSize* self) {
    return self->m_width == 0.0f && self->m_height == 0.0f;
}

void APS5_VABI _ZN7WebCore9FloatRect5scaleEff(WebCore::FloatRect* self, float sx, float sy) {
    self->m_location.m_x *= sx;
    self->m_location.m_y *= sy;
    self->m_size.m_width *= sx;
    self->m_size.m_height *= sy;
}

void APS5_VABI _ZN7WebCore9FloatRect5uniteERKS0_(WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    if (other.m_size.m_width <= 0.0f || other.m_size.m_height <= 0.0f) {
        return;
    }
    if (self->m_size.m_width <= 0.0f || self->m_size.m_height <= 0.0f) {
        *self = other;
        return;
    }
    float minX = std::min(self->m_location.m_x, other.m_location.m_x);
    float minY = std::min(self->m_location.m_y, other.m_location.m_y);
    float maxX = std::max(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    float maxY = std::max(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    self->m_location = {minX, minY};
    self->m_size = {maxX - minX, maxY - minY};
}

void APS5_VABI _ZN7WebCore9FloatRect6extendERKNS_10FloatPointE(WebCore::FloatRect* self, const WebCore::FloatPoint& p) {
    float minX = std::min(self->m_location.m_x, p.m_x);
    float minY = std::min(self->m_location.m_y, p.m_y);
    float maxX = std::max(self->m_location.m_x + self->m_size.m_width, p.m_x);
    float maxY = std::max(self->m_location.m_y + self->m_size.m_height, p.m_y);
    self->m_location = {minX, minY};
    self->m_size = {maxX - minX, maxY - minY};
}

void APS5_VABI _ZN7WebCore9FloatRect9intersectERKS0_(WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    float l = std::max(self->m_location.m_x, other.m_location.m_x);
    float t = std::max(self->m_location.m_y, other.m_location.m_y);
    float r = std::min(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    float b = std::min(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    if (l >= r || t >= b) {
        self->m_location = {0.0f, 0.0f};
        self->m_size = {0.0f, 0.0f};
    } else {
        self->m_location = {l, t};
        self->m_size = {r - l, b - t};
    }
}

void APS5_VABI _ZN7WebCore9FloatRectC1ERKNS_7IntRectE(WebCore::FloatRect* self, const WebCore::IntRect& r) {
    self->m_location.m_x = static_cast<float>(r.m_location.m_x);
    self->m_location.m_y = static_cast<float>(r.m_location.m_y);
    self->m_size.m_width = static_cast<float>(r.m_size.m_width);
    self->m_size.m_height = static_cast<float>(r.m_size.m_height);
}

void APS5_VABI _ZN7WebCore9FloatRectC2ERKNS_7IntRectE(WebCore::FloatRect* self, const WebCore::IntRect& r) {
    self->m_location.m_x = static_cast<float>(r.m_location.m_x);
    self->m_location.m_y = static_cast<float>(r.m_location.m_y);
    self->m_size.m_width = static_cast<float>(r.m_size.m_width);
    self->m_size.m_height = static_cast<float>(r.m_size.m_height);
}

bool APS5_VABI _ZNK7WebCore9FloatRect10intersectsERKS0_(const WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    return self->m_size.m_width > 0.0f && self->m_size.m_height > 0.0f
        && other.m_size.m_width > 0.0f && other.m_size.m_height > 0.0f
        && self->m_location.m_x < other.m_location.m_x + other.m_size.m_width
        && other.m_location.m_x < self->m_location.m_x + self->m_size.m_width
        && self->m_location.m_y < other.m_location.m_y + other.m_size.m_height
        && other.m_location.m_y < self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_(const WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    return self->m_size.m_width >= 0.0f && self->m_size.m_height >= 0.0f
        && other.m_size.m_width >= 0.0f && other.m_size.m_height >= 0.0f
        && self->m_location.m_x <= other.m_location.m_x + other.m_size.m_width
        && other.m_location.m_x <= self->m_location.m_x + self->m_size.m_width
        && self->m_location.m_y <= other.m_location.m_y + other.m_size.m_height
        && other.m_location.m_y <= self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(
    const WebCore::FloatRect* self, const WebCore::FloatPoint& point, std::uint32_t containsMode) {
    if (containsMode == 0) {
        return point.m_x >= self->m_location.m_x
            && point.m_x <= self->m_location.m_x + self->m_size.m_width
            && point.m_y >= self->m_location.m_y
            && point.m_y <= self->m_location.m_y + self->m_size.m_height;
    }
    return point.m_x > self->m_location.m_x
        && point.m_x < self->m_location.m_x + self->m_size.m_width
        && point.m_y > self->m_location.m_y
        && point.m_y < self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore9FloatRect8containsERKS0_(const WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    return self->m_location.m_x <= other.m_location.m_x
        && self->m_location.m_x + self->m_size.m_width >= other.m_location.m_x + other.m_size.m_width
        && self->m_location.m_y <= other.m_location.m_y
        && self->m_location.m_y + self->m_size.m_height >= other.m_location.m_y + other.m_size.m_height;
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore9FloatQuad11boundingBoxEv(const WebCore::FloatQuad* self) {
    float left = std::min({self->m_p1.m_x, self->m_p2.m_x, self->m_p3.m_x, self->m_p4.m_x});
    float top = std::min({self->m_p1.m_y, self->m_p2.m_y, self->m_p3.m_y, self->m_p4.m_y});
    float right = std::max({self->m_p1.m_x, self->m_p2.m_x, self->m_p3.m_x, self->m_p4.m_x});
    float bottom = std::max({self->m_p1.m_y, self->m_p2.m_y, self->m_p3.m_y, self->m_p4.m_y});
    return WebCore::FloatRect{{left, top}, {right - left, bottom - top}};
}

bool APS5_VABI _ZNK7WebCore9FloatQuad12containsQuadERKS0_(const WebCore::FloatQuad* self, const WebCore::FloatQuad& other) {
    auto containsPoint = [self](const WebCore::FloatPoint& p) {
        return WebCore::IsPointInTriangle(p, self->m_p1, self->m_p2, self->m_p3)
            || WebCore::IsPointInTriangle(p, self->m_p1, self->m_p3, self->m_p4);
    };
    return containsPoint(other.m_p1) && containsPoint(other.m_p2) && containsPoint(other.m_p3) && containsPoint(other.m_p4);
}

bool APS5_VABI _ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(const WebCore::FloatQuad* self, const WebCore::FloatPoint& point) {
    return WebCore::IsPointInTriangle(point, self->m_p1, self->m_p2, self->m_p3)
        || WebCore::IsPointInTriangle(point, self->m_p1, self->m_p3, self->m_p4);
}

bool APS5_VABI _ZNK7WebCore9FloatQuad13isRectilinearEv(const WebCore::FloatQuad* self) {
    return (WebCore::AreEssentiallyEqual(self->m_p1.m_x, self->m_p2.m_x)
            && WebCore::AreEssentiallyEqual(self->m_p2.m_y, self->m_p3.m_y)
            && WebCore::AreEssentiallyEqual(self->m_p3.m_x, self->m_p4.m_x)
            && WebCore::AreEssentiallyEqual(self->m_p4.m_y, self->m_p1.m_y))
        || (WebCore::AreEssentiallyEqual(self->m_p1.m_y, self->m_p2.m_y)
            && WebCore::AreEssentiallyEqual(self->m_p2.m_x, self->m_p3.m_x)
            && WebCore::AreEssentiallyEqual(self->m_p3.m_y, self->m_p4.m_y)
            && WebCore::AreEssentiallyEqual(self->m_p4.m_x, self->m_p1.m_x));
}

void APS5_VABI _ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(
    WebCore::FloatRoundedRect* self, const WebCore::FloatRect& rect, const WebCore::FloatRoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

void APS5_VABI _ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(
    WebCore::FloatRoundedRect* self, const WebCore::FloatRect& rect, const WebCore::FloatRoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

void APS5_VABI _ZN7WebCore8IntPointC1ERKNS_10FloatPointE(WebCore::IntPoint* self, const WebCore::FloatPoint& p) {
    self->m_x = WebCore::ClampToInteger(p.m_x);
    self->m_y = WebCore::ClampToInteger(p.m_y);
}

void APS5_VABI _ZN7WebCore8IntPointC2ERKNS_10FloatPointE(WebCore::IntPoint* self, const WebCore::FloatPoint& p) {
    self->m_x = WebCore::ClampToInteger(p.m_x);
    self->m_y = WebCore::ClampToInteger(p.m_y);
}

WebCore::IntPoint APS5_VABI _ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_(
    const WebCore::IntPoint* self, const WebCore::IntPoint& min, const WebCore::IntPoint& max) {
    return WebCore::IntPoint{
        std::max(min.m_x, std::min(max.m_x, self->m_x)),
        std::max(min.m_y, std::min(max.m_y, self->m_y))
    };
}

void APS5_VABI _ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(WebCore::IntSize* self, const WebCore::FloatSize& s) {
    self->m_width = WebCore::ClampToInteger(s.m_width);
    self->m_height = WebCore::ClampToInteger(s.m_height);
}

void APS5_VABI _ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE(WebCore::IntSize* self, const WebCore::FloatSize& s) {
    self->m_width = WebCore::ClampToInteger(s.m_width);
    self->m_height = WebCore::ClampToInteger(s.m_height);
}

WebCore::IntSize APS5_VABI _ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_(
    const WebCore::IntSize* self, const WebCore::IntSize& min, const WebCore::IntSize& max) {
    return WebCore::IntSize{
        std::max(min.m_width, std::min(max.m_width, self->m_width)),
        std::max(min.m_height, std::min(max.m_height, self->m_height))
    };
}

void APS5_VABI _ZN7WebCore7IntRect5scaleEf(WebCore::IntRect* self, float s) {
    self->m_location.m_x = WebCore::TruncateToIntegerOrMin(self->m_location.m_x * s);
    self->m_location.m_y = WebCore::TruncateToIntegerOrMin(self->m_location.m_y * s);
    self->m_size.m_width = WebCore::TruncateToIntegerOrMin(self->m_size.m_width * s);
    self->m_size.m_height = WebCore::TruncateToIntegerOrMin(self->m_size.m_height * s);
}

void APS5_VABI _ZN7WebCore7IntRect5uniteERKS0_(WebCore::IntRect* self, const WebCore::IntRect& other) {
    if (other.m_size.m_width <= 0 || other.m_size.m_height <= 0) {
        return;
    }
    if (self->m_size.m_width <= 0 || self->m_size.m_height <= 0) {
        *self = other;
        return;
    }
    int l = std::min(self->m_location.m_x, other.m_location.m_x);
    int t = std::min(self->m_location.m_y, other.m_location.m_y);
    int r = std::max(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    int b = std::max(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    self->m_location = {l, t};
    self->m_size = {r - l, b - t};
}

void APS5_VABI _ZN7WebCore7IntRect9intersectERKS0_(WebCore::IntRect* self, const WebCore::IntRect& other) {
    int l = std::max(self->m_location.m_x, other.m_location.m_x);
    int t = std::max(self->m_location.m_y, other.m_location.m_y);
    int r = std::min(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    int b = std::min(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    if (l >= r || t >= b) {
        self->m_location = {0, 0};
        self->m_size = {0, 0};
    } else {
        self->m_location = {l, t};
        self->m_size = {r - l, b - t};
    }
}

void APS5_VABI _ZN7WebCore7IntRectC1ERKNS_10LayoutRectE(WebCore::IntRect* self, const WebCore::LayoutRect& r) {
    self->m_location.m_x = r.m_location.m_x.m_value / 64;
    self->m_location.m_y = r.m_location.m_y.m_value / 64;
    self->m_size.m_width = r.m_size.m_width.m_value / 64;
    self->m_size.m_height = r.m_size.m_height.m_value / 64;
}

void APS5_VABI _ZN7WebCore7IntRectC1ERKNS_9FloatRectE(WebCore::IntRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x = WebCore::ClampToInteger(r.m_location.m_x);
    self->m_location.m_y = WebCore::ClampToInteger(r.m_location.m_y);
    self->m_size.m_width = WebCore::ClampToInteger(r.m_size.m_width);
    self->m_size.m_height = WebCore::ClampToInteger(r.m_size.m_height);
}

void APS5_VABI _ZN7WebCore7IntRectC2ERKNS_10LayoutRectE(WebCore::IntRect* self, const WebCore::LayoutRect& r) {
    self->m_location.m_x = r.m_location.m_x.m_value / 64;
    self->m_location.m_y = r.m_location.m_y.m_value / 64;
    self->m_size.m_width = r.m_size.m_width.m_value / 64;
    self->m_size.m_height = r.m_size.m_height.m_value / 64;
}

void APS5_VABI _ZN7WebCore7IntRectC2ERKNS_9FloatRectE(WebCore::IntRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x = WebCore::ClampToInteger(r.m_location.m_x);
    self->m_location.m_y = WebCore::ClampToInteger(r.m_location.m_y);
    self->m_size.m_width = WebCore::ClampToInteger(r.m_size.m_width);
    self->m_size.m_height = WebCore::ClampToInteger(r.m_size.m_height);
}

bool APS5_VABI _ZNK7WebCore7IntRect10intersectsERKS0_(const WebCore::IntRect* self, const WebCore::IntRect& other) {
    return self->m_size.m_width > 0 && self->m_size.m_height > 0
        && other.m_size.m_width > 0 && other.m_size.m_height > 0
        && self->m_location.m_x < other.m_location.m_x + other.m_size.m_width
        && other.m_location.m_x < self->m_location.m_x + self->m_size.m_width
        && self->m_location.m_y < other.m_location.m_y + other.m_size.m_height
        && other.m_location.m_y < self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore7IntRect8containsERKS0_(const WebCore::IntRect* self, const WebCore::IntRect& other) {
    return self->m_location.m_x <= other.m_location.m_x
        && self->m_location.m_x + self->m_size.m_width >= other.m_location.m_x + other.m_size.m_width
        && self->m_location.m_y <= other.m_location.m_y
        && self->m_location.m_y + self->m_size.m_height >= other.m_location.m_y + other.m_size.m_height;
}

void APS5_VABI _ZN7WebCore10LayoutRect5scaleEf(WebCore::LayoutRect* self, float s) {
    constexpr int minVal = std::numeric_limits<int>::min() / 2;
    constexpr int maxVal = std::numeric_limits<int>::max() - 64 / 2;
    if (self->m_location.m_x.m_value == minVal && self->m_location.m_y.m_value == minVal
        && self->m_size.m_width.m_value == maxVal && self->m_size.m_height.m_value == maxVal) {
        return;
    }
    self->m_location.m_x.m_value = WebCore::ClampToInteger(self->m_location.m_x.m_value * s);
    self->m_location.m_y.m_value = WebCore::ClampToInteger(self->m_location.m_y.m_value * s);
    self->m_size.m_width.m_value = WebCore::ClampToInteger(self->m_size.m_width.m_value * s);
    self->m_size.m_height.m_value = WebCore::ClampToInteger(self->m_size.m_height.m_value * s);
}

void APS5_VABI _ZN7WebCore10LayoutRect5uniteERKS0_(WebCore::LayoutRect* self, const WebCore::LayoutRect& other) {
    if (other.m_size.m_width.m_value <= 0 || other.m_size.m_height.m_value <= 0) {
        return;
    }
    if (self->m_size.m_width.m_value <= 0 || self->m_size.m_height.m_value <= 0) {
        *self = other;
        return;
    }
    int minX = std::min(self->m_location.m_x.m_value, other.m_location.m_x.m_value);
    int minY = std::min(self->m_location.m_y.m_value, other.m_location.m_y.m_value);
    int maxX = std::max(self->m_location.m_x.m_value + self->m_size.m_width.m_value,
                        other.m_location.m_x.m_value + other.m_size.m_width.m_value);
    int maxY = std::max(self->m_location.m_y.m_value + self->m_size.m_height.m_value,
                        other.m_location.m_y.m_value + other.m_size.m_height.m_value);
    self->m_location.m_x.m_value = minX;
    self->m_location.m_y.m_value = minY;
    self->m_size.m_width.m_value = maxX - minX;
    self->m_size.m_height.m_value = maxY - minY;
}

void APS5_VABI _ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(WebCore::LayoutRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x.m_value = WebCore::ClampToInteger(r.m_location.m_x * 64.0f);
    self->m_location.m_y.m_value = WebCore::ClampToInteger(r.m_location.m_y * 64.0f);
    self->m_size.m_width.m_value = WebCore::ClampToInteger(r.m_size.m_width * 64.0f);
    self->m_size.m_height.m_value = WebCore::ClampToInteger(r.m_size.m_height * 64.0f);
}

void APS5_VABI _ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE(WebCore::LayoutRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x.m_value = WebCore::ClampToInteger(r.m_location.m_x * 64.0f);
    self->m_location.m_y.m_value = WebCore::ClampToInteger(r.m_location.m_y * 64.0f);
    self->m_size.m_width.m_value = WebCore::ClampToInteger(r.m_size.m_width * 64.0f);
    self->m_size.m_height.m_value = WebCore::ClampToInteger(r.m_size.m_height * 64.0f);
}

bool APS5_VABI _ZNK7WebCore10LayoutRect8containsERKS0_(const WebCore::LayoutRect* self, const WebCore::LayoutRect& other) {
    return self->m_location.m_x.m_value <= other.m_location.m_x.m_value
        && self->m_location.m_x.m_value + self->m_size.m_width.m_value >= other.m_location.m_x.m_value + other.m_size.m_width.m_value
        && self->m_location.m_y.m_value <= other.m_location.m_y.m_value
        && self->m_location.m_y.m_value + self->m_size.m_height.m_value >= other.m_location.m_y.m_value + other.m_size.m_height.m_value;
}

void APS5_VABI _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(
    WebCore::RoundedRect* self, const WebCore::LayoutRect& rect,
    const WebCore::LayoutSize& topLeft, const WebCore::LayoutSize& topRight,
    const WebCore::LayoutSize& bottomLeft, const WebCore::LayoutSize& bottomRight) {
    self->m_rect = rect;
    self->m_radii.m_topLeft = topLeft;
    self->m_radii.m_topRight = topRight;
    self->m_radii.m_bottomLeft = bottomLeft;
    self->m_radii.m_bottomRight = bottomRight;
}

void APS5_VABI _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(
    WebCore::RoundedRect* self, const WebCore::LayoutRect& rect,
    const WebCore::LayoutSize& topLeft, const WebCore::LayoutSize& topRight,
    const WebCore::LayoutSize& bottomLeft, const WebCore::LayoutSize& bottomRight) {
    self->m_rect = rect;
    self->m_radii.m_topLeft = topLeft;
    self->m_radii.m_topRight = topRight;
    self->m_radii.m_bottomLeft = bottomLeft;
    self->m_radii.m_bottomRight = bottomRight;
}

bool APS5_VABI _ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(
    const WebCore::RoundedRect* self, const WebCore::LayoutRect& other) {
    bool rectContains = self->m_rect.m_location.m_x.m_value <= other.m_location.m_x.m_value
        && self->m_rect.m_location.m_x.m_value + self->m_rect.m_size.m_width.m_value >= other.m_location.m_x.m_value + other.m_size.m_width.m_value
        && self->m_rect.m_location.m_y.m_value <= other.m_location.m_y.m_value
        && self->m_rect.m_location.m_y.m_value + self->m_rect.m_size.m_height.m_value >= other.m_location.m_y.m_value + other.m_size.m_height.m_value;
    if (!rectContains || !WebCore::IsRoundedRectRenderable(*self)) {
        return false;
    }
    float rx = static_cast<float>(self->m_rect.m_location.m_x.m_value) / 64.0f;
    float ry = static_cast<float>(self->m_rect.m_location.m_y.m_value) / 64.0f;
    float rw = static_cast<float>(self->m_rect.m_size.m_width.m_value) / 64.0f;
    float rh = static_cast<float>(self->m_rect.m_size.m_height.m_value) / 64.0f;
    float ox = static_cast<float>(other.m_location.m_x.m_value) / 64.0f;
    float oy = static_cast<float>(other.m_location.m_y.m_value) / 64.0f;
    float ow = static_cast<float>(other.m_size.m_width.m_value) / 64.0f;
    float oh = static_cast<float>(other.m_size.m_height.m_value) / 64.0f;

    if (self->m_radii.m_topLeft.m_width.m_value > 0 && self->m_radii.m_topLeft.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_topLeft.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_topLeft.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + radW, ry + radH};
        if (ox <= center.m_x && oy <= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox, oy})) {
                return false;
            }
        }
    }
    if (self->m_radii.m_topRight.m_width.m_value > 0 && self->m_radii.m_topRight.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_topRight.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_topRight.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + rw - radW, ry + radH};
        if (ox + ow >= center.m_x && oy <= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox + ow, oy})) {
                return false;
            }
        }
    }
    if (self->m_radii.m_bottomLeft.m_width.m_value > 0 && self->m_radii.m_bottomLeft.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_bottomLeft.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_bottomLeft.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + radW, ry + rh - radH};
        if (ox <= center.m_x && oy + oh >= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox, oy + oh})) {
                return false;
            }
        }
    }
    if (self->m_radii.m_bottomRight.m_width.m_value > 0 && self->m_radii.m_bottomRight.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_bottomRight.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_bottomRight.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + rw - radW, ry + rh - radH};
        if (ox + ow >= center.m_x && oy + oh >= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox + ow, oy + oh})) {
                return false;
            }
        }
    }
    return true;
}

void APS5_VABI _ZN7WebCore9FloatRect11fitToPointsERKNS_10FloatPointES3_(
    WebCore::FloatRect* self, const WebCore::FloatPoint& p0, const WebCore::FloatPoint& p1) {
    float left = std::min(p0.m_x, p1.m_x);
    float top = std::min(p0.m_y, p1.m_y);
    float right = std::max(p0.m_x, p1.m_x);
    float bottom = std::max(p0.m_y, p1.m_y);
    WebCore::SetLocationAndSizeFromEdges(*self, left, top, right, bottom);
}

void APS5_VABI _ZN7WebCore9FloatRect11fitToPointsERKNS_10FloatPointES3_S3_(
    WebCore::FloatRect* self, const WebCore::FloatPoint& p0, const WebCore::FloatPoint& p1, const WebCore::FloatPoint& p2) {
    float left = WebCore::Min3(p0.m_x, p1.m_x, p2.m_x);
    float top = WebCore::Min3(p0.m_y, p1.m_y, p2.m_y);
    float right = WebCore::Max3(p0.m_x, p1.m_x, p2.m_x);
    float bottom = WebCore::Max3(p0.m_y, p1.m_y, p2.m_y);
    WebCore::SetLocationAndSizeFromEdges(*self, left, top, right, bottom);
}

void APS5_VABI _ZN7WebCore9FloatRect11fitToPointsERKNS_10FloatPointES3_S3_S3_(
    WebCore::FloatRect* self, const WebCore::FloatPoint& p0, const WebCore::FloatPoint& p1,
    const WebCore::FloatPoint& p2, const WebCore::FloatPoint& p3) {
    float left = WebCore::Min4(p0.m_x, p1.m_x, p2.m_x, p3.m_x);
    float top = WebCore::Min4(p0.m_y, p1.m_y, p2.m_y, p3.m_y);
    float right = WebCore::Max4(p0.m_x, p1.m_x, p2.m_x, p3.m_x);
    float bottom = WebCore::Max4(p0.m_y, p1.m_y, p2.m_y, p3.m_y);
    WebCore::SetLocationAndSizeFromEdges(*self, left, top, right, bottom);
}

void APS5_VABI _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS0_5RadiiE(
    WebCore::RoundedRect* self, const WebCore::LayoutRect& rect, const WebCore::RoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

void APS5_VABI _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS0_5RadiiE(
    WebCore::RoundedRect* self, const WebCore::LayoutRect& rect, const WebCore::RoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    float locationX = static_cast<float>(WebCore::ClampToInteger(std::floor(rect.m_location.m_x)));
    float locationY = static_cast<float>(WebCore::ClampToInteger(std::floor(rect.m_location.m_y)));
    float maxX = static_cast<float>(WebCore::ClampToInteger(std::ceil(rect.m_location.m_x + rect.m_size.m_width)));
    float maxY = static_cast<float>(WebCore::ClampToInteger(std::ceil(rect.m_location.m_y + rect.m_size.m_height)));
    return WebCore::IntRect{
        WebCore::IntPoint{WebCore::ClampToInteger(locationX), WebCore::ClampToInteger(locationY)},
        WebCore::IntSize{WebCore::ClampToInteger(maxX - locationX), WebCore::ClampToInteger(maxY - locationY)}};
}

WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(const WebCore::LayoutRect& rect) {
    int x = WebCore::LayoutFloor(rect.m_location.m_x);
    int y = WebCore::LayoutFloor(rect.m_location.m_y);
    int maxX = rect.m_size.m_width.m_value
        ? WebCore::LayoutCeil(WebCore::LayoutAdd(rect.m_location.m_x, rect.m_size.m_width)) : x;
    int maxY = rect.m_size.m_height.m_value
        ? WebCore::LayoutCeil(WebCore::LayoutAdd(rect.m_location.m_y, rect.m_size.m_height)) : y;
    return WebCore::IntRect{WebCore::IntPoint{x, y}, WebCore::IntSize{WebCore::WrappingSub(maxX, x), WebCore::WrappingSub(maxY, y)}};
}

WebCore::LayoutRect APS5_VABI _ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    WebCore::LayoutPoint location{
        WebCore::LayoutFromFloatFloor(rect.m_location.m_x), WebCore::LayoutFromFloatFloor(rect.m_location.m_y)};
    WebCore::LayoutPoint maxPoint{
        WebCore::LayoutFromFloatCeil(rect.m_location.m_x + rect.m_size.m_width),
        WebCore::LayoutFromFloatCeil(rect.m_location.m_y + rect.m_size.m_height)};
    return WebCore::LayoutRect{location, WebCore::LayoutSize{
        WebCore::LayoutSub(maxPoint.m_x, location.m_x), WebCore::LayoutSub(maxPoint.m_y, location.m_y)}};
}

WebCore::FloatRect APS5_VABI _ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf(
    const WebCore::FloatRect& rect, float deviceScaleFactor) {
    float x = std::floor(rect.m_location.m_x * deviceScaleFactor) / deviceScaleFactor;
    float y = std::floor(rect.m_location.m_y * deviceScaleFactor) / deviceScaleFactor;
    float maxX = std::ceil((rect.m_location.m_x + rect.m_size.m_width) * deviceScaleFactor) / deviceScaleFactor;
    float maxY = std::ceil((rect.m_location.m_y + rect.m_size.m_height) * deviceScaleFactor) / deviceScaleFactor;
    return WebCore::FloatRect{WebCore::FloatPoint{x, y}, WebCore::FloatSize{maxX - x, maxY - y}};
}

WebCore::AffineTransform APS5_VABI _ZN7WebCore19makeMapBetweenRectsERKNS_9FloatRectES2_(
    const WebCore::FloatRect& source, const WebCore::FloatRect& dest) {
    WebCore::AffineTransform transform;
    _ZN7WebCore15AffineTransform9translateEdd(&transform,
        static_cast<double>(dest.m_location.m_x - source.m_location.m_x),
        static_cast<double>(dest.m_location.m_y - source.m_location.m_y));
    WebCore::FloatSize scale{dest.m_size.m_width / source.m_size.m_width, dest.m_size.m_height / source.m_size.m_height};
    _ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE(&transform, scale);
    return transform;
}

WebCore::FloatRect APS5_VABI _ZN7WebCore19unitedBoundingBoxesERKN3WTF6VectorINS_9FloatQuadELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(
    const NKWebKit::WtfAbi::Vector<WebCore::FloatQuad>& quads) {
    const WebCore::FloatQuad* elements = WebCore::VectorElements(quads);
    if (!quads.size) {
        return WebCore::FloatRect{};
    }
    WebCore::FloatRect result = WebCore::QuadBoundingBox(elements[0]);
    for (unsigned i = 1; i < quads.size; ++i) {
        WebCore::UniteEvenIfEmpty(result, WebCore::QuadBoundingBox(elements[i]));
    }
    return result;
}

WebCore::FloatRect APS5_VABI _ZN7WebCore9unionRectERKN3WTF6VectorINS_9FloatRectELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(
    const NKWebKit::WtfAbi::Vector<WebCore::FloatRect>& rects) {
    const WebCore::FloatRect* elements = WebCore::VectorElements(rects);
    WebCore::FloatRect result;
    for (unsigned i = 0; i < rects.size; ++i) {
        _ZN7WebCore9FloatRect5uniteERKS0_(&result, elements[i]);
    }
    return result;
}

WebCore::IntRect APS5_VABI _ZN7WebCore15HitTestLocation12rectForPointERKNS_11LayoutPointEjjjj(
    const WebCore::LayoutPoint& point, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding) {
    return WebCore::RectForPoint(point, topPadding, rightPadding, bottomPadding, leftPadding);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1Ev(WebCore::HitTestLocation* self) {
    *self = WebCore::HitTestLocation{};
    self->m_isRectBased = false;
    self->m_isRectilinear = true;
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2Ev(WebCore::HitTestLocation* self) {
    _ZN7WebCore15HitTestLocationC1Ev(self);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKS0_(WebCore::HitTestLocation* self, const WebCore::HitTestLocation& other) {
    self->m_point = other.m_point;
    self->m_boundingBox = other.m_boundingBox;
    self->m_transformedPoint = other.m_transformedPoint;
    self->m_transformedRect = other.m_transformedRect;
    self->m_isRectBased = other.m_isRectBased;
    self->m_isRectilinear = other.m_isRectilinear;
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKS0_(WebCore::HitTestLocation* self, const WebCore::HitTestLocation& other) {
    _ZN7WebCore15HitTestLocationC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE(WebCore::HitTestLocation* self, const WebCore::FloatPoint& point) {
    WebCore::LayoutPoint floored{WebCore::LayoutFromFloatFloor(point.m_x), WebCore::LayoutFromFloatFloor(point.m_y)};
    WebCore::ConstructHitTestLocation(self, floored);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE(WebCore::HitTestLocation* self, const WebCore::FloatPoint& point) {
    _ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE(self, point);
}

WebCore::LayoutPoint APS5_VABI _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
    const WebCore::LayoutRect& visibleContentRect, const WebCore::LayoutSize& totalContentsSize,
    const WebCore::LayoutPoint& scrollPosition, const WebCore::LayoutPoint& scrollOrigin, int headerHeight, int footerHeight) {
    WebCore::LayoutSize idealScrollRectSize{
        WebCore::LayoutMin(visibleContentRect.m_size.m_width, totalContentsSize.m_width),
        WebCore::LayoutMin(visibleContentRect.m_size.m_height, totalContentsSize.m_height)};
    WebCore::LayoutUnit header = WebCore::LayoutFromInt(headerHeight);
    WebCore::LayoutPoint scrollLocation{
        WebCore::LayoutSub(WebCore::LayoutAdd(scrollPosition.m_x, scrollOrigin.m_x), WebCore::LayoutUnit{0}),
        WebCore::LayoutSub(WebCore::LayoutAdd(scrollPosition.m_y, scrollOrigin.m_y), header)};
    WebCore::LayoutRect scrollRect{scrollLocation, idealScrollRectSize};
    WebCore::LayoutRect documentRect{WebCore::LayoutPoint{}, WebCore::LayoutSize{totalContentsSize.m_width,
        WebCore::LayoutSub(WebCore::LayoutSub(totalContentsSize.m_height, header), WebCore::LayoutFromInt(footerHeight))}};
    WebCore::LayoutRectIntersect(scrollRect, documentRect);
    if (scrollRect.m_size.m_width.m_value != idealScrollRectSize.m_width.m_value
        || scrollRect.m_size.m_height.m_value != idealScrollRectSize.m_height.m_value) {
        scrollRect.m_size = idealScrollRectSize;
        WebCore::LayoutRectIntersect(scrollRect, documentRect);
        if (scrollRect.m_size.m_width.m_value < idealScrollRectSize.m_width.m_value) {
            WebCore::LayoutUnit dx = WebCore::LayoutNegate(WebCore::LayoutSub(idealScrollRectSize.m_width, scrollRect.m_size.m_width));
            scrollRect.m_location.m_x = WebCore::LayoutAdd(scrollRect.m_location.m_x, dx);
        }
        if (scrollRect.m_size.m_height.m_value < idealScrollRectSize.m_height.m_value) {
            WebCore::LayoutUnit dy = WebCore::LayoutNegate(WebCore::LayoutSub(idealScrollRectSize.m_height, scrollRect.m_size.m_height));
            scrollRect.m_location.m_y = WebCore::LayoutAdd(scrollRect.m_location.m_y, dy);
        }
    }
    return WebCore::LayoutPoint{
        WebCore::LayoutSub(scrollRect.m_location.m_x, scrollOrigin.m_x),
        WebCore::LayoutSub(scrollRect.m_location.m_y, scrollOrigin.m_y)};
}

void APS5_VABI _ZN7WebCore14ScrollableArea32computeScrollbarValueAndOverhangEfffRfS1_(
    float currentPosition, float totalSize, float visibleSize, float& doubleValue, float& overhangAmount) {
    doubleValue = 0.0f;
    overhangAmount = 0.0f;
    float maximum = totalSize - visibleSize;
    if (currentPosition < 0.0f) {
        doubleValue = 0.0f;
        overhangAmount = -currentPosition;
    } else if (visibleSize + currentPosition > totalSize) {
        doubleValue = 1.0f;
        overhangAmount = currentPosition + visibleSize - totalSize;
    } else if (maximum > 0.0f) {
        doubleValue = currentPosition / maximum;
    } else {
        doubleValue = 0.0f;
    }
}

WebCore::LayoutUnit APS5_VABI _ZN7WebCore14valueForLengthERKNS_6LengthENS_10LayoutUnitE(
    const WebCore::LengthAbi& length, WebCore::LayoutUnit maximumValue) {
    switch (WebCore::CheckedLengthKind(length)) {
    case WebCore::LengthKind::Fixed:
        return WebCore::LayoutFromFloat(WebCore::LengthValue(length));
    case WebCore::LengthKind::Percent:
        return WebCore::LayoutFromFloat(static_cast<float>(WebCore::LayoutToFloat(maximumValue) * WebCore::LengthValue(length) / 100.0f));
    case WebCore::LengthKind::FillAvailable:
    case WebCore::LengthKind::Auto:
        return maximumValue;
    case WebCore::LengthKind::Relative:
    case WebCore::LengthKind::Intrinsic:
    case WebCore::LengthKind::MinIntrinsic:
    case WebCore::LengthKind::MinContent:
    case WebCore::LengthKind::MaxContent:
    case WebCore::LengthKind::FitContent:
    case WebCore::LengthKind::Calculated:
    case WebCore::LengthKind::Undefined:
        return WebCore::LayoutUnit{0};
    }
    return WebCore::LayoutUnit{0};
}

float APS5_VABI _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(const WebCore::LengthAbi& length, float maximumValue) {
    switch (WebCore::CheckedLengthKind(length)) {
    case WebCore::LengthKind::Fixed:
        return WebCore::LengthValue(length);
    case WebCore::LengthKind::Percent:
        return static_cast<float>(maximumValue * WebCore::LengthValue(length) / 100.0f);
    case WebCore::LengthKind::FillAvailable:
    case WebCore::LengthKind::Auto:
        return maximumValue;
    case WebCore::LengthKind::Relative:
    case WebCore::LengthKind::Intrinsic:
    case WebCore::LengthKind::MinIntrinsic:
    case WebCore::LengthKind::MinContent:
    case WebCore::LengthKind::MaxContent:
    case WebCore::LengthKind::FitContent:
    case WebCore::LengthKind::Calculated:
    case WebCore::LengthKind::Undefined:
        return 0.0f;
    }
    return 0.0f;
}

}

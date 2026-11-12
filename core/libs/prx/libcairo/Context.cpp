#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kInvalidRestore = 2;
constexpr int kInvalidMatrix = 5;
constexpr int kNullPointer = 7;
constexpr int kInvalidDash = 19;
constexpr int kInvalidPathData = 9;

constexpr double kDefaultFontSize = 10.0;
constexpr double kTolerance = 0.1;
constexpr double kPi = 3.14159265358979323846;
constexpr double kFixedMagic = 26388279066624.0;
constexpr double kFixedMaxDouble = 2147483647.0 / 256.0;
constexpr double kFixedMinDouble = -2147483648.0 / 256.0;
constexpr int kMaxFullCircles = 65536;

constexpr CairoMatrix kIdentity{1, 0, 0, 1, 0, 0};

struct GraphicsStyle {
    int compositionOperator{2};
    int antialias{};
    int fillRule{};
    int lineCap{};
    int lineJoin{};
    double lineWidth{2};
    double miterLimit{10};
    double* dashes{};
    int dashCount{};
    double dashOffset{};
    CairoPattern* source{CairoPatternBlack()};
    CairoMatrix fontMatrix{kDefaultFontSize, 0, 0, kDefaultFontSize, 0, 0};
    CairoFontOptionsState fontOptions{};
    CairoMatrix inverse{1, 0, 0, 1, 0, 0};
    CairoMatrix sourceInverse{1, 0, 0, 1, 0, 0};
    bool identity{true};
};

struct SavedState {
    CairoMatrix matrix;
    GraphicsStyle style;
    SavedState* previous;
};

struct PathEntry {
    int type{};
    double points[6]{};
};

struct ContextStorage {
    std::atomic<unsigned> references{1};
    int status{kSuccess};
    int initialStatus{kSuccess};
    bool immortal{false};
    CairoSurface* target{};
    CairoMatrix matrix{kIdentity};
    GraphicsStyle style{};
    SavedState* saved{};
    std::vector<PathEntry>* path{};
    bool fillIsEmpty{true};
    bool hasCurrent{};
    double currentX{}, currentY{};
    double startX{}, startY{};

    explicit ContextStorage(int error = kSuccess, bool staticError = false,
                            CairoSurface* initialTarget = nullptr)
        : status(error), initialStatus(error), immortal(staticError), target(initialTarget) {}
};

ContextStorage* State(CairoContext* context) {
    return reinterpret_cast<ContextStorage*>(context);
}

CairoContext* Handle(ContextStorage* state) {
    return reinterpret_cast<CairoContext*>(state);
}

CairoContext* ErrorContext(int status) {
    static ContextStorage noMemory(kNoMemory, true);
    static ContextStorage invalidMatrix(kInvalidMatrix, true);
    static ContextStorage nullPointer(kNullPointer, true,
                                     CairoSurfaceError(kNoMemory));
    static ContextStorage invalidRestore(kInvalidRestore, true);
    switch (status) {
    case kInvalidMatrix: return Handle(&invalidMatrix);
    case kNullPointer: return Handle(&nullPointer);
    case kInvalidRestore: return Handle(&invalidRestore);
    default: return Handle(&noMemory);
    }
}

ContextStorage* Valid(CairoContext* context) {
    if (!context) return nullptr;
    auto* state = State(context);
    return state->status == kSuccess ? state : nullptr;
}

void ReleaseStorage(ContextStorage* state) {
    CairoPattern* bottomSource = state->style.source;
    delete[] state->style.dashes;
    while (state->saved) {
        cairo_pattern_destroy(bottomSource);
        SavedState* previous = state->saved->previous;
        bottomSource = state->saved->style.source;
        delete[] state->saved->style.dashes;
        delete state->saved;
        state->saved = previous;
    }
    if (state->target) cairo_surface_destroy(state->target);
    cairo_pattern_destroy(bottomSource);
    delete state->path;
    delete state;
}

int SetSource(ContextStorage* state, CairoPattern* source) {
    const int status = CairoPatternInternal(source)->status;
    if (status != kSuccess) return status;
    cairo_pattern_reference(source);
    cairo_pattern_destroy(state->style.source);
    state->style.source = source;
    state->style.sourceInverse = state->style.inverse;
    return kSuccess;
}

void Fail(ContextStorage* state, int status) {
    if (status != kSuccess) state->status = status;
}

int SetSourceRgba(ContextStorage* state, double red, double green, double blue, double alpha) {
    const auto* current = CairoPatternInternal(state->style.source);
    if (current->type == 0) {
        auto restrict = [](double value) { return value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value; };
        const CairoColor color = CairoColorFromRgba(restrict(red), restrict(green),
                                                    restrict(blue), restrict(alpha));
        if (CairoColorEqual(color, current->color)) return kSuccess;
    }
    SetSource(state, CairoPatternBlack());
    CairoPattern* pattern = CairoPatternCreateRgba(red, green, blue, alpha);
    const int status = CairoPatternInternal(pattern)->status;
    if (status != kSuccess) return status;
    const int result = SetSource(state, pattern);
    cairo_pattern_destroy(pattern);
    return result;
}

bool IsIdentity(const CairoMatrix& matrix) {
    return matrix.xx == 1.0 && matrix.yx == 0.0 && matrix.xy == 0.0 && matrix.yy == 1.0 &&
           matrix.x0 == 0.0 && matrix.y0 == 0.0;
}

bool IsInvertible(const CairoMatrix& matrix) {
    const double determinant = matrix.xx * matrix.yy - matrix.yx * matrix.xy;
    return std::isfinite(determinant) && determinant != 0.;
}

CairoMatrix Make(double xx, double yx, double xy, double yy, double x0, double y0) {
    CairoMatrix matrix{};
    cairo_matrix_init(&matrix, xx, yx, xy, yy, x0, y0);
    return matrix;
}

CairoMatrix DeviceTransform(const ContextStorage* state) {
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(state->target, &sx, &sy);
    return Make(sx, 0, 0, sy, 0, 0);
}

CairoMatrix DeviceInverse(const ContextStorage* state) {
    CairoMatrix matrix = DeviceTransform(state);
    cairo_matrix_invert(&matrix);
    return matrix;
}

bool BackendIsIdentity(const ContextStorage* state) {
    return state->style.identity && IsIdentity(DeviceTransform(state));
}

void UserToBackend(const ContextStorage* state, double& x, double& y) {
    if (BackendIsIdentity(state)) return;
    cairo_matrix_transform_point(&state->matrix, &x, &y);
    const CairoMatrix device = DeviceTransform(state);
    cairo_matrix_transform_point(&device, &x, &y);
}

void BackendToUser(const ContextStorage* state, double& x, double& y) {
    if (BackendIsIdentity(state)) return;
    const CairoMatrix device = DeviceInverse(state);
    cairo_matrix_transform_point(&device, &x, &y);
    cairo_matrix_transform_point(&state->style.inverse, &x, &y);
}

std::int32_t FixedFromDouble(double value) {
    const double biased = value + kFixedMagic;
    std::uint64_t bits = 0;
    std::memcpy(&bits, &biased, sizeof(bits));
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bits));
}

double FixedToDouble(std::int32_t value) {
    return value / 256.0;
}

std::int32_t ToFixed(double stored) {
    return static_cast<std::int32_t>(stored * 256.0);
}

double Quantize(double value) {
    return FixedToDouble(FixedFromDouble(value));
}

double QuantizeClamped(double value, double tolerance) {
    if (value > kFixedMaxDouble - tolerance)
        value = kFixedMaxDouble - tolerance;
    else if (value < kFixedMinDouble + tolerance)
        value = kFixedMinDouble + tolerance;
    return Quantize(value);
}

bool Record(ContextStorage* state, const PathEntry& entry) {
    if (!state->path) {
        state->path = new (std::nothrow) std::vector<PathEntry>;
        if (!state->path) {
            state->status = kNoMemory;
            return false;
        }
    }
    try {
        state->path->push_back(entry);
        return true;
    } catch (const std::bad_alloc&) {
        state->status = kNoMemory;
        return false;
    }
}

void DevicePoint(const ContextStorage* state, double& x, double& y) {
    UserToBackend(state, x, y);
    x = QuantizeClamped(x, state->style.lineWidth);
    y = QuantizeClamped(y, state->style.lineWidth);
}

const double* Endpoint(const PathEntry& entry) {
    return entry.type == 2 ? entry.points + 4 : entry.points;
}

double Fixed(double coordinate) {
    if (std::isnan(coordinate)) return 0;
    constexpr double kMinimum = -8388606.0;
    constexpr double kMaximum = 8388606.0 - 1.0 / 256.0;
    return std::nearbyint(std::clamp(coordinate, kMinimum, kMaximum) * 256.0) / 256.0;
}

void UserPoint(const ContextStorage* state, const CairoMatrix& inverse,
               double& x, double& y) {
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(state->target, &sx, &sy);
    x /= sx;
    y /= sy;
    cairo_matrix_transform_point(&inverse, &x, &y);
}

struct PathRange {
    double minimum{std::numeric_limits<double>::infinity()};
    double maximum{-std::numeric_limits<double>::infinity()};

    void Add(double value) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
};

void AddCubicRange(PathRange& range, double start, double first, double second, double end) {
    range.Add(start);
    range.Add(end);
    const double a = -start + 3 * first - 3 * second + end;
    const double b = 2 * (start - 2 * first + second);
    const double c = first - start;
    const auto evaluate = [&](double t) {
        if (t <= 0 || t >= 1) return;
        const double u = 1 - t;
        range.Add(Fixed(u * u * u * start + 3 * u * u * t * first +
                        3 * u * t * t * second + t * t * t * end));
    };
    if (a == 0) {
        if (b != 0) evaluate(-c / b);
        return;
    }
    const double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return;
    const double root = std::sqrt(discriminant);
    evaluate((-b + root) / (2 * a));
    evaluate((-b - root) / (2 * a));
}

bool AppendMove(ContextStorage* state, double x, double y) {
    PathEntry command{};
    command.type = 0;
    command.points[0] = x;
    command.points[1] = y;
    if (state->path && !state->path->empty() && state->path->back().type == 0)
        state->path->back() = command;
    else if (!Record(state, command)) return false;
    state->hasCurrent = true;
    state->currentX = state->startX = x;
    state->currentY = state->startY = y;
    return true;
}

std::int32_t Difference(double to, double from) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(ToFixed(to)) -
                                     static_cast<std::uint32_t>(ToFixed(from)));
}

void AppendLine(ContextStorage* state, double x, double y) {
    if (!state->hasCurrent) {
        AppendMove(state, x, y);
        return;
    }
    auto& path = *state->path;
    if (path.back().type != 0 && x == state->currentX && y == state->currentY) return;
    if (path.back().type == 1) {
        const double* previous = Endpoint(path[path.size() - 2]);
        if (previous[0] == state->currentX && previous[1] == state->currentY) {
            path.pop_back();
        } else {
            const std::int64_t dx1 = Difference(state->currentX, previous[0]);
            const std::int64_t dy1 = Difference(state->currentY, previous[1]);
            const std::int64_t dx2 = Difference(x, state->currentX);
            const std::int64_t dy2 = Difference(y, state->currentY);
            const bool backwards = (dx1 ^ dx2) < 0 || (dy1 ^ dy2) < 0;
            if (dy1 * dx2 == dy2 * dx1 && !backwards) path.pop_back();
        }
    }
    if (x != state->currentX || y != state->currentY) state->fillIsEmpty = false;
    state->currentX = x;
    state->currentY = y;
    PathEntry command{};
    command.type = 1;
    command.points[0] = x;
    command.points[1] = y;
    Record(state, command);
}

void AppendCurve(ContextStorage* state, double x1, double y1, double x2, double y2,
                 double x3, double y3) {
    if (state->currentX == x3 && state->currentY == y3 &&
        x2 == x3 && x1 == x3 && y2 == y3 && y1 == y3) {
        AppendLine(state, x3, y3);
        return;
    }
    if (!state->hasCurrent && !AppendMove(state, x1, y1)) return;
    auto& path = *state->path;
    if (path.back().type == 1) {
        const double* previous = Endpoint(path[path.size() - 2]);
        if (previous[0] == state->currentX && previous[1] == state->currentY) path.pop_back();
    }
    PathEntry command{};
    command.type = 2;
    command.points[0] = x1;
    command.points[1] = y1;
    command.points[2] = x2;
    command.points[3] = y2;
    command.points[4] = x3;
    command.points[5] = y3;
    state->currentX = x3;
    state->currentY = y3;
    state->fillIsEmpty = false;
    Record(state, command);
}

bool HasUnityScale(const CairoMatrix& matrix) {
    constexpr double kEpsilon = 1.0 / 256.0;
    const double determinant = matrix.xx * matrix.yy - matrix.yx * matrix.xy;
    if (std::fabs(determinant * determinant - 1.0) < kEpsilon) {
        if (std::fabs(matrix.xy) < kEpsilon && std::fabs(matrix.yx) < kEpsilon) return true;
        if (std::fabs(matrix.xx) < kEpsilon && std::fabs(matrix.yy) < kEpsilon) return true;
    }
    return false;
}

double MajorAxis(const CairoMatrix& matrix, double radius) {
    if (HasUnityScale(matrix)) return radius;
    const double a = matrix.xx;
    const double b = matrix.yx;
    const double c = matrix.xy;
    const double d = matrix.yy;
    const double i = a * a + b * b;
    const double j = c * c + d * d;
    const double f = 0.5 * (i + j);
    const double g = 0.5 * (i - j);
    const double h = a * c + b * d;
    return radius * std::sqrt(f + std::hypot(g, h));
}

double ArcError(double angle) {
    return 2.0 / 27.0 * std::pow(std::sin(angle / 4), 6) / std::pow(std::cos(angle / 4), 2);
}

double ArcMaxAngle(double tolerance) {
    static const double table[][2] = {
        {kPi / 1.0, 0.0185185185185185036127},
        {kPi / 2.0, 0.000272567143730179811158},
        {kPi / 3.0, 2.38647043651461047433e-05},
        {kPi / 4.0, 4.2455377443222443279e-06},
        {kPi / 5.0, 1.11281001494389081528e-06},
        {kPi / 6.0, 3.72662000942734705475e-07},
        {kPi / 7.0, 1.47783685574284411325e-07},
        {kPi / 8.0, 6.63240432022601149057e-08},
        {kPi / 9.0, 3.2715520137536980553e-08},
        {kPi / 10.0, 1.73863223499021216974e-08},
        {kPi / 11.0, 9.81410988043554039085e-09},
    };
    constexpr int kTableSize = 11;
    constexpr int kMaxSegments = 1000;
    int i = 0;
    for (; i < kTableSize; ++i)
        if (table[i][1] < tolerance) return table[i][0];
    ++i;
    double angle = 0;
    double error = 0;
    do {
        angle = kPi / i++;
        error = ArcError(angle);
    } while (error > tolerance && i < kMaxSegments);
    return angle;
}

void ArcSegment(CairoContext* context, double xc, double yc, double radius,
                double angleA, double angleB) {
    const double rSinA = radius * std::sin(angleA);
    const double rCosA = radius * std::cos(angleA);
    const double rSinB = radius * std::sin(angleB);
    const double rCosB = radius * std::cos(angleB);
    const double h = 4.0 / 3.0 * std::tan((angleB - angleA) / 4.0);
    cairo_curve_to(context,
                   xc + rCosA - h * rSinA, yc + rSinA + h * rCosA,
                   xc + rCosB + h * rSinB, yc + rSinB - h * rCosB,
                   xc + rCosB, yc + rSinB);
}

void ArcInDirection(CairoContext* context, double xc, double yc, double radius,
                    double angleMin, double angleMax, bool forward) {
    if (State(context)->status != kSuccess) return;
    if (!std::isfinite(angleMax) || !std::isfinite(angleMin)) return;
    if (angleMax - angleMin > 2 * kPi * kMaxFullCircles) {
        angleMax = std::fmod(angleMax - angleMin, 2 * kPi);
        angleMin = std::fmod(angleMin, 2 * kPi);
        angleMax += angleMin + 2 * kPi * kMaxFullCircles;
    }
    if (angleMax - angleMin > kPi) {
        const double angleMid = angleMin + (angleMax - angleMin) / 2.0;
        if (forward) {
            ArcInDirection(context, xc, yc, radius, angleMin, angleMid, forward);
            ArcInDirection(context, xc, yc, radius, angleMid, angleMax, forward);
        } else {
            ArcInDirection(context, xc, yc, radius, angleMid, angleMax, forward);
            ArcInDirection(context, xc, yc, radius, angleMin, angleMid, forward);
        }
    } else if (angleMax != angleMin) {
        const double majorAxis = MajorAxis(State(context)->matrix, radius);
        const double maxAngle = ArcMaxAngle(kTolerance / majorAxis);
        int segments = static_cast<int>(std::ceil(std::fabs(angleMax - angleMin) / maxAngle));
        double step = (angleMax - angleMin) / segments;
        segments -= 1;
        if (!forward) {
            std::swap(angleMin, angleMax);
            step = -step;
        }
        cairo_line_to(context, xc + radius * std::cos(angleMin), yc + radius * std::sin(angleMin));
        for (int i = 0; i < segments; ++i, angleMin += step)
            ArcSegment(context, xc, yc, radius, angleMin, angleMin + step);
        ArcSegment(context, xc, yc, radius, angleMin, angleMax);
    } else {
        cairo_line_to(context, xc + radius * std::cos(angleMin), yc + radius * std::sin(angleMin));
    }
}

void Arc(CairoContext* context, double xc, double yc, double radius,
         double angle1, double angle2, bool forward) {
    auto* state = State(context);
    if (radius <= 0.0) {
        UserToBackend(state, xc, yc);
        const double x = Quantize(xc);
        const double y = Quantize(yc);
        AppendLine(state, x, y);
        if (state->status != kSuccess) return;
        AppendLine(state, x, y);
        return;
    }
    cairo_line_to(context, xc + radius * std::cos(angle1), yc + radius * std::sin(angle1));
    if (state->status != kSuccess) return;
    if (forward)
        ArcInDirection(context, xc, yc, radius, angle1, angle2, true);
    else
        ArcInDirection(context, xc, yc, radius, angle2, angle1, false);
}

std::int32_t WrapSub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

std::int32_t WrapAdd(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

struct FixedPoint {
    std::int32_t x{};
    std::int32_t y{};
};

bool SamePoint(const FixedPoint& a, const FixedPoint& b) {
    return a.x == b.x && a.y == b.y;
}

FixedPoint StoredPoint(const PathEntry& entry, int index) {
    return {ToFixed(entry.points[index * 2]), ToFixed(entry.points[index * 2 + 1])};
}

int CompareEdgeForYAgainstX(const FixedPoint& p1, const FixedPoint& p2,
                            std::int32_t y, std::int32_t x) {
    const std::int32_t adx = WrapSub(p2.x, p1.x);
    const std::int32_t dx = WrapSub(x, p1.x);
    if (adx == 0) return WrapSub(0, dx);
    if ((adx ^ dx) < 0) return adx;
    const std::int32_t dy = WrapSub(y, p1.y);
    const std::int32_t ady = WrapSub(p2.y, p1.y);
    const std::int64_t left = static_cast<std::int64_t>(dy) * adx;
    const std::int64_t right = static_cast<std::int64_t>(dx) * ady;
    return left == right ? 0 : left < right ? -1 : 1;
}

struct SplineKnots {
    FixedPoint a, b, c, d;
};

FixedPoint LerpHalf(const FixedPoint& a, const FixedPoint& b) {
    return {WrapAdd(a.x, WrapSub(b.x, a.x) >> 1), WrapAdd(a.y, WrapSub(b.y, a.y) >> 1)};
}

void DeCasteljau(SplineKnots& s1, SplineKnots& s2) {
    const FixedPoint ab = LerpHalf(s1.a, s1.b);
    const FixedPoint bc = LerpHalf(s1.b, s1.c);
    const FixedPoint cd = LerpHalf(s1.c, s1.d);
    const FixedPoint abbc = LerpHalf(ab, bc);
    const FixedPoint bccd = LerpHalf(bc, cd);
    const FixedPoint middle = LerpHalf(abbc, bccd);
    s2 = {middle, bccd, cd, s1.d};
    s1.b = ab;
    s1.c = abbc;
    s1.d = middle;
}

double SplineErrorSquared(const SplineKnots& knots) {
    double bdx = FixedToDouble(WrapSub(knots.b.x, knots.a.x));
    double bdy = FixedToDouble(WrapSub(knots.b.y, knots.a.y));
    double cdx = FixedToDouble(WrapSub(knots.c.x, knots.a.x));
    double cdy = FixedToDouble(WrapSub(knots.c.y, knots.a.y));
    if (!SamePoint(knots.a, knots.d)) {
        const double dx = FixedToDouble(WrapSub(knots.d.x, knots.a.x));
        const double dy = FixedToDouble(WrapSub(knots.d.y, knots.a.y));
        const double v = dx * dx + dy * dy;
        double u = bdx * dx + bdy * dy;
        if (u <= 0) {
        } else if (u >= v) {
            bdx -= dx;
            bdy -= dy;
        } else {
            bdx -= u / v * dx;
            bdy -= u / v * dy;
        }
        u = cdx * dx + cdy * dy;
        if (u <= 0) {
        } else if (u >= v) {
            cdx -= dx;
            cdy -= dy;
        } else {
            cdx -= u / v * dx;
            cdy -= u / v * dy;
        }
    }
    const double berr = bdx * bdx + bdy * bdy;
    const double cerr = cdx * cdx + cdy * cdy;
    return berr > cerr ? berr : cerr;
}

struct InFill {
    std::int32_t x{};
    std::int32_t y{};
    bool onEdge{};
    int winding{};
    bool hasCurrent{};
    FixedPoint current{};
    FixedPoint first{};
    FixedPoint splineLast{};

    void AddEdge(FixedPoint p1, FixedPoint p2) {
        if (onEdge) return;
        int direction = 1;
        if (p2.y < p1.y) {
            std::swap(p1, p2);
            direction = -1;
        }
        const FixedPoint query{x, y};
        if (SamePoint(p1, query) || SamePoint(p2, query) ||
            (!(p2.y < y || p1.y > y || (p1.x > x && p2.x > x) || (p1.x < x && p2.x < x)) &&
             CompareEdgeForYAgainstX(p1, p2, y, x) == 0)) {
            onEdge = true;
            return;
        }
        if (p2.y <= y || p1.y > y) return;
        if (p1.x >= x && p2.x >= x) return;
        if ((p1.x <= x && p2.x <= x) || CompareEdgeForYAgainstX(p1, p2, y, x) < 0)
            winding += direction;
    }

    void MoveTo(const FixedPoint& point) {
        if (hasCurrent) AddEdge(current, first);
        first = point;
        current = point;
        hasCurrent = true;
    }

    void LineTo(const FixedPoint& point) {
        if (hasCurrent) AddEdge(current, point);
        current = point;
        hasCurrent = true;
    }

    void SplinePoint(const FixedPoint& point) {
        if (SamePoint(splineLast, point)) return;
        splineLast = point;
        LineTo(point);
    }

    void Decompose(SplineKnots s1, double toleranceSquared) {
        if (SplineErrorSquared(s1) < toleranceSquared) {
            SplinePoint(s1.a);
            return;
        }
        SplineKnots s2{};
        DeCasteljau(s1, s2);
        Decompose(s1, toleranceSquared);
        Decompose(s2, toleranceSquared);
    }

    void CurveTo(const FixedPoint& b, const FixedPoint& c, const FixedPoint& d) {
        std::int32_t top = current.y;
        std::int32_t bottom = current.y;
        for (const FixedPoint* point : {&b, &c, &d}) {
            if (point->y < top) top = point->y;
            if (point->y > bottom) bottom = point->y;
        }
        if (bottom < y || top > y) {
            current = d;
            return;
        }
        std::int32_t left = current.x;
        for (const FixedPoint* point : {&b, &c, &d})
            if (point->x < left) left = point->x;
        if (left > x) {
            current = d;
            return;
        }
        const FixedPoint a = current;
        if (SamePoint(a, b) && SamePoint(c, d)) return;
        if (SamePoint(c, d) && SamePoint(b, d)) return;
        splineLast = a;
        Decompose({a, b, c, d}, kTolerance * kTolerance);
        LineTo(d);
    }

    void ClosePath() {
        if (!hasCurrent) return;
        AddEdge(current, first);
        hasCurrent = false;
    }
};

void TransformBoundingBox(const CairoMatrix& matrix, double& x1, double& y1,
                          double& x2, double& y2) {
    if (matrix.xy == 0. && matrix.yx == 0.) {
        if (matrix.xx != 1.) {
            const double first = x1 * matrix.xx;
            const double second = x2 * matrix.xx;
            x1 = first < second ? first : second;
            x2 = first < second ? second : first;
        }
        if (matrix.x0 != 0.) {
            x1 += matrix.x0;
            x2 += matrix.x0;
        }
        if (matrix.yy != 1.) {
            const double first = y1 * matrix.yy;
            const double second = y2 * matrix.yy;
            y1 = first < second ? first : second;
            y2 = first < second ? second : first;
        }
        if (matrix.y0 != 0.) {
            y1 += matrix.y0;
            y2 += matrix.y0;
        }
        return;
    }
    double quadX[4] = {x1, x2, x1, x2};
    double quadY[4] = {y1, y1, y2, y2};
    for (int i = 0; i < 4; ++i) cairo_matrix_transform_point(&matrix, &quadX[i], &quadY[i]);
    double minX = quadX[0], maxX = quadX[0];
    double minY = quadY[0], maxY = quadY[0];
    for (int i = 1; i < 4; ++i) {
        if (quadX[i] < minX) minX = quadX[i];
        if (quadX[i] > maxX) maxX = quadX[i];
        if (quadY[i] < minY) minY = quadY[i];
        if (quadY[i] > maxY) maxY = quadY[i];
    }
    x1 = minX;
    y1 = minY;
    x2 = maxX;
    y2 = maxY;
}

} // namespace

extern "C" {

CairoContext* APS5_VABI cairo_create(CairoSurface* target) {
    if (!target) return ErrorContext(kNullPointer);
    const int status = cairo_surface_status(target);
    auto* state = new (std::nothrow) ContextStorage(status);
    if (!state) return ErrorContext(kNoMemory);
    state->target = cairo_surface_reference(target);
    return Handle(state);
}

CairoContext* APS5_VABI cairo_reference(CairoContext* context) {
    if (context && !State(context)->immortal)
        State(context)->references.fetch_add(1, std::memory_order_relaxed);
    return context;
}

void APS5_VABI cairo_destroy(CairoContext* context) {
    if (!context) return;
    auto* state = State(context);
    if (!state->immortal && state->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        ReleaseStorage(state);
}

void APS5_VABI cairo_save(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    auto* snapshot = new (std::nothrow) SavedState{state->matrix, state->style, state->saved};
    if (!snapshot) {
        state->status = kNoMemory;
        return;
    }
    if (snapshot->style.dashCount > 0) {
        snapshot->style.dashes = new (std::nothrow) double[snapshot->style.dashCount];
        if (!snapshot->style.dashes) {
            delete snapshot;
            state->status = kNoMemory;
            return;
        }
        std::copy_n(state->style.dashes, snapshot->style.dashCount, snapshot->style.dashes);
    }
    cairo_pattern_reference(snapshot->style.source);
    state->saved = snapshot;
}

void APS5_VABI cairo_restore(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (!state->saved) {
        state->status = kInvalidRestore;
        return;
    }
    SavedState* snapshot = state->saved;
    state->matrix = snapshot->matrix;
    delete[] state->style.dashes;
    cairo_pattern_destroy(state->style.source);
    state->style = snapshot->style;
    state->saved = snapshot->previous;
    delete snapshot;
}

CairoSurface* APS5_VABI cairo_get_target(CairoContext* context) {
    if (!context) return nullptr;
    const auto* state = State(context);
    if (state->status == kSuccess) return state->target;
    if (state->initialStatus != kSuccess && state->target) return state->target;
    return CairoSurfaceError(kNoMemory);
}

void APS5_VABI cairo_translate(CairoContext* context, double tx, double ty) {
    auto* state = Valid(context);
    if (!state) return;
    if (!std::isfinite(tx) || !std::isfinite(ty)) {
        state->status = kInvalidMatrix;
        return;
    }
    CairoMatrix step = Make(1, 0, 0, 1, tx, ty);
    cairo_matrix_multiply(&state->matrix, &step, &state->matrix);
    state->style.identity = false;
    if (!IsInvertible(state->matrix)) {
        state->status = kInvalidMatrix;
        return;
    }
    step = Make(1, 0, 0, 1, -tx, -ty);
    cairo_matrix_multiply(&state->style.inverse, &state->style.inverse, &step);
}

void APS5_VABI cairo_scale(CairoContext* context, double sx, double sy) {
    auto* state = Valid(context);
    if (!state) return;
    if (sx * sy == 0. || !std::isfinite(sx) || !std::isfinite(sy)) {
        state->status = kInvalidMatrix;
        return;
    }
    CairoMatrix step = Make(sx, 0, 0, sy, 0, 0);
    cairo_matrix_multiply(&state->matrix, &step, &state->matrix);
    state->style.identity = false;
    if (!IsInvertible(state->matrix)) {
        state->status = kInvalidMatrix;
        return;
    }
    step = Make(1 / sx, 0, 0, 1 / sy, 0, 0);
    cairo_matrix_multiply(&state->style.inverse, &state->style.inverse, &step);
}

void APS5_VABI cairo_rotate(CairoContext* context, double radians) {
    auto* state = Valid(context);
    if (!state || radians == 0.) return;
    if (!std::isfinite(radians)) {
        state->status = kInvalidMatrix;
        return;
    }
    double s = std::sin(radians);
    double c = std::cos(radians);
    CairoMatrix step = Make(c, s, -s, c, 0, 0);
    cairo_matrix_multiply(&state->matrix, &step, &state->matrix);
    state->style.identity = false;
    if (!IsInvertible(state->matrix)) {
        state->status = kInvalidMatrix;
        return;
    }
    s = std::sin(-radians);
    c = std::cos(-radians);
    step = Make(c, s, -s, c, 0, 0);
    cairo_matrix_multiply(&state->style.inverse, &state->style.inverse, &step);
}

void APS5_VABI cairo_transform(CairoContext* context, const CairoMatrix* matrix) {
    auto* state = Valid(context);
    if (!state || !matrix) return;
    if (!IsInvertible(*matrix)) {
        state->status = kInvalidMatrix;
        return;
    }
    if (IsIdentity(*matrix)) return;
    CairoMatrix inverse = *matrix;
    const int status = cairo_matrix_invert(&inverse);
    if (status != kSuccess) {
        state->status = status;
        return;
    }
    cairo_matrix_multiply(&state->matrix, matrix, &state->matrix);
    cairo_matrix_multiply(&state->style.inverse, &state->style.inverse, &inverse);
    state->style.identity = false;
    if (!IsInvertible(state->matrix)) state->status = kInvalidMatrix;
}

void APS5_VABI cairo_set_matrix(CairoContext* context, const CairoMatrix* matrix) {
    auto* state = Valid(context);
    if (!state || !matrix) return;
    if (std::memcmp(matrix, &state->matrix, sizeof(CairoMatrix)) == 0) return;
    if (!IsInvertible(*matrix)) {
        state->status = kInvalidMatrix;
        return;
    }
    if (IsIdentity(*matrix)) {
        cairo_identity_matrix(context);
        return;
    }
    state->matrix = *matrix;
    state->style.inverse = *matrix;
    cairo_matrix_invert(&state->style.inverse);
    state->style.identity = false;
}

void APS5_VABI cairo_get_matrix(CairoContext* context, CairoMatrix* matrix) {
    if (!matrix) return;
    auto* state = Valid(context);
    *matrix = state ? state->matrix : kIdentity;
}

void APS5_VABI cairo_identity_matrix(CairoContext* context) {
    auto* state = Valid(context);
    if (!state || IsIdentity(state->matrix)) return;
    state->matrix = kIdentity;
    state->style.inverse = kIdentity;
    state->style.identity = true;
}

void APS5_VABI cairo_user_to_device(CairoContext* context, double* x, double* y) {
    auto* state = Valid(context);
    if (state && x && y) cairo_matrix_transform_point(&state->matrix, x, y);
}

void APS5_VABI cairo_user_to_device_distance(CairoContext* context, double* dx, double* dy) {
    auto* state = Valid(context);
    if (state && dx && dy) cairo_matrix_transform_distance(&state->matrix, dx, dy);
}

void APS5_VABI cairo_device_to_user(CairoContext* context, double* x, double* y) {
    auto* state = Valid(context);
    if (state && x && y) cairo_matrix_transform_point(&state->style.inverse, x, y);
}

void APS5_VABI cairo_device_to_user_distance(CairoContext* context, double* dx, double* dy) {
    auto* state = Valid(context);
    if (state && dx && dy) cairo_matrix_transform_distance(&state->style.inverse, dx, dy);
}

void APS5_VABI cairo_set_antialias(CairoContext* context, int antialias) {
    auto* state = Valid(context);
    if (state) state->style.antialias = antialias;
}

int APS5_VABI cairo_get_antialias(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.antialias : 0;
}

void APS5_VABI cairo_set_fill_rule(CairoContext* context, int fillRule) {
    auto* state = Valid(context);
    if (state) state->style.fillRule = fillRule;
}

int APS5_VABI cairo_get_fill_rule(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.fillRule : 0;
}

void APS5_VABI cairo_set_line_cap(CairoContext* context, int cap) {
    auto* state = Valid(context);
    if (state) state->style.lineCap = cap;
}

int APS5_VABI cairo_get_line_cap(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.lineCap : 0;
}

void APS5_VABI cairo_set_line_join(CairoContext* context, int join) {
    auto* state = Valid(context);
    if (state) state->style.lineJoin = join;
}

int APS5_VABI cairo_get_line_join(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.lineJoin : 0;
}

void APS5_VABI cairo_set_line_width(CairoContext* context, double width) {
    auto* state = Valid(context);
    if (state) state->style.lineWidth = width < 0 ? 0 : width;
}

double APS5_VABI cairo_get_line_width(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.lineWidth : 2;
}

void APS5_VABI cairo_set_miter_limit(CairoContext* context, double limit) {
    auto* state = Valid(context);
    if (state) state->style.miterLimit = limit;
}

double APS5_VABI cairo_get_miter_limit(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.miterLimit : 10;
}

void APS5_VABI cairo_set_dash(CairoContext* context, const double* dashes,
                             int numDashes, double offset) {
    auto* state = Valid(context);
    if (!state) return;
    if (numDashes < 0 || (numDashes > 0 && !dashes)) {
        state->status = kInvalidDash;
        return;
    }
    double total = 0;
    for (int i = 0; i < numDashes; ++i) {
        if (dashes[i] < 0) {
            state->status = kInvalidDash;
            return;
        }
        total += dashes[i];
    }
    if (numDashes > 0 && total == 0) {
        state->status = kInvalidDash;
        return;
    }

    double* copy = nullptr;
    if (numDashes > 0) {
        copy = new (std::nothrow) double[numDashes];
        if (!copy) {
            state->status = kNoMemory;
            return;
        }
        std::copy_n(dashes, numDashes, copy);
        for (int i = 1; i + 1 < numDashes;) {
            if (copy[i] == 0) {
                copy[i - 1] += copy[i + 1];
                std::move(copy + i + 2, copy + numDashes, copy + i);
                numDashes -= 2;
                if (i > 1) --i;
            } else {
                ++i;
            }
        }
    }
    double offTotal = 0;
    for (int i = 1; i < numDashes; i += 2) offTotal += copy[i];
    const double period = (numDashes % 2 == 0 ? total : total * 2);
    if (numDashes > 0 && ((numDashes % 2 != 0 && period < 1.0 / 256.0) ||
                          (numDashes % 2 == 0 && offTotal < 1.0 / 512.0))) {
        delete[] copy;
        copy = nullptr;
        numDashes = 0;
    }
    if (numDashes > 0) {
        offset = std::fmod(offset, period);
        if (offset < 0) offset += period;
    } else {
        offset = 0;
    }
    delete[] state->style.dashes;
    state->style.dashes = copy;
    state->style.dashCount = numDashes;
    state->style.dashOffset = offset;
}

void APS5_VABI cairo_get_dash(CairoContext* context, double* dashes, double* offset) {
    auto* state = Valid(context);
    if (!state) return;
    if (dashes) std::copy_n(state->style.dashes, state->style.dashCount, dashes);
    if (offset) *offset = state->style.dashOffset;
}

int APS5_VABI cairo_get_dash_count(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.dashCount : 0;
}

void APS5_VABI cairo_set_source(CairoContext* context, CairoPattern* source) {
    auto* state = Valid(context);
    if (!state) return;
    if (!source) {
        state->status = kNullPointer;
        return;
    }
    Fail(state, SetSource(state, source));
}

void APS5_VABI cairo_set_source_rgb(CairoContext* context, double red, double green, double blue) {
    auto* state = Valid(context);
    if (state) Fail(state, SetSourceRgba(state, red, green, blue, 1.0));
}

void APS5_VABI cairo_set_source_rgba(CairoContext* context, double red, double green, double blue,
                                     double alpha) {
    auto* state = Valid(context);
    if (state) Fail(state, SetSourceRgba(state, red, green, blue, alpha));
}

void APS5_VABI cairo_set_source_surface(CairoContext* context, CairoSurface* surface,
                                        double x, double y) {
    auto* state = Valid(context);
    if (!state) return;
    if (!surface) {
        state->status = kNullPointer;
        return;
    }
    SetSource(state, CairoPatternBlack());
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    const int status = CairoPatternInternal(pattern)->status;
    if (status != kSuccess) {
        cairo_pattern_destroy(pattern);
        state->status = status;
        return;
    }
    CairoMatrix matrix{};
    cairo_matrix_init_translate(&matrix, -x, -y);
    cairo_pattern_set_matrix(pattern, &matrix);
    Fail(state, SetSource(state, pattern));
    cairo_pattern_destroy(pattern);
}

void APS5_VABI cairo_set_operator(CairoContext* context, int operation) {
    auto* state = Valid(context);
    if (state) state->style.compositionOperator = operation;
}

int APS5_VABI cairo_get_operator(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.compositionOperator : 2;
}

void APS5_VABI cairo_set_font_size(CairoContext* context, double size) {
    auto* state = Valid(context);
    if (state) cairo_matrix_init(&state->style.fontMatrix, size, 0, 0, size, 0, 0);
}

void APS5_VABI cairo_set_font_options(CairoContext* context, const cairo_font_options_t* options) {
    auto* state = Valid(context);
    if (!state) return;
    const int status = CairoFontOptionsStatus(options);
    if (status != kSuccess) {
        state->status = status;
        return;
    }
    state->style.fontOptions = *CairoFontOptionsRead(options);
}

void APS5_VABI cairo_rectangle(CairoContext* context, double x, double y,
                               double width, double height) {
    auto* state = Valid(context);
    if (!state) return;
    double wx = width, wy = 0;
    double hx = 0, hy = height;
    cairo_matrix_transform_distance(&state->matrix, &wx, &wy);
    cairo_matrix_transform_distance(&state->matrix, &hx, &hy);
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(state->target, &sx, &sy);
    wx = Fixed(wx * sx);
    wy = Fixed(wy * sy);
    hx = Fixed(hx * sx);
    hy = Fixed(hy * sy);
    DevicePoint(state, x, y);
    if (!AppendMove(state, x, y)) return;
    AppendLine(state, x + wx, y + wy);
    if (state->status != kSuccess) return;
    AppendLine(state, x + wx + hx, y + wy + hy);
    if (state->status != kSuccess) return;
    AppendLine(state, x + hx, y + hy);
    cairo_close_path(context);
}

void APS5_VABI cairo_new_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (state->path) state->path->clear();
    state->fillIsEmpty = true;
    state->hasCurrent = false;
    state->currentX = state->currentY = 0;
    state->startX = state->startY = 0;
}

void APS5_VABI cairo_new_sub_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (state->path && !state->path->empty() && state->path->back().type == 0)
        state->path->pop_back();
    state->hasCurrent = false;
}

void APS5_VABI cairo_move_to(CairoContext* context, double x, double y) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x, y);
    AppendMove(state, x, y);
}

void APS5_VABI cairo_line_to(CairoContext* context, double x, double y) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x, y);
    AppendLine(state, x, y);
}

void APS5_VABI cairo_curve_to(CairoContext* context, double x1, double y1,
                              double x2, double y2, double x3, double y3) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x1, y1);
    DevicePoint(state, x2, y2);
    DevicePoint(state, x3, y3);
    AppendCurve(state, x1, y1, x2, y2, x3, y3);
}

void APS5_VABI cairo_close_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state || !state->hasCurrent) return;
    AppendLine(state, state->startX, state->startY);
    if (state->status != kSuccess) return;
    if (state->path && !state->path->empty() && state->path->back().type == 1)
        state->path->pop_back();
    PathEntry close{};
    close.type = 3;
    if (!Record(state, close)) return;
    AppendMove(state, state->startX, state->startY);
}

int APS5_VABI cairo_has_current_point(CairoContext* context) {
    auto* state = Valid(context);
    return state && state->hasCurrent;
}

void APS5_VABI cairo_get_current_point(CairoContext* context, double* x, double* y) {
    auto* state = Valid(context);
    double currentX = 0;
    double currentY = 0;
    if (state && state->hasCurrent) {
        currentX = state->currentX;
        currentY = state->currentY;
        BackendToUser(state, currentX, currentY);
    }
    if (x) *x = currentX;
    if (y) *y = currentY;
}

CairoPath* APS5_VABI cairo_copy_path(CairoContext* context) {
    auto* state = Valid(context);
    auto* result = new (std::nothrow) CairoPath{context ? State(context)->status : kNullPointer,
                                               nullptr, 0};
    if (!result) return nullptr;
    if (!state || !state->path) return result;

    std::size_t total = 0;
    for (const auto& entry : *state->path) total += entry.type == 2 ? 4 : entry.type == 3 ? 1 : 2;
    if (total > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result->status = kNoMemory;
        return result;
    }
    if (total == 0) return result;
    result->data = new (std::nothrow) CairoPathData[total]();
    if (!result->data) {
        result->status = kNoMemory;
        return result;
    }
    std::size_t offset = 0;
    for (const auto& entry : *state->path) {
        const int length = entry.type == 2 ? 4 : entry.type == 3 ? 1 : 2;
        result->data[offset].header = {entry.type, length};
        for (int i = 1; i < length; ++i) {
            double x = entry.points[(i - 1) * 2];
            double y = entry.points[(i - 1) * 2 + 1];
            BackendToUser(state, x, y);
            result->data[offset + i].point = {x, y};
        }
        offset += length;
    }
    result->num_data = static_cast<int>(total);
    return result;
}

void APS5_VABI cairo_path_extents(CairoContext* context, double* x1, double* y1,
                                 double* x2, double* y2) {
    double px1 = 0, py1 = 0, px2 = 0, py2 = 0;
    const auto store = [&] {
        if (x1) *x1 = px1;
        if (y1) *y1 = py1;
        if (x2) *x2 = px2;
        if (y2) *y2 = py2;
    };
    auto* state = Valid(context);
    if (!state || !state->path) return store();

    PathRange xRange, yRange;
    double currentX = 0, currentY = 0, startX = 0, startY = 0;
    for (const auto& entry : *state->path) {
        if (entry.type == 0) {
            currentX = startX = entry.points[0];
            currentY = startY = entry.points[1];
        } else if (entry.type == 1) {
            xRange.Add(currentX);
            yRange.Add(currentY);
            xRange.Add(entry.points[0]);
            yRange.Add(entry.points[1]);
            currentX = entry.points[0];
            currentY = entry.points[1];
        } else if (entry.type == 2) {
            AddCubicRange(xRange, currentX, entry.points[0], entry.points[2], entry.points[4]);
            AddCubicRange(yRange, currentY, entry.points[1], entry.points[3], entry.points[5]);
            currentX = entry.points[4];
            currentY = entry.points[5];
        } else if (entry.type == 3) {
            xRange.Add(startX);
            yRange.Add(startY);
            currentX = startX;
            currentY = startY;
        }
    }
    if (!std::isfinite(xRange.minimum) || !std::isfinite(yRange.minimum)) return store();
    CairoMatrix inverse = state->matrix;
    if (cairo_matrix_invert(&inverse) != kSuccess) return store();
    PathRange usersX, usersY;
    for (double deviceX : {xRange.minimum, xRange.maximum}) {
        for (double deviceY : {yRange.minimum, yRange.maximum}) {
            double x = deviceX, y = deviceY;
            UserPoint(state, inverse, x, y);
            usersX.Add(x);
            usersY.Add(y);
        }
    }
    px1 = usersX.minimum;
    py1 = usersY.minimum;
    px2 = usersX.maximum;
    py2 = usersY.maximum;
    store();
}

void APS5_VABI cairo_append_path(CairoContext* context, const CairoPath* path) {
    auto* state = Valid(context);
    if (!state) return;
    if (path && path->status != kSuccess) {
        state->status = path->status;
        return;
    }
    if (!path || path->num_data < 0 || (path->num_data > 0 && !path->data)) {
        state->status = kNullPointer;
        return;
    }
    for (int i = 0; i < path->num_data && state->status == kSuccess;) {
        const auto header = path->data[i].header;
        const int expected = header.type == 2 ? 4 : header.type == 3 ? 1 :
                             (header.type == 0 || header.type == 1) ? 2 : 0;
        if (!expected || header.length < expected || header.length > path->num_data - i) {
            state->status = kInvalidPathData;
            return;
        }
        const CairoPathData* element = path->data + i;
        if (header.type == 0)
            cairo_move_to(context, element[1].point.x, element[1].point.y);
        else if (header.type == 1)
            cairo_line_to(context, element[1].point.x, element[1].point.y);
        else if (header.type == 2)
            cairo_curve_to(context, element[1].point.x, element[1].point.y,
                           element[2].point.x, element[2].point.y,
                           element[3].point.x, element[3].point.y);
        else
            cairo_close_path(context);
        i += header.length;
    }
}

void APS5_VABI cairo_path_destroy(CairoPath* path) {
    if (!path) return;
    delete[] path->data;
    delete path;
}

void APS5_VABI cairo_arc(CairoContext* context, double xc, double yc, double radius,
                         double angle1, double angle2) {
    if (!Valid(context)) return;
    if (angle2 < angle1) {
        angle2 = std::fmod(angle2 - angle1, 2 * kPi);
        if (angle2 < 0) angle2 += 2 * kPi;
        angle2 += angle1;
    }
    Arc(context, xc, yc, radius, angle1, angle2, true);
}

void APS5_VABI cairo_arc_negative(CairoContext* context, double xc, double yc, double radius,
                                  double angle1, double angle2) {
    if (!Valid(context)) return;
    if (angle2 > angle1) {
        angle2 = std::fmod(angle2 - angle1, 2 * kPi);
        if (angle2 > 0) angle2 -= 2 * kPi;
        angle2 += angle1;
    }
    Arc(context, xc, yc, radius, angle1, angle2, false);
}


int APS5_VABI cairo_in_fill(CairoContext* context, double x, double y) {
    auto* state = Valid(context);
    if (!state || state->fillIsEmpty) return 0;
    UserToBackend(state, x, y);
    InFill fill{};
    fill.x = FixedFromDouble(x);
    fill.y = FixedFromDouble(y);
    for (const auto& entry : *state->path) {
        if (entry.type == 0)
            fill.MoveTo(StoredPoint(entry, 0));
        else if (entry.type == 1)
            fill.LineTo(StoredPoint(entry, 0));
        else if (entry.type == 2)
            fill.CurveTo(StoredPoint(entry, 0), StoredPoint(entry, 1), StoredPoint(entry, 2));
        else
            fill.ClosePath();
    }
    fill.ClosePath();
    if (fill.onEdge) return 1;
    if (state->style.fillRule == 0) return fill.winding != 0;
    if (state->style.fillRule == 1) return fill.winding & 1;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI cairo_clip_extents(CairoContext* context, double* x1, double* y1,
                                  double* x2, double* y2) {
    if (x1) *x1 = 0.0;
    if (y1) *y1 = 0.0;
    if (x2) *x2 = 0.0;
    if (y2) *y2 = 0.0;
    auto* state = Valid(context);
    if (!state) return;
    int width = 0;
    int height = 0;
    if (!CairoSurfacePixelExtents(state->target, width, height))
        NotImplemented_nid_no_patch(__func__);
    CairoMatrix device = DeviceTransform(state);
    if (cairo_matrix_invert(&device) != kSuccess) NotImplemented_nid_no_patch(__func__);
    double px1 = 0;
    double py1 = 0;
    double px2 = width;
    double py2 = height;
    if (!IsIdentity(device) || !IsIdentity(state->style.inverse)) {
        CairoMatrix inverse{};
        cairo_matrix_multiply(&inverse, &device, &state->style.inverse);
        TransformBoundingBox(inverse, px1, py1, px2, py2);
    }
    if (x1) *x1 = px1;
    if (y1) *y1 = py1;
    if (x2) *x2 = px2;
    if (y2) *y2 = py2;
}

}

#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"

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
constexpr int kNoCurrentPoint = 4;
constexpr int kInvalidRestore = 2;
constexpr int kInvalidMatrix = 5;
constexpr int kNullPointer = 7;
constexpr int kInvalidDash = 19;
constexpr int kInvalidPathData = 9;

constexpr int kOperatorOver = 2;
constexpr double kDefaultFontSize = 10.0;
constexpr double kTolerance = 0.1;
constexpr double kPi = 3.14159265358979323846;
constexpr double kFixedMagic = 26388279066624.0;
constexpr double kFixedMaxDouble = 2147483647.0 / 256.0;
constexpr double kFixedMinDouble = -2147483648.0 / 256.0;
constexpr int kMaxFullCircles = 65536;

constexpr CairoMatrix kIdentity{1, 0, 0, 1, 0, 0};

struct GraphicsStyle {
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
    int op{kOperatorOver};
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
    bool hasCurrent{};
    double currentX{}, currentY{};
    double startX{}, startY{};
    bool hasExtents{};
    double extents[4]{};

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

void UserToBackendDistance(const ContextStorage* state, double& x, double& y) {
    if (BackendIsIdentity(state)) return;
    cairo_matrix_transform_distance(&state->matrix, &x, &y);
    const CairoMatrix device = DeviceTransform(state);
    cairo_matrix_transform_distance(&device, &x, &y);
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

void BoxAdd(ContextStorage* state, double x, double y) {
    double* box = state->extents;
    if (x < box[0])
        box[0] = x;
    else if (x > box[2])
        box[2] = x;
    if (y < box[1])
        box[1] = y;
    else if (y > box[3])
        box[3] = y;
}

bool BoxContains(const ContextStorage* state, double x, double y) {
    const double* box = state->extents;
    return box[0] <= x && x <= box[2] && box[1] <= y && y <= box[3];
}

void FindExtremes(double a, double b, double c, double* t, int& count) {
    auto add = [&](double value) {
        if (0 < value && value < 1) t[count++] = value;
    };
    if (a == 0) {
        if (b != 0) add(-c / (2 * b));
        return;
    }
    const double b2 = b * b;
    const double delta = b2 - a * c;
    if (delta > 0) {
        bool feasible;
        const double ab2 = 2 * a * b;
        if (ab2 >= 0)
            feasible = delta > b2 && delta < a * a + b2 + ab2;
        else if (-b / a >= 1)
            feasible = delta < b2 && delta > a * a + b2 + ab2;
        else
            feasible = delta < b2 || delta < a * a + b2 + ab2;
        if (feasible) {
            const double root = std::sqrt(delta);
            add((-b - root) / a);
            add((-b + root) / a);
        }
    } else if (delta == 0) {
        add(-b / a);
    }
}

void BoxAddCurve(ContextStorage* state, const double* a, const double* b,
                 const double* c, const double* d) {
    BoxAdd(state, d[0], d[1]);
    if (BoxContains(state, b[0], b[1]) && BoxContains(state, c[0], c[1])) return;
    double t[4];
    int count = 0;
    FindExtremes(-a[0] + 3 * b[0] - 3 * c[0] + d[0], a[0] - 2 * b[0] + c[0], -a[0] + b[0], t, count);
    FindExtremes(-a[1] + 3 * b[1] - 3 * c[1] + d[1], a[1] - 2 * b[1] + c[1], -a[1] + b[1], t, count);
    BoxAdd(state, a[0], a[1]);
    for (int i = 0; i < count; ++i) {
        const double t10 = t[i];
        const double t01 = 1 - t10;
        const double t20 = t10 * t10;
        const double t02 = t01 * t01;
        const double t30 = t20 * t10;
        const double t213 = t20 * t01 * 3;
        const double t123 = t10 * t02 * 3;
        const double t03 = t01 * t02;
        const double x = a[0] * t03 + b[0] * t123 + c[0] * t213 + d[0] * t30;
        const double y = a[1] * t03 + b[1] * t123 + c[1] * t213 + d[1] * t30;
        BoxAdd(state, Quantize(x), Quantize(y));
    }
    BoxAdd(state, d[0], d[1]);
}

void ApplyPendingMove(ContextStorage* state) {
    if (!state->path || state->path->empty() || state->path->back().type != 0) return;
    const double x = state->path->back().points[0];
    const double y = state->path->back().points[1];
    if (state->hasExtents) {
        BoxAdd(state, x, y);
    } else {
        state->extents[0] = state->extents[2] = x;
        state->extents[1] = state->extents[3] = y;
        state->hasExtents = true;
    }
}

const double* Endpoint(const PathEntry& entry) {
    return entry.type == 2 ? entry.points + 4 : entry.points;
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
    ApplyPendingMove(state);
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
    state->currentX = x;
    state->currentY = y;
    BoxAdd(state, x, y);
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
    ApplyPendingMove(state);
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
    const double current[2] = {state->currentX, state->currentY};
    BoxAddCurve(state, current, command.points, command.points + 2, command.points + 4);
    state->currentX = x3;
    state->currentY = y3;
    Record(state, command);
}

void ClosePath(ContextStorage* state) {
    if (!state->hasCurrent) return;
    AppendLine(state, state->startX, state->startY);
    if (state->status != kSuccess) return;
    if (state->path->back().type == 1) state->path->pop_back();
    PathEntry close{};
    close.type = 3;
    if (!Record(state, close)) return;
    AppendMove(state, state->startX, state->startY);
}

void RelativeLine(ContextStorage* state, double dx, double dy) {
    if (state->status != kSuccess) return;
    if (!state->hasCurrent) {
        state->status = kNoCurrentPoint;
        return;
    }
    UserToBackendDistance(state, dx, dy);
    const auto x = static_cast<std::int32_t>(static_cast<std::uint32_t>(ToFixed(state->currentX)) +
                                             static_cast<std::uint32_t>(FixedFromDouble(dx)));
    const auto y = static_cast<std::int32_t>(static_cast<std::uint32_t>(ToFixed(state->currentY)) +
                                             static_cast<std::uint32_t>(FixedFromDouble(dy)));
    AppendLine(state, FixedToDouble(x), FixedToDouble(y));
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

void TransformBoundingBox(const CairoMatrix& matrix, double& x1, double& y1,
                          double& x2, double& y2) {
    if (matrix.xy == 0. && matrix.yx == 0.) {
        if (matrix.xx != 1.) {
            const double a = x1 * matrix.xx;
            const double b = x2 * matrix.xx;
            if (a < b) {
                x1 = a;
                x2 = b;
            } else {
                x1 = b;
                x2 = a;
            }
        }
        if (matrix.x0 != 0.) {
            x1 += matrix.x0;
            x2 += matrix.x0;
        }
        if (matrix.yy != 1.) {
            const double a = y1 * matrix.yy;
            const double b = y2 * matrix.yy;
            if (a < b) {
                y1 = a;
                y2 = b;
            } else {
                y1 = b;
                y2 = a;
            }
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
    double minX = quadX[0];
    double maxX = quadX[0];
    double minY = quadY[0];
    double maxY = quadY[0];
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

void APS5_VABI cairo_set_operator(CairoContext* context, int op) {
    auto* state = Valid(context);
    if (state) state->style.op = op;
}

int APS5_VABI cairo_get_operator(CairoContext* context) {
    auto* state = Valid(context);
    return state ? state->style.op : kOperatorOver;
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

void APS5_VABI cairo_new_path(CairoContext* context) {
    auto* state = Valid(context);
    if (!state) return;
    if (state->path) state->path->clear();
    state->hasCurrent = false;
    state->currentX = state->currentY = 0;
    state->startX = state->startY = 0;
    state->hasExtents = false;
    std::fill(std::begin(state->extents), std::end(state->extents), 0.0);
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
    if (state) ClosePath(state);
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

void APS5_VABI cairo_rectangle(CairoContext* context, double x, double y,
                               double width, double height) {
    auto* state = Valid(context);
    if (!state) return;
    DevicePoint(state, x, y);
    if (!AppendMove(state, x, y)) return;
    RelativeLine(state, width, 0);
    RelativeLine(state, 0, height);
    RelativeLine(state, -width, 0);
    if (state->status == kSuccess) ClosePath(state);
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

void APS5_VABI cairo_path_extents(CairoContext* context, double* x1, double* y1,
                                  double* x2, double* y2) {
    auto* state = Valid(context);
    double box[4] = {0, 0, 0, 0};
    if (state && state->hasExtents) {
        std::copy_n(state->extents, 4, box);
        const CairoMatrix device = DeviceInverse(state);
        if (!IsIdentity(device) || !IsIdentity(state->style.inverse)) {
            CairoMatrix inverse{};
            cairo_matrix_multiply(&inverse, &device, &state->style.inverse);
            TransformBoundingBox(inverse, box[0], box[1], box[2], box[3]);
        }
    }
    if (x1) *x1 = box[0];
    if (y1) *y1 = box[1];
    if (x2) *x2 = box[2];
    if (y2) *y2 = box[3];
}

}

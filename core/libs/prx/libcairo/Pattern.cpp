#include "prx/libcairo/include/Pattern.hpp"

#include <cmath>
#include <cstring>
#include <new>

namespace {

constexpr int kSuccess = 0;
constexpr int kNoMemory = 1;
constexpr int kNullPointer = 7;
constexpr int kPatternTypeMismatch = 14;
constexpr int kInvalidIndex = 21;
constexpr int kInvalidMeshConstruction = 36;

constexpr int kSolid = 0;
constexpr int kSurface = 1;
constexpr int kLinear = 2;
constexpr int kRadial = 3;
constexpr int kMesh = 4;

constexpr int kExtendNone = 0;
constexpr int kExtendRepeat = 1;
constexpr int kExtendPad = 3;
constexpr int kFilterNearest = 3;

constexpr int kInvalidReference = -1;

constexpr int kMeshPathI[12] = {0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 2, 1};
constexpr int kMeshPathJ[12] = {0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0};
constexpr int kMeshControlI[4] = {1, 1, 2, 2};
constexpr int kMeshControlJ[4] = {1, 2, 2, 1};

CairoPattern* Handle(CairoPatternState* state) {
    return reinterpret_cast<CairoPattern*>(state);
}

CairoPatternState* State(CairoPattern* pattern) {
    return reinterpret_cast<CairoPatternState*>(pattern);
}

double Restrict(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

std::uint16_t ToShort(double value) {
    const double scaled = value * 65535.0 + 0.5;
    return scaled >= 0 && scaled < 65536.0 ? static_cast<std::uint16_t>(scaled) : 0;
}

CairoColor StopColor(double red, double green, double blue, double alpha) {
    return {red, green, blue, alpha, ToShort(red), ToShort(green), ToShort(blue), ToShort(alpha)};
}

CairoPatternState* StaticPattern(int status, int filter, int extend, double alpha) {
    auto* state = new CairoPatternState;
    state->references.store(kInvalidReference, std::memory_order_relaxed);
    state->status = status;
    state->type = kSolid;
    state->filter = filter;
    state->extend = extend;
    if (alpha != 0) state->color = CairoColorFromRgba(0, 0, 0, alpha);
    return state;
}

CairoPattern* Nil() {
    static CairoPatternState* const nil = StaticPattern(kNoMemory, 1, kExtendPad, 0);
    return Handle(nil);
}

CairoPattern* NilNullPointer() {
    static CairoPatternState* const nil = StaticPattern(kNullPointer, 1, kExtendPad, 0);
    return Handle(nil);
}

void SetError(CairoPatternState* state, int status) {
    if (status != kSuccess && state->status == kSuccess) state->status = status;
}

CairoPatternState* NewPattern(int type) {
    auto* state = new (std::nothrow) CairoPatternState;
    if (!state) return nullptr;
    state->type = type;
    state->extend = type == kSurface ? kExtendNone : kExtendPad;
    return state;
}

CairoPattern* CreateSolid(const CairoColor& color) {
    auto* state = NewPattern(kSolid);
    if (!state) return Nil();
    state->color = color;
    return Handle(state);
}

CairoPattern* CreateInError(int status) {
    if (status == kNoMemory) return Nil();
    CairoPattern* pattern = CreateSolid(CairoColorFromRgba(0, 0, 0, 1));
    if (State(pattern)->status == kSuccess) SetError(State(pattern), status);
    return pattern;
}

CairoPatternState* Usable(CairoPattern* pattern) {
    if (!pattern) return nullptr;
    auto* state = State(pattern);
    return state->status == kSuccess ? state : nullptr;
}

CairoPatternState* MeshInConstruction(CairoPattern* pattern) {
    auto* state = Usable(pattern);
    if (!state) return nullptr;
    if (state->type != kMesh) {
        SetError(state, kPatternTypeMismatch);
        return nullptr;
    }
    if (state->currentPatch < 0) {
        SetError(state, kInvalidMeshConstruction);
        return nullptr;
    }
    return state;
}

CairoMeshPatch& Current(CairoPatternState* state) {
    return state->patches[static_cast<std::size_t>(state->currentPatch)];
}

void CalculateControlPoint(CairoMeshPatch& patch, int controlPoint) {
    CairoPointDouble* p[3][3];
    const int ci = kMeshControlI[controlPoint];
    const int cj = kMeshControlJ[controlPoint];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            p[i][j] = &patch.points[ci ^ i][cj ^ j];

    p[0][0]->x = (-4 * p[1][1]->x
                  + 6 * (p[1][0]->x + p[0][1]->x)
                  - 2 * (p[1][2]->x + p[2][1]->x)
                  + 3 * (p[2][0]->x + p[0][2]->x)
                  - 1 * p[2][2]->x) * (1. / 9);
    p[0][0]->y = (-4 * p[1][1]->y
                  + 6 * (p[1][0]->y + p[0][1]->y)
                  - 2 * (p[1][2]->y + p[2][1]->y)
                  + 3 * (p[2][0]->y + p[0][2]->y)
                  - 1 * p[2][2]->y) * (1. / 9);
}

} // namespace

CairoPatternState* CairoPatternInternal(CairoPattern* pattern) {
    return State(pattern);
}

CairoPattern* CairoPatternBlack() {
    static CairoPatternState* const black = StaticPattern(kSuccess, kFilterNearest, kExtendRepeat, 1);
    return Handle(black);
}

CairoColor CairoColorFromRgba(double red, double green, double blue, double alpha) {
    return {red, green, blue, alpha,
            ToShort(red * alpha), ToShort(green * alpha), ToShort(blue * alpha), ToShort(alpha)};
}

bool CairoColorEqual(const CairoColor& a, const CairoColor& b) {
    if (&a == &b) return true;
    if (a.alphaShort != b.alphaShort) return false;
    if (a.alphaShort == 0) return true;
    return a.redShort == b.redShort && a.greenShort == b.greenShort && a.blueShort == b.blueShort;
}

CairoPattern* CairoPatternCreateRgba(double red, double green, double blue, double alpha) {
    return CreateSolid(CairoColorFromRgba(Restrict(red), Restrict(green), Restrict(blue),
                                          Restrict(alpha)));
}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_for_surface(CairoSurface* surface) {
    if (!surface) return NilNullPointer();
    const int status = cairo_surface_status(surface);
    if (status != kSuccess) return CreateInError(status);
    auto* state = NewPattern(kSurface);
    if (!state) return Nil();
    state->surface = cairo_surface_reference(surface);
    return Handle(state);
}

CairoPattern* APS5_VABI cairo_pattern_create_linear(double x0, double y0, double x1, double y1) {
    auto* state = NewPattern(kLinear);
    if (!state) return Nil();
    state->points[0] = {x0, y0};
    state->points[1] = {x1, y1};
    return Handle(state);
}

CairoPattern* APS5_VABI cairo_pattern_create_radial(double cx0, double cy0, double radius0,
                                                    double cx1, double cy1, double radius1) {
    auto* state = NewPattern(kRadial);
    if (!state) return Nil();
    state->points[0] = {cx0, cy0};
    state->radii[0] = std::fabs(radius0);
    state->points[1] = {cx1, cy1};
    state->radii[1] = std::fabs(radius1);
    return Handle(state);
}

CairoPattern* APS5_VABI cairo_pattern_create_mesh() {
    auto* state = NewPattern(kMesh);
    if (!state) return Nil();
    return Handle(state);
}

CairoPattern* APS5_VABI cairo_pattern_reference(CairoPattern* pattern) {
    if (!pattern || State(pattern)->references.load(std::memory_order_relaxed) == kInvalidReference)
        return pattern;
    State(pattern)->references.fetch_add(1, std::memory_order_relaxed);
    return pattern;
}

void APS5_VABI cairo_pattern_destroy(CairoPattern* pattern) {
    if (!pattern || State(pattern)->references.load(std::memory_order_relaxed) == kInvalidReference)
        return;
    auto* state = State(pattern);
    if (state->references.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
    if (state->type == kSurface) cairo_surface_destroy(state->surface);
    delete state;
}

void APS5_VABI cairo_pattern_add_color_stop_rgba(CairoPattern* pattern, double offset,
                                                 double red, double green, double blue,
                                                 double alpha) {
    auto* state = Usable(pattern);
    if (!state) return;
    if (state->type != kLinear && state->type != kRadial) {
        SetError(state, kPatternTypeMismatch);
        return;
    }
    offset = Restrict(offset);
    const CairoGradientStop stop{offset, StopColor(Restrict(red), Restrict(green),
                                                   Restrict(blue), Restrict(alpha))};
    auto position = state->stops.begin();
    while (position != state->stops.end() && !(offset < position->offset)) ++position;
    try {
        state->stops.insert(position, stop);
    } catch (const std::bad_alloc&) {
        SetError(state, kNoMemory);
    }
}

void APS5_VABI cairo_pattern_set_matrix(CairoPattern* pattern, const CairoMatrix* matrix) {
    auto* state = Usable(pattern);
    if (!state || !matrix) return;
    if (std::memcmp(&state->matrix, matrix, sizeof(CairoMatrix)) == 0) return;
    state->matrix = *matrix;
    CairoMatrix inverse = *matrix;
    SetError(state, cairo_matrix_invert(&inverse));
}

void APS5_VABI cairo_pattern_set_filter(CairoPattern* pattern, int filter) {
    if (auto* state = Usable(pattern)) state->filter = filter;
}

void APS5_VABI cairo_pattern_set_extend(CairoPattern* pattern, int extend) {
    if (auto* state = Usable(pattern)) state->extend = extend;
}

void APS5_VABI cairo_mesh_pattern_begin_patch(CairoPattern* pattern) {
    auto* state = Usable(pattern);
    if (!state) return;
    if (state->type != kMesh) {
        SetError(state, kPatternTypeMismatch);
        return;
    }
    if (state->currentPatch >= 0) {
        SetError(state, kInvalidMeshConstruction);
        return;
    }
    try {
        state->patches.emplace_back();
    } catch (const std::bad_alloc&) {
        SetError(state, kNoMemory);
        return;
    }
    state->currentPatch = static_cast<int>(state->patches.size() - 1);
    state->currentSide = -2;
    for (int i = 0; i < 4; ++i) {
        state->hasControlPoint[i] = false;
        state->hasColor[i] = false;
    }
}

void APS5_VABI cairo_mesh_pattern_end_patch(CairoPattern* pattern) {
    auto* state = MeshInConstruction(pattern);
    if (!state) return;
    if (state->currentSide == -2) {
        SetError(state, kInvalidMeshConstruction);
        return;
    }
    while (state->currentSide < 3) {
        const CairoPointDouble start = Current(state).points[0][0];
        cairo_mesh_pattern_line_to(pattern, start.x, start.y);
        const int corner = state->currentSide + 1;
        if (corner < 4 && !state->hasColor[corner]) {
            Current(state).colors[corner] = Current(state).colors[0];
            state->hasColor[corner] = true;
        }
    }
    for (int i = 0; i < 4; ++i)
        if (!state->hasControlPoint[i]) CalculateControlPoint(Current(state), i);
    for (int i = 0; i < 4; ++i)
        if (!state->hasColor[i]) Current(state).colors[i] = CairoColor{};
    state->currentPatch = -1;
}

void APS5_VABI cairo_mesh_pattern_move_to(CairoPattern* pattern, double x, double y) {
    auto* state = MeshInConstruction(pattern);
    if (!state) return;
    if (state->currentSide >= 0) {
        SetError(state, kInvalidMeshConstruction);
        return;
    }
    state->currentSide = -1;
    Current(state).points[0][0] = {x, y};
}

void APS5_VABI cairo_mesh_pattern_curve_to(CairoPattern* pattern, double x1, double y1,
                                           double x2, double y2, double x3, double y3) {
    auto* state = MeshInConstruction(pattern);
    if (!state) return;
    if (state->currentSide == 3) {
        SetError(state, kInvalidMeshConstruction);
        return;
    }
    if (state->currentSide == -2) cairo_mesh_pattern_move_to(pattern, x1, y1);
    ++state->currentSide;
    int point = 3 * state->currentSide;
    auto& patch = Current(state);
    ++point;
    patch.points[kMeshPathI[point]][kMeshPathJ[point]] = {x1, y1};
    ++point;
    patch.points[kMeshPathI[point]][kMeshPathJ[point]] = {x2, y2};
    ++point;
    if (point < 12) patch.points[kMeshPathI[point]][kMeshPathJ[point]] = {x3, y3};
}

void APS5_VABI cairo_mesh_pattern_line_to(CairoPattern* pattern, double x, double y) {
    auto* state = MeshInConstruction(pattern);
    if (!state) return;
    if (state->currentSide == 3) {
        SetError(state, kInvalidMeshConstruction);
        return;
    }
    if (state->currentSide == -2) {
        cairo_mesh_pattern_move_to(pattern, x, y);
        return;
    }
    const int last = 3 * (state->currentSide + 1);
    const CairoPointDouble lastPoint = Current(state).points[kMeshPathI[last]][kMeshPathJ[last]];
    cairo_mesh_pattern_curve_to(pattern,
                                (2 * lastPoint.x + x) * (1. / 3),
                                (2 * lastPoint.y + y) * (1. / 3),
                                (lastPoint.x + 2 * x) * (1. / 3),
                                (lastPoint.y + 2 * y) * (1. / 3),
                                x, y);
}

void APS5_VABI cairo_mesh_pattern_set_corner_color_rgba(CairoPattern* pattern,
                                                        unsigned int cornerNum,
                                                        double red, double green,
                                                        double blue, double alpha) {
    auto* state = Usable(pattern);
    if (!state) return;
    if (state->type != kMesh) {
        SetError(state, kPatternTypeMismatch);
        return;
    }
    if (cornerNum > 3) {
        SetError(state, kInvalidIndex);
        return;
    }
    if (state->currentPatch < 0) {
        SetError(state, kInvalidMeshConstruction);
        return;
    }
    Current(state).colors[cornerNum] = StopColor(Restrict(red), Restrict(green),
                                                 Restrict(blue), Restrict(alpha));
    state->hasColor[cornerNum] = true;
}

}

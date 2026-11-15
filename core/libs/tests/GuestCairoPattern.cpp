#include "prx/libcairo/libcairo.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdio>
#include <limits>
#include <utility>

namespace {

int destroyedSurfaces = 0;
CairoUserDataKey surfaceKey{};

void APS5_VABI CountSurface(void*) {
    ++destroyedSurfaces;
}

CairoSurface* CountedSurface() {
    CairoSurface* surface = cairo_image_surface_create(0, 2, 2);
    assert(cairo_surface_set_user_data(surface, &surfaceKey, &surfaceKey, CountSurface) == 0);
    return surface;
}

CairoContext* NewContext() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    return context;
}

int Status(CairoContext* context) {
    CairoPath* path = cairo_copy_path(context);
    const int status = path->status;
    cairo_path_destroy(path);
    return status;
}

int PatternStatus(CairoPattern* pattern) {
    CairoContext* context = NewContext();
    cairo_set_source(context, pattern);
    const int status = Status(context);
    cairo_destroy(context);
    return status;
}

void CheckLifetimeAndDefaults() {
    CairoPattern* linear = cairo_pattern_create_linear(1, 2, 3, 4);
    assert(PatternStatus(linear) == 0);
    assert(cairo_pattern_reference(linear) == linear);
    cairo_pattern_destroy(linear);
    assert(PatternStatus(linear) == 0);
    cairo_pattern_destroy(linear);

    CairoPattern* radial = cairo_pattern_create_radial(1, 2, -3, 4, 5, -0.0);
    assert(PatternStatus(radial) == 0);
    cairo_pattern_destroy(radial);

    assert(cairo_pattern_reference(nullptr) == nullptr);
    cairo_pattern_destroy(nullptr);
}

void CheckSurfacePatternLifetime() {
    destroyedSurfaces = 0;
    CairoSurface* surface = CountedSurface();
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    cairo_surface_destroy(surface);
    assert(destroyedSurfaces == 0);
    cairo_pattern_reference(pattern);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 0);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 1);

    CairoPattern* nullPattern = cairo_pattern_create_for_surface(nullptr);
    assert(cairo_pattern_reference(nullPattern) == nullPattern);
    cairo_pattern_destroy(nullPattern);
    cairo_pattern_set_extend(nullPattern, 1);
    cairo_pattern_set_filter(nullPattern, 0);
    assert(PatternStatus(nullPattern) == 7);
}

void CheckColorStops() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0.25, 0.5, 0.75, 1);
    cairo_pattern_add_color_stop_rgba(pattern, 0.2, 2, -1, 0.5, 0.5);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 1, 1, 1, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 7, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(pattern, -1, 0, 0, 0, 1);
    assert(PatternStatus(pattern) == 0);
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_radial(0, 0, 1, 0, 0, 2);
    cairo_pattern_add_color_stop_rgba(pattern, 0.3, 1, 0, 0, 1);
    assert(PatternStatus(pattern) == 0);
    cairo_pattern_destroy(pattern);

    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_pattern_add_color_stop_rgba(mesh, 0.3, 1, 0, 0, 1);
    assert(PatternStatus(mesh) == 14);
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_pattern_set_extend(mesh, 1);
    assert(PatternStatus(mesh) == 14);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_line_to(mesh, 1, 1);
    cairo_pattern_add_color_stop_rgba(mesh, 0, 0, 0, 0, 1);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
    CairoPattern* surfacePattern = cairo_pattern_create_for_surface(surface);
    cairo_surface_destroy(surface);
    cairo_pattern_add_color_stop_rgba(surfacePattern, 0.3, 1, 0, 0, 1);
    assert(PatternStatus(surfacePattern) == 14);
    cairo_pattern_destroy(surfacePattern);
}

void CheckMatrixFilterExtend() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    cairo_pattern_set_filter(pattern, 5);
    cairo_pattern_set_extend(pattern, 2);
    const CairoMatrix scale{2, 0, 0, 4, 1, 1};
    cairo_pattern_set_matrix(pattern, &scale);
    assert(PatternStatus(pattern) == 0);
    const CairoMatrix singular{1, 2, 2, 4, 5, 6};
    cairo_pattern_set_matrix(pattern, &singular);
    assert(PatternStatus(pattern) == 5);
    cairo_pattern_set_matrix(pattern, &scale);
    cairo_pattern_set_filter(pattern, 0);
    cairo_pattern_set_extend(pattern, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 0, 0, 0, 0, 0);
    assert(PatternStatus(pattern) == 5);
    cairo_pattern_destroy(pattern);

    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::array<std::pair<CairoMatrix, int>, 4> nonFinite{{
        {{inf, 1, 1, 1, 0, 0}, 0},
        {{nan, 0, 0, 1, 0, 0}, 0},
        {{nan, 1, 1, 1, 0, 0}, 5},
        {{1, 0, 0, 1, inf, nan}, 0},
    }};
    for (const auto& [matrix, status] : nonFinite) {
        pattern = cairo_pattern_create_linear(0, 0, 1, 0);
        cairo_pattern_set_matrix(pattern, &matrix);
        assert(PatternStatus(pattern) == status);
        cairo_pattern_destroy(pattern);
    }
}

void CheckContextSources() {
    CairoPattern* broken = cairo_pattern_create_linear(0, 0, 1, 1);
    const CairoMatrix singular{0, 0, 0, 0, 0, 0};
    cairo_pattern_set_matrix(broken, &singular);
    CairoContext* context = NewContext();
    cairo_set_source(context, broken);
    assert(Status(context) == 5);
    cairo_destroy(context);
    cairo_pattern_destroy(broken);
}

} // namespace

int main() {
    CheckLifetimeAndDefaults();
    CheckSurfacePatternLifetime();
    CheckColorStops();
    CheckMatrixFilterExtend();
    CheckContextSources();
    std::puts("guest_cairo_pattern: ok");
    return 0;
}

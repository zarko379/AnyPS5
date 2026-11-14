#include "prx/libcairo/libcairo.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdio>
#include <limits>

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

    CairoPattern* mesh = cairo_pattern_create_mesh();
    assert(PatternStatus(mesh) == 0);
    cairo_pattern_destroy(mesh);

    assert(cairo_pattern_reference(nullptr) == nullptr);
    cairo_pattern_destroy(nullptr);
}

void CheckSurfacePatterns() {
    destroyedSurfaces = 0;
    CairoSurface* surface = CountedSurface();
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    assert(PatternStatus(pattern) == 0);
    cairo_surface_destroy(surface);
    assert(destroyedSurfaces == 0);
    cairo_pattern_reference(pattern);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 0);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 1);

    CairoPattern* nullPattern = cairo_pattern_create_for_surface(nullptr);
    assert(PatternStatus(nullPattern) == 7);
    assert(cairo_pattern_reference(nullPattern) == nullPattern);
    cairo_pattern_destroy(nullPattern);
    cairo_pattern_set_extend(nullPattern, 1);
    cairo_pattern_set_filter(nullPattern, 0);
    assert(PatternStatus(nullPattern) == 7);
    assert(cairo_pattern_create_for_surface(nullptr) == nullPattern);

    CairoSurface* invalid = cairo_image_surface_create(99, 4, 4);
    CairoPattern* failed = cairo_pattern_create_for_surface(invalid);
    assert(PatternStatus(failed) == 16);
    cairo_pattern_destroy(failed);
    cairo_surface_destroy(invalid);

    CairoSurface* badSize = cairo_image_surface_create(0, -1, 4);
    failed = cairo_pattern_create_for_surface(badSize);
    assert(PatternStatus(failed) == 32);
    cairo_pattern_destroy(failed);
    cairo_surface_destroy(badSize);
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

    pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    const CairoMatrix nonFinite{std::numeric_limits<double>::infinity(), 1, 1, 1, 0, 0};
    cairo_pattern_set_matrix(pattern, &nonFinite);
    assert(PatternStatus(pattern) == 5);
    cairo_pattern_destroy(pattern);
}

void CheckMeshConstruction() {
    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 0, 0);
    cairo_mesh_pattern_line_to(mesh, 3, 0);
    cairo_mesh_pattern_line_to(mesh, 3, 3);
    cairo_mesh_pattern_line_to(mesh, 0, 3);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 0, 1, 0, 0, 1);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 2, 0, 0, 2, 0.5);
    cairo_mesh_pattern_end_patch(mesh);
    assert(PatternStatus(mesh) == 0);

    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 0, 0, 1, 0, 1);
    cairo_mesh_pattern_end_patch(mesh);
    assert(PatternStatus(mesh) == 0);

    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 4, 4);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 0, 0, 0, 1, 0.25);
    cairo_mesh_pattern_end_patch(mesh);
    assert(PatternStatus(mesh) == 0);

    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_line_to(mesh, 5, 6);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 7, 8, 9, 10, 11, 12);
    cairo_mesh_pattern_end_patch(mesh);
    assert(PatternStatus(mesh) == 0);
    cairo_pattern_destroy(mesh);
}

void CheckMeshErrors() {
    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_line_to(mesh, 1, 1);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_move_to(mesh, 1, 1);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_end_patch(mesh);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_begin_patch(mesh);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_end_patch(mesh);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 0, 0);
    cairo_mesh_pattern_line_to(mesh, 1, 0);
    cairo_mesh_pattern_move_to(mesh, 2, 2);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    for (int side = 0; side < 4; ++side) cairo_mesh_pattern_line_to(mesh, side, side + 1);
    cairo_mesh_pattern_line_to(mesh, 9, 9);
    assert(PatternStatus(mesh) == 0);
    cairo_mesh_pattern_line_to(mesh, 9, 9);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 0, 0);
    for (int side = 0; side < 4; ++side) cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 4, 0, 0, 0, 1);
    assert(PatternStatus(mesh) == 21);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 3, 0, 0, 0, 1);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);

    CairoPattern* linear = cairo_pattern_create_linear(0, 0, 1, 1);
    cairo_mesh_pattern_set_corner_color_rgba(linear, 4, 0, 0, 0, 1);
    assert(PatternStatus(linear) == 14);
    cairo_pattern_destroy(linear);

    void (*const operations[])(CairoPattern*) = {
        [](CairoPattern* p) { cairo_mesh_pattern_begin_patch(p); },
        [](CairoPattern* p) { cairo_mesh_pattern_end_patch(p); },
        [](CairoPattern* p) { cairo_mesh_pattern_move_to(p, 0, 0); },
        [](CairoPattern* p) { cairo_mesh_pattern_line_to(p, 0, 0); },
        [](CairoPattern* p) { cairo_mesh_pattern_curve_to(p, 0, 0, 0, 0, 0, 0); },
    };
    for (auto operation : operations) {
        linear = cairo_pattern_create_linear(0, 0, 1, 1);
        operation(linear);
        assert(PatternStatus(linear) == 14);
        cairo_pattern_destroy(linear);
    }

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_line_to(mesh, 1, 1);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 4, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(mesh, 0, 0, 0, 0, 1);
    assert(PatternStatus(mesh) == 36);
    cairo_pattern_destroy(mesh);
}

std::array<int, 8> order{};
int orderCount = 0;
CairoUserDataKey orderKey{};

void APS5_VABI RecordOrder(void* data) {
    order[orderCount++] = *static_cast<int*>(data);
}

void CheckContextFonts() {
    CairoContext* context = NewContext();
    cairo_set_font_size(context, 12.5);
    cairo_set_font_size(context, -3);
    cairo_font_options_t* options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_BEST);
    cairo_set_font_options(context, options);
    cairo_font_options_destroy(options);
    assert(Status(context) == 0);

    cairo_set_font_options(context, cairo_font_options_copy(nullptr));
    assert(Status(context) == 1);
    cairo_destroy(context);

    context = NewContext();
    cairo_set_font_options(context, nullptr);
    assert(Status(context) == 7);
    cairo_destroy(context);
}

void CheckContextSources() {
    destroyedSurfaces = 0;
    CairoSurface* surface = CountedSurface();
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    cairo_surface_destroy(surface);
    CairoContext* context = NewContext();
    cairo_set_source(context, pattern);
    cairo_set_source(context, pattern);
    assert(Status(context) == 0);
    cairo_save(context);
    cairo_set_source_rgb(context, 1, 0, 0);
    cairo_set_source_rgba(context, 2, -1, 0.5, 0.5);
    cairo_restore(context);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 0);
    cairo_set_source_rgba(context, 0, 0, 0, 1);
    assert(destroyedSurfaces == 1 && Status(context) == 0);
    cairo_destroy(context);

    context = NewContext();
    cairo_set_source(context, nullptr);
    assert(Status(context) == 7);
    cairo_destroy(context);

    context = NewContext();
    cairo_set_source_surface(context, nullptr, 0, 0);
    assert(Status(context) == 7);
    cairo_destroy(context);

    CairoPattern* broken = cairo_pattern_create_linear(0, 0, 1, 1);
    const CairoMatrix singular{0, 0, 0, 0, 0, 0};
    cairo_pattern_set_matrix(broken, &singular);
    context = NewContext();
    cairo_set_source(context, broken);
    assert(Status(context) == 5);
    cairo_destroy(context);
    cairo_pattern_destroy(broken);

    destroyedSurfaces = 0;
    surface = CountedSurface();
    context = NewContext();
    cairo_set_source_surface(context, surface, 0, 0);
    cairo_surface_destroy(surface);
    assert(destroyedSurfaces == 0);
    CairoSurface* invalid = cairo_image_surface_create(0, 40000, 1);
    cairo_set_source_surface(context, invalid, 1, 2);
    assert(Status(context) == 32 && destroyedSurfaces == 1);
    cairo_surface_destroy(invalid);
    cairo_destroy(context);
}

void CheckSourceSurfaceLifetime() {
    int ids[3] = {1, 2, 3};
    order.fill(0);
    orderCount = 0;
    CairoSurface* target = cairo_image_surface_create(0, 8, 8);
    CairoSurface* bottom = cairo_image_surface_create(0, 2, 2);
    CairoSurface* top = cairo_image_surface_create(0, 2, 2);
    assert(cairo_surface_set_user_data(target, &orderKey, &ids[0], RecordOrder) == 0);
    assert(cairo_surface_set_user_data(bottom, &orderKey, &ids[1], RecordOrder) == 0);
    assert(cairo_surface_set_user_data(top, &orderKey, &ids[2], RecordOrder) == 0);
    CairoContext* context = cairo_create(target);
    cairo_surface_destroy(target);
    cairo_set_source_surface(context, bottom, 0.5, -0.25);
    cairo_surface_destroy(bottom);
    cairo_save(context);
    cairo_set_source_surface(context, top, 0, 0);
    cairo_surface_destroy(top);
    assert(orderCount == 0 && Status(context) == 0);
    cairo_destroy(context);
    assert(orderCount == 3 && order[0] == 3 && order[1] == 1 && order[2] == 2);

    order.fill(0);
    orderCount = 0;
    context = NewContext();
    CairoSurface* source = cairo_image_surface_create(0, 2, 2);
    assert(cairo_surface_set_user_data(source, &orderKey, &ids[1], RecordOrder) == 0);
    cairo_set_source_surface(context, source, std::numeric_limits<double>::infinity(), 0);
    cairo_surface_destroy(source);
    assert(orderCount == 0 && Status(context) == 0);
    cairo_set_source_rgb(context, 0, 0, 1);
    assert(orderCount == 1 && order[0] == 2);
    cairo_destroy(context);
}

} // namespace

int main() {
    CheckLifetimeAndDefaults();
    CheckSurfacePatterns();
    CheckColorStops();
    CheckMatrixFilterExtend();
    CheckMeshConstruction();
    CheckMeshErrors();
    CheckContextFonts();
    CheckContextSources();
    CheckSourceSurfaceLifetime();
    std::puts("guest_cairo_pattern: ok");
    return 0;
}

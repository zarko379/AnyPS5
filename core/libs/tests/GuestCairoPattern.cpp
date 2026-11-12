#include "prx/libcairo/include/Path.hpp"
#include "prx/libcairo/include/Pattern.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

CairoPatternState* Inspect(CairoPattern* pattern) {
    return reinterpret_cast<CairoPatternState*>(pattern);
}

void Near(double actual, double expected) {
    assert(std::abs(actual - expected) <= 1e-12 * (1 + std::abs(expected)));
}

int destroyedSurfaces = 0;
CairoUserDataKey surfaceKey{};

void APS5_VABI CountSurface(void*) {
    ++destroyedSurfaces;
}

void CheckLifetimeAndDefaults() {
    CairoPattern* linear = cairo_pattern_create_linear(1, 2, 3, 4);
    auto* state = Inspect(linear);
    assert(state->status == 0 && state->type == 2 && state->references.load() == 1);
    assert(state->extend == 3 && state->filter == 1 && state->dither == 0);
    assert(state->matrix.xx == 1 && state->matrix.yx == 0 && state->matrix.xy == 0);
    assert(state->matrix.yy == 1 && state->matrix.x0 == 0 && state->matrix.y0 == 0);
    assert(state->points[0].x == 1 && state->points[0].y == 2);
    assert(state->points[1].x == 3 && state->points[1].y == 4);
    assert(cairo_pattern_reference(linear) == linear && state->references.load() == 2);
    cairo_pattern_destroy(linear);
    assert(state->references.load() == 1);
    cairo_pattern_destroy(linear);

    CairoPattern* radial = cairo_pattern_create_radial(1, 2, -3, 4, 5, -0.0);
    state = Inspect(radial);
    assert(state->type == 3 && state->extend == 3);
    assert(state->radii[0] == 3 && state->radii[1] == 0 && !std::signbit(state->radii[1]));
    assert(state->points[1].x == 4 && state->points[1].y == 5);
    cairo_pattern_destroy(radial);

    CairoPattern* mesh = cairo_pattern_create_mesh();
    state = Inspect(mesh);
    assert(state->type == 4 && state->extend == 3 && state->patches.empty());
    assert(state->currentPatch == -1);
    cairo_pattern_destroy(mesh);

    assert(cairo_pattern_reference(nullptr) == nullptr);
    cairo_pattern_destroy(nullptr);
}

void CheckSurfacePatterns() {
    destroyedSurfaces = 0;
    CairoSurface* surface = cairo_image_surface_create(0, 4, 4);
    assert(cairo_surface_set_user_data(surface, &surfaceKey, &surfaceKey, CountSurface) == 0);
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    auto* state = Inspect(pattern);
    assert(state->status == 0 && state->type == 1 && state->extend == 0 && state->filter == 1);
    assert(state->surface == surface);
    cairo_surface_destroy(surface);
    assert(destroyedSurfaces == 0);
    cairo_pattern_reference(pattern);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 0);
    cairo_pattern_destroy(pattern);
    assert(destroyedSurfaces == 1);

    CairoPattern* nullPattern = cairo_pattern_create_for_surface(nullptr);
    state = Inspect(nullPattern);
    assert(state->status == 7 && state->type == 0 && state->references.load() == -1);
    assert(cairo_pattern_reference(nullPattern) == nullPattern && state->references.load() == -1);
    cairo_pattern_destroy(nullPattern);
    cairo_pattern_set_extend(nullPattern, 1);
    cairo_pattern_set_filter(nullPattern, 0);
    assert(state->extend == 3 && state->filter == 1 && state->status == 7);
    assert(cairo_pattern_create_for_surface(nullptr) == nullPattern);

    CairoSurface* invalid = cairo_image_surface_create(99, 4, 4);
    CairoPattern* failed = cairo_pattern_create_for_surface(invalid);
    state = Inspect(failed);
    assert(state->status == 16 && state->type == 0 && state->references.load() == 1);
    assert(state->color.alpha == 1 && state->color.alphaShort == 0xffff);
    assert(state->surface == nullptr);
    cairo_pattern_destroy(failed);

    CairoSurface* badSize = cairo_image_surface_create(0, -1, 4);
    failed = cairo_pattern_create_for_surface(badSize);
    assert(Inspect(failed)->status == 32);
    cairo_pattern_destroy(failed);
}

void CheckColorStops() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0.25, 0.5, 0.75, 1);
    cairo_pattern_add_color_stop_rgba(pattern, 0.2, 2, -1, 0.5, 0.5);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 1, 1, 1, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 7, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(pattern, -1, 0, 0, 0, 1);
    auto& stops = Inspect(pattern)->stops;
    assert(stops.size() == 5);
    assert(stops[0].offset == 0 && stops[1].offset == 0.2 && stops[2].offset == 0.5);
    assert(stops[3].offset == 0.5 && stops[4].offset == 1);
    assert(stops[2].color.red == 0.25 && stops[2].color.alpha == 1);
    assert(stops[3].color.red == 1 && stops[3].color.alpha == 0);
    assert(stops[3].color.redShort == 0xffff && stops[3].color.alphaShort == 0);
    assert(stops[1].color.red == 1 && stops[1].color.green == 0 && stops[1].color.blue == 0.5);
    assert(stops[1].color.blueShort == 32768 && stops[1].color.alphaShort == 32768);
    assert(stops[1].color.redShort == 0xffff);
    assert(Inspect(pattern)->status == 0);
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_radial(0, 0, 1, 0, 0, 2);
    cairo_pattern_add_color_stop_rgba(pattern, 0.3, 1, 0, 0, 1);
    assert(Inspect(pattern)->stops.size() == 1 && Inspect(pattern)->status == 0);
    cairo_pattern_destroy(pattern);

    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_pattern_add_color_stop_rgba(mesh, 0.3, 1, 0, 0, 1);
    assert(Inspect(mesh)->status == 14);
    cairo_mesh_pattern_begin_patch(mesh);
    assert(Inspect(mesh)->status == 14 && Inspect(mesh)->patches.empty());
    cairo_pattern_set_extend(mesh, 1);
    assert(Inspect(mesh)->extend == 3);
    cairo_pattern_destroy(mesh);

    CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
    CairoPattern* surfacePattern = cairo_pattern_create_for_surface(surface);
    cairo_surface_destroy(surface);
    cairo_pattern_add_color_stop_rgba(surfacePattern, 0.3, 1, 0, 0, 1);
    assert(Inspect(surfacePattern)->status == 14);
    cairo_pattern_destroy(surfacePattern);
}

void CheckMatrixFilterExtend() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    auto* state = Inspect(pattern);
    cairo_pattern_set_filter(pattern, 5);
    cairo_pattern_set_extend(pattern, 2);
    assert(state->filter == 5 && state->extend == 2);
    const CairoMatrix scale{2, 0, 0, 4, 1, 1};
    cairo_pattern_set_matrix(pattern, &scale);
    assert(state->status == 0 && state->matrix.yy == 4 && state->matrix.x0 == 1);
    cairo_pattern_set_matrix(pattern, nullptr);
    assert(state->status == 0);
    const CairoMatrix singular{1, 2, 2, 4, 5, 6};
    cairo_pattern_set_matrix(pattern, &singular);
    assert(state->status == 5 && state->matrix.xy == 2 && state->matrix.y0 == 6);
    cairo_pattern_set_matrix(pattern, &scale);
    cairo_pattern_set_filter(pattern, 0);
    cairo_pattern_set_extend(pattern, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 0, 0, 0, 0, 0);
    assert(state->matrix.xy == 2 && state->filter == 5 && state->extend == 2);
    assert(state->stops.empty() && state->status == 5);
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    const CairoMatrix nonFinite{std::numeric_limits<double>::infinity(), 1, 1, 1, 0, 0};
    cairo_pattern_set_matrix(pattern, &nonFinite);
    assert(Inspect(pattern)->status == 5);
    cairo_pattern_destroy(pattern);
}

void CheckMeshConstruction() {
    CairoPattern* mesh = cairo_pattern_create_mesh();
    auto* state = Inspect(mesh);
    cairo_mesh_pattern_begin_patch(mesh);
    assert(state->currentPatch == 0 && state->currentSide == -2 && state->patches.size() == 1);
    cairo_mesh_pattern_move_to(mesh, 0, 0);
    cairo_mesh_pattern_line_to(mesh, 3, 0);
    cairo_mesh_pattern_line_to(mesh, 3, 3);
    cairo_mesh_pattern_line_to(mesh, 0, 3);
    assert(state->currentSide == 2);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 0, 1, 0, 0, 1);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 2, 0, 0, 2, 0.5);
    cairo_mesh_pattern_end_patch(mesh);
    assert(state->status == 0 && state->currentPatch == -1);
    const auto& patch = state->patches[0];
    const double expected[4][4][2] = {
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}},
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{0, 3}, {1, 3}, {2, 3}, {3, 3}},
    };
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            Near(patch.points[i][j].x, expected[i][j][0]);
            Near(patch.points[i][j].y, expected[i][j][1]);
        }
    assert(patch.colors[0].red == 1 && patch.colors[0].alpha == 1);
    assert(patch.colors[0].redShort == 0xffff && patch.colors[0].alphaShort == 0xffff);
    assert(patch.colors[1].alpha == 0 && patch.colors[1].alphaShort == 0);
    assert(patch.colors[2].blue == 1 && patch.colors[2].alpha == 0.5);
    assert(patch.colors[2].blueShort == 0xffff && patch.colors[2].alphaShort == 32768);
    assert(patch.colors[3].alpha == 0 && patch.colors[3].red == 0);

    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    assert(state->currentSide == 0);
    auto& second = state->patches[1];
    assert(second.points[0][0].x == 1 && second.points[0][1].x == 1);
    assert(second.points[0][2].x == 2 && second.points[0][3].y == 3);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 0, 0, 1, 0, 1);
    cairo_mesh_pattern_end_patch(mesh);
    assert(state->status == 0 && state->patches.size() == 2);
    assert(state->patches[1].colors[1].alpha == 0 && state->patches[1].colors[1].green == 0);
    for (int corner = 2; corner < 4; ++corner) {
        assert(state->patches[1].colors[corner].green == 1);
        assert(state->patches[1].colors[corner].alpha == 1);
    }
    Near(state->patches[1].points[1][0].x, 1);
    Near(state->patches[1].points[2][0].y, 1);

    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 4, 4);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 0, 0, 0, 1, 0.25);
    cairo_mesh_pattern_end_patch(mesh);
    for (int corner = 0; corner < 4; ++corner) {
        assert(state->patches[2].colors[corner].blue == 1);
        assert(state->patches[2].colors[corner].alpha == 0.25);
        for (int j = 0; j < 4; ++j) {
            Near(state->patches[2].points[corner][j].x, 4);
            Near(state->patches[2].points[corner][j].y, 4);
        }
    }

    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_line_to(mesh, 5, 6);
    assert(state->currentSide == -1);
    assert(state->patches[3].points[0][0].x == 5 && state->patches[3].points[0][0].y == 6);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 7, 8, 9, 10, 11, 12);
    assert(state->currentSide == 3 && state->status == 0);
    assert(state->patches[3].points[2][0].x == 7 && state->patches[3].points[1][0].y == 10);
    assert(state->patches[3].points[0][0].x == 5);
    cairo_mesh_pattern_end_patch(mesh);
    assert(state->status == 0 && state->currentPatch == -1);
    cairo_pattern_destroy(mesh);
}

void CheckMeshErrors() {
    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_line_to(mesh, 1, 1);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_move_to(mesh, 1, 1);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_end_patch(mesh);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_begin_patch(mesh);
    assert(Inspect(mesh)->status == 36 && Inspect(mesh)->patches.size() == 1);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_end_patch(mesh);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 0, 0);
    cairo_mesh_pattern_line_to(mesh, 1, 0);
    cairo_mesh_pattern_move_to(mesh, 2, 2);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    for (int side = 0; side < 4; ++side) cairo_mesh_pattern_line_to(mesh, side, side + 1);
    assert(Inspect(mesh)->currentSide == 2 && Inspect(mesh)->status == 0);
    cairo_mesh_pattern_line_to(mesh, 9, 9);
    assert(Inspect(mesh)->currentSide == 3 && Inspect(mesh)->status == 0);
    cairo_mesh_pattern_line_to(mesh, 9, 9);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(mesh);
    cairo_mesh_pattern_move_to(mesh, 0, 0);
    for (int side = 0; side < 4; ++side) cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    cairo_mesh_pattern_curve_to(mesh, 1, 1, 2, 2, 3, 3);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 4, 0, 0, 0, 1);
    assert(Inspect(mesh)->status == 21);
    cairo_pattern_destroy(mesh);

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 3, 0, 0, 0, 1);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    CairoPattern* linear = cairo_pattern_create_linear(0, 0, 1, 1);
    cairo_mesh_pattern_set_corner_color_rgba(linear, 4, 0, 0, 0, 1);
    assert(Inspect(linear)->status == 14);
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
        assert(Inspect(linear)->status == 14);
        cairo_pattern_destroy(linear);
    }

    mesh = cairo_pattern_create_mesh();
    cairo_mesh_pattern_line_to(mesh, 1, 1);
    cairo_mesh_pattern_set_corner_color_rgba(mesh, 4, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(mesh, 0, 0, 0, 0, 1);
    assert(Inspect(mesh)->status == 36);
    cairo_pattern_destroy(mesh);

    cairo_mesh_pattern_begin_patch(nullptr);
    cairo_mesh_pattern_end_patch(nullptr);
    cairo_mesh_pattern_move_to(nullptr, 0, 0);
    cairo_mesh_pattern_line_to(nullptr, 0, 0);
    cairo_mesh_pattern_curve_to(nullptr, 0, 0, 0, 0, 0, 0);
    cairo_mesh_pattern_set_corner_color_rgba(nullptr, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(nullptr, 0, 0, 0, 0, 0);
    cairo_pattern_set_filter(nullptr, 0);
    cairo_pattern_set_extend(nullptr, 0);
    cairo_pattern_set_matrix(nullptr, nullptr);
}

int Status(CairoContext* context) {
    CairoPath* path = cairo_copy_path(context);
    const int status = path->status;
    cairo_path_destroy(path);
    return status;
}

std::array<int, 8> order{};
int orderCount = 0;
CairoUserDataKey orderKey{};

void APS5_VABI RecordOrder(void* data) {
    order[orderCount++] = *static_cast<int*>(data);
}

CairoContext* NewContext() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    return context;
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
    CairoPattern* linear = cairo_pattern_create_linear(0, 0, 1, 1);
    auto* state = Inspect(linear);
    CairoContext* context = NewContext();
    cairo_set_source(context, linear);
    assert(Status(context) == 0 && state->references.load() == 2);
    cairo_set_source(context, linear);
    assert(state->references.load() == 2);
    cairo_save(context);
    assert(state->references.load() == 3);
    cairo_set_source_rgb(context, 1, 0, 0);
    assert(state->references.load() == 2);
    cairo_set_source_rgba(context, 2, -1, 0.5, 0.5);
    cairo_restore(context);
    assert(state->references.load() == 2);
    cairo_set_source_rgba(context, 0, 0, 0, 1);
    assert(state->references.load() == 1 && Status(context) == 0);
    cairo_set_source(context, linear);
    cairo_destroy(context);
    assert(state->references.load() == 1);

    context = NewContext();
    cairo_set_source(context, nullptr);
    assert(Status(context) == 7);
    cairo_set_source(context, linear);
    assert(state->references.load() == 1);
    cairo_set_source_rgb(context, 1, 1, 1);
    cairo_set_source_surface(context, nullptr, 0, 0);
    assert(Status(context) == 7);
    cairo_destroy(context);

    CairoPattern* broken = cairo_pattern_create_linear(0, 0, 1, 1);
    const CairoMatrix singular{0, 0, 0, 0, 0, 0};
    cairo_pattern_set_matrix(broken, &singular);
    context = NewContext();
    cairo_set_source(context, linear);
    cairo_set_source(context, broken);
    assert(Status(context) == 5 && Inspect(broken)->references.load() == 1);
    assert(state->references.load() == 2);
    cairo_destroy(context);
    assert(state->references.load() == 1);
    cairo_pattern_destroy(broken);

    context = NewContext();
    cairo_set_source_surface(context, nullptr, 0, 0);
    assert(Status(context) == 7);
    cairo_destroy(context);

    context = NewContext();
    cairo_set_source(context, linear);
    CairoSurface* invalid = cairo_image_surface_create(0, 40000, 1);
    cairo_set_source_surface(context, invalid, 1, 2);
    assert(Status(context) == 32 && state->references.load() == 1);
    cairo_destroy(context);
    cairo_pattern_destroy(linear);
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

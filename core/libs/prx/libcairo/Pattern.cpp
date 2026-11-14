#include "prx/libcairo/include/Pattern.hpp"

#include <CairoBackend.h>

#include <bit>

namespace {

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

CairoPattern* Guest(cairo_pattern_t* pattern) {
    return reinterpret_cast<CairoPattern*>(pattern);
}

}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_for_surface(CairoSurface* surface) {
    return Guest(aps5_cairo_pattern_create_for_surface(reinterpret_cast<cairo_surface_t*>(surface)));
}

CairoPattern* APS5_VABI cairo_pattern_create_linear(double x0, double y0, double x1, double y1) {
    return Guest(aps5_cairo_pattern_create_linear(x0, y0, x1, y1));
}

CairoPattern* APS5_VABI cairo_pattern_create_radial(double cx0, double cy0, double radius0,
                                                    double cx1, double cy1, double radius1) {
    return Guest(aps5_cairo_pattern_create_radial(cx0, cy0, radius0, cx1, cy1, radius1));
}

CairoPattern* APS5_VABI cairo_pattern_create_mesh() {
    return Guest(aps5_cairo_pattern_create_mesh());
}

CairoPattern* APS5_VABI cairo_pattern_reference(CairoPattern* pattern) {
    return Guest(aps5_cairo_pattern_reference(Backend(pattern)));
}

void APS5_VABI cairo_pattern_destroy(CairoPattern* pattern) {
    aps5_cairo_pattern_destroy(Backend(pattern));
}

void APS5_VABI cairo_pattern_add_color_stop_rgba(CairoPattern* pattern, double offset,
                                                 double red, double green, double blue,
                                                 double alpha) {
    aps5_cairo_pattern_add_color_stop_rgba(Backend(pattern), offset, red, green, blue, alpha);
}

void APS5_VABI cairo_pattern_set_matrix(CairoPattern* pattern, const CairoMatrix* matrix) {
    const cairo_matrix_t backend = std::bit_cast<cairo_matrix_t>(*matrix);
    aps5_cairo_pattern_set_matrix(Backend(pattern), &backend);
}

void APS5_VABI cairo_pattern_set_filter(CairoPattern* pattern, int filter) {
    aps5_cairo_pattern_set_filter(Backend(pattern), static_cast<cairo_filter_t>(filter));
}

void APS5_VABI cairo_pattern_set_extend(CairoPattern* pattern, int extend) {
    aps5_cairo_pattern_set_extend(Backend(pattern), static_cast<cairo_extend_t>(extend));
}

void APS5_VABI cairo_mesh_pattern_begin_patch(CairoPattern* pattern) {
    aps5_cairo_mesh_pattern_begin_patch(Backend(pattern));
}

void APS5_VABI cairo_mesh_pattern_end_patch(CairoPattern* pattern) {
    aps5_cairo_mesh_pattern_end_patch(Backend(pattern));
}

void APS5_VABI cairo_mesh_pattern_move_to(CairoPattern* pattern, double x, double y) {
    aps5_cairo_mesh_pattern_move_to(Backend(pattern), x, y);
}

void APS5_VABI cairo_mesh_pattern_line_to(CairoPattern* pattern, double x, double y) {
    aps5_cairo_mesh_pattern_line_to(Backend(pattern), x, y);
}

void APS5_VABI cairo_mesh_pattern_curve_to(CairoPattern* pattern, double x1, double y1,
                                           double x2, double y2, double x3, double y3) {
    aps5_cairo_mesh_pattern_curve_to(Backend(pattern), x1, y1, x2, y2, x3, y3);
}

void APS5_VABI cairo_mesh_pattern_set_corner_color_rgba(CairoPattern* pattern,
                                                        unsigned int cornerNum,
                                                        double red, double green,
                                                        double blue, double alpha) {
    aps5_cairo_mesh_pattern_set_corner_color_rgba(Backend(pattern), cornerNum, red, green, blue, alpha);
}

}

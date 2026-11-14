#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <bit>
#include <cstddef>
#include <limits>

static_assert(sizeof(CairoPathData) == sizeof(cairo_path_data_t));
static_assert(offsetof(CairoPathData, header.length) == offsetof(cairo_path_data_t, header.length));
static_assert(offsetof(CairoPathData, point.y) == offsetof(cairo_path_data_t, point.y));
static_assert(sizeof(CairoPath) == sizeof(cairo_path_t));
static_assert(offsetof(CairoPath, data) == offsetof(cairo_path_t, data));
static_assert(offsetof(CairoPath, num_data) == offsetof(cairo_path_t, num_data));

#ifdef _WIN32
extern "C" void _cairo_mutex_initialize();
#endif

namespace {

#ifdef _WIN32
const bool mutexesInitialized = (_cairo_mutex_initialize(), true);
#endif

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

CairoContext* Guest(cairo_t* context) {
    return reinterpret_cast<CairoContext*>(context);
}

}

void CairoContextSetFontOptions(CairoContext* context, const CairoFontOptionsFields* options) {
    if (!options) {
        aps5_cairo_set_font_options(Backend(context), nullptr);
        return;
    }
    cairo_font_options_t* backend = aps5_cairo_font_options_create();
    aps5_cairo_font_options_set_antialias(backend, static_cast<cairo_antialias_t>(options->antialias));
    aps5_cairo_font_options_set_subpixel_order(backend,
                                               static_cast<cairo_subpixel_order_t>(options->subpixelOrder));
    aps5_cairo_font_options_set_hint_style(backend, static_cast<cairo_hint_style_t>(options->hintStyle));
    aps5_cairo_font_options_set_hint_metrics(backend, static_cast<cairo_hint_metrics_t>(options->hintMetrics));
    aps5_cairo_set_font_options(Backend(context), backend);
    aps5_cairo_font_options_destroy(backend);
}

void CairoContextSetNilFontOptions(CairoContext* context) {
    aps5_cairo_set_font_options(Backend(context), aps5_cairo_font_options_copy(nullptr));
}

extern "C" {

CairoContext* APS5_VABI cairo_create(CairoSurface* target) {
    return Guest(aps5_cairo_create(reinterpret_cast<cairo_surface_t*>(target)));
}

CairoContext* APS5_VABI cairo_reference(CairoContext* context) {
    return Guest(aps5_cairo_reference(Backend(context)));
}

void APS5_VABI cairo_destroy(CairoContext* context) {
    aps5_cairo_destroy(Backend(context));
}

void APS5_VABI cairo_save(CairoContext* context) {
    aps5_cairo_save(Backend(context));
}

void APS5_VABI cairo_restore(CairoContext* context) {
    aps5_cairo_restore(Backend(context));
}

CairoSurface* APS5_VABI cairo_get_target(CairoContext* context) {
    return reinterpret_cast<CairoSurface*>(aps5_cairo_get_target(Backend(context)));
}

void APS5_VABI cairo_translate(CairoContext* context, double tx, double ty) {
    aps5_cairo_translate(Backend(context), tx, ty);
}

void APS5_VABI cairo_scale(CairoContext* context, double sx, double sy) {
    aps5_cairo_scale(Backend(context), sx, sy);
}

void APS5_VABI cairo_rotate(CairoContext* context, double radians) {
    aps5_cairo_rotate(Backend(context), radians);
}

void APS5_VABI cairo_transform(CairoContext* context, const CairoMatrix* matrix) {
    const cairo_matrix_t backend = std::bit_cast<cairo_matrix_t>(*matrix);
    aps5_cairo_transform(Backend(context), &backend);
}

void APS5_VABI cairo_set_matrix(CairoContext* context, const CairoMatrix* matrix) {
    const cairo_matrix_t backend = std::bit_cast<cairo_matrix_t>(*matrix);
    aps5_cairo_set_matrix(Backend(context), &backend);
}

void APS5_VABI cairo_get_matrix(CairoContext* context, CairoMatrix* matrix) {
    cairo_matrix_t backend;
    aps5_cairo_get_matrix(Backend(context), &backend);
    *matrix = std::bit_cast<CairoMatrix>(backend);
}

void APS5_VABI cairo_identity_matrix(CairoContext* context) {
    aps5_cairo_identity_matrix(Backend(context));
}

void APS5_VABI cairo_user_to_device(CairoContext* context, double* x, double* y) {
    aps5_cairo_user_to_device(Backend(context), x, y);
}

void APS5_VABI cairo_user_to_device_distance(CairoContext* context, double* dx, double* dy) {
    aps5_cairo_user_to_device_distance(Backend(context), dx, dy);
}

void APS5_VABI cairo_device_to_user(CairoContext* context, double* x, double* y) {
    aps5_cairo_device_to_user(Backend(context), x, y);
}

void APS5_VABI cairo_device_to_user_distance(CairoContext* context, double* dx, double* dy) {
    aps5_cairo_device_to_user_distance(Backend(context), dx, dy);
}

void APS5_VABI cairo_set_antialias(CairoContext* context, int antialias) {
    aps5_cairo_set_antialias(Backend(context), static_cast<cairo_antialias_t>(antialias));
}

int APS5_VABI cairo_get_antialias(CairoContext* context) {
    return aps5_cairo_get_antialias(Backend(context));
}

void APS5_VABI cairo_set_fill_rule(CairoContext* context, int fillRule) {
    aps5_cairo_set_fill_rule(Backend(context), static_cast<cairo_fill_rule_t>(fillRule));
}

int APS5_VABI cairo_get_fill_rule(CairoContext* context) {
    return aps5_cairo_get_fill_rule(Backend(context));
}

void APS5_VABI cairo_set_line_cap(CairoContext* context, int cap) {
    aps5_cairo_set_line_cap(Backend(context), static_cast<cairo_line_cap_t>(cap));
}

int APS5_VABI cairo_get_line_cap(CairoContext* context) {
    return aps5_cairo_get_line_cap(Backend(context));
}

void APS5_VABI cairo_set_line_join(CairoContext* context, int join) {
    aps5_cairo_set_line_join(Backend(context), static_cast<cairo_line_join_t>(join));
}

int APS5_VABI cairo_get_line_join(CairoContext* context) {
    return aps5_cairo_get_line_join(Backend(context));
}

void APS5_VABI cairo_set_line_width(CairoContext* context, double width) {
    aps5_cairo_set_line_width(Backend(context), width);
}

double APS5_VABI cairo_get_line_width(CairoContext* context) {
    return aps5_cairo_get_line_width(Backend(context));
}

void APS5_VABI cairo_set_miter_limit(CairoContext* context, double limit) {
    aps5_cairo_set_miter_limit(Backend(context), limit);
}

double APS5_VABI cairo_get_miter_limit(CairoContext* context) {
    return aps5_cairo_get_miter_limit(Backend(context));
}

void APS5_VABI cairo_set_dash(CairoContext* context, const double* dashes, int numDashes, double offset) {
    if (!dashes && numDashes > 0) NotImplemented_nid_no_patch(__func__);
    aps5_cairo_set_dash(Backend(context), dashes, numDashes, offset);
}

void APS5_VABI cairo_get_dash(CairoContext* context, double* dashes, double* offset) {
    aps5_cairo_get_dash(Backend(context), dashes, offset);
}

int APS5_VABI cairo_get_dash_count(CairoContext* context) {
    return aps5_cairo_get_dash_count(Backend(context));
}

void APS5_VABI cairo_set_operator(CairoContext* context, int operation) {
    aps5_cairo_set_operator(Backend(context), static_cast<cairo_operator_t>(operation));
}

int APS5_VABI cairo_get_operator(CairoContext* context) {
    return aps5_cairo_get_operator(Backend(context));
}

void APS5_VABI cairo_set_source(CairoContext* context, CairoPattern* source) {
    aps5_cairo_set_source(Backend(context), reinterpret_cast<cairo_pattern_t*>(source));
}

void APS5_VABI cairo_set_source_rgb(CairoContext* context, double red, double green, double blue) {
    aps5_cairo_set_source_rgb(Backend(context), red, green, blue);
}

void APS5_VABI cairo_set_source_rgba(CairoContext* context, double red, double green, double blue,
                                     double alpha) {
    aps5_cairo_set_source_rgba(Backend(context), red, green, blue, alpha);
}

void APS5_VABI cairo_set_source_surface(CairoContext* context, CairoSurface* surface,
                                        double x, double y) {
    aps5_cairo_set_source_surface(Backend(context), reinterpret_cast<cairo_surface_t*>(surface), x, y);
}

void APS5_VABI cairo_set_font_size(CairoContext* context, double size) {
    aps5_cairo_set_font_size(Backend(context), size);
}

void APS5_VABI cairo_clip_extents(CairoContext* context, double* x1, double* y1,
                                  double* x2, double* y2) {
    aps5_cairo_clip_extents(Backend(context), x1, y1, x2, y2);
}

void APS5_VABI cairo_rectangle(CairoContext* context, double x, double y, double width, double height) {
    aps5_cairo_rectangle(Backend(context), x, y, width, height);
}

void APS5_VABI cairo_arc(CairoContext* context, double xc, double yc, double radius,
                         double angle1, double angle2) {
    cairo_t* backend = Backend(context);
    if (aps5_cairo_status(backend) == CAIRO_STATUS_SUCCESS && !(radius <= 0.0) &&
        angle2 - angle1 == std::numeric_limits<double>::infinity())
        NotImplemented_nid_no_patch(__func__);
    aps5_cairo_arc(backend, xc, yc, radius, angle1, angle2);
}

void APS5_VABI cairo_arc_negative(CairoContext* context, double xc, double yc, double radius,
                                  double angle1, double angle2) {
    cairo_t* backend = Backend(context);
    if (aps5_cairo_status(backend) == CAIRO_STATUS_SUCCESS && !(radius <= 0.0) &&
        angle1 - angle2 == std::numeric_limits<double>::infinity())
        NotImplemented_nid_no_patch(__func__);
    aps5_cairo_arc_negative(backend, xc, yc, radius, angle1, angle2);
}

int APS5_VABI cairo_in_fill(CairoContext* context, double x, double y) {
    cairo_t* backend = Backend(context);
    if (aps5_cairo_status(backend) == CAIRO_STATUS_SUCCESS) {
        const cairo_fill_rule_t rule = aps5_cairo_get_fill_rule(backend);
        if (rule != CAIRO_FILL_RULE_WINDING && rule != CAIRO_FILL_RULE_EVEN_ODD)
            NotImplemented_nid_no_patch(__func__);
    }
    return aps5_cairo_in_fill(backend, x, y);
}

void APS5_VABI cairo_new_path(CairoContext* context) {
    aps5_cairo_new_path(Backend(context));
}

void APS5_VABI cairo_new_sub_path(CairoContext* context) {
    aps5_cairo_new_sub_path(Backend(context));
}

void APS5_VABI cairo_move_to(CairoContext* context, double x, double y) {
    aps5_cairo_move_to(Backend(context), x, y);
}

void APS5_VABI cairo_line_to(CairoContext* context, double x, double y) {
    aps5_cairo_line_to(Backend(context), x, y);
}

void APS5_VABI cairo_curve_to(CairoContext* context, double x1, double y1,
                              double x2, double y2, double x3, double y3) {
    aps5_cairo_curve_to(Backend(context), x1, y1, x2, y2, x3, y3);
}

void APS5_VABI cairo_close_path(CairoContext* context) {
    aps5_cairo_close_path(Backend(context));
}

int APS5_VABI cairo_has_current_point(CairoContext* context) {
    return aps5_cairo_has_current_point(Backend(context));
}

void APS5_VABI cairo_get_current_point(CairoContext* context, double* x, double* y) {
    aps5_cairo_get_current_point(Backend(context), x, y);
}

CairoPath* APS5_VABI cairo_copy_path(CairoContext* context) {
    return reinterpret_cast<CairoPath*>(aps5_cairo_copy_path(Backend(context)));
}

void APS5_VABI cairo_path_extents(CairoContext* context, double* x1, double* y1, double* x2, double* y2) {
    if (!context) {
        for (double* bound : {x1, y1, x2, y2})
            if (bound) *bound = 0;
        return;
    }
    aps5_cairo_path_extents(Backend(context), x1, y1, x2, y2);
}

void APS5_VABI cairo_append_path(CairoContext* context, const CairoPath* path) {
    aps5_cairo_append_path(Backend(context), reinterpret_cast<const cairo_path_t*>(path));
}

void APS5_VABI cairo_path_destroy(CairoPath* path) {
    aps5_cairo_path_destroy(reinterpret_cast<cairo_path_t*>(path));
}

}

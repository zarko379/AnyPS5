#include "prx/libcairo/include/Path.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<decltype(cairo_copy_path(nullptr)), CairoPath*>);
static_assert(sizeof(CairoPathData) == 16);

namespace {

void Near(double actual, double expected) {
    assert(std::isfinite(actual));
    assert(std::abs(actual - expected) <= 1e-10 * (1 + std::abs(expected)));
}

void CheckBasicPathAndCurrentPoint() {
    for (int iteration = 0; iteration < 2048; ++iteration) {
        CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
        CairoContext* context = cairo_create(surface);
        cairo_surface_destroy(surface);
        assert(cairo_has_current_point(context) == 0);
        double x = 123, y = 456;
        cairo_get_current_point(context, &x, &y);
        Near(x, 0);
        Near(y, 0);

        cairo_line_to(context, 4, 5);
        assert(cairo_has_current_point(context) == 1);
        CairoPath* path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 2);
        assert(path->data[0].header.type == 0 && path->data[0].header.length == 2);
        Near(path->data[1].point.x, 4);
        Near(path->data[1].point.y, 5);
        cairo_path_destroy(path);

        cairo_move_to(context, 1.2345, 2.3456);
        cairo_move_to(context, 3, 4);
        cairo_line_to(context, 5, 6);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 4);
        assert(path->data[0].header.type == 0 && path->data[2].header.type == 1);
        cairo_path_destroy(path);

        cairo_line_to(context, 5, 6);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 4);
        cairo_path_destroy(path);

        cairo_close_path(context);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 7);
        assert(path->data[4].header.type == 3);
        assert(path->data[5].header.type == 0);
        cairo_path_destroy(path);
        cairo_get_current_point(context, &x, &y);
        Near(x, 3);
        Near(y, 4);

        cairo_new_sub_path(context);
        assert(cairo_has_current_point(context) == 0);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 5);
        cairo_path_destroy(path);

        cairo_curve_to(context, 1, 2, 3, 4, 5, 6);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 11);
        assert(path->data[5].header.type == 0);
        assert(path->data[7].header.type == 2 && path->data[7].header.length == 4);
        cairo_path_destroy(path);

        cairo_new_path(context);
        assert(cairo_has_current_point(context) == 0);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 0);
        cairo_path_destroy(path);
        cairo_destroy(context);
    }
}

void CheckQuantizationTransformsAndAppend() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, 1.2345, 2.3456);
    cairo_line_to(context, 3.14159, 4.98765);

    CairoPath* original = cairo_copy_path(context);
    assert(original && original->status == 0 && original->num_data == 4);
    Near(original->data[1].point.x, 1.234375);
    Near(original->data[1].point.y, 2.34375);
    Near(original->data[3].point.x, 3.140625);
    Near(original->data[3].point.y, 4.98828125);

    double x = 0, y = 0;
    cairo_get_current_point(context, &x, &y);
    Near(x, 3.140625);
    Near(y, 4.98828125);

    cairo_translate(context, 7, 9);
    cairo_get_current_point(context, &x, &y);
    Near(x, -3.859375);
    Near(y, -4.01171875);
    CairoPath* shifted = cairo_copy_path(context);
    assert(shifted && shifted->status == 0 && shifted->num_data == 4);
    Near(shifted->data[1].point.x, 1.234375 - 7);
    Near(shifted->data[3].point.y, 4.98828125 - 9);

    CairoSurface* anotherSurface = cairo_image_surface_create(0, 8, 8);
    CairoContext* another = cairo_create(anotherSurface);
    cairo_surface_destroy(anotherSurface);
    cairo_append_path(another, original);
    CairoPath* appended = cairo_copy_path(another);
    assert(appended && appended->status == 0 && appended->num_data == 4);
    for (int i : {1, 3}) {
        Near(appended->data[i].point.x, original->data[i].point.x);
        Near(appended->data[i].point.y, original->data[i].point.y);
    }
    cairo_path_destroy(appended);
    cairo_path_destroy(shifted);
    cairo_path_destroy(original);
    cairo_destroy(another);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 8, 8);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, std::numeric_limits<double>::quiet_NaN(), 1);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->num_data == 2);
    Near(path->data[1].point.x, 0);
    cairo_path_destroy(path);
    cairo_move_to(context, std::numeric_limits<double>::infinity(), 1);
    path = cairo_copy_path(context);
    Near(path->data[1].point.x, 8388606.0 - 1.0 / 256.0);
    cairo_path_destroy(path);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 8, 8);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, 1, 2);
    cairo_curve_to(context, 1, 2, 1, 2, 1, 2);
    path = cairo_copy_path(context);
    assert(path && path->num_data == 4 && path->data[2].header.type == 1);
    cairo_path_destroy(path);
    cairo_new_path(context);
    cairo_move_to(context, 1, 2);
    cairo_curve_to(context, 2, 3, 2, 3, 2, 3);
    cairo_curve_to(context, 2, 3, 2, 3, 2, 3);
    path = cairo_copy_path(context);
    assert(path && path->num_data == 6 && path->data[2].header.type == 2);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckErrorAndSaveSemantics() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, 1, 2);
    cairo_save(context);
    cairo_line_to(context, 5, 7);
    cairo_restore(context);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 4);
    cairo_path_destroy(path);
    CairoPathData corrupt[2]{};
    corrupt[0].header = {2, 2};
    CairoPath wrong{0, corrupt, 2};
    cairo_append_path(context, &wrong);
    double x = 42, y = 24;
    cairo_get_current_point(context, &x, &y);
    Near(x, 0);
    Near(y, 0);
    cairo_path_destroy(cairo_copy_path(context));
    cairo_destroy(context);

    CairoContext* nil = cairo_create(nullptr);
    path = cairo_copy_path(nil);
    assert(path && path->status == 7);
    cairo_path_destroy(path);
    cairo_destroy(nil);
    cairo_path_destroy(nullptr);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CairoPathData extended[3]{};
    extended[0].header = {0, 3};
    extended[1].point = {1, 2};
    extended[2].point = {3, 4};
    CairoPath padded{0, extended, 3};
    cairo_append_path(context, &padded);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 2);
    Near(path->data[1].point.x, 1);
    cairo_path_destroy(path);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CairoPath alreadyFailed{5, nullptr, 3};
    cairo_append_path(context, &alreadyFailed);
    path = cairo_copy_path(context);
    assert(path && path->status == 5);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckScaledCoordinatesAndLines() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    cairo_surface_set_device_scale(surface, 2, 2);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);

    cairo_move_to(context, 0.1, 0.1);
    double x = 0, y = 0;
    cairo_get_current_point(context, &x, &y);
    Near(x, 0.099609375);
    Near(y, 0.099609375);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 2);
    Near(path->data[1].point.x, 0.099609375);
    Near(path->data[1].point.y, 0.099609375);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 1, 1);
    cairo_line_to(context, 2, 2);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 4);
    Near(path->data[3].point.x, 2);
    Near(path->data[3].point.y, 2);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 0, 0);
    cairo_line_to(context, 1, 1);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 4);
    Near(path->data[3].point.x, 1);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 2, 2);
    cairo_line_to(context, 1, 1);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 6);
    cairo_path_destroy(path);

    cairo_translate(context, 1, 2);
    cairo_get_current_point(context, &x, &y);
    Near(x, 0);
    Near(y, -1);
    cairo_destroy(context);
}

CairoContext* PathContext(double scale = 1) {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    cairo_surface_set_device_scale(surface, scale, scale);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    return context;
}

void ExpectPath(CairoContext* context, const std::vector<std::pair<int, std::vector<double>>>& expected) {
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0);
    int offset = 0;
    for (const auto& [type, points] : expected) {
        assert(offset < path->num_data);
        assert(path->data[offset].header.type == type);
        const int length = static_cast<int>(points.size()) / 2 + 1;
        assert(path->data[offset].header.length == length);
        for (int i = 1; i < length; ++i) {
            assert(path->data[offset + i].point.x == points[(i - 1) * 2]);
            assert(path->data[offset + i].point.y == points[(i - 1) * 2 + 1]);
        }
        offset += length;
    }
    assert(offset == path->num_data);
    cairo_path_destroy(path);
}

void ExpectExtents(CairoContext* context, double x1, double y1, double x2, double y2) {
    double box[4] = {-1, -1, -1, -1};
    cairo_path_extents(context, &box[0], &box[1], &box[2], &box[3]);
    assert(box[0] == x1 && box[1] == y1 && box[2] == x2 && box[3] == y2);
}

void CheckRectanglesAndCloseDegeneracy() {
    CairoContext* context = PathContext();
    ExpectExtents(context, 0, 0, 0, 0);
    cairo_rectangle(context, 1, 2, 3, 4);
    ExpectPath(context, {{0, {1, 2}}, {1, {4, 2}}, {1, {4, 6}}, {1, {1, 6}}, {3, {}}, {0, {1, 2}}});
    ExpectExtents(context, 1, 2, 4, 6);

    cairo_new_path(context);
    cairo_rectangle(context, 5, 5, 0, 0);
    ExpectPath(context, {{0, {5, 5}}, {3, {}}, {0, {5, 5}}});
    ExpectExtents(context, 5, 5, 5, 5);

    cairo_new_path(context);
    cairo_rectangle(context, 0, 0, 3, 0);
    ExpectPath(context, {{0, {0, 0}}, {1, {3, 0}}, {3, {}}, {0, {0, 0}}});

    cairo_new_path(context);
    cairo_rectangle(context, 4, 4, -2, -1);
    ExpectPath(context, {{0, {4, 4}}, {1, {2, 4}}, {1, {2, 3}}, {1, {4, 3}}, {3, {}}, {0, {4, 4}}});
    ExpectExtents(context, 2, 3, 4, 4);

    cairo_new_path(context);
    cairo_move_to(context, 1, 1);
    cairo_line_to(context, 1, 1);
    cairo_close_path(context);
    ExpectPath(context, {{0, {1, 1}}, {3, {}}, {0, {1, 1}}});

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 2, 2);
    cairo_line_to(context, 1, 1);
    cairo_close_path(context);
    ExpectPath(context, {{0, {0, 0}}, {1, {2, 2}}, {3, {}}, {0, {0, 0}}});

    cairo_new_path(context);
    cairo_move_to(context, 7, 7);
    ExpectExtents(context, 0, 0, 0, 0);
    double ignored = 0;
    cairo_path_extents(context, nullptr, &ignored, nullptr, nullptr);
    cairo_destroy(context);

    context = PathContext(2);
    cairo_scale(context, 1, 2);
    cairo_rectangle(context, 1, 1, 1, 1);
    ExpectPath(context, {{0, {1, 1}}, {1, {2, 1}}, {1, {2, 2}}, {1, {1, 2}}, {3, {}}, {0, {1, 1}}});
    ExpectExtents(context, 1, 1, 2, 2);
    cairo_destroy(context);

    context = PathContext();
    cairo_rotate(context, 0.785398163397448309616);
    cairo_rectangle(context, 0, 0, 1, 1);
    double box[4] = {};
    cairo_path_extents(context, &box[0], &box[1], &box[2], &box[3]);
    assert(std::abs(box[0] + 0.5) < 0.01 && std::abs(box[1] + 0.5) < 0.01);
    assert(std::abs(box[2] - 1.5) < 0.01 && std::abs(box[3] - 1.5) < 0.01);
    cairo_destroy(context);

    context = PathContext();
    cairo_set_dash(context, nullptr, -1, 0);
    cairo_rectangle(context, 1, 1, 1, 1);
    cairo_path_extents(context, &box[0], &box[1], &box[2], &box[3]);
    assert(box[0] == 0 && box[1] == 0 && box[2] == 0 && box[3] == 0);
    CairoPath* path = cairo_copy_path(context);
    assert(path->status == 19 && path->num_data == 0);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckCurveDegeneracyAndExtents() {
    CairoContext* context = PathContext();
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 0, 0);
    cairo_curve_to(context, 1, 1, 2, 2, 3, 0);
    ExpectPath(context, {{0, {0, 0}}, {2, {1, 1, 2, 2, 3, 0}}});

    cairo_new_path(context);
    cairo_curve_to(context, 0, 0, 0, 0, 0, 0);
    ExpectPath(context, {{0, {0, 0}}});

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_curve_to(context, 0, 10, 10, 10, 10, 0);
    ExpectExtents(context, 0, 0, 10, 7.5);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_curve_to(context, 1, 1, 2, 1, 3, 0);
    cairo_move_to(context, 20, 20);
    ExpectExtents(context, 0, 0, 3, 0.75);
    cairo_line_to(context, 21, 19);
    ExpectExtents(context, 0, 0, 21, 20);
    cairo_new_sub_path(context);
    ExpectExtents(context, 0, 0, 21, 20);
    cairo_destroy(context);
}

void CheckArcs() {
    const double pi = 3.14159265358979323846;
    const double h = 141.0 / 256.0;
    CairoContext* context = PathContext();
    cairo_arc(context, 0, 0, 1, 0, pi / 2);
    ExpectPath(context, {{0, {1, 0}}, {2, {1, h, h, 1, 0, 1}}});
    ExpectExtents(context, 0, 0, 1, 1);

    cairo_new_path(context);
    cairo_arc_negative(context, 0, 0, 1, pi / 2, 0);
    ExpectPath(context, {{0, {0, 1}}, {2, {h, 1, 1, h, 1, 0}}});

    cairo_new_path(context);
    cairo_arc(context, 0, 0, 10, 0, 2 * pi);
    CairoPath* path = cairo_copy_path(context);
    assert(path->status == 0 && path->num_data == 18);
    for (int i = 2; i < 18; i += 4) assert(path->data[i].header.type == 2);
    cairo_path_destroy(path);
    ExpectExtents(context, -10, -10, 10, 10);

    cairo_new_path(context);
    cairo_arc(context, 0, 0, 1, pi / 2, 0);
    path = cairo_copy_path(context);
    assert(path->num_data == 10 && path->data[0].header.type == 0);
    assert(path->data[1].point.x == 0 && path->data[1].point.y == 1);
    assert(path->data[9].point.x == 1 && path->data[9].point.y == 0);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_arc_negative(context, 0, 0, 1, 0, pi / 2);
    path = cairo_copy_path(context);
    assert(path->num_data == 10);
    assert(path->data[1].point.x == 1 && path->data[1].point.y == 0);
    assert(path->data[9].point.x == 0 && path->data[9].point.y == 1);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_arc(context, 3, 4, 0, 0, 1);
    ExpectPath(context, {{0, {3, 4}}, {1, {3, 4}}});
    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_arc(context, 3, 4, -1, 0, 1);
    ExpectPath(context, {{0, {0, 0}}, {1, {3, 4}}});

    cairo_new_path(context);
    cairo_arc(context, 0, 0, 1, 0, 0);
    ExpectPath(context, {{0, {1, 0}}, {1, {1, 0}}});
    cairo_new_path(context);
    cairo_arc(context, 0, 0, 1, 0, std::numeric_limits<double>::infinity());
    ExpectPath(context, {{0, {1, 0}}});

    cairo_new_path(context);
    cairo_scale(context, 1000, 1000);
    cairo_arc(context, 0, 0, 1, 0, pi / 2);
    path = cairo_copy_path(context);
    assert(path->num_data == 10);
    cairo_path_destroy(path);
    cairo_identity_matrix(context);
    cairo_new_path(context);
    cairo_scale(context, 100, 100);
    cairo_arc(context, 0, 0, 1, 0, pi / 2);
    path = cairo_copy_path(context);
    assert(path->num_data == 6);
    cairo_path_destroy(path);
    cairo_destroy(context);

    context = PathContext();
    cairo_set_dash(context, nullptr, -1, 0);
    cairo_arc(context, 0, 0, 1, 0, 1);
    cairo_arc_negative(context, 0, 0, 1, 0, 1);
    path = cairo_copy_path(context);
    assert(path->status == 19 && path->num_data == 0);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckBackendTransforms() {
    CairoContext* context = PathContext();
    cairo_set_line_width(context, 10);
    cairo_move_to(context, 1e9, -1e9);
    double x = 0, y = 0;
    cairo_get_current_point(context, &x, &y);
    assert(x == 8388597.99609375 && y == -8388598);
    cairo_set_line_width(context, 2);
    cairo_move_to(context, 1, std::numeric_limits<double>::infinity());
    cairo_get_current_point(context, &x, &y);
    assert(x == 1 && y == 8388605.99609375);
    cairo_translate(context, 0, 0);
    cairo_move_to(context, 1, std::numeric_limits<double>::infinity());
    cairo_get_current_point(context, &x, nullptr);
    assert(x == 0);
    cairo_get_current_point(context, nullptr, &y);
    assert(y == 0);
    cairo_destroy(context);

    context = PathContext(3);
    cairo_move_to(context, 5.0 / 768.0, 0);
    cairo_get_current_point(context, &x, &y);
    assert(x == (5.0 / 256.0) * (1.0 / 3.0) && y == 0);
    cairo_destroy(context);

    context = PathContext();
    cairo_rotate(context, 0.1);
    cairo_rotate(context, 0.2);
    CairoMatrix expected{};
    cairo_matrix_init_identity(&expected);
    for (double angle : {-0.1, -0.2}) {
        const CairoMatrix step{std::cos(angle), std::sin(angle), -std::sin(angle), std::cos(angle), 0, 0};
        cairo_matrix_multiply(&expected, &expected, &step);
    }
    x = 1;
    y = 2;
    cairo_device_to_user(context, &x, &y);
    double ex = 1, ey = 2;
    cairo_matrix_transform_point(&expected, &ex, &ey);
    assert(x == ex && y == ey);
    cairo_save(context);
    cairo_rotate(context, 1);
    cairo_restore(context);
    x = 1;
    y = 2;
    cairo_device_to_user(context, &x, &y);
    assert(x == ex && y == ey);
    cairo_rotate(context, 0);
    cairo_scale(context, 0, 1);
    CairoPath* path = cairo_copy_path(context);
    assert(path->status == 5);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

} // namespace

int main() {
    CheckBasicPathAndCurrentPoint();
    CheckQuantizationTransformsAndAppend();
    CheckErrorAndSaveSemantics();
    CheckScaledCoordinatesAndLines();
    CheckRectanglesAndCloseDegeneracy();
    CheckCurveDegeneracyAndExtents();
    CheckArcs();
    CheckBackendTransforms();
    std::puts("guest Cairo path-data tests passed");
}

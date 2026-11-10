#pragma once

#include "SceTypes.hpp"
#include "prx/libcairo/include/Matrix.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <atomic>
#include <cstdint>
#include <vector>

struct CairoPattern;

struct CairoColor {
    double red;
    double green;
    double blue;
    double alpha;
    std::uint16_t redShort;
    std::uint16_t greenShort;
    std::uint16_t blueShort;
    std::uint16_t alphaShort;
};

struct CairoPointDouble {
    double x;
    double y;
};

struct CairoGradientStop {
    double offset;
    CairoColor color;
};

struct CairoMeshPatch {
    CairoPointDouble points[4][4];
    CairoColor colors[4];
};

struct CairoPatternState {
    std::atomic<int> references{1};
    int status{};
    int type{};
    int filter{1};
    int extend{3};
    int dither{};
    CairoMatrix matrix{1, 0, 0, 1, 0, 0};
    double opacity{1};
    CairoColor color{};
    CairoSurface* surface{};
    CairoPointDouble points[2]{};
    double radii[2]{};
    std::vector<CairoGradientStop> stops;
    std::vector<CairoMeshPatch> patches;
    int currentPatch{-1};
    int currentSide{-2};
    bool hasControlPoint[4]{};
    bool hasColor[4]{};
};

CairoPatternState* CairoPatternInternal(CairoPattern* pattern);
CairoPattern* CairoPatternBlack();
CairoPattern* CairoPatternCreateRgba(double red, double green, double blue, double alpha);
CairoColor CairoColorFromRgba(double red, double green, double blue, double alpha);
bool CairoColorEqual(const CairoColor& a, const CairoColor& b);

extern "C" {
CairoPattern* APS5_VABI cairo_pattern_create_for_surface(CairoSurface* surface);
CairoPattern* APS5_VABI cairo_pattern_create_linear(double x0, double y0, double x1, double y1);
CairoPattern* APS5_VABI cairo_pattern_create_radial(double cx0, double cy0, double radius0,
                                                    double cx1, double cy1, double radius1);
CairoPattern* APS5_VABI cairo_pattern_create_mesh();
CairoPattern* APS5_VABI cairo_pattern_reference(CairoPattern* pattern);
void APS5_VABI cairo_pattern_destroy(CairoPattern* pattern);
void APS5_VABI cairo_pattern_add_color_stop_rgba(CairoPattern* pattern, double offset,
                                                 double red, double green, double blue,
                                                 double alpha);
void APS5_VABI cairo_pattern_set_matrix(CairoPattern* pattern, const CairoMatrix* matrix);
void APS5_VABI cairo_pattern_set_filter(CairoPattern* pattern, int filter);
void APS5_VABI cairo_pattern_set_extend(CairoPattern* pattern, int extend);
void APS5_VABI cairo_mesh_pattern_begin_patch(CairoPattern* pattern);
void APS5_VABI cairo_mesh_pattern_end_patch(CairoPattern* pattern);
void APS5_VABI cairo_mesh_pattern_move_to(CairoPattern* pattern, double x, double y);
void APS5_VABI cairo_mesh_pattern_line_to(CairoPattern* pattern, double x, double y);
void APS5_VABI cairo_mesh_pattern_curve_to(CairoPattern* pattern, double x1, double y1,
                                           double x2, double y2, double x3, double y3);
void APS5_VABI cairo_mesh_pattern_set_corner_color_rgba(CairoPattern* pattern,
                                                        unsigned int cornerNum,
                                                        double red, double green,
                                                        double blue, double alpha);
}

/* Derived from FreeBSD lib/msun/src/k_sin.c, k_cos.c, k_sinf.c and k_cosf.c (release/11.4.0) */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {

constexpr double Half = 5.00000000000000000000e-01;
constexpr double One = 1.00000000000000000000e+00;

constexpr double S1 = -1.66666666666666324348e-01;
constexpr double S2 = 8.33333333332248946124e-03;
constexpr double S3 = -1.98412698298579493134e-04;
constexpr double S4 = 2.75573137070700676789e-06;
constexpr double S5 = -2.50507602534068634195e-08;
constexpr double S6 = 1.58969099521155010221e-10;

constexpr double C1 = 4.16666666666666019037e-02;
constexpr double C2 = -1.38888888888741095749e-03;
constexpr double C3 = 2.48015872894767294178e-05;
constexpr double C4 = -2.75573143513906633035e-07;
constexpr double C5 = 2.08757232129817482790e-09;
constexpr double C6 = -1.13596475577881948265e-11;

constexpr double FS1 = -0x15555554cbac77.0p-55;
constexpr double FS2 = 0x111110896efbb2.0p-59;
constexpr double FS3 = -0x1a00f9e2cae774.0p-65;
constexpr double FS4 = 0x16cd878c3b46a7.0p-71;

constexpr double FC0 = -0x1ffffffd0c5e81.0p-54;
constexpr double FC1 = 0x155553e1053a42.0p-57;
constexpr double FC2 = -0x16c087e80f1e27.0p-62;
constexpr double FC3 = 0x199342e0ee5069.0p-68;

}

extern "C" {

double APS5_VABI __kernel_sin_nid_postfix(double x, double y, int iy) {
    const double z = x * x;
    const double w = z * z;
    const double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    const double v = z * x;
    if (iy == 0) return x + v * (S1 + z * r);
    return x - ((z * (Half * y - v * r) - y) - v * S1);
}

double APS5_VABI __kernel_cos_nid_postfix(double x, double y) {
    const double z = x * x;
    double w = z * z;
    const double r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    const double hz = 0.5 * z;
    w = One - hz;
    return w + (((One - w) - hz) + (z * r - x * y));
}

float APS5_VABI __kernel_sindf_nid_postfix(double x) {
    const double z = x * x;
    const double w = z * z;
    const double r = FS3 + z * FS4;
    const double s = z * x;
    return static_cast<float>((x + s * (FS1 + z * FS2)) + s * w * r);
}

float APS5_VABI __kernel_cosdf_nid_postfix(double x) {
    const double z = x * x;
    const double w = z * z;
    const double r = FC2 + z * FC3;
    return static_cast<float>(((One + z * FC0) + w * FC1) + (w * z) * r);
}

}

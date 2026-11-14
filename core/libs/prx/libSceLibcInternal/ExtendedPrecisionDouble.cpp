/* Derived from Microsoft STL stl/src/xxxprec.h, stl/src/xprec.cpp and stl/src/xfprec.cpp (219514876ea86491de191ceaa88632616bbc0f19) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

template <typename T>
struct Kind;

template <>
struct Kind<double> {
    static constexpr const char* Prefix = "_Xp_";
    static short Unscale(short* exponent, double* value) { return _Dunscale_nid_postfix(exponent, value); }
    static short Truncate(double* value, short bits) { return _Dint_nid_postfix(value, bits); }
    static short Scale(double* value, std::int64_t exponent) { return _Dscale_nid_postfix(value, exponent); }
    static short Test(const double* value) { return _Dtest_nid_postfix(value); }
    static double Ldexp(double value, int exponent) { return ldexp_nid_postfix(value, exponent); }
    static double Sqrt(double value) { return sqrt_nid_postfix(value); }
};

template <>
struct Kind<float> {
    static constexpr const char* Prefix = "_FXp_";
    static short Unscale(short* exponent, float* value) { return _FDunscale_nid_postfix(exponent, value); }
    static short Truncate(float* value, short bits) { return _FDint_nid_postfix(value, bits); }
    static short Scale(float* value, std::int64_t exponent) { return _FDscale_nid_postfix(value, exponent); }
    static short Test(const float* value) { return _FDtest_nid_postfix(value); }
    static float Ldexp(float value, int exponent) { return ldexpf_nid_postfix(value, exponent); }
    static float Sqrt(float value) { return sqrtf_nid_postfix(value); }
};

constexpr int DelayLine = 4;
constexpr short InfiniteCode = 1;
constexpr short NotANumberCode = 2;
constexpr int InvalidException = 0x01;

template <typename T>
struct Precision {
    using Helpers = Kind<T>;
    static constexpr int Bits = std::numeric_limits<T>::digits;
    static constexpr std::int64_t BigExponent = 2 * std::numeric_limits<T>::max_exponent;
    static constexpr short WordBits = Bits / 2;

    static void RequireCount(int count, const char* function, const char* what) {
        if (count < 0) throw std::invalid_argument(std::string(Helpers::Prefix) + function + ": negative " + what + " count " + std::to_string(count));
    }

    static void CopyUp(T* p, int j, int n) {
        int m = j;
        while (++m < n && (p[m - 1] = p[m]) != T(0)) {
        }
        p[n - 1] = T(0);
    }

    static T* Setw(T* p, int n, T x) {
        T x0 = x;
        short errx = 0;
        short xexp = 0;
        if (n > 0) {
            if (n == 1 || (errx = Helpers::Unscale(&xexp, &x0)) == 0) {
                p[0] = x0;
            } else if (0 < errx) {
                p[0] = x0;
                p[1] = T(0);
            } else {
                Helpers::Truncate(&x0, WordBits);
                Helpers::Scale(&x0, xexp);
                p[0] = x0;
                p[1] = x - x0;
                if constexpr ((Bits & 1) != 0) {
                    if (2 < n && p[1] != T(0)) {
                        x = p[1];
                        Helpers::Unscale(&xexp, &p[1]);
                        Helpers::Truncate(&p[1], WordBits);
                        Helpers::Scale(&p[1], xexp);
                        p[2] = x - p[1];
                        if (3 < n && p[2] != T(0)) p[3] = T(0);
                    } else if (2 < n) {
                        p[2] = T(0);
                    }
                } else if (2 < n) {
                    p[2] = T(0);
                }
            }
        }
        return p;
    }

    static T* Addh(T* p, int n, T x0) {
        T xscaled = x0;
        short errx = 0;
        short xexp = 0;
        if (n == 0) return p;
        if (0 < (errx = Helpers::Unscale(&xexp, &xscaled))) {
            if (errx == NotANumberCode || (errx = Helpers::Test(&p[0])) <= 0) {
                p[0] = x0;
            } else if (errx != NotANumberCode && std::signbit(x0) != std::signbit(p[0])) {
                _Feraise_nid_postfix(InvalidException);
                p[0] = std::numeric_limits<T>::quiet_NaN();
                if (1 < n) p[1] = T(0);
            }
            return p;
        }
        if (errx == 0) return p;
        std::int64_t prevexp = BigExponent;
        int k = 0;
        while (k < n) {
            T yscaled = p[k];
            const int mybits = WordBits;
            short yexp = 0;
            if (0 < (errx = Helpers::Unscale(&yexp, &yscaled))) break;
            if (errx == 0) {
                p[k] = x0;
                if (k + 1 < n) p[k + 1] = T(0);
                break;
            }
            const std::int64_t diff = static_cast<std::int64_t>(yexp) - xexp;
            if (diff <= -mybits && x0 != T(0)) {
                int j = k;
                do {
                    ++j;
                } while (j < n && p[j] != T(0));
                if (j < n - 1) {
                    ++j;
                } else if (j == n) {
                    --j;
                }
                for (; k < j; --j) p[j] = p[j - 1];
                p[k] = x0;
                x0 = T(0);
            } else if (mybits <= diff && x0 != T(0)) {
                prevexp = yexp;
                ++k;
            } else {
                if ((p[k] += x0) == T(0)) {
                    CopyUp(p, k, n);
                    if (p[k] == T(0)) break;
                }
                x0 = p[k];
                Helpers::Unscale(&xexp, &x0);
                if (prevexp - mybits < xexp) {
                    Helpers::Truncate(&x0, static_cast<short>(xexp - (prevexp - mybits)));
                    Helpers::Scale(&x0, xexp);
                    if ((p[k] -= x0) == T(0)) CopyUp(p, k, n);
                    if (--k == 0) {
                        prevexp = BigExponent;
                    } else {
                        xscaled = p[k - 1];
                        Helpers::Unscale(&yexp, &xscaled);
                        prevexp = yexp;
                    }
                } else if (k + 1 == n) {
                    break;
                } else {
                    x0 = p[k];
                    Helpers::Unscale(&yexp, &p[k]);
                    Helpers::Truncate(&p[k], WordBits);
                    Helpers::Scale(&p[k], yexp);
                    x0 -= p[k];
                    prevexp = yexp;
                    xscaled = x0 != T(0) ? x0 : p[k];
                    Helpers::Unscale(&xexp, &xscaled);
                    ++k;
                }
            }
        }
        return p;
    }

    static T* Mulh(T* p, int n, T x0) {
        T buf[DelayLine];
        if (0 < n) {
            buf[0] = p[0] * x0;
            const short errx = Helpers::Test(&buf[0]);
            if (0 <= errx) {
                if (errx == NotANumberCode) _Feraise_nid_postfix(InvalidException);
                p[0] = buf[0];
                if (0 < errx && 1 < n) p[1] = T(0);
                return p;
            }
            p[0] = T(0);
        }
        int j = 1;
        for (int k = 0; k < n; ++k, --j) {
            for (; j < DelayLine; ++j) {
                if (k + j < n && p[k + j] != T(0)) {
                    buf[j] = p[k + j] * x0;
                    p[k + j] = T(0);
                } else {
                    buf[j] = T(0);
                    j = 2 * DelayLine;
                    break;
                }
            }
            if (buf[0] == T(0)) break;
            int i = 0;
            T y0 = buf[0];
            short xexp = 0;
            Helpers::Unscale(&xexp, &y0);
            Helpers::Truncate(&y0, WordBits);
            Helpers::Scale(&y0, xexp);
            Addh(p, n, y0);
            Addh(p, n, buf[0] - y0);
            while (++i < j) {
                if ((buf[i - 1] = buf[i]) == T(0)) break;
            }
        }
        return p;
    }

    static T* Setn(T* p, int n, std::int64_t x) {
        if constexpr (27 <= Bits) {
            Setw(p, n, static_cast<T>(x));
        } else {
            Setw(p, n, static_cast<T>(x / 10000));
            Mulh(p, n, static_cast<T>(10000));
            Addh(p, n, static_cast<T>(x % 10000));
        }
        return p;
    }

    static T* Movx(T* p, int n, const T* q) {
        RequireCount(n, "movx", "word");
        if (n != 0) std::memcpy(p, q, static_cast<std::size_t>(n) * sizeof(T));
        return p;
    }

    static T* Addx(T* p, int n, const T* q, int m) {
        for (int k = 0; k < m && q[k] != T(0); ++k) Addh(p, n, q[k]);
        return p;
    }

    static T* Subx(T* p, int n, const T* q, int m) {
        for (int k = 0; k < m && q[k] != T(0); ++k) Addh(p, n, -q[k]);
        return p;
    }

    static T* Ldexpx(T* p, int n, int m) {
        for (int k = 0; k < n; ++k) {
            p[k] = Helpers::Ldexp(p[k], m);
            if (p[k] == T(0)) break;
        }
        return p;
    }

    static T* Mulx(T* p, int n, const T* q, int m, T* ptemp2) {
        RequireCount(n, "mulx", "word");
        RequireCount(m, "mulx", "multiplier word");
        if (n == 0 || m == 0) return p;
        if (m == 1 || q[0] == T(0) || q[1] == T(0)) return Mulh(p, n, q[0]);
        T* px = ptemp2;
        T* pac = ptemp2 + n;
        Movx(px, n, p);
        Mulh(p, n, q[0]);
        for (int j = 1; j < m && q[j] != T(0); ++j) {
            Movx(pac, n, px);
            Mulh(pac, n, q[j]);
            Addx(p, n, pac, n);
        }
        return p;
    }

    static T* Invx(T* p, int n, T* ptemp4) {
        RequireCount(n, "invx", "word");
        if (n == 0) return p;
        const short errx = Helpers::Test(&p[0]);
        if (0 <= errx) {
            if (errx == InfiniteCode) {
                p[0] = T(0);
            } else if (errx == 0) {
                p[0] = std::numeric_limits<T>::infinity();
            }
            return p;
        }
        T* pac = ptemp4;
        T* py = ptemp4 + n;
        T* ptemp2 = py + n;
        T x0 = p[0];
        Movx(py, n, p);
        Mulh(py, n, T(-1));
        if (1 < n) x0 += p[1];
        Setw(p, n, T(1) / x0);
        for (int k = 1; k < n; k <<= 1) {
            Movx(pac, n, p);
            Mulx(pac, n, py, n, ptemp2);
            Addh(pac, n, T(1));
            Mulx(pac, n, p, n, ptemp2);
            Addx(p, n, pac, n);
        }
        return p;
    }

    static T* Sqrtx(T* p, int n, T* ptemp4) {
        RequireCount(n, "sqrtx", "word");
        if (n == 0) return p;
        if (0 <= Helpers::Test(&p[0]) || p[0] < T(0)) {
            if (p[0] < T(0)) {
                _Feraise_nid_postfix(InvalidException);
                p[0] = std::numeric_limits<T>::quiet_NaN();
            }
            return p;
        }
        T* pac = ptemp4;
        T* py = ptemp4 + n;
        T* ptemp2 = py + n;
        T x0 = p[0];
        if (1 < n) x0 += p[1];
        Setw(py, n, T(1) / Helpers::Sqrt(x0));
        for (int k = 2; k < n; k <<= 1) {
            Movx(pac, n, py);
            Mulh(pac, n, T(-0.5));
            Mulx(pac, n, p, n, ptemp2);
            Mulx(pac, n, py, n, ptemp2);
            Addh(pac, n, T(1.5));
            Mulx(py, n, pac, n, ptemp2);
        }
        Mulx(p, n, py, n, ptemp2);
        return p;
    }
};

using DoubleWords = Precision<double>;
using FloatWords = Precision<float>;

}

extern "C" {

double APS5_VABI _Xp_getw_nid_postfix(const double* p, int n) {
    DoubleWords::RequireCount(n, "getw", "word");
    if (n == 0) return 0.0;
    if (n == 1 || p[0] == 0.0 || p[1] == 0.0) return p[0];
    if (n == 2 || p[2] == 0.0) return p[0] + p[1];
    const double p01 = p[0] + p[1];
    double p2 = p[2];
    if (4 <= n) p2 += p[3];
    if (p01 - p[0] == p[1]) return p01 + p2;
    return p[0] + (p[1] + p2);
}

double* APS5_VABI _Xp_setw_nid_postfix(double* p, int n, double x) { return DoubleWords::Setw(p, n, x); }
double* APS5_VABI _Xp_addh_nid_postfix(double* p, int n, double x0) { return DoubleWords::Addh(p, n, x0); }
double* APS5_VABI _Xp_mulh_nid_postfix(double* p, int n, double x0) { return DoubleWords::Mulh(p, n, x0); }
double* APS5_VABI _Xp_setn_nid_postfix(double* p, int n, std::int64_t x) { return DoubleWords::Setn(p, n, x); }
double* APS5_VABI _Xp_movx_nid_postfix(double* p, int n, const double* q) { return DoubleWords::Movx(p, n, q); }
double* APS5_VABI _Xp_addx_nid_postfix(double* p, int n, const double* q, int m) { return DoubleWords::Addx(p, n, q, m); }
double* APS5_VABI _Xp_subx_nid_postfix(double* p, int n, const double* q, int m) { return DoubleWords::Subx(p, n, q, m); }
double* APS5_VABI _Xp_ldexpx_nid_postfix(double* p, int n, int m) { return DoubleWords::Ldexpx(p, n, m); }
double* APS5_VABI _Xp_mulx_nid_postfix(double* p, int n, const double* q, int m, double* ptemp2) { return DoubleWords::Mulx(p, n, q, m, ptemp2); }
double* APS5_VABI _Xp_invx_nid_postfix(double* p, int n, double* ptemp4) { return DoubleWords::Invx(p, n, ptemp4); }
double* APS5_VABI _Xp_sqrtx_nid_postfix(double* p, int n, double* ptemp4) { return DoubleWords::Sqrtx(p, n, ptemp4); }

float* APS5_VABI _FXp_setw_nid_postfix(float* p, int n, float x) { return FloatWords::Setw(p, n, x); }
float* APS5_VABI _FXp_addh_nid_postfix(float* p, int n, float x0) { return FloatWords::Addh(p, n, x0); }
float* APS5_VABI _FXp_mulh_nid_postfix(float* p, int n, float x0) { return FloatWords::Mulh(p, n, x0); }
float* APS5_VABI _FXp_setn_nid_postfix(float* p, int n, std::int64_t x) { return FloatWords::Setn(p, n, x); }
float* APS5_VABI _FXp_movx_nid_postfix(float* p, int n, const float* q) { return FloatWords::Movx(p, n, q); }
float* APS5_VABI _FXp_addx_nid_postfix(float* p, int n, const float* q, int m) { return FloatWords::Addx(p, n, q, m); }
float* APS5_VABI _FXp_subx_nid_postfix(float* p, int n, const float* q, int m) { return FloatWords::Subx(p, n, q, m); }
float* APS5_VABI _FXp_ldexpx_nid_postfix(float* p, int n, int m) { return FloatWords::Ldexpx(p, n, m); }
float* APS5_VABI _FXp_mulx_nid_postfix(float* p, int n, const float* q, int m, float* ptemp2) { return FloatWords::Mulx(p, n, q, m, ptemp2); }
float* APS5_VABI _FXp_invx_nid_postfix(float* p, int n, float* ptemp4) { return FloatWords::Invx(p, n, ptemp4); }
float* APS5_VABI _FXp_sqrtx_nid_postfix(float* p, int n, float* ptemp4) { return FloatWords::Sqrtx(p, n, ptemp4); }

}

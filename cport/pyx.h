/* pyx.h: arithmetic exactly as CPython 3.15 on Windows does it, so the C port gives bit-identical numbers.
   - py_round(x, nd): round(x, nd) for a float: correctly rounded to nd decimals, ties to even (CPython's
     double_round uses _Py_dg_dtoa mode 3, then strtod). Done here exactly with 128-bit integers.
   - py_rint(x): round(x) to an integer, ties to even.
   - py_hypot(x, y): math.hypot, CPython's vector_norm (lossless scaling, fma squaring, a correction step).
   - py_cos/sin/atan2/sqrt: math.cos/sin/atan2 call the UCRT's own functions (ucrtbase.dll); we call the same ones.
   Build with -ffp-contract=off and no fast-math, so the compiler never fuses or reorders float operations. */
#ifndef PYX_H
#define PYX_H
#include <math.h>
#include <stdint.h>
#include <string.h>

static inline double py_rint(double x) {          /* float.__round__(None): round(), then ties to even */
    double r = round(x);
    if (fabs(x - r) == 0.5) r = 2.0 * round(x / 2.0);
    return r;
}

static inline double py_round(double x, int nd) {
    static const double P10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
    if (!isfinite(x) || x == 0.0) return x;
    uint64_t b; memcpy(&b, &x, 8);
    int neg = (int)(b >> 63), ex = (int)((b >> 52) & 0x7ff); uint64_t m = b & ((1ULL << 52) - 1);
    if (ex == 0) ex = 1; else m |= 1ULL << 52;
    int e = ex - 1075;                                /* |x| = m * 2^e exactly */
    unsigned __int128 N = (unsigned __int128)m * (uint64_t)P10[nd];          /* |x| * 10^nd = N * 2^e */
    double q;
    if (e >= 0) return x;                             /* |x| >= 2^52: already an integer, nothing below nd decimals */
    int s = -e;
    if (s >= 127) q = 0.0;                            /* far below half a unit of the last decimal */
    else {
        unsigned __int128 Q = N >> s, R = N - (Q << s), half = (unsigned __int128)1 << (s - 1);
        if (R > half || (R == half && (Q & 1))) Q += 1;
        q = (double)Q;                                /* Q < 2^67 / 2^s: exact for every value this kit meets (< 2^53) */
    }
    double r = q / P10[nd];                           /* nearest double to Q / 10^nd, as strtod of "Qe-nd" gives */
    return neg ? -r : r;
}

/* CPython's math.hypot for two numbers (vector_norm in Modules/mathmodule.c) */
__attribute__((target("fma"))) static inline double py_hypot(double a, double b) {
    double vec[2] = { fabs(a), fabs(b) }, max = vec[0] > vec[1] ? vec[0] : vec[1];
    if (isinf(vec[0]) || isinf(vec[1])) return INFINITY;
    if (isnan(vec[0]) || isnan(vec[1])) return NAN;
    if (max == 0.0) return max;
    int max_e; frexp(max, &max_e);
    if (max_e < -1023) return hypot(a, b);            /* subnormal: never met here */
    double scale = ldexp(1.0, -max_e), csum = 1.0, frac1 = 0.0, frac2 = 0.0, x, h;
    for (int i = 0; i < 2; i++) {
        x = vec[i] * scale;
        double phi = x * x, plo = __builtin_fma(x, x, -phi);
        double shi = csum + phi, slo = (csum - shi) + phi;
        csum = shi; frac1 += plo; frac2 += slo;
    }
    h = sqrt(csum - 1.0 + (frac1 + frac2));
    { double phi = -h * h, plo = __builtin_fma(-h, h, -phi); double shi = csum + phi, slo = (csum - shi) + phi; csum = shi; frac1 += plo; frac2 += slo; }
    x = csum - 1.0 + (frac1 + frac2);
    h += x / (2.0 * h);
    return h / scale;
}

/* the UCRT's math functions (what math.cos / math.sin / math.atan2 call) */
typedef double (*pyx_f1)(double); typedef double (*pyx_f2)(double, double);
extern pyx_f1 py_cos, py_sin, py_log, py_sqrt_u, py_exp, py_tan, py_acos, py_asin, py_atan; extern pyx_f2 py_atan2, py_pow, py_fmod;
int pyx_load(void);
#endif

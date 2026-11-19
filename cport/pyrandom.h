/* pyrandom.h: Python 3.15's random.Random, bit for bit (Lib/random.py + Modules/_randommodule.c).
   Seeds: py_seed_str (str -> utf-8 bytes + sha512, as one big-endian int), py_seed_u64 (an int >= 0),
   py_seed_float (a float: its Python hash, as size_t). Methods: random, uniform, getrandbits, randbelow, randrange,
   randint, choice index, shuffle, sample, choices (cumulative weights). */
#ifndef PYRANDOM_H
#define PYRANDOM_H
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pyx.h"

typedef struct { uint32_t mt[624]; int index; } PyRandom;

static void pr_init_genrand(PyRandom *r, uint32_t s) {
    r->mt[0] = s;
    for (int i = 1; i < 624; i++) r->mt[i] = 1812433253U * (r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) + (uint32_t)i;
    r->index = 624;
}
static void pr_init_by_array(PyRandom *r, const uint32_t *key, size_t n) {
    pr_init_genrand(r, 19650218U); uint32_t *mt = r->mt; size_t i = 1, j = 0, k = 624 > n ? 624 : n;
    for (; k; k--) { mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525U)) + key[j] + (uint32_t)j; i++; j++; if (i >= 624) { mt[0] = mt[623]; i = 1; } if (j >= n) j = 0; }
    for (k = 623; k; k--) { mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941U)) - (uint32_t)i; i++; if (i >= 624) { mt[0] = mt[623]; i = 1; } }
    mt[0] = 0x80000000U;
}
static uint32_t pr_u32(PyRandom *r) {
    uint32_t *mt = r->mt, y; static const uint32_t mag[2] = { 0, 0x9908b0dfU };
    if (r->index >= 624) {
        int kk;
        for (kk = 0; kk < 624 - 397; kk++) { y = (mt[kk] & 0x80000000U) | (mt[kk + 1] & 0x7fffffffU); mt[kk] = mt[kk + 397] ^ (y >> 1) ^ mag[y & 1]; }
        for (; kk < 623; kk++) { y = (mt[kk] & 0x80000000U) | (mt[kk + 1] & 0x7fffffffU); mt[kk] = mt[kk + (397 - 624)] ^ (y >> 1) ^ mag[y & 1]; }
        y = (mt[623] & 0x80000000U) | (mt[0] & 0x7fffffffU); mt[623] = mt[396] ^ (y >> 1) ^ mag[y & 1];
        r->index = 0;
    }
    y = mt[r->index++]; y ^= y >> 11; y ^= (y << 7) & 0x9d2c5680U; y ^= (y << 15) & 0xefc60000U; y ^= y >> 18;
    return y;
}

/* seed from the little-endian bytes of a non-negative integer (n bytes, high zero bytes allowed) */
static void pr_seed_le(PyRandom *r, const unsigned char *le, size_t n) {
    while (n && !le[n - 1]) n--;                     /* _PyLong_NumBits: leading zeros do not count */
    size_t words = n ? (n + 3) / 4 : 1; uint32_t *key = (uint32_t *)calloc(words, 4);
    for (size_t i = 0; i < n; i++) key[i / 4] |= (uint32_t)le[i] << (8 * (i % 4));
    pr_init_by_array(r, key, words); free(key);
}
static void py_seed_u64(PyRandom *r, uint64_t v) { unsigned char b[8]; for (int i = 0; i < 8; i++) b[i] = (unsigned char)(v >> (8 * i)); pr_seed_le(r, b, 8); }

/* hash(float), CPython's _Py_HashDouble (finite values) */
static int64_t py_hash_double(double v) {
    const uint64_t MOD = (1ULL << 61) - 1; int e, sign = 1; double m = frexp(v, &e); uint64_t x = 0, y;
    if (isinf(v)) return v > 0 ? 314159 : -314159;
    if (m < 0) { sign = -1; m = -m; }
    while (m) { x = ((x << 28) & MOD) | x >> (61 - 28); m *= 268435456.0; e -= 28; y = (uint64_t)m; m -= (double)y; x += y; if (x >= MOD) x -= MOD; }
    e = e >= 0 ? e % 61 : 61 - 1 - ((-1 - e) % 61);
    x = ((x << e) & MOD) | x >> (61 - e);
    x = x * (uint64_t)(int64_t)sign;
    if (x == (uint64_t)-1) x = (uint64_t)-2;
    return (int64_t)x;
}
static void py_seed_float(PyRandom *r, double v) { py_seed_u64(r, (uint64_t)py_hash_double(v)); }          /* PyLong_FromSize_t((size_t)hash) */

/* str seed: int.from_bytes(s + sha512(s).digest()) (big-endian) */
int pr_sha512(const unsigned char *data, size_t n, unsigned char out[64]);          /* pyrandom.c */
static void py_seed_bytes(PyRandom *r, const unsigned char *s, size_t n) {
    unsigned char *be = (unsigned char *)malloc(n + 64), *le = (unsigned char *)malloc(n + 64);
    memcpy(be, s, n); pr_sha512(s, n, be + n);
    for (size_t i = 0; i < n + 64; i++) le[i] = be[n + 63 - i];
    pr_seed_le(r, le, n + 64); free(be); free(le);
}
static void py_seed_str(PyRandom *r, const char *utf8) { py_seed_bytes(r, (const unsigned char *)utf8, strlen(utf8)); }

/* the methods */
static double py_random(PyRandom *r) { uint32_t a = pr_u32(r) >> 5, b = pr_u32(r) >> 6; return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0); }
static double py_uniform(PyRandom *r, double a, double b) { return a + (b - a) * py_random(r); }
static uint64_t py_getrandbits(PyRandom *r, int k) {          /* k <= 64 */
    if (k == 0) return 0;
    if (k <= 32) return pr_u32(r) >> (32 - k);
    uint64_t lo = pr_u32(r), hi = pr_u32(r); if (k < 64) hi >>= (64 - k); return lo | (hi << 32);
}
static int64_t py_randbelow(PyRandom *r, int64_t n) {          /* _randbelow_with_getrandbits, n > 0 */
    int k = 0; for (uint64_t t = (uint64_t)n; t; t >>= 1) k++;
    uint64_t v = py_getrandbits(r, k); while (v >= (uint64_t)n) v = py_getrandbits(r, k);
    return (int64_t)v;
}
static int64_t py_randint(PyRandom *r, int64_t a, int64_t b) { return a + py_randbelow(r, b - a + 1); }
static int64_t py_randrange(PyRandom *r, int64_t start, int64_t stop, int64_t step) {          /* stop given */
    int64_t width = stop - start, n;
    if (step == 1) return start + py_randbelow(r, width);
    if (step > 0) n = (width + step - 1) / step; else n = (width + step + 1) / step;          /* Python // on these signs: exact here */
    return start + step * py_randbelow(r, n);
}
static int64_t py_choice(PyRandom *r, int64_t len) { return py_randbelow(r, len); }          /* the index: seq[py_choice(r, len)] */
/* shuffle: x[i] <-> x[j]; done on an index permutation the caller applies, or directly with this swap callback-free form */
static void py_shuffle_idx(PyRandom *r, int64_t *x, int64_t n) { for (int64_t i = n - 1; i >= 1; i--) { int64_t j = py_randbelow(r, i + 1), t = x[i]; x[i] = x[j]; x[j] = t; } }
/* sample(range(n), k) -> indices in out (k of them) */
static void py_sample_idx(PyRandom *r, int64_t n, int64_t k, int64_t *out) {
    int64_t setsize = 21;
    if (k > 5) { double l = py_log((double)(k * 3)) / py_log(4.0);          /* math.log(x, 4): UCRT log of each, divided */ setsize += (int64_t)pow(4.0, ceil(l)); }          /* 4 ** _ceil(_log(k * 3, 4)) */
    if (n <= setsize) {
        int64_t *pool = (int64_t *)malloc(sizeof(int64_t) * (n ? n : 1)); for (int64_t i = 0; i < n; i++) pool[i] = i;
        for (int64_t i = 0; i < k; i++) { int64_t j = py_randbelow(r, n - i); out[i] = pool[j]; pool[j] = pool[n - i - 1]; }
        free(pool);
    } else {
        for (int64_t i = 0; i < k; i++) {
            int64_t j, dup;
            do { j = py_randbelow(r, n); dup = 0; for (int64_t t = 0; t < i; t++) if (out[t] == j) { dup = 1; break; } } while (dup);
            out[i] = j;
        }
    }
}
/* choices(population, weights) for k = 1: the index; cum = accumulated weights (Python's itertools.accumulate) */
static int64_t py_choices1(PyRandom *r, const double *cum, int64_t n) {
    double total = cum[n - 1] + 0.0, x = py_random(r) * total; int64_t lo = 0, hi = n - 1;          /* bisect_right(cum, x, 0, n - 1) */
    while (lo < hi) { int64_t mid = (lo + hi) / 2; if (x < cum[mid]) hi = mid; else lo = mid + 1; }
    return lo;
}
static int64_t py_choices1_unweighted(PyRandom *r, int64_t n) { return (int64_t)floor(py_random(r) * (double)n); }
#endif

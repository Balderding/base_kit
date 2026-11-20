/* pyexact.h: the Python 3.15 / numpy 2.5.4 behaviour the scene code depends on, bit for bit (pyexact.c).
   Proven against C:\Python315 + numpy on random inputs: work\cscene\pyexact (prove_*.py, pxtest.dll).

   1. numpy:   np_arange (float and int dtype), np_linspace, the meshgrid + disc mask + column_stack of
               scenes._bare_grid, np_minimum / np_minimum_at, np_argmax / np_argmin (first index, NaN wins).
   2. CPython: PySetF, a set of float tuples that iterates in exactly CPython's order (float hash, xxHash tuple
               hash, setobject.c open addressing with LINEAR_PROBES, dummies, resizing); PyDictF, a dict with float
               tuple keys (insertion order, the first key object kept).
   3. random:  the random.Random calls pyrandom.h lacks or does without error checks (int seeds of any sign,
               randrange/randint/choice/sample with Python's ValueErrors, choices with weights / cum_weights / k,
               getstate / setstate).
   4. numbers: float // and %, int // and %, int(x), x ** y (incl. the complex result of a negative base),
               sum() of floats (Neumaier, as builtin sum does), math.dist / hypot of n values, max/min key
               first-wins, sorted() stable (by float keys or float tuples), tuple < .
   Errors: functions returning int give 0 = fine, or one of the PX_E* codes where Python raises. */
#ifndef PYEXACT_H
#define PYEXACT_H
#include <stdint.h>
#include "pyrandom.h"

enum { PX_OK = 0, PX_EVALUE = -1, PX_EZERODIV = -2, PX_EOVERFLOW = -3, PX_EINDEX = -4, PX_ETYPE = -5, PX_EMEM = -6 };

/* call once before px_pow / px_real / py_sample_e (loads the UCRT's pow, atan2, cos, sin, hypot, fmod, log via pyx_load) */
int px_init(void);

/* ---- 1. numpy ---------------------------------------------------------------------------------------------- */
/* numpy.arange(start, stop, step) with float64 dtype (any argument a Python float). Returns the length (0 for an
   empty range) or a PX_E* code (step 0 -> PX_EZERODIV, NaN -> PX_EVALUE, too long -> PX_EOVERFLOW).
   out may be NULL to ask only the length; else it gets min(length, cap) values. */
int64_t np_arange(double start, double stop, double step, double *out, int64_t cap);
/* numpy.arange with int64 dtype (all three arguments Python ints) */
int64_t np_arange_i64(int64_t start, int64_t stop, int64_t step, int64_t *out, int64_t cap);
/* numpy.linspace(start, stop, num, endpoint) (float64); out gets num values. PX_EVALUE if num < 0. */
int np_linspace(double start, double stop, int64_t num, int endpoint, double *out);
/* scenes._bare_grid: gx, gy = meshgrid(xs, xs); keep = gx*gx + gy*gy <= r*r; P = column_stack([gx[keep], gy[keep]]).
   P gets 2 doubles a point (x, y), row-major as numpy gives them (needs 2 * n * n room). Returns the point count. */
int64_t np_disc_grid(const double *xs, int64_t n, double r, double *P);
/* the same, starting from r and st: xs = arange(-r + st / 2, r, st) (what _bare_grid does with r = R0 - 1, st = lim / 3).
   P NULL -> only the count. */
int64_t np_bare_grid(double r, double st, double *P, int64_t cap_points);
double np_minimum(double a, double b);                    /* np.minimum: NaN wins, else a < b ? a : b */
double np_maximum(double a, double b);
void np_minimum_at(double *d, const int64_t *idx, const double *v, int64_t n);          /* np.minimum.at(d, idx, v) */
int64_t np_argmax(const double *a, int64_t n);           /* first index of the maximum; the first NaN if any; -1 if n == 0 (ValueError) */
int64_t np_argmin(const double *a, int64_t n);

/* ---- 2. CPython set / dict of float tuples ------------------------------------------------------------------ */
/* hash() of the values the keys are made of */
int64_t px_hash_float(double v);                          /* = hash(v) for a finite or infinite float (not NaN) */
int64_t px_hash_tuple(const int64_t *lane_hashes, int64_t n);          /* tuplehash: xxHash lanes */
/* Key shapes: outer == 0 -> a flat tuple of `inner` floats ((x, y)); outer > 0 -> a tuple of `outer` tuples of
   `inner` floats (((ax, ay), (bx, by)) of the voronoi edge sets); inner == 0 -> a plain float. A key is given as its
   floats in order (outer * inner of them, or inner, or 1). Equality is float == per item (so -0.0 == 0.0; the first
   key added is the one kept, as CPython keeps it). NaN keys are not supported (CPython hashes them by identity). */
typedef struct { const double *key; int64_t hash; } PxSetEntry;          /* key NULL = unused, PX_DUMMY = dummy */
typedef struct {
    int outer, inner, k;                                  /* k = floats a key */
    int64_t fill, used, mask, finger;
    PxSetEntry *table;
    double *store; int64_t nstore, capstore;              /* the keys' floats (never moved: blocks) */
    double **blocks; int nblocks;
} PySetF;
PySetF *pyset_new(int outer, int inner);
void pyset_free(PySetF *s);
int64_t pyset_hash(const PySetF *s, const double *key);          /* hash(key) as CPython computes it */
int pyset_add(PySetF *s, const double *key);             /* set.add: 1 = added, 0 = was there already */
int pyset_discard(PySetF *s, const double *key);         /* set.discard: 1 = removed, 0 = was not there */
int pyset_contains(const PySetF *s, const double *key);
int64_t pyset_len(const PySetF *s);
/* iteration: for (int64_t pos = 0; (key = pyset_next(s, &pos)); ) ... gives list(set) order */
const double *pyset_next(const PySetF *s, int64_t *pos);
void pyset_clear(PySetF *s);                              /* set.clear() */
int pyset_pop(PySetF *s, double *key_out);               /* set.pop(): 0 = popped into key_out, PX_EINDEX if empty */

/* dict keyed by float tuples (same key shapes) with int64 values, in insertion order */
typedef struct { const double *key; int64_t hash, value; int live; } PxDictEntry;
typedef struct {
    int outer, inner, k;
    PxDictEntry *ent; int64_t nent, capent, used;         /* entries in insertion order (dead ones skipped) */
    int64_t *index; int64_t mask;                         /* open addressing over ent indices, -1 empty, -2 dummy */
    double **blocks; int nblocks; double *store; int64_t nstore, capstore;
} PyDictF;
PyDictF *pydict_new(int outer, int inner);
void pydict_free(PyDictF *d);
int pydict_set(PyDictF *d, const double *key, int64_t value);          /* d[key] = value: 1 = new key, 0 = updated (old key kept) */
int pydict_get(const PyDictF *d, const double *key, int64_t *value);   /* 1 = found */
int pydict_del(PyDictF *d, const double *key);           /* del d[key]: 1 = removed, 0 = KeyError */
int64_t pydict_len(const PyDictF *d);
const double *pydict_next(const PyDictF *d, int64_t *pos, int64_t *value);          /* list(d.items()) order */

/* ---- 3. random.Random -------------------------------------------------------------------------------------- */
void py_seed_i64(PyRandom *r, int64_t v);                 /* Random(int), any sign (CPython seeds with abs(n)) */
int py_randrange_e(PyRandom *r, int64_t start, int64_t stop, int64_t step, int64_t *out);          /* randrange(start, stop, step) */
int py_randrange1_e(PyRandom *r, int64_t stop, int64_t *out);          /* randrange(stop) */
int py_randint_e(PyRandom *r, int64_t a, int64_t b, int64_t *out);
int py_choice_e(PyRandom *r, int64_t len, int64_t *out);          /* the index: seq[out]; PX_EINDEX if empty */
int py_sample_e(PyRandom *r, int64_t n, int64_t k, int64_t *out);          /* sample(range(n), k): the indices */
void py_shuffle_i64(PyRandom *r, int64_t *x, int64_t n);          /* shuffle a list of anything: permute the indices x */
/* choices(population of n, weights, k=k): out gets k indices. weights NULL = unweighted (floor(random() * n)).
   The weights are accumulated as itertools.accumulate does (float adds in order). */
int py_choices_w(PyRandom *r, int64_t n, const double *weights, int64_t k, int64_t *out);
int py_choices_cum(PyRandom *r, int64_t n, const double *cum_weights, int64_t k, int64_t *out);          /* cum_weights= */
void py_getstate(const PyRandom *r, PyRandom *state);     /* getstate / setstate (the MT state and its index) */
void py_setstate(PyRandom *r, const PyRandom *state);

/* ---- 4. Python numbers -------------------------------------------------------------------------------------- */
int px_floordiv_d(double a, double b, double *out);       /* a // b for floats (float_floor_div) */
int px_mod_d(double a, double b, double *out);            /* a % b for floats (float_rem) */
int px_divmod_d(double a, double b, double *q, double *m);
int px_floordiv_i(int64_t a, int64_t b, int64_t *out);    /* a // b, a % b for ints */
int px_mod_i(int64_t a, int64_t b, int64_t *out);
int px_int(double x, int64_t *out);                       /* int(x): truncation; NaN -> PX_EVALUE, inf / outside int64 -> PX_EOVERFLOW */
int px_floor(double x, int64_t *out);                     /* math.floor / math.ceil -> int */
int px_ceil(double x, int64_t *out);
double px_round(double x, int nd);                      /* round(x, nd) for 0 <= nd <= 8, exact for every finite x (pyx.h's py_round only below 2^53 / 10^nd) */
/* x ** y for floats (float_pow). Returns 0 with *re = the float result (*im = 0), or 1 when Python gives a complex
   (a negative base to a fractional power: complex_pow / _Py_c_pow), with *re, *im its parts; or a PX_E* code. */
int px_pow(double x, double y, double *re, double *im);
double px_real(double x, double y);                       /* scenes._real(x ** y): the float, or the complex's .real (errors -> NaN) */
double px_sum(const double *x, int64_t n);                /* sum(list of floats) (start 0; Neumaier as builtin sum); n == 0 -> 0 */
double px_vector_norm(const double *v, int64_t n);        /* math.hypot(*v) */
double px_dist(const double *p, const double *q, int64_t n);          /* math.dist(p, q) */
int64_t px_max_idx(const double *keys, int64_t n);        /* max(range(n), key=keys.__getitem__): first of equal keys; -1 if empty */
int64_t px_min_idx(const double *keys, int64_t n);
int px_tuple_lt(const double *a, const double *b, int k);          /* a < b for two tuples of k floats */
int px_tuple_eq(const double *a, const double *b, int k);
/* sorted(range(n), key=...) stable: by float keys (k = 1) or by tuples of k floats (keys row-major, n x k).
   idx gets the order. PX_EVALUE if a key holds a NaN (CPython's timsort order with NaN is not copied). */
int px_sorted_idx(const double *keys, int64_t n, int k, int64_t *idx);
#endif

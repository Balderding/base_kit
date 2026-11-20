/* pyexact.c: Python 3.15 / numpy 2.5.4 behaviour, bit for bit (see pyexact.h). Each function names the CPython or
   numpy routine it copies. Build with -ffp-contract=off -fno-fast-math. */
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pyexact.h"

static double (*px_hypot_u)(double, double);          /* the UCRT's hypot (what _Py_c_pow calls) */

int px_init(void) {
    if (!pyx_load()) return 0;
    HMODULE m = LoadLibraryA("ucrtbase.dll");
    if (!m) return 0;
    px_hypot_u = (double (*)(double, double))(void *)GetProcAddress(m, "_hypot");
    return px_hypot_u != NULL;
}

/* ============================================================================================ 1. numpy */

/* numpy/_core/src/multiarray/ctors.c: PyArray_ArangeObj + _calc_length (len = ceil((stop - start) / step), done in
   Python float arithmetic), then buffer[0] = start, buffer[1] = start + step, DOUBLE_fill for i >= 2:
   buffer[i] = start + i * delta with delta = buffer[1] - buffer[0]. */
int64_t np_arange(double start, double stop, double step, double *out, int64_t cap) {
    if (step == 0.0) return PX_EZERODIV;          /* PyNumber_TrueDivide: ZeroDivisionError */
    double delta0 = stop - start, value = delta0 / step;
    if (isnan(value)) return PX_EVALUE;           /* "arange: cannot compute length" */
    double iv = ceil(value);
    if (value == 0.0 && delta0 != 0.0) iv = signbit(value) ? 0.0 : 1.0;          /* underflow / a step of inf: one value or none */
    if (!(iv <= 9.2233720368547758e18 && iv >= -9.2233720368547758e18)) return PX_EVALUE;          /* "Maximum allowed size exceeded" */
    int64_t len = (int64_t)iv;
    if (len <= 0) return 0;
    if (out) {
        double next = start + step, delta = next - start;
        if (cap > 0) out[0] = start;
        if (cap > 1 && len > 1) out[1] = next;
        for (int64_t i = 2; i < len && i < cap; i++) out[i] = start + (double)i * delta;
    }
    return len;
}

/* the same with int64 dtype (LONG_fill: start + i * delta, delta = step) */
int64_t np_arange_i64(int64_t start, int64_t stop, int64_t step, int64_t *out, int64_t cap) {
    if (step == 0) return PX_EZERODIV;
    double value = (double)(stop - start) / (double)step;          /* int / int true division: correctly rounded */
    double iv = ceil(value);
    int64_t len = (int64_t)iv;
    if (len <= 0) return 0;
    if (out) for (int64_t i = 0; i < len && i < cap; i++) out[i] = start + i * step;
    return len;
}

/* numpy/_core/function_base.py: linspace */
int np_linspace(double start, double stop, int64_t num, int endpoint, double *out) {
    if (num < 0) return PX_EVALUE;
    int64_t div = endpoint ? num - 1 : num;
    double delta = stop - start;
    if (div > 0) {
        double step = delta / (double)div;
        if (step == 0.0) for (int64_t i = 0; i < num; i++) out[i] = ((double)i / (double)div) * delta + start;
        else for (int64_t i = 0; i < num; i++) out[i] = (double)i * step + start;
    } else for (int64_t i = 0; i < num; i++) out[i] = (double)i * delta + start;          /* step = nan: y * delta */
    if (endpoint && num > 1) out[num - 1] = stop;
    return PX_OK;
}

/* scenes._bare_grid: meshgrid(xs, xs) (gx[i][j] = xs[j], gy[i][j] = xs[i]); keep = gx*gx + gy*gy <= r*r;
   column_stack([gx[keep], gy[keep]]) in C order */
int64_t np_disc_grid(const double *xs, int64_t n, double r, double *P) {
    double r2 = r * r; int64_t m = 0;
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < n; j++) {
            double gx = xs[j], gy = xs[i], a = gx * gx, b = gy * gy;
            if (a + b <= r2) { if (P) { P[2 * m] = gx; P[2 * m + 1] = gy; } m++; }
        }
    return m;
}
int64_t np_bare_grid(double r, double st, double *P, int64_t cap_points) {
    int64_t n = np_arange(-r + st / 2.0, r, st, NULL, 0);
    if (n <= 0) return n < 0 ? n : 0;
    double *xs = (double *)malloc(sizeof(double) * (size_t)n);
    if (!xs) return PX_EMEM;
    np_arange(-r + st / 2.0, r, st, xs, n);
    int64_t m = np_disc_grid(xs, n, r, NULL);
    if (P && m <= cap_points) np_disc_grid(xs, n, r, P);
    free(xs); return m;
}

/* loops_minmax.dispatch.c.src: scalar_min / scalar_max (NaN propagates; equal values give the second) */
double np_minimum(double a, double b) { return (a < b || isnan(a)) ? a : b; }
double np_maximum(double a, double b) { return (a > b || isnan(a)) ? a : b; }
void np_minimum_at(double *d, const int64_t *idx, const double *v, int64_t n) { for (int64_t i = 0; i < n; i++) d[idx[i]] = np_minimum(d[idx[i]], v[i]); }

/* DOUBLE_argmax / argmin: the first NaN, else the first index of the extreme */
int64_t np_argmax(const double *a, int64_t n) {
    if (n <= 0) return -1;
    int64_t best = 0; double mp = a[0];
    if (isnan(mp)) return 0;
    for (int64_t i = 1; i < n; i++) { if (a[i] > mp) { mp = a[i]; best = i; } else if (isnan(a[i])) return i; }
    return best;
}
int64_t np_argmin(const double *a, int64_t n) {
    if (n <= 0) return -1;
    int64_t best = 0; double mp = a[0];
    if (isnan(mp)) return 0;
    for (int64_t i = 1; i < n; i++) { if (a[i] < mp) { mp = a[i]; best = i; } else if (isnan(a[i])) return i; }
    return best;
}

/* ============================================================================ 2. CPython set and dict */

int64_t px_hash_float(double v) { return py_hash_double(v); }          /* Python/pyhash.c _Py_HashDouble (pyrandom.h) */

/* Objects/tupleobject.c tuple_hash (xxHash-based, 64-bit lanes) */
int64_t px_hash_tuple(const int64_t *lane, int64_t n) {
    const uint64_t P1 = 11400714785074694791ULL, P2 = 14029467366897019727ULL, P5 = 2870177450012600261ULL;
    uint64_t acc = P5;
    for (int64_t i = 0; i < n; i++) { acc += (uint64_t)lane[i] * P2; acc = (acc << 31) | (acc >> 33); acc *= P1; }
    acc += (uint64_t)n ^ (P5 ^ 3527539UL);
    if (acc == (uint64_t)-1) return 1546275796;
    return (int64_t)acc;
}

static int64_t key_hash(int outer, int inner, const double *key) {
    if (inner == 0) return py_hash_double(key[0]);
    if (outer == 0) { int64_t h[64]; for (int i = 0; i < inner; i++) h[i] = py_hash_double(key[i]); return px_hash_tuple(h, inner); }
    int64_t ho[64];
    for (int o = 0; o < outer; o++) { int64_t h[64]; for (int i = 0; i < inner; i++) h[i] = py_hash_double(key[o * inner + i]); ho[o] = px_hash_tuple(h, inner); }
    return px_hash_tuple(ho, outer);
}
static int key_eq(const double *a, const double *b, int k) { for (int i = 0; i < k; i++) if (!(a[i] == b[i])) return 0; return 1; }

/* the keys' floats live in blocks that never move */
#define PX_BLOCK 4096
static const double *store_key(double ***blocks, int *nblocks, double **store, int64_t *nstore, int64_t *capstore, const double *key, int k) {
    if (!*store || *nstore + k > *capstore) {
        int64_t cap = PX_BLOCK * (int64_t)k;
        double *b = (double *)malloc(sizeof(double) * (size_t)cap);
        double **nb = (double **)realloc(*blocks, sizeof(double *) * (size_t)(*nblocks + 1));
        if (!b || !nb) { free(b); if (nb) *blocks = nb; return NULL; }
        *blocks = nb; nb[(*nblocks)++] = b; *store = b; *nstore = 0; *capstore = cap;
    }
    double *p = *store + *nstore; memcpy(p, key, sizeof(double) * (size_t)k); *nstore += k; return p;
}

/* Objects/setobject.c */
#define LINEAR_PROBES 9
#define PERTURB_SHIFT 5
#define PY_SET_MINSIZE 8
static const double px_dummy_key = 0.0;
#define PX_DUMMY (&px_dummy_key)

PySetF *pyset_new(int outer, int inner) {
    PySetF *s = (PySetF *)calloc(1, sizeof(PySetF));
    if (!s) return NULL;
    s->outer = outer; s->inner = inner; s->k = inner == 0 ? 1 : (outer == 0 ? inner : outer * inner);
    s->mask = PY_SET_MINSIZE - 1; s->table = (PxSetEntry *)calloc(PY_SET_MINSIZE, sizeof(PxSetEntry));
    if (!s->table) { free(s); return NULL; }
    return s;
}
void pyset_free(PySetF *s) {
    if (!s) return;
    for (int i = 0; i < s->nblocks; i++) free(s->blocks[i]);
    free(s->blocks); free(s->table); free(s);
}
int64_t pyset_hash(const PySetF *s, const double *key) { return key_hash(s->outer, s->inner, key); }

/* set_insert_clean */
static void set_insert_clean(PxSetEntry *table, uint64_t mask, const double *key, int64_t hash) {
    uint64_t perturb = (uint64_t)hash, i = (uint64_t)hash & mask; PxSetEntry *entry;
    for (;;) {
        entry = &table[i];
        if (entry->key == NULL) goto found_null;
        if (i + LINEAR_PROBES <= mask) {
            for (int j = 0; j < LINEAR_PROBES; j++) { entry++; if (entry->key == NULL) goto found_null; }
        }
        perturb >>= PERTURB_SHIFT;
        i = (i * 5 + 1 + perturb) & mask;
    }
found_null:
    entry->key = key; entry->hash = hash;
}

/* set_table_resize */
static int set_table_resize(PySetF *s, int64_t minused) {
    uint64_t newsize = PY_SET_MINSIZE;
    while (newsize <= (uint64_t)minused) newsize <<= 1;
    PxSetEntry *old = s->table, *nt = (PxSetEntry *)calloc((size_t)newsize, sizeof(PxSetEntry));
    if (!nt) return PX_EMEM;
    uint64_t oldmask = (uint64_t)s->mask;
    s->mask = (int64_t)newsize - 1; s->table = nt;
    if (s->fill == s->used) {
        for (uint64_t i = 0; i <= oldmask; i++) if (old[i].key != NULL) set_insert_clean(nt, newsize - 1, old[i].key, old[i].hash);
    } else {
        s->fill = s->used;
        for (uint64_t i = 0; i <= oldmask; i++) if (old[i].key != NULL && old[i].key != PX_DUMMY) set_insert_clean(nt, newsize - 1, old[i].key, old[i].hash);
    }
    free(old); return PX_OK;
}

/* set_add_entry */
static int set_add_entry(PySetF *s, const double *key, int64_t hash) {
    uint64_t mask = (uint64_t)s->mask, i = (uint64_t)hash & mask, perturb = (uint64_t)hash;
    PxSetEntry *freeslot = NULL, *entry;
    for (;;) {
        entry = &s->table[i];
        int probes = (i + LINEAR_PROBES <= mask) ? LINEAR_PROBES : 0;
        do {
            if (entry->hash == 0 && entry->key == NULL) goto found_unused_or_dummy;
            if (entry->hash == hash) { if (key_eq(entry->key, key, s->k)) return 0; }          /* found_active */
            else if (entry->hash == -1) freeslot = entry;          /* CPython 3.15 keeps the LAST dummy met (measured) */
            entry++;
        } while (probes--);
        perturb >>= PERTURB_SHIFT;
        i = (i * 5 + 1 + perturb) & mask;
    }
found_unused_or_dummy:
    if (freeslot != NULL) {
        const double *kp = store_key(&s->blocks, &s->nblocks, &s->store, &s->nstore, &s->capstore, key, s->k);
        if (!kp) return PX_EMEM;
        s->used++; freeslot->key = kp; freeslot->hash = hash; return 1;
    }
    {
        const double *kp = store_key(&s->blocks, &s->nblocks, &s->store, &s->nstore, &s->capstore, key, s->k);
        if (!kp) return PX_EMEM;
        s->fill++; s->used++; entry->key = kp; entry->hash = hash;
    }
    if ((uint64_t)s->fill * 5 < mask * 3) return 1;
    int e = set_table_resize(s, s->used > 50000 ? s->used * 2 : s->used * 4);
    return e ? e : 1;
}
int pyset_add(PySetF *s, const double *key) { return set_add_entry(s, key, key_hash(s->outer, s->inner, key)); }

/* set_lookkey */
static PxSetEntry *set_lookkey(const PySetF *s, const double *key, int64_t hash) {
    uint64_t mask = (uint64_t)s->mask, i = (uint64_t)hash & mask, perturb = (uint64_t)hash;
    for (;;) {
        PxSetEntry *entry = &s->table[i];
        int probes = (i + LINEAR_PROBES <= mask) ? LINEAR_PROBES : 0;
        do {
            if (entry->hash == 0 && entry->key == NULL) return entry;
            if (entry->hash == hash && key_eq(entry->key, key, s->k)) return entry;
            entry++;
        } while (probes--);
        perturb >>= PERTURB_SHIFT;
        i = (i * 5 + 1 + perturb) & mask;
    }
}
int pyset_contains(const PySetF *s, const double *key) { return set_lookkey(s, key, key_hash(s->outer, s->inner, key))->key != NULL; }
/* set_discard_entry */
int pyset_discard(PySetF *s, const double *key) {
    PxSetEntry *e = set_lookkey(s, key, key_hash(s->outer, s->inner, key));
    if (e->key == NULL) return 0;
    e->key = PX_DUMMY; e->hash = -1; s->used--; return 1;
}
int64_t pyset_len(const PySetF *s) { return s->used; }
/* setiter_iternext: table order, skipping unused and dummy slots */
const double *pyset_next(const PySetF *s, int64_t *pos) {
    while (*pos <= s->mask) {
        const PxSetEntry *e = &s->table[(*pos)++];
        if (e->key != NULL && e->key != PX_DUMMY) return e->key;
    }
    return NULL;
}
/* set_clear_internal: back to the 8-slot small table */
void pyset_clear(PySetF *s) {
    free(s->table); s->table = (PxSetEntry *)calloc(PY_SET_MINSIZE, sizeof(PxSetEntry));
    s->mask = PY_SET_MINSIZE - 1; s->fill = s->used = 0;          /* finger is left as it was (set_clear_internal does not touch it) */
}
/* set_pop */
int pyset_pop(PySetF *s, double *key_out) {
    if (s->used == 0) return PX_EINDEX;
    PxSetEntry *entry = s->table + (s->finger & s->mask), *limit = s->table + s->mask;
    while (entry->key == NULL || entry->key == PX_DUMMY) { entry++; if (entry > limit) entry = s->table; }
    memcpy(key_out, entry->key, sizeof(double) * (size_t)s->k);
    entry->key = PX_DUMMY; entry->hash = -1; s->used--;
    s->finger = entry - s->table + 1;
    return PX_OK;
}

/* dict: CPython's compact dict iterates its entries array in insertion order; a deleted key leaves its slot, a key
   added again goes to the end, assigning to a present key keeps its place and its first key object. The index
   below is our own (only the order is visible). */
PyDictF *pydict_new(int outer, int inner) {
    PyDictF *d = (PyDictF *)calloc(1, sizeof(PyDictF));
    if (!d) return NULL;
    d->outer = outer; d->inner = inner; d->k = inner == 0 ? 1 : (outer == 0 ? inner : outer * inner);
    d->mask = 7; d->index = (int64_t *)malloc(8 * sizeof(int64_t));
    for (int i = 0; i < 8; i++) d->index[i] = -1;
    return d;
}
void pydict_free(PyDictF *d) {
    if (!d) return;
    for (int i = 0; i < d->nblocks; i++) free(d->blocks[i]);
    free(d->blocks); free(d->ent); free(d->index); free(d);
}
static int64_t *dict_slot(const PyDictF *d, const double *key, int64_t hash, int for_insert) {
    uint64_t mask = (uint64_t)d->mask, i = (uint64_t)hash & mask; int64_t *first_dummy = NULL;
    for (;;) {
        int64_t *sl = &d->index[i];
        if (*sl == -1) return (for_insert && first_dummy) ? first_dummy : sl;
        if (*sl == -2) { if (!first_dummy) first_dummy = sl; }
        else if (d->ent[*sl].hash == hash && key_eq(d->ent[*sl].key, key, d->k)) return sl;
        i = (i + 1) & mask;
    }
}
static void dict_rebuild(PyDictF *d, int64_t want) {
    int64_t size = 8; while (size < want * 3) size <<= 1;
    free(d->index); d->index = (int64_t *)malloc(sizeof(int64_t) * (size_t)size); d->mask = size - 1;
    for (int64_t i = 0; i < size; i++) d->index[i] = -1;
    for (int64_t e = 0; e < d->nent; e++) if (d->ent[e].live) {
        uint64_t i = (uint64_t)d->ent[e].hash & (uint64_t)d->mask;
        while (d->index[i] != -1) i = (i + 1) & (uint64_t)d->mask;
        d->index[i] = e;
    }
}
int pydict_set(PyDictF *d, const double *key, int64_t value) {
    int64_t hash = key_hash(d->outer, d->inner, key);
    int64_t *sl = dict_slot(d, key, hash, 0);
    if (*sl >= 0) { d->ent[*sl].value = value; return 0; }
    if (d->nent == d->capent) {
        int64_t cap = d->capent ? d->capent * 2 : 16;
        PxDictEntry *ne = (PxDictEntry *)realloc(d->ent, sizeof(PxDictEntry) * (size_t)cap);
        if (!ne) return PX_EMEM;
        d->ent = ne; d->capent = cap;
    }
    const double *kp = store_key(&d->blocks, &d->nblocks, &d->store, &d->nstore, &d->capstore, key, d->k);
    if (!kp) return PX_EMEM;
    int64_t e = d->nent++;
    d->ent[e].key = kp; d->ent[e].hash = hash; d->ent[e].value = value; d->ent[e].live = 1; d->used++;
    if ((d->nent + 1) * 3 > (d->mask + 1) * 2) dict_rebuild(d, d->nent + 1);
    else *dict_slot(d, key, hash, 1) = e;
    return 1;
}
int pydict_get(const PyDictF *d, const double *key, int64_t *value) {
    int64_t *sl = dict_slot(d, key, key_hash(d->outer, d->inner, key), 0);
    if (*sl < 0) return 0;
    if (value) *value = d->ent[*sl].value;
    return 1;
}
int pydict_del(PyDictF *d, const double *key) {
    int64_t *sl = dict_slot(d, key, key_hash(d->outer, d->inner, key), 0);
    if (*sl < 0) return 0;
    d->ent[*sl].live = 0; *sl = -2; d->used--; return 1;
}
int64_t pydict_len(const PyDictF *d) { return d->used; }
const double *pydict_next(const PyDictF *d, int64_t *pos, int64_t *value) {
    while (*pos < d->nent) {
        const PxDictEntry *e = &d->ent[(*pos)++];
        if (e->live) { if (value) *value = e->value; return e->key; }
    }
    return NULL;
}

/* ===================================================================================== 3. random.Random */

/* Modules/_randommodule.c random_seed: an int seeds with abs(n) */
void py_seed_i64(PyRandom *r, int64_t v) { py_seed_u64(r, v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v); }

static int64_t floordiv_i(int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }

/* Lib/random.py randrange */
int py_randrange_e(PyRandom *r, int64_t start, int64_t stop, int64_t step, int64_t *out) {
    int64_t width = stop - start, n;
    if (step == 1) { if (width > 0) { *out = start + py_randbelow(r, width); return PX_OK; } return PX_EVALUE; }
    if (step > 0) n = floordiv_i(width + step - 1, step);
    else if (step < 0) n = floordiv_i(width + step + 1, step);
    else return PX_EVALUE;
    if (n <= 0) return PX_EVALUE;
    *out = start + step * py_randbelow(r, n); return PX_OK;
}
int py_randrange1_e(PyRandom *r, int64_t stop, int64_t *out) { if (stop > 0) { *out = py_randbelow(r, stop); return PX_OK; } return PX_EVALUE; }
int py_randint_e(PyRandom *r, int64_t a, int64_t b, int64_t *out) { if (b < a) return PX_EVALUE; *out = a + py_randbelow(r, b - a + 1); return PX_OK; }
int py_choice_e(PyRandom *r, int64_t len, int64_t *out) { if (len <= 0) return PX_EINDEX; *out = py_randbelow(r, len); return PX_OK; }
int py_sample_e(PyRandom *r, int64_t n, int64_t k, int64_t *out) { if (!(0 <= k && k <= n)) return PX_EVALUE; py_sample_idx(r, n, k, out); return PX_OK; }
void py_shuffle_i64(PyRandom *r, int64_t *x, int64_t n) { py_shuffle_idx(r, x, n); }

/* Lib/random.py choices */
int py_choices_cum(PyRandom *r, int64_t n, const double *cum, int64_t k, int64_t *out) {
    if (n <= 0) return PX_EINDEX;                 /* cum_weights[-1] */
    double total = cum[n - 1] + 0.0;
    if (total <= 0.0) return PX_EVALUE;
    if (!isfinite(total)) return PX_EVALUE;
    int64_t hi0 = n - 1;
    for (int64_t t = 0; t < k; t++) {
        double x = py_random(r) * total; int64_t lo = 0, hi = hi0;          /* bisect_right(cum, x, 0, hi) */
        while (lo < hi) { int64_t mid = (lo + hi) / 2; if (x < cum[mid]) hi = mid; else lo = mid + 1; }
        out[t] = lo;
    }
    return PX_OK;
}
int py_choices_w(PyRandom *r, int64_t n, const double *w, int64_t k, int64_t *out) {
    if (!w) {
        double fn = (double)n + 0.0;
        for (int64_t t = 0; t < k; t++) {
            int64_t i = (int64_t)floor(py_random(r) * fn);
            if (i >= n) return PX_EINDEX;          /* population[...] of an empty population */
            out[t] = i;
        }
        return PX_OK;
    }
    if (n <= 0) return PX_EINDEX;
    double *cum = (double *)malloc(sizeof(double) * (size_t)n);
    if (!cum) return PX_EMEM;
    double acc = w[0]; cum[0] = acc;
    for (int64_t i = 1; i < n; i++) { acc = acc + w[i]; cum[i] = acc; }          /* itertools.accumulate */
    int e = py_choices_cum(r, n, cum, k, out);
    free(cum); return e;
}
void py_getstate(const PyRandom *r, PyRandom *st) { *st = *r; }
void py_setstate(PyRandom *r, const PyRandom *st) { *r = *st; }

/* ===================================================================================== 4. Python numbers */

/* Objects/floatobject.c _float_div_mod / float_floor_div / float_rem */
int px_divmod_d(double vx, double wx, double *floordiv, double *mod) {
    if (wx == 0.0) return PX_EZERODIV;
    double m = py_fmod(vx, wx), div = (vx - m) / wx, fd;
    if (m) { if ((wx < 0) != (m < 0)) { m += wx; div -= 1.0; } }
    else m = copysign(0.0, wx);
    if (div) { fd = floor(div); if (div - fd > 0.5) fd += 1.0; }
    else fd = copysign(0.0, vx / wx);
    if (floordiv) *floordiv = fd;
    if (mod) *mod = m;
    return PX_OK;
}
int px_floordiv_d(double a, double b, double *out) { return px_divmod_d(a, b, out, NULL); }
int px_mod_d(double vx, double wx, double *out) {
    if (wx == 0.0) return PX_EZERODIV;
    double m = py_fmod(vx, wx);
    if (m) { if ((wx < 0) != (m < 0)) m += wx; }
    else m = copysign(0.0, wx);
    *out = m; return PX_OK;
}
int px_floordiv_i(int64_t a, int64_t b, int64_t *out) {
    if (b == 0) return PX_EZERODIV;
    if (a == INT64_MIN && b == -1) return PX_EOVERFLOW;          /* a Python int would grow past int64 */
    *out = floordiv_i(a, b); return PX_OK;
}
int px_mod_i(int64_t a, int64_t b, int64_t *out) {
    if (b == 0) return PX_EZERODIV;
    if (b == -1) { *out = 0; return PX_OK; }
    int64_t m = a % b; if (m != 0 && ((m < 0) != (b < 0))) m += b;
    *out = m; return PX_OK;
}
static int to_i64(double t, int64_t *out) {
    if (isnan(t)) return PX_EVALUE;
    if (!(t >= -9223372036854775808.0 && t < 9223372036854775808.0)) return PX_EOVERFLOW;
    *out = (int64_t)t; return PX_OK;
}
int px_int(double x, int64_t *out) { return to_i64(trunc(x), out); }          /* float.__trunc__ */
int px_floor(double x, int64_t *out) { return to_i64(floor(x), out); }
int px_ceil(double x, int64_t *out) { return to_i64(ceil(x), out); }

/* Objects/floatobject.c double_round (round(x, nd), 0 <= nd <= 8): the exact x * 10^nd rounded half-even to an
   integer Q (dtoa mode 3), then the nearest double to Q / 10^nd (strtod). pyx.h's py_round does the same but is
   exact only while Q < 2^53; this one is exact for every finite x. */
double px_round(double x, int nd) {
    static const uint64_t P10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
    if (!isfinite(x) || x == 0.0 || nd < 0 || nd > 8) return x;
    uint64_t b; memcpy(&b, &x, 8);
    int neg = (int)(b >> 63), ex = (int)((b >> 52) & 0x7ff); uint64_t m = b & ((1ULL << 52) - 1);
    if (ex == 0) ex = 1; else m |= 1ULL << 52;
    int s = 1075 - ex;                                /* |x| = m / 2^s */
    if (s <= nd) return x;                            /* x has at most nd decimals: round gives x back */
    unsigned __int128 N = (unsigned __int128)m * P10[nd], Q;          /* |x| * 10^nd = N / 2^s */
    if (s >= 127) Q = 0;
    else {
        unsigned __int128 half = (unsigned __int128)1 << (s - 1), R;
        Q = N >> s; R = N - (Q << s);
        if (R > half || (R == half && (Q & 1))) Q += 1;
    }
    double r;
    if (Q < ((unsigned __int128)1 << 53)) r = (double)Q / (double)P10[nd];          /* both exact: one IEEE division */
    else {                                            /* Q < 2^71: Q * 2^56 / 10^nd as an integer with a sticky bit */
        unsigned __int128 T = (Q << 56) / P10[nd], Rm = (Q << 56) - T * P10[nd];
        if (Rm) T |= 1;                               /* T has > 80 bits: its lowest bit is far below the rounding place */
        r = ldexp((double)T, -56);
    }
    return neg ? -r : r;
}

/* Objects/floatobject.c float_pow; the complex branch is Objects/complexobject.c complex_pow -> _Py_c_pow */
static int is_odd_integer(double x) { return fmod(fabs(x), 2.0) == 1.0; }          /* DOUBLE_IS_ODD_INTEGER */
int px_pow(double iv, double iw, double *re, double *im) {
    int negate = 0; *im = 0.0;
    if (iw == 0) { *re = 1.0; return 0; }
    if (isnan(iv)) { *re = iv; return 0; }
    if (isnan(iw)) { *re = iv == 1.0 ? 1.0 : iw; return 0; }
    if (isinf(iw)) {
        iv = fabs(iv);
        if (iv == 1.0) *re = 1.0;
        else if ((iw > 0.0) == (iv > 1.0)) *re = fabs(iw);
        else *re = 0.0;
        return 0;
    }
    if (isinf(iv)) {
        int odd = is_odd_integer(iw);
        if (iw > 0.0) *re = odd ? iv : fabs(iv);
        else *re = odd ? copysign(0.0, iv) : 0.0;
        return 0;
    }
    if (iv == 0.0) {
        int odd = is_odd_integer(iw);
        if (iw < 0.0) return PX_EZERODIV;
        *re = odd ? iv : 0.0; return 0;
    }
    if (iv < 0.0) {
        if (iw != floor(iw)) {          /* complex: _Py_c_pow((iv, 0), (iw, 0)) */
            double vabs = px_hypot_u(iv, 0.0), len = py_pow(vabs, iw), at = py_atan2(0.0, iv), phase = at * iw;
            double r_ = len * py_cos(phase), i_ = len * py_sin(phase);
            if (isinf(r_) || isinf(i_)) return PX_EOVERFLOW;          /* _Py_ADJUST_ERANGE2: "complex exponentiation" */
            *re = r_; *im = i_; return 1;
        }
        iv = -iv; negate = is_odd_integer(iw);
    }
    if (iv == 1.0) { *re = negate ? -1.0 : 1.0; return 0; }
    double ix = py_pow(iv, iw);
    if (isinf(ix)) return PX_EOVERFLOW;          /* _Py_ADJUST_ERANGE1: an infinite result from finite input = ERANGE */
    *re = negate ? -ix : ix; return 0;
}
double px_real(double x, double y) { double re, im; int e = px_pow(x, y, &re, &im); return e < 0 ? NAN : re; }

/* Python/bltinmodule.c builtin_sum_impl for a list of floats (start = int 0): 0 + x0, then Neumaier compensation */
double px_sum(const double *x, int64_t n) {
    if (n <= 0) return 0.0;
    double f = 0.0 + x[0], c = 0.0;
    for (int64_t i = 1; i < n; i++) {
        double xi = x[i], t = f + xi;
        if (fabs(f) >= fabs(xi)) c += (f - t) + xi; else c += (xi - t) + f;
        f = t;
    }
    if (c && isfinite(c)) f += c;
    return f;
}

/* Modules/mathmodule.c vector_norm (math.hypot, math.dist) */
__attribute__((target("fma"))) static double vector_norm(int64_t n, double *vec, double max, int found_nan) {
    double x, h, scale, csum = 1.0, frac1 = 0.0, frac2 = 0.0; int max_e;
    if (isinf(max)) return max;
    if (found_nan) return NAN;
    if (max == 0.0 || n <= 1) return max;
    frexp(max, &max_e);
    if (max_e < -1023) {
        for (int64_t i = 0; i < n; i++) vec[i] /= 2.2250738585072014e-308;
        return 2.2250738585072014e-308 * vector_norm(n, vec, max / 2.2250738585072014e-308, found_nan);
    }
    scale = ldexp(1.0, -max_e);
    for (int64_t i = 0; i < n; i++) {
        x = vec[i] * scale;
        double phi = x * x, plo = __builtin_fma(x, x, -phi);
        double shi = csum + phi, slo = (csum - shi) + phi;          /* dl_fast_sum */
        csum = shi; frac1 += plo; frac2 += slo;
    }
    h = sqrt(csum - 1.0 + (frac1 + frac2));
    { double phi = -h * h, plo = __builtin_fma(-h, h, -phi); double shi = csum + phi, slo = (csum - shi) + phi; csum = shi; frac1 += plo; frac2 += slo; }
    x = csum - 1.0 + (frac1 + frac2);
    h += x / (2.0 * h);
    return h / scale;
}
double px_vector_norm(const double *v, int64_t n) {
    double buf[64], *vec = n <= 64 ? buf : (double *)malloc(sizeof(double) * (size_t)n), max = 0.0; int nan_ = 0;
    for (int64_t i = 0; i < n; i++) { double x = fabs(v[i]); vec[i] = x; nan_ |= isnan(x); if (x > max) max = x; }
    double r = vector_norm(n, vec, max, nan_);
    if (vec != buf) free(vec);
    return r;
}
double px_dist(const double *p, const double *q, int64_t n) {
    double buf[64], *vec = n <= 64 ? buf : (double *)malloc(sizeof(double) * (size_t)n), max = 0.0; int nan_ = 0;
    for (int64_t i = 0; i < n; i++) { double x = fabs(p[i] - q[i]); vec[i] = x; nan_ |= isnan(x); if (x > max) max = x; }
    double r = vector_norm(n, vec, max, nan_);
    if (vec != buf) free(vec);
    return r;
}

/* Python/bltinmodule.c min_max: a later item wins only if strictly greater (max) / smaller (min) */
int64_t px_max_idx(const double *k, int64_t n) { if (n <= 0) return -1; int64_t b = 0; for (int64_t i = 1; i < n; i++) if (k[i] > k[b]) b = i; return b; }
int64_t px_min_idx(const double *k, int64_t n) { if (n <= 0) return -1; int64_t b = 0; for (int64_t i = 1; i < n; i++) if (k[i] < k[b]) b = i; return b; }

/* Objects/tupleobject.c tuple_richcompare: the first items that differ decide */
int px_tuple_lt(const double *a, const double *b, int k) { for (int i = 0; i < k; i++) if (!(a[i] == b[i])) return a[i] < b[i]; return 0; }
int px_tuple_eq(const double *a, const double *b, int k) { return key_eq(a, b, k); }

/* sorted(): stable; with no NaN keys every stable sort gives timsort's order. A merge sort. */
static void msort(const double *keys, int k, int64_t *a, int64_t *tmp, int64_t n) {
    if (n < 2) return;
    int64_t h = n / 2; msort(keys, k, a, tmp, h); msort(keys, k, a + h, tmp, n - h);
    int64_t i = 0, j = h, t = 0;
    while (i < h && j < n) { if (px_tuple_lt(keys + a[j] * k, keys + a[i] * k, k)) tmp[t++] = a[j++]; else tmp[t++] = a[i++]; }
    while (i < h) tmp[t++] = a[i++];
    while (j < n) tmp[t++] = a[j++];
    memcpy(a, tmp, sizeof(int64_t) * (size_t)n);
}
int px_sorted_idx(const double *keys, int64_t n, int k, int64_t *idx) {
    for (int64_t i = 0; i < n * k; i++) if (isnan(keys[i])) return PX_EVALUE;
    for (int64_t i = 0; i < n; i++) idx[i] = i;
    int64_t *tmp = (int64_t *)malloc(sizeof(int64_t) * (size_t)(n ? n : 1));
    if (!tmp) return PX_EMEM;
    msort(keys, k, idx, tmp, n); free(tmp);
    return PX_OK;
}

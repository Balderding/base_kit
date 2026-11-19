/* omap.h: a hash map from a key of KW 64-bit integers to a dense index 0, 1, 2 ... in insertion order, like a
   Python dict's order. Values live in the caller's own arrays, indexed by that number. */
#ifndef OMAP_H
#define OMAP_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int kw; int n, cap; int64_t *keys; int32_t *slot; uint32_t smask; } OMap;

static inline uint64_t om_hash(const int64_t *k, int kw) {
    uint64_t h = 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < kw; i++) { h ^= (uint64_t)k[i] + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2); h *= 0xff51afd7ed558ccdULL; }
    return h ^ (h >> 33);
}
static inline void om_init(OMap *m, int kw) { memset(m, 0, sizeof *m); m->kw = kw; }
static inline void om_free(OMap *m) { free(m->keys); free(m->slot); memset(m, 0, sizeof *m); }
static inline void om_clear(OMap *m) { int kw = m->kw; om_free(m); om_init(m, kw); }
static void om_rehash(OMap *m, uint32_t ns) {
    free(m->slot); m->slot = (int32_t *)malloc(sizeof(int32_t) * ns); memset(m->slot, 0xff, sizeof(int32_t) * ns); m->smask = ns - 1;
    for (int i = 0; i < m->n; i++) {
        uint32_t s = (uint32_t)om_hash(m->keys + (size_t)i * m->kw, m->kw) & m->smask;
        while (m->slot[s] >= 0) s = (s + 1) & m->smask;
        m->slot[s] = i;
    }
}
/* index of key k, or -1 */
static inline int om_find(const OMap *m, const int64_t *k) {
    if (!m->slot) return -1;
    uint32_t s = (uint32_t)om_hash(k, m->kw) & m->smask;
    for (;;) {
        int32_t i = m->slot[s];
        if (i < 0) return -1;
        if (!memcmp(m->keys + (size_t)i * m->kw, k, sizeof(int64_t) * m->kw)) return i;
        s = (s + 1) & m->smask;
    }
}
/* index of key k, added at the end if new (*isnew set when given) */
static inline int om_put(OMap *m, const int64_t *k, int *isnew) {
    int i = om_find(m, k);
    if (isnew) *isnew = i < 0;
    if (i >= 0) return i;
    if (m->n == m->cap) { m->cap = m->cap ? m->cap * 2 : 64; m->keys = (int64_t *)realloc(m->keys, sizeof(int64_t) * m->kw * m->cap); }
    memcpy(m->keys + (size_t)m->n * m->kw, k, sizeof(int64_t) * m->kw); i = m->n++;
    if (!m->slot || (uint32_t)m->n * 2 > m->smask + 1) om_rehash(m, m->slot ? (m->smask + 1) * 2 : 128);
    else {
        uint32_t s = (uint32_t)om_hash(k, m->kw) & m->smask;
        while (m->slot[s] >= 0) s = (s + 1) & m->smask;
        m->slot[s] = i;
    }
    return i;
}
static inline const int64_t *om_key(const OMap *m, int i) { return m->keys + (size_t)i * m->kw; }

/* a growable array */
#define VEC(T) struct { T *a; int n, cap; }
#define VPUSH(v, x) do { if ((v).n == (v).cap) { (v).cap = (v).cap ? (v).cap * 2 : 8; (v).a = realloc((v).a, sizeof(*(v).a) * (v).cap); } (v).a[(v).n++] = (x); } while (0)
#define VFREE(v) do { free((v).a); (v).a = NULL; (v).n = (v).cap = 0; } while (0)
#endif

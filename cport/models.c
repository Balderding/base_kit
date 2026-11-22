/* models.c: see models.h. Part 1: trimesh's OBJ and GLB loading (trimesh 5.2.0, numpy 2.5.4 with OpenBLAS 0.3.34
   on this PC). Part 2: sd_mesh's posing. Part 3: sd_mesh.solid and its sections. Part 4: tools\get_models.py.
   Part 5: the model files packed into the exe (mdl_pack, mdl_blob_use). Proofs: work\cscene\models\prove_*.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "models.h"
#include "pyx.h"

#define FMA(a, b, c) __builtin_fma((a), (b), (c))
#define FMAFN __attribute__((target("fma")))          /* the hardware fused multiply-add, as OpenBLAS uses it */

static int unsup_line;                                              /* where the last MDL_ERR_UNSUPPORTED came from */
int mdl_unsupported_line(void) { return unsup_line; }
#define UNSUP (unsup_line = __LINE__, MDL_ERR_UNSUPPORTED)

void mdl_free(MdlMesh *m) { if (m) { free(m->v); free(m->f); m->v = NULL; m->f = NULL; m->nv = m->nf = 0; } }

/* ======================= small growable buffers ======================= */
typedef struct { double *a; size_t n, cap; } DVec;
typedef struct { int64_t *a; size_t n, cap; } IVec;
static int dpush(DVec *v, double x) {
    if (v->n == v->cap) { size_t c = v->cap ? v->cap * 2 : 256; double *p = realloc(v->a, c * sizeof *p); if (!p) return 0; v->a = p; v->cap = c; }
    v->a[v->n++] = x; return 1;
}
static int ipush(IVec *v, int64_t x) {
    if (v->n == v->cap) { size_t c = v->cap ? v->cap * 2 : 256; int64_t *p = realloc(v->a, c * sizeof *p); if (!p) return 0; v->a = p; v->cap = c; }
    v->a[v->n++] = x; return 1;
}

/* ======================= Python str helpers on ASCII bytes ======================= */
typedef struct { const char *p; size_t n; } Str;
static int py_space(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x1f); }   /* str.isspace() for ASCII */
static Str s_strip(Str s) {                                         /* str.strip() */
    while (s.n && py_space((unsigned char)s.p[0])) { s.p++; s.n--; }
    while (s.n && py_space((unsigned char)s.p[s.n - 1])) s.n--;
    return s;
}
static long s_find(Str s, const char *pat, long start, long end) {  /* str.find(sub, start, end) */
    size_t m = strlen(pat);
    if (end < 0 || end > (long)s.n) end = (long)s.n;
    for (long i = start; i + (long)m <= end; i++) if (!memcmp(s.p + i, pat, m)) return i;
    return -1;
}
static long s_rfind(Str s, const char *pat) {                       /* str.rfind(sub) */
    size_t m = strlen(pat);
    if (m > s.n) return -1;
    for (long i = (long)(s.n - m); i >= 0; i--) if (!memcmp(s.p + i, pat, m)) return i;
    return -1;
}
static int s_starts(Str s, const char *pat) { size_t m = strlen(pat); return s.n >= m && !memcmp(s.p, pat, m); }
static Str s_line(Str s) { const char *e = memchr(s.p, '\n', s.n); if (e) s.n = (size_t)(e - s.p); return s; }   /* s.split("\n", 1)[0] */
static int s_count_split(Str s) {                                   /* len(s.split()) */
    int n = 0; size_t i = 0;
    while (i < s.n) { while (i < s.n && py_space((unsigned char)s.p[i])) i++; if (i < s.n) { n++; while (i < s.n && !py_space((unsigned char)s.p[i])) i++; } }
    return n;
}
static int s_count_split_slash(Str s) {                             /* len(s.replace("/", " ").split()) */
    int n = 0; size_t i = 0;
#define SP(c) (py_space((unsigned char)(c)) || (c) == '/')
    while (i < s.n) { while (i < s.n && SP(s.p[i])) i++; if (i < s.n) { n++; while (i < s.n && !SP(s.p[i])) i++; } }
#undef SP
    return n;
}
/* s.replace(a, b) into a new malloc'd buffer (left to right, not overlapping, as Python) */
static char *s_replace(Str s, const char *a, const char *b, size_t *outn) {
    size_t na = strlen(a), nb = strlen(b), cap = s.n + 1, o = 0;
    if (nb > na) cap = s.n * (nb / na + 1) + 1;
    char *r = malloc(cap);
    if (!r) return NULL;
    for (size_t i = 0; i < s.n;) {
        if (i + na <= s.n && !memcmp(s.p + i, a, na)) { memcpy(r + o, b, nb); o += nb; i += na; }
        else r[o++] = s.p[i++];
    }
    r[o] = 0; *outn = o; return r;
}

/* ======================= numpy's text parsing ======================= */
/* the longest prefix of s that PyOS_string_to_double / NumPyOS_ascii_strtod reads as a float (no leading space):
   [+-] (inf | infinity | nan | digits [. digits] | . digits) [e [+-] digits]; returns its length (0 = none) */
static size_t float_prefix(const char *s, size_t n) {
    size_t i = 0, d = 0;
    if (i < n && (s[i] == '+' || s[i] == '-')) i++;
    if (n - i >= 3) {
        if (!strncasecmp(s + i, "infinity", n - i >= 8 ? 8 : 0) && n - i >= 8) return i + 8;
        if (!strncasecmp(s + i, "inf", 3)) return i + 3;
        if (!strncasecmp(s + i, "nan", 3)) return i + 3;
    }
    while (i < n && s[i] >= '0' && s[i] <= '9') { i++; d++; }
    if (i < n && s[i] == '.') { i++; while (i < n && s[i] >= '0' && s[i] <= '9') { i++; d++; } }
    if (!d) return 0;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        size_t j = i + 1;
        if (j < n && (s[j] == '+' || s[j] == '-')) j++;
        if (j < n && s[j] >= '0' && s[j] <= '9') { while (j < n && s[j] >= '0' && s[j] <= '9') j++; i = j; }
    }
    return i;
}
static double parse_float(const char *s, size_t n) {               /* the token, correctly rounded (strtod) */
    char buf[512];
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, s, n); buf[n] = 0;
    return strtod(buf, NULL);
}
/* np.fromstring(s, sep=" ", dtype=np.float64): numbers until one does not parse */
static int fromstring_f64(Str s, DVec *out) {
    size_t i = 0;
    for (;;) {
        while (i < s.n && py_space((unsigned char)s.p[i])) i++;     /* the parser skips leading space */
        size_t k = float_prefix(s.p + i, s.n - i);
        if (!k) break;
        if (!dpush(out, parse_float(s.p + i, k))) return 0;
        i += k;
        while (i < s.n && py_space((unsigned char)s.p[i])) i++;     /* the separator " ": any run of space */
        if (i >= s.n) break;
    }
    return 1;
}
/* np.fromstring(s, sep=" ", dtype=np.int64): NumPyOS_strtoll base 10 */
static int fromstring_i64(Str s, IVec *out) {
    size_t i = 0;
    for (;;) {
        while (i < s.n && py_space((unsigned char)s.p[i])) i++;
        size_t j = i; int neg = 0;
        if (j < s.n && (s.p[j] == '+' || s.p[j] == '-')) { neg = s.p[j] == '-'; j++; }
        size_t d0 = j; uint64_t v = 0; int ovf = 0;
        while (j < s.n && s.p[j] >= '0' && s.p[j] <= '9') { uint64_t nv = v * 10 + (uint64_t)(s.p[j] - '0'); if (nv / 10 != v) ovf = 1; v = nv; j++; }
        if (j == d0) break;
        if (ovf || v > (uint64_t)INT64_MAX + (uint64_t)neg) v = neg ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;   /* strtoll clamps */
        if (!ipush(out, neg ? (int64_t)(0 - v) : (int64_t)v)) return 0;
        i = j;
        while (i < s.n && py_space((unsigned char)s.p[i])) i++;
        if (i >= s.n) break;
    }
    return 1;
}
/* Python float(token) for a whole token (the slow vertex path): -1 = would raise ValueError, -2 = not copied */
static int py_float_token(Str t, double *x) {
    t = s_strip(t);
    for (size_t i = 0; i < t.n; i++) if (t.p[i] == '_') return -2;
    size_t k = float_prefix(t.p, t.n);
    if (!k || k != t.n) return -1;
    *x = parse_float(t.p, k); return 0;
}

/* ======================= OBJ: trimesh.exchange.obj ======================= */
typedef struct { double *a; long rows; int cols; int present; } Arr2;   /* a vertex property array (None = !present) */

/* one key's rows: [i.split("\n", 1)[0] for i in chunk.split(f"\n{k} ")[1:]] */
typedef struct { Str *v; long n, cap; } StrList;
static int slpush(StrList *l, Str s) {
    if (l->n == l->cap) { long c = l->cap ? l->cap * 2 : 256; Str *p = realloc(l->v, (size_t)c * sizeof *p); if (!p) return 0; l->v = p; l->cap = c; }
    l->v[l->n++] = s; return 1;
}
static int split_rows(Str chunk, const char *k, StrList *out) {
    char pat[8]; snprintf(pat, sizeof pat, "\n%s ", k);
    size_t m = strlen(pat); long i = 0, first = 1;
    for (;;) {
        long j = s_find(chunk, pat, i, -1);
        if (!first) { Str piece = { chunk.p + i, (size_t)((j < 0 ? (long)chunk.n : j) - i) }; if (!slpush(out, s_line(piece))) return 0; }
        if (j < 0) break;
        first = 0; i = j + (long)m;
    }
    return 1;
}
/* _parse_vertices for one key: the fast path (np.fromstring of the joined rows) or the slow loop */
static int parse_rows(StrList *rows, Arr2 *out) {
    int per_row = s_count_split(rows->v[0]);
    size_t tot = 0;
    for (long i = 0; i < rows->n; i++) tot += rows->v[i].n + 1;
    char *j = malloc(tot + 1); size_t o = 0;
    if (!j) return MDL_ERR_MEM;
    for (long i = 0; i < rows->n; i++) { if (i) j[o++] = ' '; memcpy(j + o, rows->v[i].p, rows->v[i].n); o += rows->v[i].n; }
    DVec d = {0};
    Str js = { j, o };
    if (!fromstring_f64(js, &d)) { free(j); free(d.a); return MDL_ERR_MEM; }
    free(j);
    if ((long)d.n == rows->n * (long)per_row) { out->a = d.a; out->rows = rows->n; out->cols = per_row; out->present = 1; return MDL_OK; }
    free(d.a); d = (DVec){0};
    /* the slow loop: str.splitlines of each row, .strip().split(), clipped to the shortest, np.array(float64) */
    long nl = 0; int count = 1 << 30;
    for (long i = 0; i < rows->n; i++) {
        Str r = rows->v[i];
        for (size_t a = 0; a < r.n; a++) if (r.p[a] == '\r' || r.p[a] == 0x0b || r.p[a] == 0x0c || (r.p[a] >= 0x1c && r.p[a] <= 0x1e)) return UNSUP;
        int c = s_count_split(r); if (c < count) count = c; nl++;
    }
    if (count < 1) return UNSUP;
    for (long i = 0; i < rows->n; i++) {
        Str r = rows->v[i]; size_t a = 0; int c = 0;
        while (c < count) {
            while (a < r.n && py_space((unsigned char)r.p[a])) a++;
            size_t b = a; while (b < r.n && !py_space((unsigned char)r.p[b])) b++;
            double x; int e = py_float_token((Str){ r.p + a, b - a }, &x);
            if (e == -1) { free(d.a); return MDL_ERR_RAISE; }
            if (e) { free(d.a); return UNSUP; }
            if (!dpush(&d, x)) { free(d.a); return MDL_ERR_MEM; }
            a = b; c++;
        }
    }
    out->a = d.a; out->rows = nl; out->cols = count; out->present = 1; return MDL_OK;
}
/* _parse_vertices(text): v (n x 3 after the colour clip), and whether vn is there */
static int parse_vertices(Str text, Arr2 *v, int *has_vn, int *has_v) {
    static const char *K[3] = { "v", "vt", "vn" };
    long starts[3], ends[3]; char pat[8];
    *has_vn = 0; *has_v = 0; memset(v, 0, sizeof *v);
    int any = 0;
    for (int k = 0; k < 3; k++) { snprintf(pat, sizeof pat, "\n%s ", K[k]); starts[k] = s_find(text, pat, 0, -1); any |= starts[k] >= 0; }
    if (!any) return MDL_OK;
    long start = -1, end = -1;
    for (int k = 0; k < 3; k++) if (starts[k] >= 0) {
        snprintf(pat, sizeof pat, "\n%s ", K[k]);
        ends[k] = s_find(text, "\n", s_rfind(text, pat) + 2 + (long)strlen(K[k]), -1);
        if (start < 0 || starts[k] < start) start = starts[k];
        if (ends[k] >= 0 && ends[k] > end) end = ends[k];
    }
    Str raw = { text.p + start, (size_t)(end - start) };           /* text[start:end] (end >= 0: the text ends with "\n") */
    if (end < start) raw.n = 0;
    size_t n1, n2;
    char *c1 = s_replace(raw, "+e", "e", &n1); if (!c1) return MDL_ERR_MEM;
    char *c2 = s_replace((Str){ c1, n1 }, "-e", "e", &n2); free(c1); if (!c2) return MDL_ERR_MEM;
    Str chunk = { c2, n2 };
    int rc = MDL_OK;
    for (int k = 0; k < 3 && rc == MDL_OK; k++) if (starts[k] >= 0) {
        StrList rows = {0}; Arr2 a = {0};
        if (!split_rows(chunk, K[k], &rows)) { rc = MDL_ERR_MEM; break; }
        if (rows.n == 0) { free(rows.v); rc = MDL_ERR_RAISE; break; }     /* v[0] of an empty list: IndexError */
        rc = parse_rows(&rows, &a); free(rows.v);
        if (rc) break;
        if (k == 0) {
            if (a.cols < 3) { free(a.a); rc = UNSUP; break; }
            if (a.cols > 3) {                                       /* v[:, :3] (colours after the points) */
                for (long r = 0; r < a.rows; r++) for (int c = 0; c < 3; c++) a.a[r * 3 + c] = a.a[r * a.cols + c];
                a.cols = 3;
            }
            *v = a; *has_v = 1;
        } else { free(a.a); if (k == 2) *has_vn = 1; }
    }
    free(c2);
    if (rc) { free(v->a); memset(v, 0, sizeof *v); *has_v = 0; }
    return rc;
}

/* _preprocess_faces + _group_by(use_mtl=True): the face chunks grouped by material, in order of first appearance */
typedef struct { Str mtl; int has_mtl; char *text; size_t n, cap; } Group;
typedef struct { Group *g; int n, cap; } Groups;
static Group *group_of(Groups *G_, Str mtl, int has) {
    for (int i = 0; i < G_->n; i++) {
        Group *g = &G_->g[i];
        if (g->has_mtl == has && (!has || (g->mtl.n == mtl.n && !memcmp(g->mtl.p, mtl.p, mtl.n)))) return g;
    }
    if (G_->n == G_->cap) { int c = G_->cap ? G_->cap * 2 : 8; Group *p = realloc(G_->g, (size_t)c * sizeof *p); if (!p) return NULL; G_->g = p; G_->cap = c; }
    Group *g = &G_->g[G_->n++]; memset(g, 0, sizeof *g); g->mtl = mtl; g->has_mtl = has; return g;
}
static int preprocess_faces(Str text, Groups *out) {
    static const char *ST[5] = { "\nusemtl ", "\no ", "\nf ", "\ng ", "\ns " };
    long f_start = (long)text.n;
    for (int i = 0; i < 5; i++) { long s = s_find(text, ST[i], 0, f_start); if (s >= 0 && s < f_start) f_start = s; }
    long f_end = s_find(text, "\n", s_rfind(text, "\nf ") + 3, -1);
    Str fc = { text.p + f_start, f_end >= 0 ? (size_t)(f_end - f_start) : text.n - (size_t)f_start };
    if (f_end >= 0 && f_end < f_start) fc.n = 0;                    /* text[a:b] with b < a is empty */
    /* splits: 0, len, and every "usemtl " (re.finditer), sorted and unique */
    IVec sp = {0};
    if (!ipush(&sp, 0)) return MDL_ERR_MEM;
    for (long i = 0; (i = s_find(fc, "usemtl ", i, -1)) >= 0; i += 7) if (!ipush(&sp, i)) { free(sp.a); return MDL_ERR_MEM; }
    if (!ipush(&sp, (int64_t)fc.n)) { free(sp.a); return MDL_ERR_MEM; }
    /* already sorted except len at the end; make unique (0 may equal a "usemtl " at 0; len never equals one) */
    size_t u = 0;
    for (size_t i = 0; i < sp.n; i++) if (!u || sp.a[i] != sp.a[u - 1]) sp.a[u++] = sp.a[i];
    sp.n = u;
    for (size_t i = 1; i < sp.n; i++) if (sp.a[i] < sp.a[i - 1]) { free(sp.a); return UNSUP; }
    Str cur_mtl = {0}; int has_mtl = 0;
    for (size_t i = 0; i + 1 < sp.n; i++) {
        /* chunk = f_chunk[start:end].strip() + "\n"; chunk is c followed by that "\n" */
        Str c = s_strip((Str){ fc.p + sp.a[i], (size_t)(sp.a[i + 1] - sp.a[i]) });
        Str body = c;                                                /* the chunk kept is body + "\n" */
        int first_o = s_starts(c, "o "), first_m = !first_o && s_starts(c, "usemtl"), first_g = !first_o && !first_m && s_starts(c, "g ");
        if (first_o || first_m || first_g) {                         /* line, chunk = chunk.split("\n", 1) */
            const char *e = memchr(c.p, '\n', c.n);
            Str first = { c.p, e ? (size_t)(e - c.p) : c.n };
            body = e ? (Str){ e + 1, c.n - first.n - 1 } : (Str){ c.p + c.n, 0 };
            if (first_m) { cur_mtl = s_strip((Str){ first.p + 6, first.n - 6 }); has_mtl = 1; }
        }
        /* chunk.startswith("f ") or "\nf" in chunk  (chunk = body + "\n": a "\nf" needs the f inside body) */
        int take = s_starts(body, "f ");
        for (size_t a = 0; !take && a + 1 < body.n; a++) if (body.p[a] == '\n' && body.p[a + 1] == 'f') take = 1;
        if (take) {
            Group *g = group_of(out, cur_mtl, has_mtl);
            if (!g) { free(sp.a); return MDL_ERR_MEM; }
            size_t need = g->n + body.n + 3;
            if (need > g->cap) { size_t cc = need * 2 + 64; char *p = realloc(g->text, cc); if (!p) { free(sp.a); return MDL_ERR_MEM; } g->text = p; g->cap = cc; }
            if (g->n) g->text[g->n++] = '\n';                        /* "\n".join(chunks) */
            memcpy(g->text + g->n, body.p, body.n); g->n += body.n; g->text[g->n++] = '\n';
        }
    }
    free(sp.a);
    return MDL_OK;
}

/* the faces of one group: _parse_faces_vectorized or _parse_faces_fallback.
   faces (nf x fc), tex / norm the same shape when present */
typedef struct { int64_t *f, *t, *n; long nf; int fc; } FaceSet;
static void faceset_free(FaceSet *s) { free(s->f); free(s->t); free(s->n); memset(s, 0, sizeof *s); }
static int py_int_token(Str t, int64_t *x) {                       /* int(str) for an OBJ index token; -1 raises */
    t = s_strip(t);
    size_t i = 0; int neg = 0; int64_t v = 0;
    if (i < t.n && (t.p[i] == '+' || t.p[i] == '-')) { neg = t.p[i] == '-'; i++; }
    if (i >= t.n) return -1;
    for (; i < t.n; i++) { if (t.p[i] == '_') return -2; if (t.p[i] < '0' || t.p[i] > '9') return -1; v = v * 10 + (t.p[i] - '0'); }
    *x = neg ? -v : v; return 0;
}
static int parse_faces(Str chunk, FaceSet *fs) {
    memset(fs, 0, sizeof *fs);
    StrList lines = {0};
    /* re.split("^f", chunk, flags=re.MULTILINE)[1:], each .split("\n", 1)[0].strip() */
    long prev = -1;
    for (size_t i = 0; i <= chunk.n; i++) {
        int at = i < chunk.n && chunk.p[i] == 'f' && (i == 0 || chunk.p[i - 1] == '\n');
        if (at || i == chunk.n) {
            if (prev >= 0) { Str piece = { chunk.p + prev, i - (size_t)prev }; if (!slpush(&lines, s_strip(s_line(piece)))) return MDL_ERR_MEM; }
            if (at) prev = (long)i + 1;
        }
    }
    if (lines.n == 0) { free(lines.v); return MDL_ERR_RAISE; }       /* face_lines[0]: IndexError */
    int columns = s_count_split_slash(lines.v[0]), flat = 1;
    for (long i = 1; i < lines.n && flat; i++) if (s_count_split_slash(lines.v[i]) != columns) flat = 0;
    int rc = MDL_OK;
    if (flat) {
        size_t tot = 0; for (long i = 0; i < lines.n; i++) tot += lines.v[i].n + 1;
        char *j = malloc(tot + 1); size_t o = 0;
        if (!j) { free(lines.v); return MDL_ERR_MEM; }
        for (long i = 0; i < lines.n; i++) { if (i) j[o++] = ' '; for (size_t a = 0; a < lines.v[i].n; a++) j[o++] = lines.v[i].p[a] == '/' ? ' ' : lines.v[i].p[a]; }
        IVec arr = {0};
        if (!fromstring_i64((Str){ j, o }, &arr)) { free(j); free(lines.v); return MDL_ERR_MEM; }
        free(j);
        for (size_t i = 0; i < arr.n; i++) if (arr.a[i] > 0) arr.a[i] -= 1;
        if (columns == 0 || arr.n % (size_t)columns) { free(arr.a); free(lines.v); return MDL_ERR_RAISE; }   /* reshape fails */
        long rows = (long)(arr.n / (size_t)columns);
        int gc = s_count_split(lines.v[0]);
        if (gc == 0) { free(arr.a); free(lines.v); return MDL_ERR_RAISE; }       /* columns / 0 */
        int per_ref = columns / gc;
        int want_t = 0, want_n = 0, toff = 1, noff = 2;
        if (columns == gc * 2) {
            /* "".join(i.strip("/") for i in sample_line.split()).count("/") */
            Str sl = lines.v[0]; int cnt = 0; size_t a = 0;
            while (a < sl.n) {
                while (a < sl.n && py_space((unsigned char)sl.p[a])) a++;
                size_t b = a; while (b < sl.n && !py_space((unsigned char)sl.p[b])) b++;
                size_t x = a, y = b; while (x < y && sl.p[x] == '/') x++; while (y > x && sl.p[y - 1] == '/') y--;
                for (size_t q = x; q < y; q++) cnt += sl.p[q] == '/';
                a = b;
            }
            if (cnt == columns) { want_n = 1; noff = 1; }
            else if (cnt == columns / 2) want_t = 1;
        } else if (columns == gc * 3) { want_t = 1; want_n = 1; }
        fs->nf = rows; fs->fc = gc;
        fs->f = malloc((size_t)rows * gc * sizeof(int64_t) + 8);
        if (want_t) fs->t = malloc((size_t)rows * gc * sizeof(int64_t) + 8);
        if (want_n) fs->n = malloc((size_t)rows * gc * sizeof(int64_t) + 8);
        if (!fs->f || (want_t && !fs->t) || (want_n && !fs->n)) { free(arr.a); free(lines.v); faceset_free(fs); return MDL_ERR_MEM; }
        for (long r = 0; r < rows; r++) for (int c = 0; c < gc; c++) {
            const int64_t *row = arr.a + (size_t)r * columns;
            fs->f[r * gc + c] = row[c * per_ref];
            if (want_t) fs->t[r * gc + c] = row[c * per_ref + toff];
            if (want_n) fs->n[r * gc + c] = row[c * per_ref + noff];
        }
        free(arr.a);
    } else {
        IVec v = {0}, vt = {0}, vn = {0};
        for (long li = 0; li < lines.n && rc == MDL_OK; li++) {
            Str l = s_line(s_strip(lines.v[li]));                    /* line.strip().split("\n")[0].split() */
            Str tok[4096]; int nt = 0; size_t a = 0;
            while (a < l.n) {
                while (a < l.n && py_space((unsigned char)l.p[a])) a++;
                size_t b = a; while (b < l.n && !py_space((unsigned char)l.p[b])) b++;
                if (b > a) { if (nt >= 4096) { rc = UNSUP; break; } tok[nt++] = (Str){ l.p + a, b - a }; }
                a = b;
            }
            if (rc) break;
            int order[3 * 4096], no = 0;
            if (nt == 3) { order[0] = 0; order[1] = 1; order[2] = 2; no = 3; }
            else if (nt == 4) { int q[6] = { 0, 1, 2, 2, 3, 0 }; memcpy(order, q, sizeof q); no = 6; }
            else if (nt > 4) { for (int i = 0; i < nt - 2; i++) { order[no++] = 0; order[no++] = i + 1; order[no++] = i + 2; } }
            else continue;                                           /* fewer than 3: skipped */
            for (int q = 0; q < no; q++) {
                Str t = tok[order[q]];
                /* f.split("/") */
                Str part[3] = {{0}}; int np_ = 0; size_t s0 = 0;
                for (size_t x = 0; x <= t.n; x++) if (x == t.n || t.p[x] == '/') { if (np_ < 3) part[np_] = (Str){ t.p + s0, x - s0 }; np_++; s0 = x + 1; }
                int64_t x0;
                int e = py_int_token(part[0], &x0);
                if (e == -1) { rc = MDL_ERR_RAISE; break; }
                if (e) { rc = UNSUP; break; }
                if (!ipush(&v, x0)) { rc = MDL_ERR_MEM; break; }
                if (np_ > 1) { int64_t x1; e = py_int_token(part[1], &x1); if (e == -2) { rc = UNSUP; break; } if (!e && !ipush(&vt, x1)) { rc = MDL_ERR_MEM; break; } }
                if (np_ > 2) { int64_t x2; e = py_int_token(part[2], &x2); if (e == -2) { rc = UNSUP; break; } if (!e && !ipush(&vn, x2)) { rc = MDL_ERR_MEM; break; } }
            }
        }
        if (rc == MDL_OK && v.n % 3) rc = MDL_ERR_RAISE;              /* reshape((-1, 3)) */
        if (rc == MDL_OK) {
            for (size_t i = 0; i < v.n; i++) if (v.a[i] > 0) v.a[i]--;
            fs->f = v.a; v.a = NULL; fs->nf = (long)(v.n / 3); fs->fc = 3;
            if (vt.n == v.n) { for (size_t i = 0; i < vt.n; i++) if (vt.a[i] > 0) vt.a[i]--; fs->t = vt.a; vt.a = NULL; }
            if (vn.n == v.n) { for (size_t i = 0; i < vn.n; i++) if (vn.a[i] > 0) vn.a[i]--; fs->n = vn.a; vn.a = NULL; }
        }
        free(v.a); free(vt.a); free(vn.a);
    }
    free(lines.v);
    return rc;
}

/* unmerge_faces(faces, *args) (maintain_faces=False): returns new faces and mask_v (the vertex index of each new
   vertex). numpy sorts the (vertex) column with its unstable quicksort; equal vertex indices are taken here in
   their stable order: the vertices (their coordinates) come out the same, the face indices may name another of
   two vertices with identical coordinates. */
typedef struct { uint64_t key; int64_t pos; } KeyPos;
static int cmp_keypos(const void *a, const void *b) {                /* by key, then position: a stable sort */
    const KeyPos *x = a, *y = b;
    if (x->key != y->key) return x->key < y->key ? -1 : 1;
    return x->pos < y->pos ? -1 : x->pos > y->pos;
}
typedef struct { int64_t v; int64_t pos; } VPos;
static int cmp_vpos(const void *a, const void *b) {
    const VPos *x = a, *y = b;
    if (x->v != y->v) return x->v < y->v ? -1 : 1;
    return x->pos < y->pos ? -1 : x->pos > y->pos;
}
static int unmerge(const FaceSet *fs, int use_t, int use_n, int64_t **new_faces, int64_t **mask_v, long *nmask) {
    long n = fs->nf * fs->fc; int ncol = 1 + use_t + use_n;
    const int64_t *cols[3]; int c = 0;
    cols[c++] = fs->f; if (use_t) cols[c++] = fs->t; if (use_n) cols[c++] = fs->n;
    /* hashable_rows: ints, <= 4 columns: bit-banged into one uint64 when every value fits */
    int precision = 64 / ncol;
    int64_t threshold = ((int64_t)1 << (precision - 1)) - 1, dmin = INT64_MAX, dmax = INT64_MIN;
    for (int k = 0; k < ncol; k++) for (long i = 0; i < n; i++) { if (cols[k][i] < dmin) dmin = cols[k][i]; if (cols[k][i] > dmax) dmax = cols[k][i]; }
    if (n == 0) return UNSUP;
    if (!(dmax < threshold && dmin > -threshold)) return UNSUP;   /* the np.void path */
    KeyPos *kp = malloc((size_t)n * sizeof *kp);
    int64_t *inverse = malloc((size_t)n * sizeof(int64_t));
    if (!kp || !inverse) { free(kp); free(inverse); return MDL_ERR_MEM; }
    for (long i = 0; i < n; i++) {
        uint64_t h = 0;
        for (int k = 0; k < ncol; k++) h ^= ((uint64_t)(cols[k][i] + threshold + 1)) << (k * precision);
        kp[i].key = h; kp[i].pos = i;
    }
    qsort(kp, (size_t)n, sizeof *kp, cmp_keypos);                    /* np.unique: argsort(kind="mergesort") */
    long nu = 0;
    int64_t *uidx = malloc((size_t)n * sizeof(int64_t));
    if (!uidx) { free(kp); free(inverse); return MDL_ERR_MEM; }
    for (long i = 0; i < n; i++) {
        if (i == 0 || kp[i].key != kp[i - 1].key) uidx[nu++] = kp[i].pos;   /* return_index: the first of each */
        inverse[kp[i].pos] = nu - 1;
    }
    free(kp);
    /* pairs = stack[unique]; order = pairs[:, 0].argsort() */
    VPos *vp = malloc((size_t)nu * sizeof *vp);
    int64_t *remap = malloc((size_t)nu * sizeof(int64_t)), *mv = malloc((size_t)nu * sizeof(int64_t));
    int64_t *nf = malloc((size_t)n * sizeof(int64_t));
    if (!vp || !remap || !mv || !nf) { free(vp); free(remap); free(mv); free(nf); free(uidx); free(inverse); return MDL_ERR_MEM; }
    for (long i = 0; i < nu; i++) { vp[i].v = fs->f[uidx[i]]; vp[i].pos = i; }
    qsort(vp, (size_t)nu, sizeof *vp, cmp_vpos);
    for (long i = 0; i < nu; i++) { remap[vp[i].pos] = i; mv[i] = vp[i].v; }
    for (long i = 0; i < n; i++) nf[i] = remap[inverse[i]];
    free(vp); free(remap); free(uidx); free(inverse);
    *new_faces = nf; *mask_v = mv; *nmask = nu;
    return MDL_OK;
}

/* one geometry: vertices v[mask], faces (nf x fc) -> triangles (Trimesh's faces setter splits quads) */
typedef struct { double *v; long nv; int64_t *f; long nf; } Geom;
static int take_vertices(const Arr2 *v, const int64_t *mask, long nmask, double **out) {
    double *o = malloc((size_t)(nmask ? nmask : 1) * 3 * sizeof(double));
    if (!o) return MDL_ERR_MEM;
    for (long i = 0; i < nmask; i++) {
        int64_t k = mask[i];
        if (k < 0) k += v->rows;                                     /* numpy's negative index */
        if (k < 0 || k >= v->rows) { free(o); return MDL_ERR_RAISE; }   /* IndexError */
        memcpy(o + i * 3, v->a + k * 3, 3 * sizeof(double));
    }
    *out = o; return MDL_OK;
}
static int triangulate(int64_t *f, long nf, int fc, int64_t **out, long *nout) {
    if (fc == 3) { *out = f; *nout = nf; return MDL_OK; }
    if (fc != 4) { free(f); return UNSUP; }
    int64_t *t = malloc((size_t)(nf ? nf : 1) * 6 * sizeof(int64_t));
    if (!t) { free(f); return MDL_ERR_MEM; }
    for (long i = 0; i < nf; i++) {                                  /* vstack((q[:, [0, 1, 2]], q[:, [2, 3, 0]])) */
        t[i * 3 + 0] = f[i * 4 + 0]; t[i * 3 + 1] = f[i * 4 + 1]; t[i * 3 + 2] = f[i * 4 + 2];
        t[(nf + i) * 3 + 0] = f[i * 4 + 2]; t[(nf + i) * 3 + 1] = f[i * 4 + 3]; t[(nf + i) * 3 + 2] = f[i * 4 + 0];
    }
    free(f); *out = t; *nout = nf * 2; return MDL_OK;
}
/* util.concatenate of the dumped meshes (append_faces): vertices stacked, faces shifted by the counts before */
static int concat(Geom *g, int n, MdlMesh *out) {
    long nv = 0, nf = 0;
    for (int i = 0; i < n; i++) { nv += g[i].nv; nf += g[i].nf; }
    out->v = malloc((size_t)(nv ? nv : 1) * 3 * sizeof(double)); out->f = malloc((size_t)(nf ? nf : 1) * 3 * sizeof(int64_t));
    if (!out->v || !out->f) { mdl_free(out); return MDL_ERR_MEM; }
    long ov = 0, of = 0;
    for (int i = 0; i < n; i++) {
        memcpy(out->v + ov * 3, g[i].v, (size_t)g[i].nv * 3 * sizeof(double));
        for (long k = 0; k < g[i].nf * 3; k++) out->f[of * 3 + k] = g[i].f[k] + ov;
        ov += g[i].nv; of += g[i].nf;
    }
    out->nv = (int)nv; out->nf = (int)nf;
    return MDL_OK;
}

int mdl_trimesh_obj(const unsigned char *data, size_t n, MdlMesh *out) {
    memset(out, 0, sizeof *out);
    for (size_t i = 0; i < n; i++) if (data[i] > 127) return UNSUP;   /* only ASCII is copied (str.strip / split of other text differ) */
    /* text = "\n{}\n".format(text.strip().replace("\r\n", "\n")); text = text.replace("\\\n", "") */
    Str s = s_strip((Str){ (const char *)data, n });
    size_t n1, n2;
    char *a = s_replace(s, "\r\n", "\n", &n1); if (!a) return MDL_ERR_MEM;
    char *b = malloc(n1 + 3); if (!b) { free(a); return MDL_ERR_MEM; }
    b[0] = '\n'; memcpy(b + 1, a, n1); b[n1 + 1] = '\n'; b[n1 + 2] = 0; free(a);
    char *t = s_replace((Str){ b, n1 + 2 }, "\\\n", "", &n2); free(b); if (!t) return MDL_ERR_MEM;
    Str text = { t, n2 };
    Arr2 v; int has_vn, has_v;
    int rc = parse_vertices(text, &v, &has_vn, &has_v);
    if (rc) { free(t); return rc; }
    Groups gs = {0};
    rc = preprocess_faces(text, &gs);
    Geom *geo = NULL; int ng = 0;
    if (rc == MDL_OK && gs.n && has_v) {
        geo = calloc((size_t)gs.n, sizeof *geo);
        if (!geo) rc = MDL_ERR_MEM;
        for (int gi = gs.n - 1; gi >= 0 && rc == MDL_OK; gi--) {       /* face_tuples.pop(): the last group first */
            FaceSet fs;
            rc = parse_faces((Str){ gs.g[gi].text, gs.g[gi].n }, &fs);
            if (rc) break;
            int64_t *nf = NULL, *mask = NULL; long nmask = 0;
            if (fs.t) {
                rc = unmerge(&fs, 1, fs.n != NULL, &nf, &mask, &nmask);   /* len(faces_norm) == len(faces) always holds here */
            } else if (has_vn && fs.n) {
                rc = unmerge(&fs, 0, 1, &nf, &mask, &nmask);
            } else {
                /* mask_v[faces] = True; inverse[mask_v] = arange; new_faces = inverse[faces] */
                long N = v.rows, cnt = 0;
                char *m = calloc((size_t)(N ? N : 1), 1); int64_t *inv = calloc((size_t)(N ? N : 1), sizeof(int64_t));
                long tot = fs.nf * fs.fc;
                if (!m || !inv) rc = MDL_ERR_MEM;
                for (long i = 0; i < tot && rc == MDL_OK; i++) { int64_t k = fs.f[i]; if (k < 0) k += N; if (k < 0 || k >= N) rc = MDL_ERR_RAISE; else m[k] = 1; }
                if (rc == MDL_OK) {
                    for (long i = 0; i < N; i++) if (m[i]) cnt++;
                    mask = malloc((size_t)(cnt ? cnt : 1) * sizeof(int64_t)); nf = malloc((size_t)(tot ? tot : 1) * sizeof(int64_t));
                    if (!mask || !nf) rc = MDL_ERR_MEM;
                }
                if (rc == MDL_OK) {
                    long c = 0;
                    for (long i = 0; i < N; i++) if (m[i]) { inv[i] = c; mask[c++] = i; }
                    for (long i = 0; i < tot; i++) { int64_t k = fs.f[i]; if (k < 0) k += N; nf[i] = inv[k]; }
                    nmask = cnt;
                }
                free(m); free(inv);
            }
            int fc = fs.fc; long nfaces = fs.nf;
            faceset_free(&fs);
            if (rc) { free(nf); free(mask); break; }
            rc = take_vertices(&v, mask, nmask, &geo[ng].v); free(mask);
            if (rc) { free(nf); break; }
            geo[ng].nv = nmask;
            rc = triangulate(nf, nfaces, fc, &geo[ng].f, &geo[ng].nf);
            if (rc) { free(geo[ng].v); break; }
            ng++;
        }
    }
    if (rc == MDL_OK) {
        if (ng == 1) {                                               /* concatenate of one: a copy */
            out->v = geo[0].v; out->f = geo[0].f; out->nv = (int)geo[0].nv; out->nf = (int)geo[0].nf; geo[0].v = NULL; geo[0].f = NULL;
        } else rc = concat(geo, ng, out);                            /* none: Trimesh() (no faces: a point cloud, dropped) */
    }
    for (int i = 0; i < ng; i++) { free(geo[i].v); free(geo[i].f); }
    free(geo);
    for (int i = 0; i < gs.n; i++) free(gs.g[i].text);
    free(gs.g); free(v.a); free(t);
    return rc;
}

/* ======================= a small JSON reader (json.loads) ======================= */
enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };
typedef struct Json { int type; double num; int is_int; int64_t ival; char *str; size_t slen; struct Json *items; char **keys; int n; } Json;
static void json_free(Json *j) {
    if (!j) return;
    for (int i = 0; i < j->n; i++) { json_free(&j->items[i]); if (j->keys) free(j->keys[i]); }
    free(j->items); free(j->keys); free(j->str);
}
typedef struct { const char *p, *e; int err; } JP;
static void jws(JP *s) { while (s->p < s->e && (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r')) s->p++; }
static int jhex4(JP *s, unsigned *cp) {
    if (s->e - s->p < 4) return 0;
    unsigned v = 0;
    for (int i = 0; i < 4; i++) { char h = s->p[i]; v <<= 4; if (h >= '0' && h <= '9') v |= (unsigned)(h - '0'); else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10); else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10); else return 0; }
    s->p += 4; *cp = v; return 1;
}
static int jstr(JP *s, char **out, size_t *len) {
    if (s->p >= s->e || *s->p != '"') return 0;
    s->p++;
    size_t cap = 32, n = 0; char *b = malloc(cap);
    if (!b) return 0;
    while (s->p < s->e && *s->p != '"') {
        char c = *s->p++;
        if (c == '\\') {
            if (s->p >= s->e) { free(b); return 0; }
            char d = *s->p++;
            switch (d) { case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break; case 'b': c = '\b'; break; case 'f': c = '\f'; break;
                case 'u': {                                          /* \uXXXX (and surrogate pairs) as UTF-8 */
                    unsigned cp = 0;
                    if (!jhex4(s, &cp)) { free(b); return 0; }
                    if (cp >= 0xD800 && cp < 0xDC00 && s->e - s->p >= 6 && s->p[0] == '\\' && s->p[1] == 'u') {
                        JP t = *s; t.p += 2; unsigned lo;
                        if (jhex4(&t, &lo) && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); *s = t; }
                    }
                    if (n + 8 > cap) { cap = cap * 2 + 8; char *q = realloc(b, cap); if (!q) { free(b); return 0; } b = q; }
                    if (cp < 0x80) b[n++] = (char)cp;
                    else if (cp < 0x800) { b[n++] = (char)(0xC0 | cp >> 6); b[n++] = (char)(0x80 | (cp & 63)); }
                    else if (cp < 0x10000) { b[n++] = (char)(0xE0 | cp >> 12); b[n++] = (char)(0x80 | ((cp >> 6) & 63)); b[n++] = (char)(0x80 | (cp & 63)); }
                    else { b[n++] = (char)(0xF0 | cp >> 18); b[n++] = (char)(0x80 | ((cp >> 12) & 63)); b[n++] = (char)(0x80 | ((cp >> 6) & 63)); b[n++] = (char)(0x80 | (cp & 63)); }
                    continue;
                }
                default: c = d; }
        }
        if (n + 2 > cap) { cap *= 2; char *q = realloc(b, cap); if (!q) { free(b); return 0; } b = q; }
        b[n++] = c;
    }
    if (s->p >= s->e) { free(b); return 0; }
    s->p++; b[n] = 0; *out = b; *len = n; return 1;
}
static int jval(JP *s, Json *j, int depth) {
    memset(j, 0, sizeof *j);
    if (depth > 200) return 0;
    jws(s);
    if (s->p >= s->e) return 0;
    char c = *s->p;
    if (c == '{' || c == '[') {
        int obj = c == '{'; s->p++; j->type = obj ? J_OBJ : J_ARR;
        int cap = 0;
        jws(s);
        if (s->p < s->e && *s->p == (obj ? '}' : ']')) { s->p++; return 1; }
        for (;;) {
            if (j->n == cap) {
                cap = cap ? cap * 2 : 8;
                Json *ni = realloc(j->items, (size_t)cap * sizeof *ni); if (!ni) return 0; j->items = ni;
                if (obj) { char **nk = realloc(j->keys, (size_t)cap * sizeof *nk); if (!nk) return 0; j->keys = nk; }
            }
            if (obj) { size_t l; jws(s); if (!jstr(s, &j->keys[j->n], &l)) return 0; jws(s); if (s->p >= s->e || *s->p != ':') { free(j->keys[j->n]); return 0; } s->p++; }
            if (!jval(s, &j->items[j->n], depth + 1)) { if (obj) free(j->keys[j->n]); return 0; }
            j->n++;
            jws(s);
            if (s->p < s->e && *s->p == ',') { s->p++; continue; }
            if (s->p < s->e && *s->p == (obj ? '}' : ']')) { s->p++; return 1; }
            return 0;
        }
    }
    if (c == '"') { j->type = J_STR; return jstr(s, &j->str, &j->slen); }
    if (s->e - s->p >= 4 && !memcmp(s->p, "null", 4)) { s->p += 4; j->type = J_NULL; return 1; }
    if (s->e - s->p >= 4 && !memcmp(s->p, "true", 4)) { s->p += 4; j->type = J_BOOL; j->ival = 1; return 1; }
    if (s->e - s->p >= 5 && !memcmp(s->p, "false", 5)) { s->p += 5; j->type = J_BOOL; return 1; }
    /* a number: json's NUMBER_RE, -?(0|[1-9]\d*)(\.\d+)?([eE][-+]?\d+)?; int when no fraction or exponent */
    const char *b = s->p;
    if (s->p < s->e && *s->p == '-') s->p++;
    if (s->p >= s->e || *s->p < '0' || *s->p > '9') return 0;
    if (*s->p == '0') s->p++; else while (s->p < s->e && *s->p >= '0' && *s->p <= '9') s->p++;
    int frac = 0;
    if (s->p + 1 < s->e && *s->p == '.' && s->p[1] >= '0' && s->p[1] <= '9') { frac = 1; s->p++; while (s->p < s->e && *s->p >= '0' && *s->p <= '9') s->p++; }
    if (s->p < s->e && (*s->p == 'e' || *s->p == 'E')) {
        const char *q = s->p + 1;
        if (q < s->e && (*q == '+' || *q == '-')) q++;
        if (q < s->e && *q >= '0' && *q <= '9') { frac = 1; while (q < s->e && *q >= '0' && *q <= '9') q++; s->p = q; }
    }
    j->type = J_NUM; j->is_int = !frac;
    size_t len = (size_t)(s->p - b);
    char buf[400]; if (len >= sizeof buf) return 0;
    memcpy(buf, b, len); buf[len] = 0;
    if (j->is_int) { j->ival = strtoll(buf, NULL, 10); j->num = (double)j->ival; if (len > 18) return 0; }   /* float(int) rounds like Python for |i| < 2^63 */
    else j->num = strtod(buf, NULL);
    return 1;
}
static const Json *jget(const Json *o, const char *key) {
    if (!o || o->type != J_OBJ) return NULL;
    for (int i = 0; i < o->n; i++) if (!strcmp(o->keys[i], key)) return &o->items[i];
    return NULL;
}
static const Json *jat(const Json *a, int i) { return a && a->type == J_ARR && i >= 0 && i < a->n ? &a->items[i] : NULL; }
static int jint(const Json *j, int64_t *x) { if (!j || j->type != J_NUM || !j->is_int) return 0; *x = j->ival; return 1; }
static int jnums(const Json *a, double *x, int n) {                 /* a list of n numbers as float64 */
    if (!a || a->type != J_ARR || a->n != n) return 0;
    for (int i = 0; i < n; i++) { if (a->items[i].type != J_NUM) return 0; x[i] = a->items[i].num; }
    return 1;
}

/* ======================= 4x4 matrices as numpy + OpenBLAS give them ======================= */
FMAFN static void mat4_mul(const double A[16], const double B[16], double C[16]) {   /* np.dot(A, B): dgemm, fma k = 0..3 */
    double r[16];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        double acc = 0.0;
        for (int k = 0; k < 4; k++) acc = FMA(A[i * 4 + k], B[k * 4 + j], acc);
        r[i * 4 + j] = acc;
    }
    memcpy(C, r, sizeof r);
}
static const double EYE4[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
static double absmax_minus_eye(const double M[16]) {               /* np.abs(M - I).max() */
    double m = 0.0;
    for (int i = 0; i < 16; i++) { double d = fabs(M[i] - EYE4[i]); if (d > m || i == 0) m = d; }
    return m;
}
static double ptp_minus_eye(const double *M, int dim, int stride) {  /* np.ptp(M - I) over dim x dim */
    double lo = 0, hi = 0;
    for (int i = 0; i < dim; i++) for (int j = 0; j < dim; j++) {
        double d = M[i * stride + j] - (i == j ? 1.0 : 0.0);
        if ((i == 0 && j == 0) || d < lo) lo = d;
        if ((i == 0 && j == 0) || d > hi) hi = d;
    }
    return hi - lo;
}
/* transformations.quaternion_matrix(q), q = (w, x, y, z) */
FMAFN static void quaternion_matrix(const double qin[4], double R[16]) {
    double q[4] = { qin[0], qin[1], qin[2], qin[3] };
    double n = 0.0;                                                  /* diagonal_dot: np.dot(q * q, [1.0] * 4), dgemv */
    for (int i = 0; i < 4; i++) n = FMA(q[i] * q[i], 1.0, n);
    double eps = DBL_EPSILON * 4.0;
    if (n < eps) { memcpy(R, EYE4, sizeof EYE4); return; }
    double f = sqrt(2.0 / n);
    for (int i = 0; i < 4; i++) q[i] *= f;
    double Q[4][4];                                                  /* q[:, None, :] * q[:, :, None]: Q[a][b] = q[b] * q[a] */
    for (int a = 0; a < 4; a++) for (int b = 0; b < 4; b++) Q[a][b] = q[b] * q[a];
    memset(R, 0, 16 * sizeof(double));
    R[0] = 1.0 - Q[2][2] - Q[3][3]; R[1] = Q[1][2] - Q[3][0]; R[2] = Q[1][3] + Q[2][0];
    R[4] = Q[1][2] + Q[3][0]; R[5] = 1.0 - Q[1][1] - Q[3][3]; R[6] = Q[2][3] - Q[1][0];
    R[8] = Q[1][3] - Q[2][0]; R[9] = Q[2][3] + Q[1][0]; R[10] = 1.0 - Q[1][1] - Q[2][2];
    R[15] = 1.0;
}
FMAFN static int fix_rigid(const double M[16]) {                          /* fix_rigid(M, 1e-5): 0 = left as it is, -1 = SVD repair (not copied) */
    double check = 0.0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        double acc = 0.0;
        for (int k = 0; k < 3; k++) acc = FMA(M[i * 4 + k], M[j * 4 + k], acc);
        double d = fabs(acc - (i == j ? 1.0 : 0.0));
        if ((i == 0 && j == 0) || d > check) check = d;
    }
    return (check > 1e-13 && check < 1e-5) ? -1 : 0;
}
static double det3(const double *M, int s) {
    return M[0] * (M[s + 1] * M[2 * s + 2] - M[s + 2] * M[2 * s + 1]) - M[1] * (M[s] * M[2 * s + 2] - M[s + 2] * M[2 * s]) + M[2] * (M[s] * M[2 * s + 1] - M[s + 1] * M[2 * s]);
}

/* ======================= GLB: trimesh.exchange.gltf.load_glb + Scene.to_mesh ======================= */
typedef struct { const unsigned char *p; size_t n; int ok; int ctype; int comps; int64_t count; int placeholder; unsigned char *own; } Acc;
typedef struct { int u, v; double m[16]; int geom; } Edge;          /* node ids: -1 = "world", glTF nodes 0.., extra frames after */

FMAFN int mdl_trimesh_glb(const unsigned char *data, size_t n, MdlMesh *out) {
    memset(out, 0, sizeof *out);
    if (n < 20) return MDL_ERR_RAISE;
    uint32_t h[5]; memcpy(h, data, 20);
    if (h[0] != 0x46546C67u) return MDL_ERR_RAISE;                   /* "incorrect header on GLB file" */
    if (h[1] != 2) return MDL_ERR_RAISE;
    if (h[4] != 0x4E4F534Au) return MDL_ERR_RAISE;                   /* "no initial JSON header!" */
    size_t jl = h[3]; if (20 + jl > n) jl = n - 20;
    {                                                                /* util.decode_text: UTF-8 (only names use more than ASCII) */
        const unsigned char *u = data + 20; size_t i = 0;
        while (i < jl) {
            unsigned char c = u[i]; int k = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 && c >= 0xC2 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 && c <= 0xF4 ? 3 : -1;
            if (k < 0 || i + (size_t)k >= jl + (k ? 0 : 1)) return UNSUP;           /* not UTF-8: trimesh guesses the coding */
            for (int q = 1; q <= k; q++) if ((u[i + q] & 0xC0) != 0x80) return UNSUP;
            i += (size_t)k + 1;
        }
    }
    JP jp = { (const char *)data + 20, (const char *)data + 20 + jl, 0 };
    Json J;
    if (!jval(&jp, &J, 0) || J.type != J_OBJ) { json_free(&J); return MDL_ERR_RAISE; }
    int rc = MDL_OK;
    /* the buffers: one BIN chunk per buffer that has no uri */
    const Json *jb = jget(&J, "buffers");
    const unsigned char *bufs[64]; size_t blen[64]; int nb = 0, info = jb && jb->type == J_ARR ? jb->n : 0, ii = 0;
    size_t pos = 20 + h[3];
    for (;;) {
        if (ii < info) { if (jget(jat(jb, ii), "uri")) { rc = UNSUP; break; } ii++; }
        if (pos + 8 > n) break;
        uint32_t cl, ct; memcpy(&cl, data + pos, 4); memcpy(&ct, data + pos + 4, 4); pos += 8;
        if (pos + cl > n) { rc = MDL_ERR_RAISE; break; }             /* "chunk was not expected length!" */
        if (ct == 0x004E4942u) { if (nb == 64) { rc = UNSUP; break; } bufs[nb] = data + pos; blen[nb] = cl; nb++; }
        pos += cl;
    }
    const Json *jv = jget(&J, "bufferViews"), *ja = jget(&J, "accessors");
    if (rc == MDL_OK && !jv) rc = UNSUP;
    int nacc = ja && ja->type == J_ARR ? ja->n : 0;
    Acc *acc = calloc((size_t)(nacc ? nacc : 1), sizeof *acc);
    if (!acc) rc = MDL_ERR_MEM;
    for (int i = 0; i < nacc && rc == MDL_OK; i++) {                  /* every accessor is read (a broken one raises) */
        const Json *a = jat(ja, i);
        int64_t count, ctype, bv, off = 0;
        if (!jint(jget(a, "count"), &count) || !jint(jget(a, "componentType"), &ctype)) { rc = MDL_ERR_RAISE; break; }
        int isz = ctype == 5120 || ctype == 5121 ? 1 : ctype == 5122 || ctype == 5123 ? 2 : ctype == 5125 || ctype == 5126 ? 4 : 0;
        if (!isz) { rc = MDL_ERR_RAISE; break; }
        const Json *ty = jget(a, "type"); int comps = 0;
        if (ty && ty->type == J_STR) {
            if (!strcmp(ty->str, "SCALAR")) comps = 1; else if (!strcmp(ty->str, "VEC2")) comps = 2; else if (!strcmp(ty->str, "VEC3")) comps = 3;
            else if (!strcmp(ty->str, "VEC4") || !strcmp(ty->str, "MAT2")) comps = 4; else if (!strcmp(ty->str, "MAT3")) comps = 9; else if (!strcmp(ty->str, "MAT4")) comps = 16;
        }
        if (!comps) { rc = MDL_ERR_RAISE; break; }
        acc[i].ctype = (int)ctype; acc[i].comps = comps; acc[i].count = count;
        if (jget(a, "sparse")) { rc = UNSUP; break; }
        if (!jint(jget(a, "bufferView"), &bv)) { acc[i].placeholder = 1; acc[i].ok = 1; continue; }   /* zeros */
        const Json *view = jat(jv, (int)bv);
        int64_t vb, vlen, voff = 0, stride;
        if (!view || !jint(jget(view, "buffer"), &vb) || !jint(jget(view, "byteLength"), &vlen)) { rc = MDL_ERR_RAISE; break; }
        jint(jget(view, "byteOffset"), &voff);
        if (vb < 0 || vb >= nb) { rc = MDL_ERR_RAISE; break; }
        if (voff + vlen > (int64_t)blen[vb]) { rc = MDL_ERR_RAISE; break; }   /* assert len(views[i]) == byteLength */
        jint(jget(a, "byteOffset"), &off);
        const unsigned char *vd = bufs[vb] + voff;
        int64_t length = (int64_t)isz * count * comps;
        if (jint(jget(view, "byteStride"), &stride)) {
            int64_t per_row = comps * isz;
            int64_t slen = (count - 1) * stride + per_row;              /* as_strided over the view: rows stride bytes apart */
            if (stride <= 0 || off < 0 || (count > 0 && off + slen > vlen)) { rc = MDL_ERR_RAISE; break; }   /* the asserts */
            if (stride != per_row) {
                unsigned char *cp = malloc((size_t)(length ? length : 1));
                if (!cp) { rc = MDL_ERR_MEM; break; }
                for (int64_t r = 0; r < count; r++) memcpy(cp + r * per_row, vd + off + r * stride, (size_t)per_row);
                acc[i].own = cp; acc[i].p = cp; acc[i].n = (size_t)length; acc[i].ok = 1; continue;
            }
        } else if (off < 0 || off + length > vlen) { rc = MDL_ERR_RAISE; break; }
        acc[i].p = vd + off; acc[i].n = (size_t)length; acc[i].ok = 1;
    }
    /* the meshes: one geometry per primitive (triangles); mesh_prim[mesh] = its geometries */
    const Json *jm = jget(&J, "meshes");
    int nmesh = jm && jm->type == J_ARR ? jm->n : 0;
    int *mp_first = calloc((size_t)(nmesh ? nmesh : 1), sizeof(int)), *mp_count = calloc((size_t)(nmesh ? nmesh : 1), sizeof(int));
    Geom *geo = NULL; int ngeo = 0, capgeo = 0;
    if (!mp_first || !mp_count) rc = MDL_ERR_MEM;
    for (int mi = 0; mi < nmesh && rc == MDL_OK; mi++) {
        const Json *prims = jget(jat(jm, mi), "primitives");
        if (!prims || prims->type != J_ARR) { rc = MDL_ERR_RAISE; break; }
        mp_first[mi] = ngeo;
        for (int pi = 0; pi < prims->n && rc == MDL_OK; pi++) {
            const Json *p = &prims->items[pi];
            if (jget(p, "extensions")) { rc = UNSUP; break; }
            int64_t mode = 4, ip, ix;
            jint(jget(p, "mode"), &mode);
            if (mode == 0 || mode == 1) continue;                    /* points / lines: not a Trimesh, dropped by to_mesh */
            if (mode == 5) { rc = UNSUP; break; }
            if (mode != 4) continue;                                 /* "skipping primitive with mode" */
            if (!jint(jget(jget(p, "attributes"), "POSITION"), &ip) || ip < 0 || ip >= nacc) { rc = MDL_ERR_RAISE; break; }
            Acc *pa = &acc[ip];
            if (pa->ctype != 5126 || pa->comps != 3) { rc = UNSUP; break; }
            if (ngeo == capgeo) { capgeo = capgeo ? capgeo * 2 : 8; Geom *g2 = realloc(geo, (size_t)capgeo * sizeof *g2); if (!g2) { rc = MDL_ERR_MEM; break; } geo = g2; }
            Geom *g = &geo[ngeo]; memset(g, 0, sizeof *g);
            g->nv = (long)pa->count;
            g->v = malloc((size_t)(g->nv ? g->nv : 1) * 3 * sizeof(double));
            if (!g->v) { rc = MDL_ERR_MEM; break; }
            for (long k = 0; k < g->nv * 3; k++) { float f; if (pa->placeholder) f = 0.0f; else memcpy(&f, pa->p + k * 4, 4); g->v[k] = (double)f; }
            if (jint(jget(p, "indices"), &ix)) {
                if (ix < 0 || ix >= nacc) { free(g->v); rc = MDL_ERR_RAISE; break; }
                Acc *ia = &acc[ix]; long tot = (long)(ia->count * ia->comps);
                if (tot % 3) { free(g->v); rc = MDL_ERR_RAISE; break; }
                if (ia->ctype != 5121 && ia->ctype != 5123 && ia->ctype != 5125) { free(g->v); rc = UNSUP; break; }
                g->nf = tot / 3; g->f = malloc((size_t)(tot ? tot : 1) * sizeof(int64_t));
                if (!g->f) { free(g->v); rc = MDL_ERR_MEM; break; }
                for (long k = 0; k < tot; k++) {
                    if (ia->placeholder) g->f[k] = 0;
                    else if (ia->ctype == 5121) g->f[k] = ia->p[k];
                    else if (ia->ctype == 5123) { uint16_t x; memcpy(&x, ia->p + k * 2, 2); g->f[k] = x; }
                    else { uint32_t x; memcpy(&x, ia->p + k * 4, 4); g->f[k] = x; }
                }
            } else {
                if (g->nv % 3) { free(g->v); rc = MDL_ERR_RAISE; break; }
                g->nf = g->nv / 3; g->f = malloc((size_t)(g->nv ? g->nv : 1) * sizeof(int64_t));
                if (!g->f) { free(g->v); rc = MDL_ERR_MEM; break; }
                for (long k = 0; k < g->nv; k++) g->f[k] = k;
            }
            ngeo++; mp_count[mi]++;
        }
    }
    /* the scene graph: depth first from the scene's roots, the queue popped from its end */
    const Json *jn = jget(&J, "nodes");
    int nnodes = jn && jn->type == J_ARR ? jn->n : 0;
    Edge *edges = NULL; int ne = 0, cape = 0, nextra = 0;
    typedef struct { int a, b; } QE;
    QE *queue = NULL; int nq = 0, capq = 0;
    QE *consumed = NULL; int nc = 0, capc = 0;
#define QPUSH(A, B) do { if (nq == capq) { capq = capq ? capq * 2 : 16; QE *q2 = realloc(queue, (size_t)capq * sizeof *q2); if (!q2) { rc = MDL_ERR_MEM; break; } queue = q2; } queue[nq].a = (A); queue[nq].b = (B); nq++; } while (0)
#define EPUSH(U, V, M, GEO) do { if (ne == cape) { cape = cape ? cape * 2 : 16; Edge *e2 = realloc(edges, (size_t)cape * sizeof *e2); if (!e2) { rc = MDL_ERR_MEM; break; } edges = e2; } edges[ne].u = (U); edges[ne].v = (V); memcpy(edges[ne].m, (M), sizeof edges[ne].m); edges[ne].geom = (GEO); ne++; } while (0)
    if (rc == MDL_OK) {
        int64_t si = 0; jint(jget(&J, "scene"), &si);
        const Json *sc = jat(jget(&J, "scenes"), (int)si);
        const Json *roots = jget(sc, "nodes");
        if (jget(&J, "scenes") && !sc) rc = MDL_ERR_RAISE;
        for (int i = 0; roots && i < roots->n && rc == MDL_OK; i++) { int64_t r; if (!jint(&roots->items[i], &r)) { rc = MDL_ERR_RAISE; break; } QPUSH(-1, (int)r); }
    }
    while (nq > 0 && rc == MDL_OK) {
        QE e = queue[--nq];
        int seen = 0;
        for (int i = 0; i < nc; i++) if (consumed[i].a == e.a && consumed[i].b == e.b) { seen = 1; break; }
        if (seen) continue;
        if (nc == capc) { capc = capc ? capc * 2 : 16; QE *c2 = realloc(consumed, (size_t)capc * sizeof *c2); if (!c2) { rc = MDL_ERR_MEM; break; } consumed = c2; }
        consumed[nc++] = e;
        const Json *child = jat(jn, e.b);
        if (!child) { rc = MDL_ERR_RAISE; break; }
        const Json *ch = jget(child, "children");
        for (int i = 0; ch && i < ch->n && rc == MDL_OK; i++) { int64_t c; if (!jint(&ch->items[i], &c)) { rc = MDL_ERR_RAISE; break; } QPUSH(e.b, (int)c); }
        if (rc) break;
        double M[16], T[16];
        const Json *jmx = jget(child, "matrix");
        if (jmx) {                                                   /* np.array(matrix).reshape((4, 4)).T */
            double m[16]; if (!jnums(jmx, m, 16)) { rc = MDL_ERR_RAISE; break; }
            for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) M[i * 4 + j] = m[j * 4 + i];
        } else memcpy(M, EYE4, sizeof M);
        const Json *jt = jget(child, "translation"), *jr = jget(child, "rotation"), *js = jget(child, "scale");
        if (jt) { double t[3]; if (!jnums(jt, t, 3)) { rc = UNSUP; break; } memcpy(T, EYE4, sizeof T); T[3] = t[0]; T[7] = t[1]; T[11] = t[2]; mat4_mul(M, T, M); }
        if (jr) { double q[4], qw[4]; if (!jnums(jr, q, 4)) { rc = UNSUP; break; } qw[0] = q[3]; qw[1] = q[0]; qw[2] = q[1]; qw[3] = q[2]; quaternion_matrix(qw, T); mat4_mul(M, T, M); }
        if (js) { double s[3]; if (!jnums(js, s, 3)) { rc = UNSUP; break; } memset(T, 0, sizeof T); T[0] = s[0]; T[5] = s[1]; T[10] = s[2]; T[15] = 1.0; mat4_mul(M, T, M); }
        if (jget(child, "camera")) { rc = UNSUP; break; }
        int64_t mi;
        if (jint(jget(child, "mesh"), &mi)) {
            if (mi < 0 || mi >= nmesh) { rc = MDL_ERR_RAISE; break; }
            int cnt = mp_count[mi];
            if (cnt > 1) {                                           /* the node, then one identity frame per primitive */
                EPUSH(e.a, e.b, M, -1);
                for (int k = 0; k < cnt && rc == MDL_OK; k++) { int fid = nnodes + nextra++; EPUSH(e.b, fid, EYE4, mp_first[mi] + k); }
            } else if (cnt == 1) EPUSH(e.a, e.b, M, mp_first[mi]);
        } else EPUSH(e.a, e.b, M, -1);
    }
#undef QPUSH
#undef EPUSH
    /* Scene: graph.update for each edge in order (EnforcedForest.add_edge); node order = first appearance */
    int nids = nnodes + nextra + 1;                                  /* id + 1: 0 = world */
    int *parent = malloc((size_t)nids * sizeof(int)), *order = malloc((size_t)nids * sizeof(int)), *inorder = calloc((size_t)nids, sizeof(int));
    int *geomof = malloc((size_t)nids * sizeof(int)), *edge_of = malloc((size_t)nids * sizeof(int));
    int nord = 0;
    if (rc == MDL_OK && (!parent || !order || !inorder || !geomof || !edge_of)) rc = MDL_ERR_MEM;
    if (rc == MDL_OK) {
        for (int i = 0; i < nids; i++) { parent[i] = -2; geomof[i] = -1; edge_of[i] = -1; }
        for (int k = 0; k < ne; k++) {
            int u = edges[k].u + 1, v = edges[k].v + 1;
            if (edge_of[v] >= 0 && parent[v] == u) {
                /* the same edge again: unchanged when the matrix is close (ptp < 1e-8) and the geometry the same */
                Edge *o = &edges[edge_of[v]]; double d[16];
                for (int i = 0; i < 16; i++) d[i] = edges[k].m[i] - o->m[i];
                double lo = d[0], hi = d[0]; for (int i = 1; i < 16; i++) { if (d[i] < lo) lo = d[i]; if (d[i] > hi) hi = d[i]; }
                if (hi - lo < 1e-8 && o->geom == edges[k].geom) continue;
            }
            if (edge_of[v] >= 0 && parent[v] != u) { rc = UNSUP; break; }   /* a node under two parents: not met */
            parent[v] = u; edge_of[v] = k;
            if (!inorder[u]) { inorder[u] = 1; order[nord++] = u; }
            if (!inorder[v]) { inorder[v] = 1; order[nord++] = v; }
            if (edges[k].geom >= 0) geomof[v] = edges[k].geom;
        }
    }
    /* dump(): every node with geometry, in node order, moved by its world transform */
    Geom *dumped = calloc((size_t)(nord ? nord : 1), sizeof *dumped); int nd = 0;
    if (rc == MDL_OK && !dumped) rc = MDL_ERR_MEM;
    for (int oi = 0; oi < nord && rc == MDL_OK; oi++) {
        int node = order[oi];
        if (geomof[node] < 0) continue;
        double W[16];
        /* graph.get(node): the edge matrix when the edge is (world, node), else the chain, identities dropped */
        if (parent[node] == 0) memcpy(W, edges[edge_of[node]].m, sizeof W);
        else {
            int chain[256], nch = 0, x = node;
            while (x != 0) { if (x < 0 || parent[x] < 0 || nch == 256) { rc = MDL_ERR_RAISE; break; } chain[nch++] = edge_of[x]; x = parent[x]; }
            if (rc) break;
            const double *mats[256]; int nm = 0;
            for (int i = nch - 1; i >= 0; i--) if (absmax_minus_eye(edges[chain[i]].m) > 1e-8) mats[nm++] = edges[chain[i]].m;
            if (nm == 0) memcpy(W, EYE4, sizeof W);
            else {                                                   /* multi_dot: dot(A, B), and A . (B . (C ...)) for 3 and more */
                memcpy(W, mats[nm - 1], sizeof W);
                for (int i = nm - 2; i >= 0; i--) mat4_mul(mats[i], W, W);
            }
        }
        if (fix_rigid(W)) { rc = UNSUP; break; }
        Geom *src = &geo[geomof[node]], *g = &dumped[nd];
        g->nv = src->nv; g->nf = src->nf;
        g->v = malloc((size_t)(g->nv ? g->nv : 1) * 3 * sizeof(double)); g->f = malloc((size_t)(g->nf ? g->nf : 1) * 3 * sizeof(int64_t));
        if (!g->v || !g->f) { free(g->v); free(g->f); rc = MDL_ERR_MEM; break; }
        memcpy(g->v, src->v, (size_t)g->nv * 3 * sizeof(double)); memcpy(g->f, src->f, (size_t)g->nf * 3 * sizeof(int64_t));
        nd++;
        /* apply_transform(W) */
        if (ptp_minus_eye(W, 4, 4) < 1e-8) continue;                 /* util.allclose(matrix, I, 1e-8) */
        if (g->nv > 0 && !(absmax_minus_eye(W) < 1e-8)) {            /* transform_points: np.dot(W, [p, 1].T).T */
            if (g->nv == 1) { rc = UNSUP; break; }      /* one point goes through dgemv: not copied */
            for (long k = 0; k < g->nv; k++) {
                double p[4] = { g->v[k * 3], g->v[k * 3 + 1], g->v[k * 3 + 2], 1.0 }, r[3];
                for (int i = 0; i < 3; i++) { double a2 = 0.0; for (int j = 0; j < 4; j++) a2 = FMA(W[i * 4 + j], p[j], a2); r[i] = a2; }
                memcpy(g->v + k * 3, r, sizeof r);
            }
        }
        int has_rot = !(ptp_minus_eye(W, 3, 4) < 1e-6);
        if (has_rot && det3(W, 4) < 0.0)                             /* flips_winding: np.fliplr(faces) */
            for (long k = 0; k < g->nf; k++) { int64_t t0 = g->f[k * 3]; g->f[k * 3] = g->f[k * 3 + 2]; g->f[k * 3 + 2] = t0; }
    }
    if (rc == MDL_OK) {
        if (nd == 1) { out->v = dumped[0].v; out->f = dumped[0].f; out->nv = (int)dumped[0].nv; out->nf = (int)dumped[0].nf; dumped[0].v = NULL; dumped[0].f = NULL; }
        else rc = concat(dumped, nd, out);
    }
    for (int i = 0; i < nd; i++) { free(dumped[i].v); free(dumped[i].f); }
    for (int i = 0; i < ngeo; i++) { free(geo[i].v); free(geo[i].f); }
    for (int i = 0; i < nacc; i++) free(acc[i].own);
    free(dumped); free(geo); free(acc); free(mp_first); free(mp_count); free(edges); free(queue); free(consumed);
    free(parent); free(order); free(inorder); free(geomof); free(edge_of);
    json_free(&J);
    return rc;
}

static int read_file(const char *path, unsigned char **data, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return MDL_ERR_IO;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return MDL_ERR_IO; }
    *data = malloc((size_t)len + 1);
    if (!*data) { fclose(f); return MDL_ERR_MEM; }
    *n = fread(*data, 1, (size_t)len, f); fclose(f);
    return *n == (size_t)len ? MDL_OK : MDL_ERR_IO;
}
static int ends_with_ci(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && !strcasecmp(s + a - b, suf);
}
int mdl_trimesh_load(const char *path, MdlMesh *out) {
    unsigned char *d = NULL; size_t n = 0;
    memset(out, 0, sizeof *out);
    int rc = read_file(path, &d, &n);
    if (rc) { free(d); return rc; }
    if (ends_with_ci(path, ".glb")) rc = mdl_trimesh_glb(d, n, out);
    else if (ends_with_ci(path, ".obj")) rc = mdl_trimesh_obj(d, n, out);
    else rc = UNSUP;
    free(d);
    return rc;
}

static int blob_on(void);
static int blob_load(const char *rel, MdlMesh *out);
/* sd_mesh._load: the cache and y up -> z up, v = stack([x, -z, y]) */
typedef struct { char *rel; MdlMesh m; } CacheEnt;
static CacheEnt *cache; static int ncache, capcache;
void mdl_rest_clear(void);
void mdl_cache_clear(void) { mdl_rest_clear(); for (int i = 0; i < ncache; i++) { free(cache[i].rel); mdl_free(&cache[i].m); } free(cache); cache = NULL; ncache = capcache = 0; }
const MdlMesh *mdl_load(const char *root, const char *rel, int *err) {
    for (int i = 0; i < ncache; i++) if (!strcmp(cache[i].rel, rel)) { if (err) *err = MDL_OK; return &cache[i].m; }
    char path[1024];
    snprintf(path, sizeof path, "%s\\%s", root, rel);
    MdlMesh m;
    int rc = blob_on() ? blob_load(rel, &m) : mdl_trimesh_load(path, &m);   /* the packed files when a blob is in use */
    if (rc) { if (err) *err = rc; return NULL; }
    for (int k = 0; k < m.nv; k++) { double y = m.v[k * 3 + 1], z = m.v[k * 3 + 2]; m.v[k * 3 + 1] = -z; m.v[k * 3 + 2] = y; }
    if (ncache == capcache) { int c = capcache ? capcache * 2 : 64; CacheEnt *p = realloc(cache, (size_t)c * sizeof *p); if (!p) { mdl_free(&m); if (err) *err = MDL_ERR_MEM; return NULL; } cache = p; capcache = c; }
    cache[ncache].rel = strdup(rel); cache[ncache].m = m;
    if (err) *err = MDL_OK;
    return &cache[ncache++].m;
}

/* ======================= Part 2: sd_mesh's posing ======================= */
void mdl_rx(double t, double R[9]) { double c = py_cos(t), s = py_sin(t); double r[9] = { 1, 0, 0, 0, c, -s, 0, s, c }; memcpy(R, r, sizeof r); }
void mdl_ry(double t, double R[9]) { double c = py_cos(t), s = py_sin(t); double r[9] = { c, 0, s, 0, 1, 0, -s, 0, c }; memcpy(R, r, sizeof r); }
void mdl_rz(double t, double R[9]) { double c = py_cos(t), s = py_sin(t); double r[9] = { c, -s, 0, s, c, 0, 0, 0, 1 }; memcpy(R, r, sizeof r); }
int mdl_pose(char pose, double R[9]) {                               /* POSES */
    static const double PI = 3.141592653589793;
    switch (pose) {
    case 's': { double r[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }; memcpy(R, r, sizeof r); return 1; }
    case 'b': mdl_rx(-PI / 2, R); return 1;
    case 'f': mdl_rx(PI / 2, R); return 1;
    case 'l': mdl_ry(PI / 2, R); return 1;
    case 'r': mdl_ry(-PI / 2, R); return 1;
    case 'u': mdl_rx(PI, R); return 1;
    }
    return 0;
}
FMAFN void mdl_mat3(const double A[9], const double B[9], double C[9]) {  /* A @ B: OpenBLAS dgemm, fma over k = 0..2 from 0 */
    double r[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        double acc = 0.0;
        for (int k = 0; k < 3; k++) acc = FMA(A[i * 3 + k], B[k * 3 + j], acc);
        r[i * 3 + j] = acc;
    }
    memcpy(C, r, sizeof r);
}
FMAFN void mdl_vRT(const double *v, int n, const double R[9], double *out) {   /* v @ R.T: out[p][i] = sum_k v[p][k] R[i][k] */
    for (int p = 0; p < n; p++) for (int i = 0; i < 3; i++) {
        double acc = 0.0;
        for (int k = 0; k < 3; k++) acc = FMA(v[p * 3 + k], R[i * 3 + k], acc);
        out[p * 3 + i] = acc;
    }
}

/* ======================= Part 3: sd_mesh.solid ======================= */
#define LAD_K 24
struct MdlSolid {
    double *v; int nv;                 /* posed points */
    double *tri; int nt;               /* tri (nt x 3 x 3) */
    double *t_; double *zmax_, *zmin_; int ns;   /* the triangles seen from above */
    double top;
    int built;                         /* the ladder is there */
    double lad_z[LAD_K + 1]; GEOSGeometry *lad_g[LAD_K + 1];
    int has_ground; GEOSGeometry *ground;
};

int mdl_above(const double t[9], double z, double out[8]) {          /* sd_mesh._above */
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const double *p = t + i * 3, *q = t + ((i + 1) % 3) * 3;
        int pin = p[2] > z, qin = q[2] > z;
        if (pin) { out[n * 2] = p[0]; out[n * 2 + 1] = p[1]; n++; }
        if (pin != qin) { double k = (z - p[2]) / (q[2] - p[2]); out[n * 2] = p[0] + k * (q[0] - p[0]); out[n * 2 + 1] = p[1] + k * (q[1] - p[1]); n++; }
    }
    return n;
}

/* clip(pick, z) with pick = (zmax_ > a) & (zmin_ <= b); the pieces appended to l. 0 = a shapely call raised */
static int clip(const MdlSolid *s, double a, double b, double z, ShpList *l) {
    for (int i = 0; i < s->ns; i++) {                                 /* wholly above: shapely.polygons, all of them first */
        if (!(s->zmax_[i] > a && s->zmin_[i] <= b) || !(s->zmin_[i] >= z)) continue;
        const double *t = s->t_ + i * 9; double xy[6] = { t[0], t[1], t[3], t[4], t[6], t[7] };
        GEOSGeometry *g = shp_polygon(xy, 3); if (!g) return 0;
        shp_list_push(l, g);
    }
    for (int i = 0; i < s->ns; i++) {                                 /* cut by the level: its part above */
        if (!(s->zmax_[i] > a && s->zmin_[i] <= b) || s->zmin_[i] >= z) continue;
        double p[8]; int n = mdl_above(s->t_ + i * 9, z, p);
        if (n < 3) continue;
        GEOSGeometry *g = shp_polygon(p, n); if (!g) return 0;
        double ar = shp_area(g);
        if (ar > 1e-4) {
            int ok = shp_is_valid(g);
            if (ok < 0) { shp_free(g); return 0; }
            if (!ok) { GEOSGeometry *h = shp_buffer_q(g, 0.0, 16); shp_free(g); if (!h) return 0; g = h; }
            shp_list_push(l, g);
        } else shp_free(g);
    }
    return 1;
}
/* join(ps): unary_union, or on an exception the union of every piece fattened by 0.004; *err when that raises too */
static GEOSGeometry *join(ShpList *l, int *err) {
    if (l->n == 0) return NULL;
    GEOSGeometry *u = shp_union_all((const GEOSGeometry *const *)l->g, l->n);
    if (u) return u;
    ShpList f = {0};
    for (int i = 0; i < l->n; i++) { GEOSGeometry *g = shp_buffer_q(l->g[i], 0.004, 16); if (!g) { shp_list_free(&f); *err = MDL_ERR_RAISE; return NULL; } shp_list_push(&f, g); }
    u = shp_union_all((const GEOSGeometry *const *)f.g, f.n);
    shp_list_free(&f);
    if (!u) *err = MDL_ERR_RAISE;
    return u;
}
static void ladder(MdlSolid *s, int *err) {                          /* ladder(): 24 levels from the top down */
    double zp = INFINITY; GEOSGeometry *gp = NULL;
    for (int k = 1; k <= LAD_K; k++) {
        double z = s->top * (1.0 - (double)k / LAD_K);
        ShpList l = {0};
        if (!clip(s, z, zp, z, &l)) { shp_list_free(&l); *err = MDL_ERR_RAISE; return; }
        if (gp) shp_list_push(&l, shp_clone(gp));
        GEOSGeometry *g = join(&l, err);
        shp_list_free(&l);
        if (*err) return;
        s->lad_z[k] = z; s->lad_g[k] = g; zp = z; if (g) gp = g;
    }
    s->built = 1;
}
GEOSGeometry *mdl_sect(MdlSolid *s, double z, int *err) {            /* sect(z) */
    *err = MDL_OK;
    if (s->top <= 0 || z >= s->top) return NULL;
    if (z == 0.0 && !s->built) {                                      /* only the outline on the ground so far */
        if (!s->has_ground) {
            ShpList l = {0};
            if (!clip(s, 0.0, INFINITY, 0.0, &l)) { shp_list_free(&l); *err = MDL_ERR_RAISE; return NULL; }
            s->ground = join(&l, err); shp_list_free(&l);
            if (*err) return NULL;
            s->has_ground = 1;
        }
        return s->ground ? shp_clone(s->ground) : NULL;
    }
    if (!s->built) { ladder(s, err); if (*err) return NULL; }
    double fk = floor((1.0 - z / s->top) * LAD_K + 1e-9);
    int k = fk < 0 ? 0 : fk > LAD_K ? LAD_K : (int)fk;               /* max(0, min(K, int(floor(...)))) */
    double zp = k ? s->lad_z[k] : INFINITY; GEOSGeometry *gp = k ? s->lad_g[k] : NULL;
    if (fabs(zp - z) < 1e-9) return gp ? shp_clone(gp) : NULL;
    ShpList l = {0};
    if (!clip(s, z, zp, z, &l)) { shp_list_free(&l); *err = MDL_ERR_RAISE; return NULL; }
    if (gp) shp_list_push(&l, shp_clone(gp));
    GEOSGeometry *g = join(&l, err);
    shp_list_free(&l);
    return g;
}
void mdl_solid_free(MdlSolid *s) {
    if (!s) return;
    for (int k = 0; k <= LAD_K; k++) shp_free(s->lad_g[k]);
    shp_free(s->ground);
    free(s->v); free(s->tri); free(s->t_); free(s->zmax_); free(s->zmin_); free(s);
}
void mdl_solid_arrays(const MdlSolid *s, const double **v, int *nv, const double **tri, int *nt) { *v = s->v; *nv = s->nv; *tri = s->tri; *nt = s->nt; }
/* smooth details (2.1, not in the Python kit): the top of the posed model above each grid node, a z-buffer of the
   triangles seen from above (the ones sect cuts) */
int mdl_hraster(const MdlSolid *s, double x0, double y0, double g, int nx, int ny, double *H) {
    for (int k = 0; k < s->ns; k++) {
        const double *t = s->t_ + k * 9;
        double ax = t[0], ay = t[1], az = t[2], bx = t[3], by = t[4], bz = t[5], cx = t[6], cy = t[7], cz = t[8];
        double d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy), mnx, mxx, mny, mxy;
        int i0, i1, j0, j1;
        if (fabs(d) < 1e-12) continue;
        mnx = ax < bx ? ax : bx; if (cx < mnx) mnx = cx; mxx = ax > bx ? ax : bx; if (cx > mxx) mxx = cx;
        mny = ay < by ? ay : by; if (cy < mny) mny = cy; mxy = ay > by ? ay : by; if (cy > mxy) mxy = cy;
        i0 = (int)ceil((mnx - x0) / g - 1e-9); i1 = (int)floor((mxx - x0) / g + 1e-9);
        j0 = (int)ceil((mny - y0) / g - 1e-9); j1 = (int)floor((mxy - y0) / g + 1e-9);
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 > nx - 1) i1 = nx - 1; if (j1 > ny - 1) j1 = ny - 1;
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
            double px = x0 + i * g, py = y0 + j * g;
            double l1 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / d, l2 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / d, l3 = 1 - l1 - l2, z;
            if (l1 < -1e-9 || l2 < -1e-9 || l3 < -1e-9) continue;
            z = l1 * az + l2 * bz + l3 * cz;
            if (z > H[(size_t)j * nx + i]) H[(size_t)j * nx + i] = z;
        }
    }
    return 0;
}

static double col_max(const double *v, int n, int c) { double m = v[c]; for (int i = 1; i < n; i++) if (v[i * 3 + c] > m) m = v[i * 3 + c]; return m; }
static double col_min(const double *v, int n, int c) { double m = v[c]; for (int i = 1; i < n; i++) if (v[i * 3 + c] < m) m = v[i * 3 + c]; return m; }

/* ---- resting (2026-11-21, not in the Python kit): a model put down settles on a face of its convex hull, the way a
   dropped thing rolls until it lies still, instead of balancing on a corner. Per mesh, once: the convex hull of its
   points, the centre of mass (the solid's own, or the surface's when the mesh is not closed), and the outward normals
   of the hull's faces the centre of mass stands over (the faces it can lie on). Kept until mdl_rest_clear. ---- */
typedef struct { const MdlMesh *m; double *nrm; int nn; } RestEnt;
static RestEnt *rest_c; static int nrest, caprest; static volatile long rest_lk;
static void rlock(void) { while (__sync_lock_test_and_set(&rest_lk, 1)) ; }
static void runlock(void) { __sync_lock_release(&rest_lk); }
typedef struct { int a, b, c; double n[3], d; int live; } HFace;
static void v_sub(const double *a, const double *b, double *o) { o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2]; }
static void v_cross(const double *a, const double *b, double *o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
static double v_dot(const double *a, const double *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static int hface(HFace *f, const double *P, int a, int b, int c) {    /* the plane of a, b, c (counter-clockwise from outside) */
    double e1[3], e2[3], n[3]; v_sub(P + b * 3, P + a * 3, e1); v_sub(P + c * 3, P + a * 3, e2); v_cross(e1, e2, n);
    double l = sqrt(v_dot(n, n)); if (l < 1e-15) return 0;
    f->a = a; f->b = b; f->c = c; f->n[0] = n[0] / l; f->n[1] = n[1] / l; f->n[2] = n[2] / l; f->d = v_dot(f->n, P + a * 3); f->live = 1;
    return 1;
}
/* the convex hull of P (n points), incremental: *F its faces (outward), returns how many (0 = flat or too few points) */
static int hull3(const double *P, int n, double scale, HFace **F) {
    double eps = 1e-9 * scale, best; int i, j, k, nf = 0, cap = 64, i0 = 0, i1 = -1, i2 = -1, i3 = -1;
    *F = NULL; if (n < 4) return 0;
    for (i = 1; i < n; i++) if (P[i * 3] < P[i0 * 3]) i0 = i;
    best = 0; for (i = 0; i < n; i++) { double d[3]; v_sub(P + i * 3, P + i0 * 3, d); double q = v_dot(d, d); if (q > best) { best = q; i1 = i; } }
    if (i1 < 0) return 0;
    best = 0; for (i = 0; i < n; i++) { double e1[3], e2[3], c[3]; v_sub(P + i1 * 3, P + i0 * 3, e1); v_sub(P + i * 3, P + i0 * 3, e2); v_cross(e1, e2, c); double q = v_dot(c, c); if (q > best) { best = q; i2 = i; } }
    if (i2 < 0) return 0;
    HFace t; if (!hface(&t, P, i0, i1, i2)) return 0;
    best = 0; for (i = 0; i < n; i++) { double q = fabs(v_dot(t.n, P + i * 3) - t.d); if (q > best) { best = q; i3 = i; } }
    if (i3 < 0 || best < 1e-6 * scale) return 0;
    HFace *f = malloc((size_t)cap * sizeof *f); if (!f) return 0;
    if (v_dot(t.n, P + i3 * 3) - t.d > 0) { int s = i1; i1 = i2; i2 = s; }   /* i3 behind the first face */
    int T[4][3] = { { i0, i1, i2 }, { i0, i3, i1 }, { i1, i3, i2 }, { i2, i3, i0 } };
    for (k = 0; k < 4; k++) if (hface(&f[nf], P, T[k][0], T[k][1], T[k][2])) nf++;
    int ecap = 4096, *ea = malloc(sizeof(int) * 2 * (size_t)ecap);
    if (!ea) { free(f); return 0; }
    for (i = 0; i < n; i++) {
        if (i == i0 || i == i1 || i == i2 || i == i3) continue;
        const double *p = P + i * 3; int ne = 0, any = 0;
        for (j = 0; j < nf; j++) if (f[j].live && v_dot(f[j].n, p) - f[j].d > eps) {
            any = 1; f[j].live = 2;                                        /* 2 = seen from p: it goes */
            int e[3][2] = { { f[j].a, f[j].b }, { f[j].b, f[j].c }, { f[j].c, f[j].a } };
            for (k = 0; k < 3; k++) {
                int r, found = -1;
                for (r = 0; r < ne; r++) if (ea[r * 2] == e[k][1] && ea[r * 2 + 1] == e[k][0]) { found = r; break; }
                if (found >= 0) { ea[found * 2] = ea[(ne - 1) * 2]; ea[found * 2 + 1] = ea[(ne - 1) * 2 + 1]; ne--; }   /* between two seen faces: inside */
                else {
                    if (ne == ecap) { int *q = realloc(ea, sizeof(int) * 2 * (size_t)(ecap * 2)); if (!q) { free(ea); free(f); return 0; } ea = q; ecap *= 2; }
                    ea[ne * 2] = e[k][0]; ea[ne * 2 + 1] = e[k][1]; ne++;
                }
            }
        }
        if (!any) continue;
        int w = 0; for (j = 0; j < nf; j++) if (f[j].live == 1) f[w++] = f[j]; nf = w;
        for (k = 0; k < ne; k++) {                                         /* the horizon joined to p */
            if (nf == cap) { HFace *q = realloc(f, (size_t)(cap * 2) * sizeof *f); if (!q) { free(ea); free(f); return 0; } f = q; cap *= 2; }
            if (hface(&f[nf], P, ea[k * 2], ea[k * 2 + 1], i)) nf++;
        }
    }
    free(ea);
    *F = f; return nf;
}
/* the normals of the faces M can lie on (unit, outward, in the mesh's own z-up frame); 0 when it has none */
static int rest_normals(const MdlMesh *M, double **out) {
    int n = M->nv, nt = M->nf, i, j, k; *out = NULL;
    if (n < 4 || nt < 1) return 0;
    const double *V = M->v;
    double lo[3] = { V[0], V[1], V[2] }, hi[3] = { V[0], V[1], V[2] };
    for (i = 1; i < n; i++) for (k = 0; k < 3; k++) { if (V[i * 3 + k] < lo[k]) lo[k] = V[i * 3 + k]; if (V[i * 3 + k] > hi[k]) hi[k] = V[i * 3 + k]; }
    double scale = fmax(fmax(hi[0] - lo[0], hi[1] - lo[1]), hi[2] - lo[2]); if (scale <= 0) return 0;
    /* the points the hull can have: every point when few, else the lowest point along 1200 directions and the 6 axes */
    int np_ = 0; double *P;
    if (n <= 1500) { P = malloc((size_t)n * 3 * sizeof(double)); if (!P) return 0; memcpy(P, V, (size_t)n * 3 * sizeof(double)); np_ = n; }
    else {
        const int K = 1200; const double ga = 3.141592653589793 * (3.0 - sqrt(5.0));
        char *mark = calloc((size_t)n, 1); int *pick = malloc(sizeof(int) * (size_t)(K + 6));
        if (!mark || !pick) { free(mark); free(pick); return 0; }
        for (j = 0; j < K + 6; j++) {
            double d[3];
            if (j < K) { double z = 1 - 2 * (j + 0.5) / K, r = sqrt(1 - z * z); d[0] = cos(ga * j) * r; d[1] = sin(ga * j) * r; d[2] = z; }
            else { d[0] = d[1] = d[2] = 0; d[(j - K) / 2] = (j - K) % 2 ? 1 : -1; }
            int bi = 0; double bv = v_dot(d, V);
            for (i = 1; i < n; i++) { double q = v_dot(d, V + i * 3); if (q < bv) { bv = q; bi = i; } }
            if (!mark[bi]) { mark[bi] = 1; pick[np_++] = bi; }
        }
        free(mark);
        P = malloc((size_t)np_ * 3 * sizeof(double)); if (!P) { free(pick); return 0; }
        for (i = 0; i < np_; i++) memcpy(P + i * 3, V + pick[i] * 3, 3 * sizeof(double));
        free(pick);
    }
    /* the centre of mass: the solid's (signed tetrahedra) when its volume makes sense, else the surface's */
    double com[3] = { 0, 0, 0 }, vol = 0, sc[3] = { 0, 0, 0 }, ar = 0;
    for (i = 0; i < nt; i++) {
        int64_t ia = M->f[i * 3], ib = M->f[i * 3 + 1], ic = M->f[i * 3 + 2];
        if (ia < 0) ia += n;
        if (ib < 0) ib += n;
        if (ic < 0) ic += n;
        if (ia < 0 || ib < 0 || ic < 0 || ia >= n || ib >= n || ic >= n) continue;
        const double *a = V + ia * 3, *b = V + ib * 3, *c = V + ic * 3;
        double bc[3], e1[3], e2[3], cr[3]; v_cross(b, c, bc); double t6 = v_dot(a, bc) / 6.0;
        v_sub(b, a, e1); v_sub(c, a, e2); v_cross(e1, e2, cr); double s2 = sqrt(v_dot(cr, cr)) / 2;
        vol += t6; ar += s2;
        for (k = 0; k < 3; k++) { com[k] += t6 * (a[k] + b[k] + c[k]) / 4; sc[k] += s2 * (a[k] + b[k] + c[k]) / 3; }
    }
    double bvol = (hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]);
    int solid_ok = vol > 0 && vol > 0.02 * bvol;
    for (k = 0; k < 3 && solid_ok; k++) { com[k] /= vol; if (com[k] < lo[k] || com[k] > hi[k]) solid_ok = 0; }
    if (!solid_ok) { if (ar <= 0) { free(P); return 0; } for (k = 0; k < 3; k++) com[k] = sc[k] / ar; }
    HFace *F; int nf = hull3(P, np_, scale, &F);
    if (nf < 4) { free(P); free(F); return 0; }
    /* the faces, joined where they lie in one plane (within about 1.5 degrees): stable when the centre of mass stands
       over the joined face (over one of its triangles) at least 2 % of the model's size in from the face's outer edges
       (the triangles' edges no other triangle of the face shares) */
    char *used = calloc((size_t)nf, 1); double *N = malloc((size_t)nf * 3 * sizeof(double)); int *grp = malloc(sizeof(int) * (size_t)nf), nn = 0;
    if (!used || !N || !grp) { free(used); free(N); free(grp); free(F); free(P); return 0; }
    /* the margin: 2 % of the model's size, at most 15 % of its thinnest side (a trowel or a bone lies on narrow faces) */
    double margin = fmin(0.02 * scale, 0.15 * fmin(fmin(hi[0] - lo[0], hi[1] - lo[1]), hi[2] - lo[2])), bestd = -1e300, bestn[3] = { 0, 0, -1 };
    for (i = 0; i < nf; i++) {
        if (used[i]) continue;
        double acc[3] = { 0, 0, 0 }; int over = 0, ng = 0, g, h, e2_;
        for (j = i; j < nf; j++) {
            if (used[j] || v_dot(F[i].n, F[j].n) < 0.99966 || fabs(F[j].d - F[i].d) > 0.004 * scale) continue;
            used[j] = 1; grp[ng++] = j;
            const double *a = P + F[j].a * 3, *b = P + F[j].b * 3, *c = P + F[j].c * 3;
            double e1[3], e2[3], cr[3]; v_sub(b, a, e1); v_sub(c, a, e2); v_cross(e1, e2, cr);
            for (k = 0; k < 3; k++) acc[k] += cr[k];
            const double *Q[3] = { a, b, c }; int in = 1;                  /* the centre of mass over this triangle? */
            for (k = 0; k < 3 && in; k++) {
                double ed[3], tc[3], en[3]; v_sub(Q[(k + 1) % 3], Q[k], ed); v_sub(com, Q[k], tc); v_cross(F[j].n, ed, en);
                double l = sqrt(v_dot(en, en)); if (l < 1e-15 || v_dot(en, tc) / l < -1e-9 * scale) in = 0;
            }
            if (in) over = 1;
        }
        double l = sqrt(v_dot(acc, acc));
        if (!over || l < 1e-15) continue;
        double nrm[3] = { acc[0] / l, acc[1] / l, acc[2] / l }, dmin = 1e300;
        for (g = 0; g < ng; g++) for (e2_ = 0; e2_ < 3; e2_++) {           /* the outer edges: how near is the centre of mass? */
            const HFace *t = &F[grp[g]]; int A = e2_ == 0 ? t->a : e2_ == 1 ? t->b : t->c, B = e2_ == 0 ? t->b : e2_ == 1 ? t->c : t->a, shared = 0;
            for (h = 0; h < ng && !shared; h++) { const HFace *o = &F[grp[h]]; if (h != g && ((o->a == B && o->b == A) || (o->b == B && o->c == A) || (o->c == B && o->a == A))) shared = 1; }
            if (shared) continue;
            double ed[3], tc[3], en[3]; v_sub(P + B * 3, P + A * 3, ed); v_sub(com, P + A * 3, tc); v_cross(nrm, ed, en);
            double el = sqrt(v_dot(en, en)); if (el < 1e-15) continue;
            double dd = v_dot(en, tc) / el; if (dd < dmin) dmin = dd;
        }
        if (dmin > bestd) { bestd = dmin; memcpy(bestn, nrm, sizeof bestn); }
        if (dmin >= margin) { for (k = 0; k < 3; k++) N[nn * 3 + k] = nrm[k]; nn++; }
    }
    if (!nn && bestd > 0) { memcpy(N, bestn, sizeof bestn); nn = 1; }   /* none with the margin: the steadiest face */
    free(used); free(grp); free(F); free(P);
    { const char *lp = getenv("BBB_REST_LOG"); FILE *lf = lp && *lp ? fopen(lp, "a") : NULL;          /* BBB_REST_LOG: the hull of each mesh (a test) */
      if (lf) { fprintf(lf, "hull of %d points (%d used): %d triangles, centre of mass %s (%.3f %.3f %.3f) in box (%.3f %.3f %.3f)-(%.3f %.3f %.3f), %d faces to lie on\n",
                        n, np_, nf, solid_ok ? "solid" : "surface", com[0], com[1], com[2], lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], nn); fclose(lf); } }
    if (!nn) { free(N); return 0; }
    *out = N; return nn;
}
/* the stable faces of M, worked out once */
static int rest_of(const MdlMesh *M, const double **N) {
    int i, nn; double *nrm;
    rlock();
    for (i = 0; i < nrest; i++) if (rest_c[i].m == M) { *N = rest_c[i].nrm; nn = rest_c[i].nn; runlock(); return nn; }
    runlock();
    nn = rest_normals(M, &nrm);
    rlock();
    if (nrest == caprest) { int c = caprest ? caprest * 2 : 64; RestEnt *p = realloc(rest_c, (size_t)c * sizeof *p); if (!p) { runlock(); *N = nrm; return 0; } rest_c = p; caprest = c; }
    rest_c[nrest].m = M; rest_c[nrest].nrm = nrm; rest_c[nrest].nn = nn; nrest++;
    runlock();
    *N = nrm; return nn;
}
void mdl_rest_clear(void) { rlock(); for (int i = 0; i < nrest; i++) free(rest_c[i].nrm); free(rest_c); rest_c = NULL; nrest = caprest = 0; runlock(); }
/* R (the posing, applied after the mirror) turned further so the stable face nearest to straight down lies flat */
static void settle(const MdlMesh *M, int mirror, double R[9]) {
    const double *N = NULL; int nn = rest_of(M, &N), i, best = -1; double bd = -2, w[3] = { 0, 0, -1 };
    for (i = 0; i < nn; i++) {
        double m[3] = { mirror ? -N[i * 3] : N[i * 3], N[i * 3 + 1], N[i * 3 + 2] };
        double z = R[6] * m[0] + R[7] * m[1] + R[8] * m[2];                /* (R m).z: straight down is -1 */
        if (-z > bd) { bd = -z; best = i; w[0] = R[0] * m[0] + R[1] * m[1] + R[2] * m[2]; w[1] = R[3] * m[0] + R[4] * m[1] + R[5] * m[2]; w[2] = z; }
    }
    { const char *lp = getenv("BBB_REST_LOG"); FILE *lf = lp && *lp ? fopen(lp, "a") : NULL;          /* BBB_REST_LOG: every settle (a test) */
      if (lf) { fprintf(lf, "points %d faces-it-can-lie-on %d turned %.0f deg\n", M->nv, nn, best < 0 ? 0.0 : acos(fmin(1, bd)) * 57.29578); fclose(lf); } }
    if (best < 0 || bd > 1 - 1e-12) return;                           /* none, or flat already */
    /* Q turns w onto (0, 0, -1) about the axis w x down (Rodrigues); R = Q R */
    double ax[3] = { -w[1], w[0], 0 }, s = sqrt(ax[0] * ax[0] + ax[1] * ax[1]), c = -w[2], Q[9];
    if (s < 1e-12) { double q[9] = { 1, 0, 0, 0, -1, 0, 0, 0, -1 }; memcpy(Q, q, sizeof q); }   /* face straight up: over on its back */
    else {
        double x = ax[0] / s, y = ax[1] / s, t = 1 - c;
        double q[9] = { c + x * x * t, x * y * t, y * s, x * y * t, c + y * y * t, -x * s, -y * s, x * s, c };
        memcpy(Q, q, sizeof q);
    }
    mdl_mat3(Q, R, R);
}
static _Thread_local int SETTLE;
void mdl_settle_next(int on) { SETTLE = on; }

int mdl_solid(const MdlMesh *VF, double mm, double cx, double cy, double yaw, char pose, double sink, int mirror,
              double tilt, double tilt_dir, double cap, MdlSolid **out, GEOSGeometry **foot, double *top) {
    int settle_ = SETTLE; SETTLE = 0;
    *out = NULL; *foot = NULL;
    int n = VF->nv, nt = VF->nf;
    if (n == 0 || nt == 0) return MDL_ERR_RAISE;                      /* .max() of an empty array */
    for (long i = 0; i < (long)nt * 3; i++) if (VF->f[i] < -(int64_t)n || VF->f[i] >= n) return MDL_ERR_RAISE;   /* v[F]: IndexError */
    MdlSolid *s = calloc(1, sizeof *s);
    double *v = malloc((size_t)n * 3 * sizeof(double)), *w = malloc((size_t)n * 3 * sizeof(double));
    if (!s || !v || !w) { free(s); free(v); free(w); return MDL_ERR_MEM; }
    for (int i = 0; i < n * 3; i++) v[i] = VF->v[i] * ((i % 3 == 0 && mirror) ? -1.0 : 1.0);   /* V * [-1, 1, 1] */
    /* R = _rz(yaw) @ _rz(tilt_dir) @ _rx(tilt) @ _rz(-tilt_dir) @ POSES[pose], left to right */
    double R[9], A[9];
    mdl_rz(yaw, R);
    mdl_rz(tilt_dir, A); mdl_mat3(R, A, R);
    mdl_rx(tilt, A); mdl_mat3(R, A, R);
    mdl_rz(-tilt_dir, A); mdl_mat3(R, A, R);
    if (!mdl_pose(pose, A)) { free(s); free(v); free(w); return MDL_ERR_RAISE; }   /* KeyError */
    mdl_mat3(R, A, R);
    if (settle_) settle(VF, mirror, R);                               /* resting on a face (2026-11-21) */
    mdl_vRT(v, n, R, w);                                              /* v @ R.T * mm */
    for (int i = 0; i < n * 3; i++) v[i] = w[i] * mm;
    free(w);
    double h = col_max(v, n, 2) - col_min(v, n, 2);
    if (cap != 0.0 && h * (1 - sink) > cap) {
        double f = cap / (h * (1 - sink));
        for (int i = 0; i < n * 3; i++) v[i] = v[i] * f;
        h = col_max(v, n, 2) - col_min(v, n, 2);
    }
    double c0 = (col_max(v, n, 0) + col_min(v, n, 0)) / 2, c1 = (col_max(v, n, 1) + col_min(v, n, 1)) / 2;
    double sub[3] = { c0 - cx, c1 - cy, col_min(v, n, 2) + sink * h };
    for (int i = 0; i < n * 3; i++) v[i] = v[i] - sub[i % 3];
    s->v = v; s->nv = n;
    double *tri = malloc((size_t)nt * 9 * sizeof(double));
    double *zmax = malloc((size_t)nt * sizeof(double)), *zmin = malloc((size_t)nt * sizeof(double));
    s->t_ = malloc((size_t)nt * 9 * sizeof(double)); s->zmax_ = malloc((size_t)nt * sizeof(double)); s->zmin_ = malloc((size_t)nt * sizeof(double));
    s->tri = tri; s->nt = nt;
    if (!tri || !zmax || !zmin || !s->t_ || !s->zmax_ || !s->zmin_) { free(zmax); free(zmin); mdl_solid_free(s); return MDL_ERR_MEM; }
    for (int i = 0; i < nt; i++) for (int c = 0; c < 3; c++) {        /* v[F], or v[F[:, ::-1]] when mirrored */
        int64_t k = VF->f[i * 3 + (mirror ? 2 - c : c)]; if (k < 0) k += n;
        memcpy(tri + i * 9 + c * 3, v + k * 3, 3 * sizeof(double));
    }
    double tp = 0;
    for (int i = 0; i < nt; i++) {                                    /* max / min over the 3 corners */
        const double *t = tri + i * 9;
        double a = t[2]; if (t[5] > a) a = t[5]; if (t[8] > a) a = t[8]; zmax[i] = a;
        double b = t[2]; if (t[5] < b) b = t[5]; if (t[8] < b) b = t[8]; zmin[i] = b;
        if (i == 0 || a > tp) tp = a;
    }
    s->top = tp; *top = tp;
    int ns = 0;
    for (int i = 0; i < nt; i++) {                                    /* seen: |e1 x e2| * 0.5 > 1e-4 */
        const double *t = tri + i * 9;
        double e1x = t[3] - t[0], e1y = t[4] - t[1], e2x = t[6] - t[0], e2y = t[7] - t[1];
        if (fabs(e1x * e2y - e1y * e2x) * 0.5 > 1e-4) { memcpy(s->t_ + ns * 9, t, 9 * sizeof(double)); s->zmax_[ns] = zmax[i]; s->zmin_[ns] = zmin[i]; ns++; }
    }
    s->ns = ns;
    free(zmax); free(zmin);
    /* the footprint: the points on or above the ground, then where edges cross it, edge by edge */
    double *pts = malloc(((size_t)n + 3 * (size_t)nt + 1) * 2 * sizeof(double)); int np_ = 0;
    if (!pts) { mdl_solid_free(s); return MDL_ERR_MEM; }
    for (int i = 0; i < n; i++) if (v[i * 3 + 2] >= 0) { pts[np_ * 2] = v[i * 3]; pts[np_ * 2 + 1] = v[i * 3 + 1]; np_++; }
    static const int AB[3][2] = { { 0, 1 }, { 1, 2 }, { 2, 0 } };
    for (int e = 0; e < 3; e++) for (int i = 0; i < nt; i++) {
        const double *p = tri + i * 9 + AB[e][0] * 3, *q = tri + i * 9 + AB[e][1] * 3;
        if ((p[2] > 0) != (q[2] > 0)) {
            double k = (0.0 - p[2]) / (q[2] - p[2]);
            pts[np_ * 2] = p[0] + k * (q[0] - p[0]); pts[np_ * 2 + 1] = p[1] + k * (q[1] - p[1]); np_++;
        }
    }
    GEOSGeometry *ft = NULL;
    if (np_ >= 3) {
        GEOSGeometry *mp = shp_multipoint(pts, np_);
        if (!mp) { free(pts); mdl_solid_free(s); return MDL_ERR_RAISE; }
        ft = shp_convex_hull(mp); shp_free(mp);
        if (!ft) { free(pts); mdl_solid_free(s); return MDL_ERR_RAISE; }
    }
    free(pts);
    if (!ft || shp_type(ft) != GEOS_POLYGON || shp_area(ft) < 1e-4) {  /* MultiPoint([(cx, cy)]).buffer(0.5) */
        shp_free(ft);
        double c[2] = { cx, cy };
        GEOSGeometry *mp = shp_multipoint(c, 1);
        ft = mp ? shp_buffer_q(mp, 0.5, 16) : NULL; shp_free(mp);
        if (!ft) { mdl_solid_free(s); return MDL_ERR_RAISE; }
    }
    *foot = ft; *out = s;
    return MDL_OK;
}

/* ======================= Part 4: tools\get_models.py ======================= */
#include <windows.h>
#include <winhttp.h>
#include <stdarg.h>
#include <time.h>

/* ---- files with UTF-8 names (Python's os / open on Windows) ---- */
static wchar_t *wpath(const char *p) {
    int n = MultiByteToWideChar(CP_UTF8, 0, p, -1, NULL, 0);
    wchar_t *w = malloc((size_t)(n > 0 ? n : 1) * sizeof(wchar_t));
    if (w) { if (n > 0) MultiByteToWideChar(CP_UTF8, 0, p, -1, w, n); else w[0] = 0; }
    return w;
}
static int f_attr(const char *p, DWORD *a) { wchar_t *w = wpath(p); if (!w) return 0; *a = GetFileAttributesW(w); free(w); return *a != INVALID_FILE_ATTRIBUTES; }
static int is_dir(const char *p) { DWORD a; return f_attr(p, &a) && (a & FILE_ATTRIBUTE_DIRECTORY); }   /* os.path.isdir */
static int is_file(const char *p) { DWORD a; return f_attr(p, &a) && !(a & FILE_ATTRIBUTE_DIRECTORY); }  /* os.path.isfile */
static int exists(const char *p) { DWORD a; return f_attr(p, &a); }
static int makedirs(const char *p) {                                 /* os.makedirs(p, exist_ok=True) */
    if (is_dir(p)) return 1;
    char *q = strdup(p); if (!q) return 0;
    size_t n = strlen(q);
    for (size_t i = 3; i <= n; i++) if (i == n || q[i] == '\\' || q[i] == '/') {
        char c = q[i]; q[i] = 0;
        if (!is_dir(q)) { wchar_t *w = wpath(q); BOOL ok = w && CreateDirectoryW(w, NULL); free(w); if (!ok && !is_dir(q)) { free(q); return 0; } }
        q[i] = c;
    }
    free(q); return 1;
}
static int write_all(const char *p, const void *d, size_t n) {       /* open(p, "wb").write(d) */
    wchar_t *w = wpath(p); if (!w) return 0;
    FILE *f = _wfopen(w, L"wb"); free(w);
    if (!f) return 0;
    int ok = fwrite(d, 1, n, f) == n; ok &= fclose(f) == 0;
    return ok;
}
static int read_all(const char *p, unsigned char **d, size_t *n) {
    wchar_t *w = wpath(p); if (!w) return 0;
    FILE *f = _wfopen(w, L"rb"); free(w);
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    *d = malloc((size_t)len + 1); if (!*d) { fclose(f); return 0; }
    *n = fread(*d, 1, (size_t)len, f); fclose(f); return *n == (size_t)len;
}
static long long file_size(const char *p) {                          /* os.path.getsize */
    wchar_t *w = wpath(p); WIN32_FILE_ATTRIBUTE_DATA a; BOOL ok = w && GetFileAttributesExW(w, GetFileExInfoStandard, &a); free(w);
    return ok ? ((long long)a.nFileSizeHigh << 32) | a.nFileSizeLow : -1;
}
static int replace_file(const char *a, const char *b) {              /* os.replace */
    wchar_t *wa = wpath(a), *wb = wpath(b); BOOL ok = wa && wb && MoveFileExW(wa, wb, MOVEFILE_REPLACE_EXISTING); free(wa); free(wb); return ok;
}
static char *pjoin(const char *a, const char *b) {                   /* os.path.join(a, b) for a relative b */
    size_t na = strlen(a), nb = strlen(b);
    char *r = malloc(na + nb + 2); if (!r) return NULL;
    memcpy(r, a, na);
    size_t o = na;
    if (na && a[na - 1] != '\\' && a[na - 1] != '/' && !(na == 2 && a[1] == ':')) r[o++] = '\\';
    memcpy(r + o, b, nb); r[o + nb] = 0; return r;
}

/* ---- text: bytes.decode("utf-8", "replace"), and the "say" lines ---- */
static char *utf8_replace(const unsigned char *s, size_t n, size_t *outn) {
    char *o = malloc(n * 3 + 1); size_t k = 0;
    if (!o) return NULL;
    for (size_t i = 0; i < n;) {
        unsigned char c = s[i]; int need; unsigned lo = 0x80, hi = 0xBF;
        if (c < 0x80) { o[k++] = (char)c; i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c >= 0xE0 && c <= 0xEF) { need = 2; if (c == 0xE0) lo = 0xA0; if (c == 0xED) hi = 0x9F; }
        else if (c >= 0xF0 && c <= 0xF4) { need = 3; if (c == 0xF0) lo = 0x90; if (c == 0xF4) hi = 0x8F; }
        else { o[k++] = (char)0xEF; o[k++] = (char)0xBF; o[k++] = (char)0xBD; i++; continue; }
        size_t j = 1;                                                /* the longest valid start: one U+FFFD for it */
        while (j <= (size_t)need && i + j < n) {
            unsigned char d = s[i + j];
            if (j == 1 ? (d < lo || d > hi) : (d < 0x80 || d > 0xBF)) break;
            j++;
        }
        if (j == (size_t)need + 1) { memcpy(o + k, s + i, j); k += j; i += j; }
        else { o[k++] = (char)0xEF; o[k++] = (char)0xBF; o[k++] = (char)0xBD; i += j; }
    }
    o[k] = 0; *outn = k; return o;
}
static int has_cc0(const char *t, size_t n) {                        /* "cc0" in txt.lower() or "creative commons zero" in ... */
    static const char *W[2] = { "cc0", "creative commons zero" };
    for (int w = 0; w < 2; w++) {
        size_t m = strlen(W[w]);
        for (size_t i = 0; i + m <= n; i++) {
            size_t q = 0;
            while (q < m) { char c = t[i + q]; if (c >= 'A' && c <= 'Z') c = (char)(c + 32); if (c != W[w][q]) break; q++; }
            if (q == m) return 1;
        }
    }
    return 0;
}
static void say_stdout(const char *line, void *ctx) { (void)ctx; fputs(line, stdout); fputc('\n', stdout); fflush(stdout); }
static void sayf(MdlSay say, void *ctx, const char *fmt, ...) {
    char buf[8192]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof buf, fmt, a); va_end(a);
    (say ? say : say_stdout)(buf, ctx);
}
static void fmt_fixed(double x, int nd, char *out, size_t cap) {    /* f"{x:.{nd}f}" (correctly rounded, ties to even) */
    snprintf(out, cap, "%.*f", nd, py_round(x, nd));
}
static void pad_left(const char *s, int width, char *out, size_t cap) {   /* f"{s:<width}" counting code points */
    int cps = 0; for (const unsigned char *p = (const unsigned char *)s; *p; p++) cps += (*p & 0xC0) != 0x80;
    snprintf(out, cap, "%s%*s", s, cps < width ? width - cps : 0, "");
}

/* ---- packs.json ---- */
typedef struct { Json j; char *models, *dl, *kit; } Manifest;
static int manifest(const char *kit, Manifest *m) {                  /* manifest() */
    memset(m, 0, sizeof *m);
    m->kit = strdup(kit); m->models = pjoin(kit, "models"); m->dl = pjoin(m->models, "_dl");
    char *f = pjoin(m->models, "packs.json");
    unsigned char *d = NULL; size_t n = 0;
    int ok = f && read_all(f, &d, &n);
    free(f);
    if (!ok) { free(d); return 0; }
    size_t off = n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF ? 3 : 0;
    JP jp = { (const char *)d + off, (const char *)d + n, 0 };
    ok = jval(&jp, &m->j, 0) && m->j.type == J_OBJ;
    free(d);
    return ok;
}
static void manifest_free(Manifest *m) { json_free(&m->j); free(m->models); free(m->dl); free(m->kit); }
static const Json *packs_of(const Manifest *m) { const Json *p = jget(&m->j, "packs"); return p && p->type == J_ARR ? p : NULL; }
static const char *sget(const Json *o, const char *k) { const Json *v = jget(o, k); return v && v->type == J_STR ? v->str : NULL; }
static int truthy(const Json *v) {                                   /* Python truth of a JSON value */
    if (!v) return 0;
    switch (v->type) { case J_NULL: return 0; case J_BOOL: return (int)v->ival; case J_NUM: return v->num != 0; case J_STR: return v->slen > 0; default: return v->n > 0; }
}
static const Json *find_pack(const Manifest *m, const char *name) {
    const Json *ps = packs_of(m);
    for (int i = 0; ps && i < ps->n; i++) { const char *n = sget(&ps->items[i], "name"); if (n && !strcmp(n, name)) return &ps->items[i]; }
    return NULL;
}
static char *file_of(const Manifest *m, const Json *p, const char *n) {   /* file_of(p, n) */
    char a[2048]; snprintf(a, sizeof a, "%s\\%s\\glb\\%s.glb", m->models, sget(p, "name"), n); return strdup(a);
}
static int have_p(const Manifest *m, const Json *p) {                /* have(p) */
    if (truthy(jget(p, "files"))) {
        const Json *l = jget(p, "list");
        if (!truthy(l) || l->type != J_ARR) return 0;
        for (int i = 0; i < l->n; i++) { if (l->items[i].type != J_STR) return 0; char *f = file_of(m, p, l->items[i].str); int ok = f && is_file(f); free(f); if (!ok) return 0; }
        return 1;
    }
    char a[2048];
    snprintf(a, sizeof a, "%s\\%s\\%s", m->models, sget(p, "name"), truthy(jget(p, "zip")) ? "obj" : "Models\\OBJ format");
    return is_dir(a);
}
int mdl_have(const char *kit, const char *pack) {
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return -1; }
    const Json *p = find_pack(&m, pack); int r = p ? have_p(&m, p) : -1;
    manifest_free(&m); return r;
}
void mdl_strs_free(char **s, int n) { for (int i = 0; i < n; i++) free(s[i]); free(s); }
static int missing_m(const Manifest *m, int all_, char ***names) {   /* missing(all_) */
    const Json *ps = packs_of(m); int n = 0;
    char **r = malloc(sizeof(char *) * (size_t)(ps ? ps->n + 1 : 1));
    for (int i = 0; ps && r && i < ps->n; i++) { const Json *p = &ps->items[i]; if ((all_ || truthy(jget(p, "used"))) && !have_p(m, p)) r[n++] = strdup(sget(p, "name")); }
    *names = r; return n;
}
int mdl_missing(const char *kit, int all_, char ***names) {
    Manifest m; *names = NULL; if (!manifest(kit, &m)) { manifest_free(&m); return -1; }
    int n = missing_m(&m, all_, names); manifest_free(&m); return n;
}

/* sum(p.get("mb", 4) for p in packs): CPython 3.15's float sum (Neumaier) after the first item */
static double py_sum_mb(const Json *const *packs, int n) {
    if (n == 0) return 0.0;
    const Json *v0 = jget(packs[0], "mb");
    double f = v0 && v0->type == J_NUM ? v0->num : 4.0, c = 0.0;
    int all_int = !(v0 && v0->type == J_NUM && !v0->is_int);
    for (int i = 1; i < n; i++) {
        const Json *v = jget(packs[i], "mb"); double x = v && v->type == J_NUM ? v->num : 4.0;
        if (all_int && (!v || v->is_int)) { f += x; continue; }      /* still the int sum (exact for these sizes) */
        all_int = 0;
        double t = f + x;
        if (fabs(f) >= fabs(x)) c += (f - t) + x; else c += (x - t) + f;
        f = t;
    }
    if (c != 0.0 && isfinite(c)) f += c;
    return f;
}
static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void explain_m(const Manifest *m, const Json *const *packs, int n, MdlSay say, void *ctx) {   /* explain(packs) */
    double mb = py_sum_mb(packs, n);
    const char *author = sget(&m->j, "author");
    char **by = malloc(sizeof(char *) * (size_t)(n + 1)); int nb = 0;
    for (int i = 0; i < n; i++) {
        const char *b = sget(packs[i], "by"); if (!b) b = author;
        int dup = 0; for (int k = 0; k < nb; k++) if (!strcmp(by[k], b)) dup = 1;
        if (!dup) by[nb++] = (char *)b;
    }
    qsort(by, (size_t)nb, sizeof *by, cmp_str);
    char joined[4096] = ""; size_t o = 0;
    for (int k = 0; k < nb; k++) o += (size_t)snprintf(joined + o, sizeof joined - o, "%s%s", k ? " and " : "", by[k]);
    free(by);
    char mbs[64]; fmt_fixed(mb, 0, mbs, sizeof mbs);
    sayf(say, ctx, "The kit turns ready-made 3D models into base details (gravestones, barrels, cannon ...). They are not part of the kit:");
    sayf(say, ctx, "they are free models by %s, in the public domain (%s: anyone may use them for any purpose).", joined, sget(&m->j, "licence"));
    sayf(say, ctx, "%d pack%s the kit uses %s not on this computer yet, so the kit will now download %s (about %s MB in all):",
         n, n != 1 ? "s" : "", n == 1 ? "is" : "are", n == 1 ? "it" : "them", mbs);
    for (int i = 0; i < n; i++) {
        char t[1024]; pad_left(sget(packs[i], "title"), 28, t, sizeof t);
        if (truthy(jget(packs[i], "files"))) { const Json *l = jget(packs[i], "list"); sayf(say, ctx, "    %s %s  (%d single models out of the collection, not all of it)", t, sget(packs[i], "page"), l ? l->n : 0); }
        else sayf(say, ctx, "    %s %s", t, sget(packs[i], "page"));
    }
    sayf(say, ctx, "They go into %s\\<pack>. This happens once. Nothing else is downloaded and nothing is sent anywhere.", m->models);
    sayf(say, ctx, "To stop it: start with --models off (bases are then made without the model details).");
}
void mdl_explain(const char *kit, char **names, int n, MdlSay say, void *ctx) {
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return; }
    const Json **ps = malloc(sizeof(Json *) * (size_t)(n + 1)); int k = 0;
    for (int i = 0; i < n; i++) { const Json *p = find_pack(&m, names[i]); if (p) ps[k++] = p; }
    explain_m(&m, ps, k, say, ctx); free(ps); manifest_free(&m);
}
static void by_hand(const Manifest *m, const Json *p, MdlSay say, void *ctx) {   /* by_hand(p) */
    const char *name = sget(p, "name");
    if (truthy(jget(p, "files"))) {
        const Json *l = jget(p, "list");
        sayf(say, ctx, "    By hand: open %s , and from its folder models/<name> download <name>.glb for each of the %d models", sget(p, "page"), l ? l->n : 0);
        sayf(say, ctx, "    named under \"list\" for this pack in %s\\packs.json, into %s\\%s\\glb. Then start again.", m->models, m->models, name); return;
    }
    if (truthy(jget(p, "zip"))) {
        sayf(say, ctx, "    By hand: download %s , and copy the .obj files from its folder %s", sget(p, "zip"), sget(p, "obj"));
        sayf(say, ctx, "    into %s\\%s\\obj. Then start again.", m->models, name); return;
    }
    sayf(say, ctx, "    By hand: open %s , press Download, and unzip the file into %s\\%s", sget(p, "page"), m->models, name);
    sayf(say, ctx, "    (so that %s\\%s\\Models\\OBJ format exists). Then start again.", m->models, name);
}

/* ---- zip files (zipfile.ZipFile: stored and deflated members) ---- */
static uint32_t crc_tab[256];
static uint32_t crc32_of(const unsigned char *d, size_t n) {
    if (!crc_tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_tab[i] = c; }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_tab[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
/* inflate (RFC 1951), after zlib's puff.c */
typedef struct { const unsigned char *in; size_t inlen, inpos; int bitbuf, bitcnt; unsigned char *out; size_t outlen, outcap; } Inf;
typedef struct { short count[16]; short symbol[320]; } Huff;
static int bits(Inf *s, int need) {
    long val = s->bitbuf;
    while (s->bitcnt < need) { if (s->inpos >= s->inlen) return -1; val |= (long)s->in[s->inpos++] << s->bitcnt; s->bitcnt += 8; }
    s->bitbuf = (int)(val >> need); s->bitcnt -= need;
    return (int)(val & ((1L << need) - 1));
}
static int outb(Inf *s, unsigned char c) {
    if (s->outlen == s->outcap) { size_t nc = s->outcap ? s->outcap * 2 : 65536; unsigned char *p = realloc(s->out, nc); if (!p) return 0; s->out = p; s->outcap = nc; }
    s->out[s->outlen++] = c; return 1;
}
static int decode(Inf *s, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        int b = bits(s, 1); if (b < 0) return -1;
        code |= b; int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -2;
}
static int construct(Huff *h, const short *length, int n) {
    short offs[16];
    for (int len = 0; len <= 15; len++) h->count[len] = 0;
    for (int sym = 0; sym < n; sym++) h->count[length[sym]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= 15; len++) { left <<= 1; left -= h->count[len]; if (left < 0) return left; }
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (int sym = 0; sym < n; sym++) if (length[sym] != 0) h->symbol[offs[length[sym]]++] = (short)sym;
    return left;
}
static int codes(Inf *s, const Huff *lencode, const Huff *distcode) {
    static const short lens[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const short dists[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
    static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
    int symbol;
    do {
        symbol = decode(s, lencode);
        if (symbol < 0) return symbol;
        if (symbol < 256) { if (!outb(s, (unsigned char)symbol)) return -9; }
        else if (symbol > 256) {
            symbol -= 257; if (symbol >= 29) return -10;
            int e = bits(s, lext[symbol]); if (e < 0) return -1;
            int len = lens[symbol] + e;
            symbol = decode(s, distcode); if (symbol < 0) return symbol;
            if (symbol >= 30) return -10;
            e = bits(s, dext[symbol]); if (e < 0) return -1;
            size_t dist = (size_t)(dists[symbol] + e);
            if (dist > s->outlen) return -11;
            while (len--) if (!outb(s, s->out[s->outlen - dist])) return -9;
        }
    } while (symbol != 256);
    return 0;
}
static int inflate_raw(const unsigned char *in, size_t n, unsigned char **out, size_t *outn) {
    Inf s = { in, n, 0, 0, 0, NULL, 0, 0 };
    int last, err = 0;
    static Huff flen, fdist; static int fixed_done;
    do {
        last = bits(&s, 1); if (last < 0) { err = -1; break; }
        int type = bits(&s, 2); if (type < 0) { err = -1; break; }
        if (type == 0) {
            s.bitbuf = 0; s.bitcnt = 0;
            if (s.inpos + 4 > s.inlen) { err = -1; break; }
            unsigned len = s.in[s.inpos] | (unsigned)s.in[s.inpos + 1] << 8, nlen = s.in[s.inpos + 2] | (unsigned)s.in[s.inpos + 3] << 8;
            s.inpos += 4;
            if (len != (~nlen & 0xffff)) { err = -2; break; }
            if (s.inpos + len > s.inlen) { err = -1; break; }
            for (unsigned i = 0; i < len; i++) if (!outb(&s, s.in[s.inpos + i])) { err = -9; break; }
            s.inpos += len;
        } else if (type == 1) {
            if (!fixed_done) {
                short l[288]; int k;
                for (k = 0; k < 144; k++) l[k] = 8;
                for (; k < 256; k++) l[k] = 9;
                for (; k < 280; k++) l[k] = 7;
                for (; k < 288; k++) l[k] = 8;
                construct(&flen, l, 288);
                for (k = 0; k < 30; k++) l[k] = 5;
                construct(&fdist, l, 30);
                fixed_done = 1;
            }
            err = codes(&s, &flen, &fdist);
        } else if (type == 2) {
            static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            short lengths[320]; Huff lc, dc;
            int nlen = bits(&s, 5), ndist = bits(&s, 5), ncode = bits(&s, 4);
            if (nlen < 0 || ndist < 0 || ncode < 0) { err = -1; break; }
            nlen += 257; ndist += 1; ncode += 4;
            if (nlen > 286 || ndist > 30) { err = -3; break; }
            int idx;
            for (idx = 0; idx < ncode; idx++) { int b = bits(&s, 3); if (b < 0) { err = -1; break; } lengths[order[idx]] = (short)b; }
            if (err) break;
            for (; idx < 19; idx++) lengths[order[idx]] = 0;
            if (construct(&lc, lengths, 19) != 0) { err = -4; break; }
            idx = 0;
            while (idx < nlen + ndist) {
                int symbol = decode(&s, &lc), len;
                if (symbol < 0) { err = symbol; break; }
                if (symbol < 16) lengths[idx++] = (short)symbol;
                else {
                    len = 0;
                    if (symbol == 16) { if (idx == 0) { err = -5; break; } len = lengths[idx - 1]; symbol = bits(&s, 2); if (symbol < 0) { err = -1; break; } symbol += 3; }
                    else if (symbol == 17) { symbol = bits(&s, 3); if (symbol < 0) { err = -1; break; } symbol += 3; }
                    else { symbol = bits(&s, 7); if (symbol < 0) { err = -1; break; } symbol += 11; }
                    if (idx + symbol > nlen + ndist) { err = -6; break; }
                    while (symbol--) lengths[idx++] = (short)len;
                }
            }
            if (err) break;
            if (lengths[256] == 0) { err = -9; break; }
            int e1 = construct(&lc, lengths, nlen); if (e1 < 0 || (e1 > 0 && nlen - lc.count[0] != 1)) { err = -7; break; }
            int e2 = construct(&dc, lengths + nlen, ndist); if (e2 < 0 || (e2 > 0 && ndist - dc.count[0] != 1)) { err = -8; break; }
            err = codes(&s, &lc, &dc);
        } else { err = -2; break; }
    } while (!err && !last);
    if (err) { free(s.out); return 0; }
    *out = s.out ? s.out : malloc(1); *outn = s.outlen; return 1;
}
typedef struct { char *name; char *orig; int method, flags; uint32_t crc; size_t csize, usize, hoff; } ZEnt;
typedef struct { unsigned char *d; size_t n; ZEnt *e; int ne; char err[160]; } Zip;
static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static const uint16_t CP437[128] = { 0xc7,0xfc,0xe9,0xe2,0xe4,0xe0,0xe5,0xe7,0xea,0xeb,0xe8,0xef,0xee,0xec,0xc4,0xc5,0xc9,0xe6,0xc6,0xf4,0xf6,0xf2,0xfb,0xf9,0xff,0xd6,0xdc,0xa2,0xa3,0xa5,0x20a7,0x192,0xe1,0xed,0xf3,0xfa,0xf1,0xd1,0xaa,0xba,0xbf,0x2310,0xac,0xbd,0xbc,0xa1,0xab,0xbb,0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,0x3b1,0xdf,0x393,0x3c0,0x3a3,0x3c3,0xb5,0x3c4,0x3a6,0x398,0x3a9,0x3b4,0x221e,0x3c6,0x3b5,0x2229,0x2261,0xb1,0x2265,0x2264,0x2320,0x2321,0xf7,0x2248,0xb0,0x2219,0xb7,0x221a,0x207f,0xb2,0x25a0,0xa0 };
static char *zname(const unsigned char *p, size_t n, int utf8) {     /* the name as Python decodes it (cp437 unless flag 0x800), as UTF-8 */
    char *o = malloc(n * 3 + 1); size_t k = 0;
    if (!o) return NULL;
    if (utf8) {
        size_t m; char *t = utf8_replace(p, n, &m);
        if (!t || m != n || memcmp(t, p, n)) { free(t); free(o); return NULL; }   /* strict: not UTF-8 raises */
        free(o); return t;
    }
    for (size_t i = 0; i < n; i++) {
        unsigned c = p[i] < 128 ? p[i] : CP437[p[i] - 128];
        if (c < 0x80) o[k++] = (char)c;
        else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 63)); }
        else { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | ((c >> 6) & 63)); o[k++] = (char)(0x80 | (c & 63)); }
    }
    o[k] = 0; return o;
}
static void zip_close(Zip *z) { for (int i = 0; i < z->ne; i++) { free(z->e[i].name); free(z->e[i].orig); } free(z->e); free(z->d); memset(z, 0, sizeof *z); }
/* _EndRecData: the end record (no zip64), -1 = none */
static long end_rec(const unsigned char *d, size_t n) {
    if (n >= 22 && !memcmp(d + n - 22, "PK\005\006", 4) && d[n - 2] == 0 && d[n - 1] == 0) return (long)(n - 22);
    size_t start = n > 65535 + 22 ? n - 65535 - 22 : 0;
    for (long i = (long)n - 4; i >= (long)start; i--) if (!memcmp(d + i, "PK\005\006", 4)) return (size_t)i + 22 <= n ? i : -1;
    return -1;
}
static int is_zipfile(const unsigned char *d, size_t n) {            /* zipfile.is_zipfile */
    long e = end_rec(d, n);
    if (e < 0) return 0;
    const unsigned char *r = d + e;
    uint16_t disk = rd16(r + 4), dstart = rd16(r + 6), total = rd16(r + 10); uint32_t size = rd32(r + 12), off = rd32(r + 16);
    if (total == 0 && size == 0 && off == 0) return 1;
    if (disk != dstart) return 0;
    long long concat = (long long)e - size - off, at = (long long)off + concat;   /* _handle_prepended_data */
    if (size < 46 || at < 0 || (size_t)at + 46 > n) return 0;
    return !memcmp(d + at, "PK\001\002", 4);
}
static int zip_open(const char *path, Zip *z) {                      /* zipfile.ZipFile(path): the central directory */
    memset(z, 0, sizeof *z);
    if (!read_all(path, &z->d, &z->n)) { snprintf(z->err, sizeof z->err, "FileNotFoundError: %s", path); return 0; }
    long e = end_rec(z->d, z->n);
    if (e < 0) { snprintf(z->err, sizeof z->err, "BadZipFile: File is not a zip file"); return 0; }
    if (e >= 20 && !memcmp(z->d + e - 20, "PK\006\007", 4)) { snprintf(z->err, sizeof z->err, "zip64 archive (not copied)"); return 0; }
    const unsigned char *r = z->d + e;
    uint32_t size = rd32(r + 12), off = rd32(r + 16);
    long long concat = (long long)e - size - off, at = (long long)off + concat;
    if (at < 0 || (size_t)(at + size) > z->n) { snprintf(z->err, sizeof z->err, "BadZipFile: Truncated central directory"); return 0; }
    size_t p = (size_t)at, endp = (size_t)(at + size); int cap = 0;
    while (p < endp) {
        if (p + 46 > z->n || memcmp(z->d + p, "PK\001\002", 4)) { snprintf(z->err, sizeof z->err, "BadZipFile: Bad magic number for central directory"); return 0; }
        const unsigned char *c = z->d + p;
        if (c[6] > 63) { snprintf(z->err, sizeof z->err, "NotImplementedError: zip file version %.1f", c[6] / 10.0); return 0; }
        size_t fl = rd16(c + 28), xl = rd16(c + 30), cl = rd16(c + 32);
        if (p + 46 + fl + xl + cl > z->n) { snprintf(z->err, sizeof z->err, "BadZipFile: Truncated central directory"); return 0; }
        if (z->ne == cap) { cap = cap ? cap * 2 : 64; ZEnt *ne = realloc(z->e, (size_t)cap * sizeof *ne); if (!ne) return 0; z->e = ne; }
        ZEnt *x = &z->e[z->ne];
        x->flags = rd16(c + 8); x->method = rd16(c + 10); x->crc = rd32(c + 16); x->csize = rd32(c + 20); x->usize = rd32(c + 24);
        x->hoff = (size_t)((long long)rd32(c + 42) + concat);
        if (x->csize == 0xFFFFFFFFu || x->usize == 0xFFFFFFFFu || rd32(c + 42) == 0xFFFFFFFFu) { snprintf(z->err, sizeof z->err, "zip64 member (not copied)"); return 0; }
        x->orig = zname(c + 46, fl, x->flags & 0x800);
        if (!x->orig) { snprintf(z->err, sizeof z->err, "UnicodeDecodeError: a member name"); return 0; }
        /* _sanitize_filename: cut at NUL, "\\" -> "/" */
        x->name = strdup(x->orig);
        for (char *q = x->name; *q; q++) if (*q == '\\') *q = '/';
        z->ne++;
        p += 46 + fl + xl + cl;
    }
    return 1;
}
static const ZEnt *zip_info(const Zip *z, const char *name) {        /* getinfo: the last member of that name */
    for (int i = z->ne - 1; i >= 0; i--) if (!strcmp(z->e[i].name, name)) return &z->e[i];
    return NULL;
}
static int zip_read(Zip *z, const ZEnt *x, unsigned char **out, size_t *n) {   /* ZipFile.read(name) */
    if (x->hoff + 30 > z->n) { snprintf(z->err, sizeof z->err, "BadZipFile: Truncated file header"); return 0; }
    const unsigned char *h = z->d + x->hoff;
    if (memcmp(h, "PK\003\004", 4)) { snprintf(z->err, sizeof z->err, "BadZipFile: Bad magic number for file header"); return 0; }
    size_t fl = rd16(h + 26), xl = rd16(h + 28);
    if (x->flags & 0x20) { snprintf(z->err, sizeof z->err, "NotImplementedError: compressed patched data (flag bit 5)"); return 0; }
    if (x->flags & 0x40) { snprintf(z->err, sizeof z->err, "NotImplementedError: strong encryption (flag bit 6)"); return 0; }
    char *fn = zname(h + 30, fl, rd16(h + 6) & 0x800);
    if (!fn || strcmp(fn, x->orig)) { free(fn); snprintf(z->err, sizeof z->err, "BadZipFile: File name in directory and header differ."); return 0; }
    free(fn);
    if (x->flags & 1) { snprintf(z->err, sizeof z->err, "RuntimeError: File is encrypted, password required for extraction"); return 0; }
    size_t at = x->hoff + 30 + fl + xl;
    if (at + x->csize > z->n) { snprintf(z->err, sizeof z->err, "EOFError"); return 0; }
    if (x->method == 0) { *out = malloc(x->csize ? x->csize : 1); if (!*out) return 0; memcpy(*out, z->d + at, x->csize); *n = x->csize; }
    else if (x->method == 8) { if (!inflate_raw(z->d + at, x->csize, out, n)) { snprintf(z->err, sizeof z->err, "zlib.error: Error -3 while decompressing data"); return 0; } }
    else { snprintf(z->err, sizeof z->err, "NotImplementedError: That compression method is not supported"); return 0; }
    if (*n > x->usize) *n = x->usize;
    if (crc32_of(*out, *n) != x->crc) { free(*out); snprintf(z->err, sizeof z->err, "BadZipFile: Bad CRC-32 for file '%s'", x->name); return 0; }
    return 1;
}
/* ZipFile._extract_member's path: splitroot, drop "", ".", "..", sanitize for Windows. NULL = no name left */
static char *arc_path(const char *name) {
    char *a = strdup(name); if (!a) return NULL;
    for (char *q = a; *q; q++) if (*q == '/') *q = '\\';
    char *rest = a;                                                  /* ntpath.splitroot */
    if (a[0] == '\\') {
        if (a[1] == '\\') {
            size_t start = !strncasecmp(a, "\\\\?\\UNC\\", 8) ? 8 : 2;
            char *i1 = strchr(a + start, '\\'); char *i2 = i1 ? strchr(i1 + 1, '\\') : NULL;
            rest = i2 ? i2 + 1 : a + strlen(a);
        } else rest = a + 1;
    } else if (a[0] && a[1] == ':') rest = a + (a[2] == '\\' ? 3 : 2);
    char *o = malloc(strlen(rest) + 1); size_t k = 0;
    char *save = NULL;
    for (char *part = strtok_r(rest, "\\", &save); part; part = strtok_r(NULL, "\\", &save)) {
        if (!strcmp(part, ".") || !strcmp(part, "..")) continue;
        for (char *q = part; *q; q++) if (strchr(":<>|\"?*", *q)) *q = '_';
        size_t L = strlen(part); while (L && (part[L - 1] == ' ' || part[L - 1] == '.')) L--;
        if (!L) continue;
        if (k) o[k++] = '\\';
        memcpy(o + k, part, L); k += L;
    }
    o[k] = 0; free(a);
    return o;
}
static int extractall(Zip *z, const char *dst) {                     /* zf.extractall(dst) */
    for (int i = 0; i < z->ne; i++) {
        const ZEnt *x = zip_info(z, z->e[i].name);
        int isdir = x->name[0] && x->name[strlen(x->name) - 1] == '/';
        char *arc = arc_path(x->name);
        if (!arc) return 0;
        if (!*arc && !isdir) { free(arc); snprintf(z->err, sizeof z->err, "ValueError: Empty filename."); return 0; }
        char *tp = *arc ? pjoin(dst, arc) : strdup(dst); free(arc);
        char *up = strdup(tp); char *sl = strrchr(up, '\\'); if (sl) *sl = 0;
        if (sl && *up && !exists(up) && !makedirs(up)) { free(up); free(tp); snprintf(z->err, sizeof z->err, "OSError: makedirs"); return 0; }
        free(up);
        if (isdir) { if (!is_dir(tp)) { wchar_t *w = wpath(tp); CreateDirectoryW(w, NULL); free(w); } free(tp); continue; }
        unsigned char *d; size_t n;
        if (!zip_read(z, x, &d, &n)) { free(tp); return 0; }
        int ok = write_all(tp, d, n); free(d);
        if (!ok) { snprintf(z->err, sizeof z->err, "OSError: %s", tp); free(tp); return 0; }
        free(tp);
    }
    return 1;
}

/* ---- HTTP (urllib.request.urlopen with the kit's headers) ---- */
static const char *UA = "Mozilla/5.0 base-kit (fetches CC0 model packs once)";
static char http_err[160];
int mdl_http_get(const char *url, const char *referer, int timeout_s, unsigned char **body, size_t *len) {
    *body = NULL; *len = 0; http_err[0] = 0;
    wchar_t *wu = wpath(url), *wua = wpath(UA);
    URL_COMPONENTS uc; memset(&uc, 0, sizeof uc); uc.dwStructSize = sizeof uc;
    wchar_t host[512], pathq[4096];
    uc.lpszHostName = host; uc.dwHostNameLength = 512; uc.lpszUrlPath = pathq; uc.dwUrlPathLength = 4096;
    wchar_t extra[4096]; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 4096;
    int status = -1;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    if (!wu || !wua || !WinHttpCrackUrl(wu, 0, 0, &uc)) { snprintf(http_err, sizeof http_err, "ValueError: unknown url type"); goto done; }
    wcsncat(pathq, extra, 4096 - wcslen(pathq) - 1);
    ses = WinHttpOpen(wua, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { snprintf(http_err, sizeof http_err, "URLError: WinHttpOpen %lu", GetLastError()); goto done; }
    int ms = timeout_s * 1000;
    WinHttpSetTimeouts(ses, ms, ms, ms, ms);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { snprintf(http_err, sizeof http_err, "URLError: connect %lu", GetLastError()); goto done; }
    req = WinHttpOpenRequest(con, L"GET", pathq, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { snprintf(http_err, sizeof http_err, "URLError: request %lu", GetLastError()); goto done; }
    if (referer) {
        char hb[4200]; snprintf(hb, sizeof hb, "Referer: %s", referer);
        wchar_t *wh = wpath(hb); WinHttpAddRequestHeaders(req, wh, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD); free(wh);
    }
    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(req, NULL)) {
        snprintf(http_err, sizeof http_err, "URLError: <urlopen error %lu>", GetLastError()); goto done;
    }
    DWORD code = 0, sz = sizeof code;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
    status = (int)code;
    size_t cap = 65536, n = 0; unsigned char *b = malloc(cap);
    for (;;) {
        DWORD avail = 0, got = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) { free(b); b = NULL; status = -1; snprintf(http_err, sizeof http_err, "TimeoutError: read %lu", GetLastError()); break; }
        if (!avail) break;
        if (n + avail > cap) { while (n + avail > cap) cap *= 2; unsigned char *q = realloc(b, cap); if (!q) { free(b); b = NULL; status = -1; break; } b = q; }
        if (!WinHttpReadData(req, b + n, avail, &got)) { free(b); b = NULL; status = -1; snprintf(http_err, sizeof http_err, "TimeoutError: read %lu", GetLastError()); break; }
        n += got;
    }
    if (b) { *body = b; *len = n; }
    if (status >= 300 || status < 200) { if (status > 0) snprintf(http_err, sizeof http_err, "HTTPError: HTTP Error %d", status); }
done:
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    free(wu); free(wua);
    return status;
}
static int http_ok(const char *url, const char *ref, int t, unsigned char **b, size_t *n) {   /* urlopen(...).read(); 0 = raised */
    int st = mdl_http_get(url, ref, t, b, n);
    if (st >= 200 && st < 300 && *b) return 1;
    free(*b); *b = NULL; return 0;
}

/* ---- fetch ---- */
static void could_not(const Manifest *m, const Json *p, const char *why, MdlSay say, void *ctx) {
    char w[96]; snprintf(w, sizeof w, "%.80s", why);
    sayf(say, ctx, "    %s: could not be fetched (%s).", sget(p, "title"), w);
    by_hand(m, p, say, ctx);
}
static int unpack_kenney(const Manifest *m, const Json *p, const char *z, MdlSay say, void *ctx) {   /* fetch(): the zip part */
    char *dst = pjoin(m->models, sget(p, "name"));
    Zip zf; int r = 0;
    if (!zip_open(z, &zf)) { could_not(m, p, zf.err, say, ctx); zip_close(&zf); free(dst); return 0; }
    const ZEnt *lic = NULL;
    for (int i = 0; i < zf.ne && !lic; i++) {
        size_t L = strlen(zf.e[i].name);
        if (L >= 11) { char t[12]; for (int k = 0; k < 11; k++) { char c = zf.e[i].name[L - 11 + k]; t[k] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c); } t[11] = 0; if (!strcmp(t, "license.txt")) lic = zip_info(&zf, zf.e[i].name); }
    }
    char *txt = NULL; size_t tn = 0;
    if (lic) {
        unsigned char *d; size_t n;
        if (!zip_read(&zf, lic, &d, &n)) { could_not(m, p, zf.err, say, ctx); goto out; }
        txt = utf8_replace(d, n, &tn); free(d);
    }
    if (!txt || !has_cc0(txt, tn)) { sayf(say, ctx, "    %s: its licence file does not say CC0, so it is NOT unpacked or used (the zip stays in %s).", sget(p, "title"), m->dl); goto out; }
    if (!extractall(&zf, dst)) { could_not(m, p, zf.err, say, ctx); goto out; }
    char mbs[64]; fmt_fixed((double)file_size(z) / 1e6, 1, mbs, sizeof mbs);
    sayf(say, ctx, "    %s: fetched (%s MB), licence CC0, in %s", sget(p, "title"), mbs, dst);
    r = have_p(m, p);
out:
    free(txt); zip_close(&zf); free(dst);
    return r;
}
static int unpack_zip_pack(const Manifest *m, const Json *p, const char *z, MdlSay say, void *ctx) {   /* fetch_zip(): the zip part */
    char *dst = pjoin(m->models, sget(p, "name"));
    Zip zf; int r = 0;
    if (!zip_open(z, &zf)) { could_not(m, p, zf.err, say, ctx); zip_close(&zf); free(dst); return 0; }
    const ZEnt *lic = NULL;
    for (int i = 0; i < zf.ne && !lic; i++) {                         /* os.path.basename(n).lower().startswith("license") */
        const char *b = zf.e[i].name, *q;
        for (q = b; *q; q++) if (*q == '/' || *q == '\\') b = q + 1;
        if (!strncasecmp(b, "license", 7)) lic = zip_info(&zf, zf.e[i].name);
    }
    char *txt = NULL; size_t tn = 0;
    if (lic) { unsigned char *d; size_t n; if (!zip_read(&zf, lic, &d, &n)) { could_not(m, p, zf.err, say, ctx); goto out; } txt = utf8_replace(d, n, &tn); free(d); }
    if (!txt || !has_cc0(txt, tn)) { sayf(say, ctx, "    %s: its licence file does not say CC0, so it is NOT unpacked or used (the zip stays in %s).", sget(p, "title"), m->dl); goto out; }
    /* pre = p["obj"].strip("/") + "/" */
    const char *o = sget(p, "obj"); size_t a = 0, b = strlen(o);
    while (a < b && o[a] == '/') a++;
    while (b > a && o[b - 1] == '/') b--;
    char pre[2048]; snprintf(pre, sizeof pre, "%.*s/", (int)(b - a), o + a);
    char *od = pjoin(dst, "obj");
    if (!makedirs(od)) { free(od); could_not(m, p, "OSError: makedirs", say, ctx); goto out; }
    int n = 0;
    for (int i = 0; i < zf.ne; i++) {
        const char *nm = zf.e[i].name; size_t L = strlen(nm);
        if (strncmp(nm, pre, strlen(pre)) || (L && nm[L - 1] == '/')) continue;
        if (!(L >= 4 && (!strcasecmp(nm + L - 4, ".obj") || !strcasecmp(nm + L - 4, ".mtl")))) continue;
        const char *base = nm, *q; for (q = nm; *q; q++) if (*q == '/' || *q == '\\') base = q + 1;
        unsigned char *d; size_t dn;
        if (!zip_read(&zf, zip_info(&zf, nm), &d, &dn)) { free(od); could_not(m, p, zf.err, say, ctx); goto out; }
        char *f = pjoin(od, base); int ok = write_all(f, d, dn); free(f); free(d);
        if (!ok) { free(od); could_not(m, p, "OSError: open", say, ctx); goto out; }
        n++;
    }
    free(od);
    {                                                                /* open(License.txt, "w", encoding="utf-8"): "\n" written as "\r\n" */
        char *t2 = malloc(tn * 2 + 1); size_t k = 0;
        for (size_t i = 0; i < tn; i++) { if (txt[i] == '\n') t2[k++] = '\r'; t2[k++] = txt[i]; }
        char *f = pjoin(dst, "License.txt"); int ok = write_all(f, t2, k); free(f); free(t2);
        if (!ok) { could_not(m, p, "OSError: open", say, ctx); goto out; }
    }
    if (!n) { sayf(say, ctx, "    %s: the zip holds no models where they were expected (%s).", sget(p, "title"), pre); goto out; }
    char mbs[64]; fmt_fixed((double)file_size(z) / 1e6, 1, mbs, sizeof mbs);
    sayf(say, ctx, "    %s: fetched (%s MB), licence CC0, in %s", sget(p, "title"), mbs, dst);
    r = have_p(m, p);
out:
    free(txt); zip_close(&zf); free(dst);
    return r;
}
static int zip_here(const char *z) {                                 /* os.path.exists(z) and zipfile.is_zipfile(z) */
    unsigned char *d = NULL; size_t n = 0;
    int ok = exists(z) && read_all(z, &d, &n) && is_zipfile(d, n);
    free(d); return ok;
}
static int download_to(const char *url, const char *ref, int t, const char *path, char *why, size_t wn) {
    unsigned char *b = NULL; size_t n = 0;
    if (!http_ok(url, ref, t, &b, &n)) { snprintf(why, wn, "%s", http_err); return 0; }
    int ok = write_all(path, b, n); free(b);
    if (!ok) snprintf(why, wn, "OSError: %s", path);
    return ok;
}
static void licence_text(const Manifest *m, const Json *p, const char *const *seen, int nseen, const char *date, char **out, size_t *n) {
    const Json *l = jget(p, "list");
    size_t cap = 65536, k = 0; char *b = malloc(cap);
#define ADD(...) k += (size_t)snprintf(b + k, cap - k, __VA_ARGS__)
    ADD("%s\nby %s\n%s\n\nLicence: %s\nStated at: %s\n\nChecked %s before fetching; these pages of the repository said CC0:\n",
        sget(p, "title"), sget(p, "by"), sget(p, "page"), sget(p, "licence"), sget(p, "licence_at"), date);
    for (int i = 0; i < nseen; i++) ADD("  %s\n", seen[i]);
    ADD("\nOnly the %d models the kit uses are here (glb\\<name>.glb), each fetched from\n  %s\n", l ? l->n : 0, sget(p, "files"));
#undef ADD
    (void)m; *out = b; *n = k;
}
static void today(char d[16]) { time_t t = time(NULL); struct tm *tm = localtime(&t); strftime(d, 16, "%Y-%m-%d", tm); }
static int fetch_files(const Manifest *m, const Json *p, MdlSay say, void *ctx) {   /* fetch_files() */
    char *dst = pjoin(m->models, sget(p, "name"));
    const Json *chk = jget(p, "check"), *list = jget(p, "list");
    const char *seen[64]; int ns = 0, r = 0;
    for (int i = 0; chk && chk->type == J_ARR && i < chk->n; i++) {
        const char *u = chk->items[i].str; unsigned char *b = NULL; size_t n = 0;
        if (!http_ok(u, NULL, 60, &b, &n)) { could_not(m, p, http_err, say, ctx); free(dst); return 0; }
        size_t tn; char *t = utf8_replace(b, n, &tn); free(b);
        int ok = has_cc0(t, tn); free(t);
        if (!ok) { sayf(say, ctx, "    %s: %s no longer says CC0, so nothing is fetched and the pack is NOT used.", sget(p, "title"), u); free(dst); return 0; }
        if (ns < 64) seen[ns++] = u;
    }
    if (!ns) { sayf(say, ctx, "    %s: packs.json names no licence page to check, so nothing is fetched and the pack is NOT used.", sget(p, "title")); free(dst); return 0; }
    char *gd = pjoin(dst, "glb"); int ok = makedirs(gd); free(gd);
    if (!ok) { could_not(m, p, "OSError: makedirs", say, ctx); free(dst); return 0; }
    int n = 0; long long size = 0;
    for (int i = 0; list && i < list->n; i++) {
        const char *name = list->items[i].str; int good = name && *name;
        for (const char *q = name; good && *q; q++) good = (*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') || *q == '_' || *q == '.' || *q == '-';
        if (!good || strstr(name, "..")) { char w[96]; snprintf(w, sizeof w, "RuntimeError: not a model name: %.30s", name ? name : ""); could_not(m, p, w, say, ctx); free(dst); return 0; }
        char *f = file_of(m, p, name);
        if (is_file(f)) { free(f); continue; }
        const char *tpl = sget(p, "files"); char url[4096] = ""; size_t k = 0;
        for (const char *q = tpl; *q && k < sizeof url - 64;) { if (!strncmp(q, "<name>", 6)) { k += (size_t)snprintf(url + k, sizeof url - k, "%s", name); q += 6; } else url[k++] = *q++; }
        url[k] = 0;
        unsigned char *b = NULL; size_t bn = 0;
        if (!http_ok(url, NULL, 120, &b, &bn)) { free(f); could_not(m, p, http_err, say, ctx); free(dst); return 0; }
        if (bn < 4 || memcmp(b, "glTF", 4)) { free(b); free(f); char w[96]; snprintf(w, sizeof w, "RuntimeError: %s.glb is not a GLB model", name); could_not(m, p, w, say, ctx); free(dst); return 0; }
        char part[2100]; snprintf(part, sizeof part, "%s.part", f);
        if (!write_all(part, b, bn) || !replace_file(part, f)) { free(b); free(f); could_not(m, p, "OSError: open", say, ctx); free(dst); return 0; }
        free(b); free(f); n++; size += (long long)bn; Sleep(400);
    }
    char date[16]; today(date);
    char *t; size_t tn; licence_text(m, p, seen, ns, date, &t, &tn);
    char *lf = pjoin(dst, "License.txt"); ok = write_all(lf, t, tn); free(lf); free(t);
    if (!ok) { could_not(m, p, "OSError: open", say, ctx); free(dst); return 0; }
    char mbs[64]; fmt_fixed((double)size / 1e6, 1, mbs, sizeof mbs);
    sayf(say, ctx, "    %s: fetched %d model%s (%s MB), licence CC0 (read at %d page%s of the repository), in %s", sget(p, "title"), n, n != 1 ? "s" : "", mbs, ns, ns != 1 ? "s" : "", dst);
    r = have_p(m, p); free(dst);
    return r;
}
static int fetch_m(const Manifest *m, const Json *p, MdlSay say, void *ctx) {   /* fetch(p) */
    makedirs(m->dl);
    char zn[1024]; snprintf(zn, sizeof zn, "%s.zip", sget(p, "name"));
    char *z = pjoin(m->dl, zn); int r;
    if (truthy(jget(p, "files"))) r = fetch_files(m, p, say, ctx);
    else if (truthy(jget(p, "zip"))) {                               /* fetch_zip() */
        char why[200];
        if (!zip_here(z) && !download_to(sget(p, "zip"), NULL, 180, z, why, sizeof why)) { could_not(m, p, why, say, ctx); r = 0; }
        else r = unpack_zip_pack(m, p, z, say, ctx);
    } else {
        char why[200] = "";
        int ok = 1;
        if (!zip_here(z)) {
            /* key = ("assets/" + name).encode().hex(); the site's JSON {"url": ...} */
            char key[1024] = "", a[512]; snprintf(a, sizeof a, "assets/%s", sget(p, "name"));
            for (size_t i = 0; a[i]; i++) snprintf(key + 2 * i, 3, "%02x", (unsigned char)a[i]);
            const char *tpl = sget(&m->j, "link"); char link[2048] = ""; size_t k = 0;
            for (const char *q = tpl; q && *q && k < sizeof link - 1;) { if (!strncmp(q, "<key>", 5)) { k += (size_t)snprintf(link + k, sizeof link - k, "%s", key); q += 5; } else link[k++] = *q++; }
            link[k] = 0;
            unsigned char *b = NULL; size_t bn = 0;
            if (!http_ok(link, sget(p, "page"), 30, &b, &bn)) { snprintf(why, sizeof why, "%s", http_err); ok = 0; }
            else {
                JP jp = { (const char *)b, (const char *)b + bn, 0 }; Json j;
                const char *url = NULL;
                if (jval(&jp, &j, 0)) url = sget(&j, "url"); else snprintf(why, sizeof why, "JSONDecodeError");
                if (!url || !*url) { if (!why[0]) snprintf(why, sizeof why, "RuntimeError: the site gave no download link"); ok = 0; }
                else ok = download_to(url, sget(p, "page"), 120, z, why, sizeof why);
                json_free(&j); free(b);
            }
        }
        if (!ok) { could_not(m, p, why, say, ctx); r = 0; }
        else r = unpack_kenney(m, p, z, say, ctx);
    }
    free(z);
    return r;
}
int mdl_fetch(const char *kit, const char *pack, MdlSay say, void *ctx) {
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return 0; }
    const Json *p = find_pack(&m, pack); int r = p ? fetch_m(&m, p, say, ctx) : 0;
    manifest_free(&m); return r;
}
int mdl_ensure(const char *kit, int all_, const char *only, MdlSay say, void *ctx) {   /* ensure(say, all_, only) */
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return -1; }
    char **names; int n = missing_m(&m, only ? 1 : all_, &names), k = 0;
    const Json **todo = malloc(sizeof(Json *) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) if (!only || !strcmp(names[i], only)) todo[k++] = find_pack(&m, names[i]);
    mdl_strs_free(names, n);
    if (!k) { free(todo); manifest_free(&m); return 0; }
    explain_m(&m, todo, k, say, ctx);
    int left = 0;
    for (int i = 0; i < k; i++) { left += !fetch_m(&m, todo[i], say, ctx); Sleep(2000); }
    if (left) sayf(say, ctx, "%d pack%s still missing: bases are made without %s details until %s there.", left, left != 1 ? "s" : "", left == 1 ? "its" : "their", left == 1 ? "it is" : "they are");
    free(todo); manifest_free(&m);
    return left;
}
void mdl_list(const char *kit, MdlSay say, void *ctx) {               /* get_models.py --list */
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return; }
    const Json *ps = packs_of(&m);
    for (int i = 0; ps && i < ps->n; i++) {
        const Json *p = &ps->items[i]; char nm[512]; pad_left(sget(p, "name"), 36, nm, sizeof nm);
        sayf(say, ctx, "%s %s %s %s", have_p(&m, p) ? "here   " : "missing", truthy(jget(p, "used")) ? "used" : "    ", nm, sget(p, "page"));
    }
    manifest_free(&m);
}
/* the network-free pieces, for proofs */
int mdl_unpack_kenney(const char *kit, const char *pack, const char *zip_path, MdlSay say, void *ctx) {
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return 0; }
    const Json *p = find_pack(&m, pack); int r = p ? unpack_kenney(&m, p, zip_path, say, ctx) : 0;
    manifest_free(&m); return r;
}
int mdl_unpack_zip_pack(const char *kit, const char *pack, const char *zip_path, MdlSay say, void *ctx) {
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return 0; }
    const Json *p = find_pack(&m, pack); int r = p ? unpack_zip_pack(&m, p, zip_path, say, ctx) : 0;
    manifest_free(&m); return r;
}
int mdl_write_files_licence(const char *kit, const char *pack, const char *const *seen, int nseen, const char *date) {
    Manifest m; if (!manifest(kit, &m)) { manifest_free(&m); return 0; }
    const Json *p = find_pack(&m, pack); int ok = 0;
    if (p) {
        char d[16]; if (!date) { today(d); date = d; }
        char *t; size_t tn; licence_text(&m, p, seen, nseen, date, &t, &tn);
        char *dst = pjoin(m.models, sget(p, "name")); char *lf = pjoin(dst, "License.txt");
        ok = makedirs(dst) && write_all(lf, t, tn); free(lf); free(dst); free(t);
    }
    manifest_free(&m); return ok;
}
int mdl_zip_here(const char *path) { return zip_here(path); }

/* ======================= Part 5: the model files packed into the exe ======================= */
/* The blob: "BBMDL1\0\0", u32 count, u32 raw size, u32 packed size, u32 crc32 of the raw bytes, then one raw deflate
   stream (RFC 1951) of the raw bytes: per file u16 name length, the name (its path under models\, "/" between
   folders), u32 size, the file's own bytes. The files are kept as they are, so a mesh read from the blob goes through
   the same proven loader and gives the same arrays as the file on disk. */

/* ---- deflate: LZ77 (hash chains, lazy by one) + one dynamic Huffman block per 64K symbols ---- */
typedef struct { unsigned char *b; size_t n, cap; uint32_t bitbuf; int bitcnt; } BitOut;
static int bo_byte(BitOut *o, unsigned char c) {
    if (o->n == o->cap) { size_t nc = o->cap ? o->cap * 2 : 1 << 20; unsigned char *p = realloc(o->b, nc); if (!p) return 0; o->b = p; o->cap = nc; }
    o->b[o->n++] = c; return 1;
}
static int bo_put(BitOut *o, uint32_t v, int n) {                     /* n bits of v, first bit first */
    o->bitbuf |= v << o->bitcnt; o->bitcnt += n;
    while (o->bitcnt >= 8) { if (!bo_byte(o, (unsigned char)o->bitbuf)) return 0; o->bitbuf >>= 8; o->bitcnt -= 8; }
    return 1;
}
static uint32_t rev_bits(uint32_t c, int n) { uint32_t r = 0; for (int i = 0; i < n; i++) { r = (r << 1) | (c & 1); c >>= 1; } return r; }
/* code lengths (at most lim bits) for the counts: Huffman, then lengths over lim cut and the Kraft sum repaired */
static void huff_lengths(const uint32_t *cnt, int n, int lim, unsigned char *len) {
    int *nodes = malloc(sizeof(int) * (size_t)n * 2), *parent = malloc(sizeof(int) * (size_t)n * 2); uint64_t *w = malloc(sizeof(uint64_t) * (size_t)n * 2);
    int live = 0, m = 0;
    for (int i = 0; i < n; i++) { len[i] = 0; if (cnt[i]) { nodes[live++] = m; w[m] = cnt[i]; parent[m] = -1; m++; } }
    int sym_of[320]; { int k = 0; for (int i = 0; i < n; i++) if (cnt[i]) sym_of[k++] = i; }
    if (live == 1) { len[sym_of[0]] = 1; free(nodes); free(parent); free(w); return; }
    int leaves = live;
    while (live > 1) {                                                /* join the two lightest (n is small: a plain scan) */
        int a = -1, b = -1;
        for (int i = 0; i < live; i++) { int x = nodes[i]; if (a < 0 || w[x] < w[nodes[a]]) { b = a; a = i; } else if (b < 0 || w[x] < w[nodes[b]]) b = i; }
        int na = nodes[a], nb = nodes[b];
        w[m] = w[na] + w[nb]; parent[m] = -1; parent[na] = m; parent[nb] = m;
        if (a > b) { int t = a; a = b; b = t; }
        nodes[a] = m; nodes[b] = nodes[--live]; m++;
    }
    for (int i = 0; i < leaves; i++) { int d = 0, x = i; while (parent[x] >= 0) { x = parent[x]; d++; } len[sym_of[i]] = (unsigned char)(d > lim ? lim : d); }
    for (;;) {                                                        /* Kraft: sum 2^-len <= 1 */
        uint64_t k = 0; for (int i = 0; i < n; i++) if (len[i]) k += (uint64_t)1 << (lim - len[i]);
        if (k <= ((uint64_t)1 << lim)) break;
        int best = -1;                                                /* lengthen the longest code still under lim */
        for (int i = 0; i < n; i++) if (len[i] && len[i] < lim && (best < 0 || len[i] > len[best])) best = i;
        len[best]++;
    }
    for (;;) {                                                        /* and fill it again to exactly 1 (a complete code) */
        uint64_t k = 0; for (int i = 0; i < n; i++) if (len[i]) k += (uint64_t)1 << (lim - len[i]);
        uint64_t slack = ((uint64_t)1 << lim) - k;
        if (!slack) break;
        int best = -1;                                                /* shorten the longest code whose step fits */
        for (int i = 0; i < n; i++) if (len[i] > 1 && ((uint64_t)1 << (lim - len[i])) <= slack && (best < 0 || len[i] > len[best])) best = i;
        if (best < 0) break;
        len[best]--;
    }
    free(nodes); free(parent); free(w);
}
static void huff_codes(const unsigned char *len, int n, uint32_t *code) {   /* canonical codes, bit-reversed for output */
    int bl[16] = {0}, next[16]; for (int i = 0; i < n; i++) bl[len[i]]++;
    bl[0] = 0; int c = 0;
    for (int b = 1; b < 16; b++) { c = (c + bl[b - 1]) << 1; next[b] = c; }
    for (int i = 0; i < n; i++) if (len[i]) code[i] = rev_bits((uint32_t)next[len[i]]++, len[i]);
}
static const short LBASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const short LEXT[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const int DBASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const short DEXT[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
typedef struct { uint16_t lit; uint16_t dist; } Sym;                  /* lit < 256: a byte; else lit = length (3..258), dist */
static int len_code(int L) { int c = 28; while (LBASE[c] > L) c--; return c; }
static int dist_code(int D) { int c = 29; while (DBASE[c] > D) c--; return c; }
static int write_block(BitOut *o, const Sym *s, int ns, int last) {
    uint32_t lc[286] = {0}, dc[30] = {0};
    for (int i = 0; i < ns; i++) { if (s[i].dist) { lc[257 + len_code(s[i].lit)]++; dc[dist_code(s[i].dist)]++; } else lc[s[i].lit]++; }
    lc[256] = 1;
    int nd = 0; for (int i = 0; i < 30; i++) if (dc[i]) nd++;
    if (nd == 0) dc[0] = 1;
    if (nd <= 1) { dc[0] = dc[0] ? dc[0] : 1; dc[1] = dc[1] ? dc[1] : 1; }  /* two distance codes, so the code is complete */
    unsigned char ll[286], dl[30]; uint32_t lcode[286] = {0}, dcode[30] = {0};
    huff_lengths(lc, 286, 15, ll); huff_lengths(dc, 30, 15, dl);
    huff_codes(ll, 286, lcode); huff_codes(dl, 30, dcode);
    int hlit = 286; while (hlit > 257 && !ll[hlit - 1]) hlit--;
    int hdist = 30; while (hdist > 1 && !dl[hdist - 1]) hdist--;
    unsigned char all[316]; int na = 0;
    for (int i = 0; i < hlit; i++) all[na++] = ll[i];
    for (int i = 0; i < hdist; i++) all[na++] = dl[i];
    /* the code lengths, run-length coded (16 / 17 / 18) */
    int rs[316], rx[316], nr = 0;
    for (int i = 0; i < na;) {
        int j = i; while (j < na && all[j] == all[i]) j++;
        int run = j - i;
        if (all[i] == 0) {
            while (run >= 11) { int k = run > 138 ? 138 : run; rs[nr] = 18; rx[nr++] = k - 11; run -= k; }
            if (run >= 3) { rs[nr] = 17; rx[nr++] = run - 3; run = 0; }
            while (run--) { rs[nr] = 0; rx[nr++] = 0; }
        } else {
            rs[nr] = all[i]; rx[nr++] = 0; run--;
            while (run >= 3) { int k = run > 6 ? 6 : run; rs[nr] = 16; rx[nr++] = k - 3; run -= k; }
            while (run--) { rs[nr] = all[i]; rx[nr++] = 0; }
        }
        i = j;
    }
    uint32_t cc[19] = {0}; for (int i = 0; i < nr; i++) cc[rs[i]]++;
    { int used = 0; for (int i = 0; i < 19; i++) used += cc[i] > 0; if (used < 2) { if (!cc[0]) cc[0] = 1; else cc[1] = 1; } }   /* two codes at least: a complete code */
    unsigned char cl[19]; uint32_t ccode[19] = {0};
    huff_lengths(cc, 19, 7, cl); huff_codes(cl, 19, ccode);
    static const int ORD[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    int hclen = 19; while (hclen > 4 && !cl[ORD[hclen - 1]]) hclen--;
    int ok = bo_put(o, (uint32_t)last, 1) && bo_put(o, 2, 2) && bo_put(o, (uint32_t)(hlit - 257), 5) && bo_put(o, (uint32_t)(hdist - 1), 5) && bo_put(o, (uint32_t)(hclen - 4), 4);
    for (int i = 0; ok && i < hclen; i++) ok = bo_put(o, cl[ORD[i]], 3);
    for (int i = 0; ok && i < nr; i++) {
        ok = bo_put(o, ccode[rs[i]], cl[rs[i]]);
        if (ok && rs[i] == 16) ok = bo_put(o, (uint32_t)rx[i], 2);
        if (ok && rs[i] == 17) ok = bo_put(o, (uint32_t)rx[i], 3);
        if (ok && rs[i] == 18) ok = bo_put(o, (uint32_t)rx[i], 7);
    }
    for (int i = 0; ok && i < ns; i++) {
        if (!s[i].dist) { ok = bo_put(o, lcode[s[i].lit], ll[s[i].lit]); continue; }
        int c = len_code(s[i].lit), d = dist_code(s[i].dist);
        ok = bo_put(o, lcode[257 + c], ll[257 + c]) && bo_put(o, (uint32_t)(s[i].lit - LBASE[c]), LEXT[c])
          && bo_put(o, dcode[d], dl[d]) && bo_put(o, (uint32_t)(s[i].dist - DBASE[d]), DEXT[d]);
    }
    if (ok) ok = bo_put(o, lcode[256], ll[256]);
    return ok;
}
static int deflate_raw(const unsigned char *in, size_t n, unsigned char **out, size_t *outn) {
    enum { WIN = 32768, HB = 16, CHAIN = 256 };
    int32_t *head = malloc(sizeof(int32_t) << HB), *prev = malloc(sizeof(int32_t) * WIN);
    Sym *syms = malloc(sizeof(Sym) * 65536); int ns = 0;
    BitOut o = {0};
    if (!head || !prev || !syms) { free(head); free(prev); free(syms); return 0; }
    for (int i = 0; i < 1 << HB; i++) head[i] = -1;
#define HASH(p) ((((uint32_t)in[p] << 10) ^ ((uint32_t)in[(p) + 1] << 5) ^ in[(p) + 2]) & ((1u << HB) - 1))
#define INSERT(p) do { if ((p) + 2 < n) { uint32_t h_ = HASH(p); prev[(p) & (WIN - 1)] = head[h_]; head[h_] = (int32_t)(p); } } while (0)
    int ok = 1;
    size_t i = 0;
    while (i < n && ok) {
        int best = 0, bd = 0;
        if (i + 2 < n) {
            uint32_t h = HASH(i); int32_t c = head[h]; int depth = CHAIN;
            size_t maxl = n - i < 258 ? n - i : 258;
            while (c >= 0 && i - (size_t)c <= WIN - 1 && depth--) {
                if (in[c + best] == in[i + best]) {
                    size_t l = 0; while (l < maxl && in[c + l] == in[i + l]) l++;
                    if ((int)l > best) { best = (int)l; bd = (int)(i - (size_t)c); if (l == maxl) break; }
                }
                int32_t nx = prev[c & (WIN - 1)]; if (nx >= c) break; c = nx;
            }
        }
        if (best >= 3) {
            syms[ns].lit = (uint16_t)best; syms[ns].dist = (uint16_t)bd; ns++;
            for (int k = 0; k < best; k++) INSERT(i + (size_t)k);
            i += (size_t)best;
        } else { syms[ns].lit = in[i]; syms[ns].dist = 0; ns++; INSERT(i); i++; }
        if (ns == 65536) { ok = write_block(&o, syms, ns, 0); ns = 0; }
    }
#undef HASH
#undef INSERT
    if (ok) ok = write_block(&o, syms, ns, 1);
    if (ok && o.bitcnt) ok = bo_put(&o, 0, 8 - o.bitcnt);
    free(head); free(prev); free(syms);
    if (!ok) { free(o.b); return 0; }
    *out = o.b; *outn = o.n; return 1;
}

/* ---- the packer (run once when the exe is built) ---- */
int mdl_pack(const char *root, const char *const *rels, int n, const char *out_path, size_t *raw_size, size_t *packed_size) {
    size_t cap = 1 << 20, k = 0; unsigned char *raw = malloc(cap);
    if (!raw) return MDL_ERR_MEM;
    for (int i = 0; i < n; i++) {
        char path[1024]; snprintf(path, sizeof path, "%s\\%s", root, rels[i]);
        unsigned char *d = NULL; size_t dn = 0;
        int rc = read_file(path, &d, &dn);
        if (rc) { free(d); free(raw); return rc; }
        size_t nl = strlen(rels[i]);
        while (k + nl + dn + 6 > cap) { cap *= 2; unsigned char *p = realloc(raw, cap); if (!p) { free(d); free(raw); return MDL_ERR_MEM; } raw = p; }
        raw[k++] = (unsigned char)(nl & 255); raw[k++] = (unsigned char)(nl >> 8);
        for (size_t c = 0; c < nl; c++) raw[k++] = (unsigned char)(rels[i][c] == '\\' ? '/' : rels[i][c]);
        uint32_t s32 = (uint32_t)dn; memcpy(raw + k, &s32, 4); k += 4;
        memcpy(raw + k, d, dn); k += dn; free(d);
    }
    unsigned char *z = NULL; size_t zn = 0;
    if (!deflate_raw(raw, k, &z, &zn)) { free(raw); return MDL_ERR_MEM; }
    uint32_t hdr[5] = { (uint32_t)n, (uint32_t)k, (uint32_t)zn, crc32_of(raw, k), 0 };
    FILE *f = fopen(out_path, "wb");
    int ok = f && fwrite("BBMDL1\0\0", 1, 8, f) == 8 && fwrite(hdr, 4, 4, f) == 4 && fwrite(z, 1, zn, f) == zn;
    if (f) ok &= fclose(f) == 0;
    free(raw); free(z);
    if (raw_size) *raw_size = k;
    if (packed_size) *packed_size = zn + 24;
    return ok ? MDL_OK : MDL_ERR_IO;
}

/* ---- reading meshes from the blob ---- */
typedef struct { const char *name; size_t nl; const unsigned char *d; size_t n; } BlobFile;
static unsigned char *blob_raw; static BlobFile *blob_files; static int blob_n;
int mdl_blob_use(const unsigned char *blob, size_t n) {
    free(blob_raw); free(blob_files); blob_raw = NULL; blob_files = NULL; blob_n = 0;
    mdl_cache_clear();
    if (!blob) return MDL_OK;                                         /* back to the files on disk */
    if (n < 24 || memcmp(blob, "BBMDL1\0\0", 8)) return MDL_ERR_RAISE;
    uint32_t hdr[4]; memcpy(hdr, blob + 8, 16);
    if (24 + (size_t)hdr[2] > n) return MDL_ERR_RAISE;
    unsigned char *raw; size_t rn;
    if (!inflate_raw(blob + 24, hdr[2], &raw, &rn) || rn != hdr[1] || crc32_of(raw, rn) != hdr[3]) { if (rn != hdr[1]) {} return MDL_ERR_RAISE; }
    BlobFile *fs = malloc(sizeof *fs * (hdr[0] ? hdr[0] : 1));
    size_t k = 0;
    for (uint32_t i = 0; i < hdr[0]; i++) {
        if (k + 2 > rn) { free(raw); free(fs); return MDL_ERR_RAISE; }
        size_t nl = raw[k] | (size_t)raw[k + 1] << 8; k += 2;
        if (k + nl + 4 > rn) { free(raw); free(fs); return MDL_ERR_RAISE; }
        fs[i].name = (const char *)raw + k; fs[i].nl = nl; k += nl;
        uint32_t s; memcpy(&s, raw + k, 4); k += 4;
        if (k + s > rn) { free(raw); free(fs); return MDL_ERR_RAISE; }
        fs[i].d = raw + k; fs[i].n = s; k += s;
    }
    blob_raw = raw; blob_files = fs; blob_n = (int)hdr[0];
    return MDL_OK;
}
int mdl_blob_from_resource(void *module, const char *name) {          /* the blob carried as an RCDATA resource */
    HRSRC r = FindResourceA((HMODULE)module, name, (LPCSTR)RT_RCDATA);
    if (!r) return MDL_ERR_IO;
    HGLOBAL g = LoadResource((HMODULE)module, r);
    const void *p = g ? LockResource(g) : NULL;
    if (!p) return MDL_ERR_IO;
    return mdl_blob_use(p, SizeofResource((HMODULE)module, r));
}
static const BlobFile *blob_find(const char *rel) {
    size_t L = strlen(rel);
    for (int i = 0; i < blob_n; i++) {
        if (blob_files[i].nl != L) continue;
        size_t c = 0;
        for (; c < L; c++) { char a = rel[c] == '\\' ? '/' : rel[c]; if (a != blob_files[i].name[c]) break; }
        if (c == L) return &blob_files[i];
    }
    return NULL;
}
/* the models the user took out (2026-11-21, the Models window: a click on a picture): removed_models.txt beside the exe, a
   model file a line, as the model table names it. A removed model is as if its file were missing: no detail is made of it.
   Read once a process (every base's layout is its own helper process, so a change counts from the next base). No file:
   nothing removed, the kit as it always was. */
static char **rm_list; static int rm_n, rm_read;
static int rm_same(const char *a, const char *b) { for (; *a && *b; a++, b++) { char x = *a == '\\' ? '/' : *a, y = *b == '\\' ? '/' : *b; if (x != y) return 0; } return *a == *b; }
int mdl_removed(const char *rel) {
    int i;
    if (!rm_read) {
        WCHAR w[MAX_PATH + 40]; DWORD n = GetModuleFileNameW(NULL, w, MAX_PATH); FILE *f; char line[600];
        rm_read = 1;
        if (n && n < MAX_PATH) {
            while (n && w[n - 1] != L'\\' && w[n - 1] != L'/') n--;
            wcscpy(w + n, L"removed_models.txt"); f = _wfopen(w, L"rb");
            if (f) {
                while (fgets(line, sizeof line, f)) {
                    size_t L = strlen(line); while (L && (line[L - 1] == '\n' || line[L - 1] == '\r' || line[L - 1] == ' ')) line[--L] = 0;
                    if (!L || line[0] == '#') continue;
                    rm_list = (char **)realloc(rm_list, sizeof(char *) * (size_t)(rm_n + 1)); rm_list[rm_n++] = _strdup(line);
                }
                fclose(f);
            }
        }
    }
    for (i = 0; i < rm_n; i++) if (rm_same(rm_list[i], rel)) return 1;
    return 0;
}
int mdl_has(const char *root, const char *rel) {                     /* os.path.exists(os.path.join(sd_mesh.ROOT, rel)) */
    if (mdl_removed(rel)) return 0;          /* taken out by the user */
    if (blob_files) return blob_find(rel) != NULL;
    char path[1024]; snprintf(path, sizeof path, "%s\\%s", root, rel);
    return exists(path);
}
static int blob_load(const char *rel, MdlMesh *out) {
    const BlobFile *f = blob_find(rel);
    if (!f) return MDL_ERR_IO;
    if (ends_with_ci(rel, ".glb")) return mdl_trimesh_glb(f->d, f->n, out);
    if (ends_with_ci(rel, ".obj")) return mdl_trimesh_obj(f->d, f->n, out);
    return UNSUP;
}

static int blob_on(void) { return blob_files != NULL; }

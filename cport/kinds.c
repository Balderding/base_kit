/* kinds.c: the shared kind sets, colour tables and style tables of the scene code (see kinds.h), and KVal. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "kinds.h"

Kinds KD;

/* ================================================================ KVal */
static KVal *kv_new(int t) { KVal *x = (KVal *)calloc(1, sizeof *x); x->t = t; return x; }
KVal *kv_none(void) { return kv_new(KV_NONE); }
KVal *kv_bool(int b) { KVal *x = kv_new(KV_BOOL); x->i = b != 0; return x; }
KVal *kv_int(int64_t i) { KVal *x = kv_new(KV_INT); x->i = i; return x; }
KVal *kv_float(double f) { KVal *x = kv_new(KV_FLOAT); x->f = f; return x; }
KVal *kv_str(const char *s) { KVal *x; if (!s) return kv_none(); x = kv_new(KV_STR); x->s = _strdup(s); return x; }
KVal *kv_list(void) { return kv_new(KV_LIST); }
KVal *kv_dict(void) { return kv_new(KV_DICT); }
static void kv_grow(KVal *l) {
    if (l->n < l->cap) return;
    l->cap = l->cap ? 2 * l->cap : 8;
    l->v = (KVal **)realloc(l->v, sizeof(KVal *) * (size_t)l->cap);
    if (l->t == KV_DICT) l->k = (char **)realloc(l->k, sizeof(char *) * (size_t)l->cap);
}
void kv_push(KVal *l, KVal *x) { kv_grow(l); l->v[l->n++] = x; }
void kv_set(KVal *d, const char *key, KVal *x) {
    int i;
    for (i = 0; i < d->n; i++) if (strcmp(d->k[i], key) == 0) { kv_free(d->v[i]); d->v[i] = x; return; }
    kv_grow(d); d->k[d->n] = _strdup(key); d->v[d->n++] = x;
}
KVal *kv_get(const KVal *d, const char *key) {
    int i; if (!d || d->t != KV_DICT) return NULL;
    for (i = 0; i < d->n; i++) if (strcmp(d->k[i], key) == 0) return d->v[i];
    return NULL;
}
KVal *kv_copy(const KVal *x) {
    KVal *y; int i;
    if (!x) return NULL;
    y = kv_new(x->t); y->i = x->i; y->f = x->f; if (x->s) y->s = _strdup(x->s);
    for (i = 0; i < x->n; i++) { if (x->t == KV_DICT) kv_set(y, x->k[i], kv_copy(x->v[i])); else kv_push(y, kv_copy(x->v[i])); }
    return y;
}
void kv_free(KVal *x) {
    int i; if (!x) return;
    for (i = 0; i < x->n; i++) { kv_free(x->v[i]); if (x->k) free(x->k[i]); }
    free(x->v); free(x->k); free(x->s); free(x);
}
int kv_is_num(const KVal *x) { return x && (x->t == KV_INT || x->t == KV_FLOAT || x->t == KV_BOOL); }
double kv_num(const KVal *x) { return !x ? 0.0 : x->t == KV_FLOAT ? x->f : (double)x->i; }
const char *kv_s(const KVal *d, const char *key) { KVal *x = kv_get(d, key); return x && x->t == KV_STR ? x->s : NULL; }

/* ---------------- json.loads, with the JSONDecodeError Python's json (its C scanner) raises ---------------- */
typedef struct { const char *s, *p; int bad; const char *msg; const char *at; } Jp;
static void jfail(Jp *j, const char *msg, const char *at) { if (!j->bad) { j->bad = 1; j->msg = msg; j->at = at; } }
static int jisws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static const char *jskip(const char *p) { while (jisws(*p)) p++; return p; }
static void put_utf8(char **b, size_t *n, size_t *cap, unsigned cp) {
    char t[4]; int k = 0, i;
    if (cp < 0x80) t[k++] = (char)cp;
    else if (cp < 0x800) { t[k++] = (char)(0xC0 | (cp >> 6)); t[k++] = (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { t[k++] = (char)(0xE0 | (cp >> 12)); t[k++] = (char)(0x80 | ((cp >> 6) & 63)); t[k++] = (char)(0x80 | (cp & 63)); }
    else { t[k++] = (char)(0xF0 | (cp >> 18)); t[k++] = (char)(0x80 | ((cp >> 12) & 63)); t[k++] = (char)(0x80 | ((cp >> 6) & 63)); t[k++] = (char)(0x80 | (cp & 63)); }
    for (i = 0; i < k; i++) { if (*n + 2 > *cap) { *cap = *cap * 2 + 64; *b = (char *)realloc(*b, *cap); } (*b)[(*n)++] = t[i]; }
}
static int hex4(const char *p, unsigned *v) {
    int i; *v = 0;
    for (i = 0; i < 4; i++) {
        char c = p[i]; *v <<= 4;
        if (c >= '0' && c <= '9') *v |= (unsigned)(c - '0'); else if (c >= 'a' && c <= 'f') *v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') *v |= (unsigned)(c - 'A' + 10); else return 0;
    }
    return 1;
}
/* scanstring: q points at the opening quote; *end gets the place after the closing one */
static char *jstr(Jp *j, const char *q, const char **end) {
    size_t n = 0, cap = 64; char *b = (char *)malloc(cap); const char *p = q + 1;
    for (;;) {
        unsigned char c = (unsigned char)*p;
        if (!c) { jfail(j, "Unterminated string starting at", q); break; }
        if (c == '"') { p++; break; }
        if (c < 0x20) { jfail(j, "Invalid control character at", p); break; }
        if (c == '\\') {
            const char *bs = p; char e = p[1];
            if (!e) { jfail(j, "Unterminated string starting at", q); break; }
            if (e == 'u') {
                unsigned cp;
                if (!hex4(p + 2, &cp)) { jfail(j, "Invalid \\uXXXX escape", p + 1); break; }
                p += 6;
                if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                    unsigned lo;
                    if (!hex4(p + 2, &lo)) { jfail(j, "Invalid \\uXXXX escape", p + 1); break; }
                    if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00)); p += 6; }
                }
                put_utf8(&b, &n, &cap, cp); continue;
            }
            switch (e) {
            case '"': case '\\': case '/': put_utf8(&b, &n, &cap, (unsigned char)e); break;
            case 'b': put_utf8(&b, &n, &cap, 8); break; case 'f': put_utf8(&b, &n, &cap, 12); break;
            case 'n': put_utf8(&b, &n, &cap, 10); break; case 'r': put_utf8(&b, &n, &cap, 13); break; case 't': put_utf8(&b, &n, &cap, 9); break;
            default: jfail(j, "Invalid \\escape", bs); goto out;
            }
            p += 2; continue;
        }
        if (n + 2 > cap) { cap *= 2; b = (char *)realloc(b, cap); }
        b[n++] = (char)c; p++;
    }
out:
    b[n] = 0; *end = p; return b;
}
/* scan_once at p: the value and *end, or NULL (StopIteration at p, or a raised error in j) */
static KVal *jval(Jp *j, const char *p, const char **end, int depth) {
    KVal *x;
    if (j->bad) return NULL;
    if (depth > 900) { jfail(j, "Expecting value", p); return NULL; }
    switch (*p) {
    case '"': { char *s = jstr(j, p, end); x = j->bad ? NULL : kv_str(s); free(s); return x; }
    case '{': {
        const char *q = jskip(p + 1);
        x = kv_dict();
        if (*q == '}') { *end = q + 1; return x; }
        if (*q != '"') { jfail(j, "Expecting property name enclosed in double quotes", q); kv_free(x); return NULL; }
        for (;;) {
            char *k; KVal *v; const char *e;
            k = jstr(j, q, &e); if (j->bad) { free(k); kv_free(x); return NULL; }
            e = jskip(e);
            if (*e != ':') { jfail(j, "Expecting ':' delimiter", e); free(k); kv_free(x); return NULL; }
            e = jskip(e + 1);
            v = jval(j, e, &e, depth + 1);
            if (!v) { if (!j->bad) jfail(j, "Expecting value", e); free(k); kv_free(x); return NULL; }
            kv_set(x, k, v); free(k);
            e = jskip(e);
            if (*e == '}') { *end = e + 1; return x; }
            if (*e != ',') { jfail(j, "Expecting ',' delimiter", e); kv_free(x); return NULL; }
            q = jskip(e + 1);
            if (*q != '"') { jfail(j, *q == '}' ? "Illegal trailing comma before end of object" : "Expecting property name enclosed in double quotes", *q == '}' ? e : q); kv_free(x); return NULL; }
        }
    }
    case '[': {
        const char *q = jskip(p + 1);
        x = kv_list();
        if (*q == ']') { *end = q + 1; return x; }
        for (;;) {
            const char *e; KVal *v = jval(j, q, &e, depth + 1);
            if (!v) { if (!j->bad) jfail(j, "Expecting value", q); kv_free(x); return NULL; }
            kv_push(x, v);
            e = jskip(e);
            if (*e == ']') { *end = e + 1; return x; }
            if (*e != ',') { jfail(j, "Expecting ',' delimiter", e); kv_free(x); return NULL; }
            q = jskip(e + 1);
            if (*q == ']') { jfail(j, "Illegal trailing comma before end of array", e); kv_free(x); return NULL; }
        }
    }
    case 'n': if (!strncmp(p, "null", 4)) { *end = p + 4; return kv_none(); } return NULL;
    case 't': if (!strncmp(p, "true", 4)) { *end = p + 4; return kv_bool(1); } return NULL;
    case 'f': if (!strncmp(p, "false", 5)) { *end = p + 5; return kv_bool(0); } return NULL;
    case 'N': if (!strncmp(p, "NaN", 3)) { *end = p + 3; return kv_float(NAN); } return NULL;
    case 'I': if (!strncmp(p, "Infinity", 8)) { *end = p + 8; return kv_float(INFINITY); } return NULL;
    default: {          /* NUMBER_RE: (-?(?:0|[1-9][0-9]*))(\.[0-9]+)?([eE][-+]?[0-9]+)? ; "-Infinity" */
        const char *q = p; int isf = 0; char buf[400]; size_t n;
        if (*q == '-') { if (!strncmp(q, "-Infinity", 9)) { *end = q + 9; return kv_float(-INFINITY); } q++; }
        if (*q == '0') q++;
        else if (*q >= '1' && *q <= '9') while (*q >= '0' && *q <= '9') q++;
        else return NULL;
        if (*q == '.' && q[1] >= '0' && q[1] <= '9') { isf = 1; q++; while (*q >= '0' && *q <= '9') q++; }
        if ((*q == 'e' || *q == 'E') && ((q[1] >= '0' && q[1] <= '9') || ((q[1] == '+' || q[1] == '-') && q[2] >= '0' && q[2] <= '9'))) {
            isf = 1; q += 2; while (*q >= '0' && *q <= '9') q++;
        }
        n = (size_t)(q - p); if (n >= sizeof buf) return NULL;
        memcpy(buf, p, n); buf[n] = 0; *end = q;
        return isf ? kv_float(strtod(buf, NULL)) : kv_int(_strtoi64(buf, NULL, 10));
    }
    }
}
KVal *kv_parse_json_e(const char *text, char *err, size_t nerr) {
    Jp j; KVal *x = NULL; const char *e, *p;
    if (err && nerr) err[0] = 0;
    if (!text) return NULL;
    memset(&j, 0, sizeof j); j.s = text;
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) jfail(&j, "Unexpected UTF-8 BOM (decode using utf-8-sig)", text);
    else {
        p = jskip(text);
        x = jval(&j, p, &e, 0);
        if (!x && !j.bad) jfail(&j, "Expecting value", p);
        if (x) { e = jskip(e); if (*e) { jfail(&j, "Extra data", e); kv_free(x); x = NULL; } }
    }
    if (j.bad && err && nerr) {          /* '%s: line %d column %d (char %d)' with positions in characters */
        int64_t pos = 0, line = 1, last = -1; const char *q;
        for (q = text; q < j.at; q++) if (((unsigned char)*q & 0xC0) != 0x80) { if (*q == '\n') { line++; last = pos; } pos++; }
        snprintf(err, nerr, "%s: line %lld column %lld (char %lld)", j.msg, (long long)line, (long long)(pos - last), (long long)pos);
    }
    return j.bad ? NULL : x;
}
KVal *kv_parse_json(const char *text) { return kv_parse_json_e(text, NULL, 0); }

/* ================================================================ maps and sets */
const char *kmap_get(const KMap *m, const char *k) { int i; for (i = 0; i < m->n; i++) if (strcmp(m->k[i], k) == 0) return m->v[i]; return NULL; }
const char *kmap_setdefault(KMap *m, const char *k, const char *v) {
    const char *o = kmap_get(m, k);
    if (o) return o;
    if (m->n == m->cap) { m->cap = m->cap ? 2 * m->cap : 64; m->k = (char **)realloc(m->k, sizeof(char *) * (size_t)m->cap); m->v = (char **)realloc(m->v, sizeof(char *) * (size_t)m->cap); }
    m->k[m->n] = _strdup(k); m->v[m->n] = _strdup(v ? v : ""); m->n++;
    return m->v[m->n - 1];
}
void kmap_set(KMap *m, const char *k, const char *v) {          /* m[k] = v (an existing key keeps its place) */
    int i; for (i = 0; i < m->n; i++) if (strcmp(m->k[i], k) == 0) { free(m->v[i]); m->v[i] = _strdup(v); return; }
    kmap_setdefault(m, k, v);
}
int kd_has(const SmStrSet *s, const char *k) { return k && s && sm_set_has(s, k); }
int kd_kind_in(const SmStrSet *s, const Kind *k) { return k && kd_has(s, k->s); }

/* ================================================================ the generated tables */
/* ---- generated by work\cscene\compose\gen_kinds.py: do not edit by hand ---- */
static const char KD_JSON[] =
    "{\"col0\":[[\"rockc\",\"#b56a4a\"],[\"rock\",\"#8a7f74\"],[\"grit\",\"#9a7158\"],[\"slate\",\"#6f7377\"],[\"grass\",\"#6d7a3a\"],[\"flower\",\"#e3c22b\"],[\"sandstone\",\"#d8b47c\"],[\"pebble\",\"#8f8"
    "a82\"],[\"ripple\",\"#d7b682\"],[\"rail\",\"#7c8796\"],[\"metal\",\"#7d838c\"],[\"pipe\",\"#6e7c8f\"],[\"plate\",\"#56606e\"],[\"flesh\",\"#9c4a4a\"],[\"pustule\",\"#c9a35a\"],[\"curb\",\"#b9b6b0\"],["
    "\"slab\",\"#9f9c96\"],[\"paver\",\"#b2aea6\"],[\"rubble\",\"#c4c0b8\"],[\"wood\",\"#8a5a32\"],[\"iron\",\"#4d4f55\"],[\"quartz\",\"#e8e4dc\"],[\"crate\",\"#a8743f\"],[\"bedrock\",\"#7a7068\"],[\"dune\""
    ",\"#dcbc86\"],[\"pan\",\"#b89a70\"],[\"cable\",\"#3c3f45\"],[\"lens\",\"#9fd7e8\"],[\"paint\",\"#e8e4d8\"],[\"cobble\",\"#8f8a82\"],[\"spire\",\"#c47450\"],[\"agave\",\"#7f8c5c\"],[\"reed\",\"#6f8a3a\""
    "],[\"moss\",\"#5d7a3a\"],[\"heather\",\"#9b5d9b\"],[\"fern\",\"#52803c\"],[\"standing\",\"#80858b\"],[\"shrub\",\"#6f7a3a\"],[\"crystal\",\"#b9dcec\"],[\"salt\",\"#ece6d6\"],[\"obelisk\",\"#c2a576\"],"
    "[\"ruin\",\"#cfb28c\"],[\"egg\",\"#c7b37a\"],[\"vein\",\"#5e2238\"],[\"spore\",\"#b45ed2\"],[\"shell\",\"#e6d8c4\"],[\"carapace\",\"#45536a\"],[\"brick\",\"#9c5242\"],[\"concrete\",\"#a3a09a\"],[\"pos"
    "t\",\"#6b5a44\"],[\"acidrim\",\"#7fae3a\"],[\"gauge\",\"#c9b458\"],[\"bone\",\"#e6dcc4\"],[\"glass\",\"#a9cfd6\"],[\"column\",\"#c9bfa8\"],[\"tendril\",\"#7a3b4a\"],[\"spine\",\"#d8c9a8\"],[\"pavers\""
    ",\"#b8ab94\"],[\"mud\",\"#5b4a33\"],[\"root\",\"#4a3622\"],[\"lily\",\"#4f8a4a\"],[\"log\",\"#6b4a2b\"],[\"basalt\",\"#3b3534\"],[\"obsidian\",\"#1c1a22\"],[\"ember\",\"#c8501e\"],[\"ice\",\"#bfe3ee\""
    "],[\"snow\",\"#eef4f7\"],[\"alien\",\"#5a7f8c\"],[\"duck\",\"#7a5a36\"],[\"sandbag\",\"#a8946a\"],[\"wire\",\"#55504a\"],[\"tatami\",\"#b9b06a\"],[\"plank\",\"#8a5a32\"],[\"lantern\",\"#9a968c\"],[\"g"
    "arden\",\"#7d8a80\"],[\"fleshfloor\",\"#8c4046\"],[\"tooth\",\"#e8dcc0\"],[\"board\",\"#94683c\"],[\"barrel\",\"#7a5230\"],[\"headstone\",\"#8f9396\"],[\"coffin\",\"#5c4630\"],[\"earth\",\"#5a4634\"],"
    "[\"fence\",\"#33363a\"],[\"stump\",\"#7a5a3a\"],[\"shroom\",\"#c9a27a\"],[\"leaf\",\"#9a6a2a\"],[\"flag\",\"#77726c\"],[\"chain\",\"#4a4c50\"],[\"trap\",\"#6a4c2c\"],[\"tflag\",\"#b8a888\"],[\"tcolumn"
    "\",\"#cbbf9f\"],[\"gold\",\"#d4a72c\"],[\"shelf\",\"#6f6a64\"],[\"drift\",\"#b09a78\"],[\"rope\",\"#b8a070\"],[\"weed\",\"#3f6a4a\"],[\"ashdrift\",\"#a39e97\"],[\"ashstone\",\"#77736d\"],[\"ashcinder\""
    ",\"#4f4c49\"],[\"ashscrap\",\"#73625a\"],[\"ashhull\",\"#5f5a55\"],[\"ashcargo\",\"#6d6a5f\"],[\"ashpipe\",\"#7d7b76\"],[\"ashpylon\",\"#8e8a82\"],[\"ashtarp\",\"#857a66\"],[\"ashpaint\",\"#b89a3c\"],"
    "[\"blightrock\",\"#584a54\"],[\"thorn\",\"#2a2230\"],[\"fleshrock\",\"#8c5a60\"],[\"blister\",\"#b48a6a\"],[\"runestone\",\"#6a6270\"],[\"slag\",\"#3d3431\"],[\"girder\",\"#5e3526\"],[\"dust\",\"#9c4a"
    "2c\"],[\"oxide\",\"#b35a2c\"],[\"monolith\",\"#1e1f26\"],[\"obfloor\",\"#4b4954\"],[\"obmetal\",\"#8d939e\"],[\"casket\",\"#34323c\"],[\"sand\",\"#cdb68a\"],[\"obglow\",\"#5fc4b8\"],[\"obinlay\",\"#b4"
    "9a52\"],[\"ashlar\",\"#a6a196\"],[\"statue\",\"#c4bfb2\"],[\"spoil\",\"#77716a\"],[\"rust\",\"#7a4a32\"],[\"brass\",\"#b8923c\"],[\"spikethorn\",\"#5b6130\"],[\"creeper\",\"#3b5a2a\"],[\"maw\",\"#9a3a"
    "55\"],[\"glowcap\",\"#8fdcc0\"],[\"sporepod\",\"#b39a3c\"],[\"pitcher\",\"#6c7e34\"],[\"hullplate\",\"#6b737a\"],[\"sundew\",\"#a04a4a\"],[\"seedburr\",\"#7a6a34\"],[\"rootbark\",\"#5a4430\"]],\"cutc0"
    "\":[[\"crack\",\"#1e1612\"],[\"trough\",\"#a88a5c\"],[\"slot\",\"#0d1014\"],[\"pothole\",\"#3a3836\"],[\"hollow\",\"#3a3026\"],[\"patch\",\"#55524e\"],[\"water\",\"#2d4a5a\"],[\"pit\",\"#2a2018\"],[\""
    "rut\",\"#3b2a1e\"],[\"wash\",\"#6e3a28\"],[\"acid\",\"#63e042\"],[\"coolant\",\"#2f92b4\"],[\"scorch\",\"#1f1d1b\"],[\"glyph\",\"#3a2c18\"],[\"mudcrack\",\"#2a1a12\"],[\"seam\",\"#15120f\"],[\"joint\""
    ",\"#1a1714\"],[\"lava\",\"#e2571c\"],[\"bile\",\"#9aa63a\"],[\"tar\",\"#15130f\"]],\"pieces0\":[\"crate\",\"flower\",\"grit\",\"lens\",\"obelisk\",\"pebble\",\"post\",\"pustule\",\"quartz\",\"rock\",\""
    "rubble\",\"spire\"],\"chamfer0\":[\"bedrock\",\"brick\",\"concrete\",\"rock\",\"rubble\",\"sandstone\",\"slate\"],\"bridge0\":[\"alien\",\"basalt\",\"board\",\"duck\",\"flag\",\"fleshfloor\",\"ice\",\""
    "plank\",\"plate\",\"shelf\",\"tatami\",\"tflag\"],\"soft0\":[\"grit\"],\"terrk0\":[\"bedrock\",\"dune\"],\"wetk0\":[\"acid\",\"bile\",\"coolant\",\"lava\",\"tar\",\"wash\",\"water\"],\"debris0\":[\"ba"
    "salt\",\"brick\",\"concrete\",\"crystal\",\"ember\",\"flag\",\"garden\",\"glass\",\"ice\",\"obsidian\",\"pebble\",\"quartz\",\"rock\",\"rubble\",\"salt\",\"sandstone\",\"shelf\",\"slate\",\"snow\",\"s"
    "pire\",\"tcolumn\"],\"growth0\":[\"agave\",\"carapace\",\"earth\",\"egg\",\"fern\",\"flesh\",\"flower\",\"grass\",\"heather\",\"leaf\",\"lily\",\"moss\",\"mud\",\"pustule\",\"reed\",\"root\",\"shell\""
    ",\"shroom\",\"shrub\",\"spine\",\"spore\",\"stump\",\"tendril\",\"tooth\",\"vein\",\"weed\"],\"cutk0\":[\"acid\",\"bile\",\"coolant\",\"crack\",\"glyph\",\"hollow\",\"lava\",\"pan\",\"pit\",\"pothole\""
    ",\"rut\",\"scorch\",\"seam\",\"slot\",\"tar\",\"wash\",\"water\"],\"tall0\":[\"colonnade\",\"dais_altar\",\"dais_brazier\",\"dais_column\",\"machine\",\"portal\",\"ruin\",\"spires\",\"statue\",\"wall\""
    "],\"also_ok\":[\"camp\",\"crater\",\"depot\",\"graves\",\"machine\",\"nest\",\"pool\",\"shaft\",\"statue\",\"tent\",\"tree\",\"wagon\",\"well\"],\"edge_ok\":[\"portal\",\"wall\",\"hull\",\"ruin\"],\"r"
    "elations\":[\"at\",\"wall\",\"front\",\"back\",\"path\",\"start\",\"yard\",\"rim\",\"also\",\"far\",\"in\"],\"abstract\":[\"crescent\",\"cross\",\"hex\",\"ring\",\"star\"],\"built0\":[\"corridor\",\"d"
    "ungeon\",\"fortress\",\"graveyard\",\"targa\",\"temple\",\"trederra\",\"urban\"],\"enclosed0\":[\"beast\",\"corridor\",\"cynder\",\"dungeon\",\"fortress\",\"infestation\",\"mine\",\"temple\"],\"cliffs"
    "0\":[\"badlands\",\"coast\",\"highland\",\"maahl\",\"targa\",\"urban\"],\"pal0\":{\"badlands\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"sandstone\",\"crate\":\"crate\",\"wate"
    "r\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"sandstone\",\"plant\":\"shrub\",\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"highland\":{\"wood\":\"wood\",\"iron\":\"i"
    "ron\",\"rock\":\"slate\",\"stone\":\"slate\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"slate\",\"plant\":\"heather\",\"soft\":\"moss\",\"bone\":\"bone\",\"twig\""
    ":\"twig\",\"vein\":\"vein\"},\"maahl\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"ruin\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":\"crystal\",\"floor\":\""
    "pavers\",\"plant\":\"shrub\",\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"corridor\":{\"wood\":\"metal\",\"iron\":\"metal\",\"rock\":null,\"stone\":\"metal\",\"crate\":\"crat"
    "e\",\"water\":\"coolant\",\"pit\":\"slot\",\"gem\":null,\"floor\":\"plate\",\"plant\":null,\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"infestation\":{\"wood\":\"metal\",\"ir"
    "on\":\"metal\",\"rock\":\"rock\",\"stone\":\"carapace\",\"crate\":\"crate\",\"water\":\"acid\",\"pit\":\"pit\",\"gem\":\"spine\",\"floor\":\"plate\",\"plant\":null,\"soft\":\"flesh\",\"bone\":\"bone\""
    ",\"twig\":\"twig\",\"vein\":\"tendril\"},\"urban\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"rubble\",\"stone\":\"concrete\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pothole\",\"gem\":n"
    "ull,\"floor\":\"concrete\",\"plant\":null,\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"mine\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"rock\",\"crat"
    "e\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":\"quartz\",\"floor\":\"wood\",\"plant\":null,\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"swamp\":{\"wood\":\"log\","
    "\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"tflag\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"tflag\",\"plant\":\"reed\",\"soft\":\"mud\",\"bone\":\"bone\",\""
    "twig\":\"reed\",\"vein\":\"root\"},\"cynder\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"basalt\",\"stone\":\"basalt\",\"crate\":\"crate\",\"water\":\"lava\",\"pit\":\"pit\",\"gem\":\"obsidian\","
    "\"floor\":\"basalt\",\"plant\":null,\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"targa\":{\"wood\":\"alien\",\"iron\":\"metal\",\"rock\":\"ice\",\"stone\":\"alien\",\"crate\""
    ":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":\"crystal\",\"floor\":\"alien\",\"plant\":null,\"soft\":\"snow\",\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"trederra\":{\"wood\":\""
    "wood\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"brick\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"duck\",\"plant\":null,\"soft\":\"mud\",\"bone\":\"bone\""
    ",\"twig\":\"twig\",\"vein\":\"vein\"},\"fortress\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"garden\",\"stone\":\"garden\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\""
    "floor\":\"plank\",\"plant\":\"shrub\",\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"beast\":{\"wood\":\"bone\",\"iron\":\"iron\",\"rock\":\"flesh\",\"stone\":\"tooth\",\"crate"
    "\":\"crate\",\"water\":\"bile\",\"pit\":\"pit\",\"gem\":\"tooth\",\"floor\":\"fleshfloor\",\"plant\":null,\"soft\":\"flesh\",\"bone\":\"bone\",\"twig\":\"flesh\",\"vein\":\"vein\"},\"frontier\":{\"woo"
    "d\":\"wood\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"rock\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"board\",\"plant\":\"shrub\",\"soft\":null,\"bone\":"
    "\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"graveyard\":{\"wood\":\"wood\",\"iron\":\"fence\",\"rock\":\"rock\",\"stone\":\"headstone\",\"crate\":\"coffin\",\"water\":\"water\",\"pit\":\"pit\",\"g"
    "em\":null,\"floor\":\"headstone\",\"plant\":\"shrub\",\"soft\":\"earth\",\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"forest\":{\"wood\":\"log\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone"
    "\":\"rock\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"log\",\"plant\":\"shrub\",\"soft\":\"moss\",\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"root\"},\"dungeo"
    "n\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"flag\",\"stone\":\"flag\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"flag\",\"plant\":null,\"soft\":null,\"bon"
    "e\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"temple\":{\"wood\":\"wood\",\"iron\":\"gold\",\"rock\":\"tcolumn\",\"stone\":\"tcolumn\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"g"
    "em\":\"gold\",\"floor\":\"tflag\",\"plant\":\"shrub\",\"soft\":null,\"bone\":\"bone\",\"twig\":\"twig\",\"vein\":\"vein\"},\"coast\":{\"wood\":\"drift\",\"iron\":\"iron\",\"rock\":\"shelf\",\"stone\":"
    "\"shelf\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"drift\",\"plant\":\"weed\",\"soft\":null,\"bone\":\"bone\",\"twig\":\"weed\",\"vein\":\"vein\"}},\"path_ok0\""
    ":{\"badlands\":\"trail ruts track stream stones\",\"highland\":\"trail stones stream ruts\",\"maahl\":\"trail way stones ruts\",\"corridor\":\"way pipe cable track\",\"infestation\":\"veins cable stre"
    "am pipe\",\"urban\":\"way cable pipe track ruts\",\"mine\":\"track trail ruts cable boards stream\",\"swamp\":\"boards stones stream veins trail\",\"cynder\":\"trail way stream track stones\",\"targa\""
    ":\"way pipe cable stones trail\",\"trederra\":\"boards trail ruts track stream\",\"fortress\":\"stones way stream boards\",\"beast\":\"veins stream\",\"frontier\":\"ruts trail boards track stream\",\""
    "graveyard\":\"trail stones way\",\"forest\":\"trail stones stream boards veins\",\"dungeon\":\"way stones stream\",\"temple\":\"way stones stream\",\"coast\":\"trail boards stones stream\"},\"says0\":"
    "{\"badlands\":\"dry red rock country\",\"highland\":\"wet moor and slate\",\"maahl\":\"a desert with buried ruins and crystals\",\"corridor\":\"the deck of a spaceship\",\"infestation\":\"a ship overr"
    "un by an alien growth\",\"urban\":\"a ruined city street\",\"mine\":\"an 1800s mine dug by pick and powder\",\"swamp\":\"a swamp with a sunken serpent temple\",\"cynder\":\"volcanic caverns with lava\""
    ",\"targa\":\"a frozen plateau with an alien city under the ice\",\"trederra\":\"a trench war in mud\",\"fortress\":\"a feudal Japanese castle and its garden\",\"beast\":\"the inside of a gigantic livi"
    "ng creature\",\"frontier\":\"a wild west town\",\"graveyard\":\"an old graveyard\",\"forest\":\"a forest floor\",\"dungeon\":\"a stone dungeon\",\"temple\":\"an ancient temple\",\"coast\":\"a rocky se"
    "a shore\"},\"extra0\":{\"mine\":\"gear cables trapdoor\",\"swamp\":\"tentacle eggs fairy nest anchor\",\"targa\":\"oildrum tank gear grate wrench vent cables\",\"trederra\":\"oildrum tank rifle casing"
    "s sandbags gear cables\",\"beast\":\"tentacle teeth pustules eggs anchor nest\",\"badlands\":\"rifle nest\",\"highland\":\"fairy nest\",\"maahl\":\"nest\",\"corridor\":\"oildrum tank gear grate wrench"
    " vent cables casings rifle\",\"infestation\":\"oildrum tank gear grate wrench vent cables casings rifle tentacle pustules eggs nest\",\"urban\":\"oildrum tank gear grate wrench vent cables casings rif"
    "le sandbags\",\"frontier\":\"rifle casings nest\",\"graveyard\":\"coffin headstone fairy\",\"forest\":\"fairy nest\",\"dungeon\":\"coins trapdoor coffin grate\",\"temple\":\"coins grate\",\"coast\":\""
    "anchor nest eggs\",\"cynder\":\"\",\"fortress\":\"\"},\"subjects0\":[\"wall\",\"dais_brazier\",\"dais_column\",\"dais_altar\",\"skeleton\",\"crater\",\"colonnade\",\"hull\",\"vehicle\",\"tree\",\"circ"
    "le\",\"ruin\",\"pool\",\"nest\",\"spires\",\"tentacles\",\"portal\",\"shaft\",\"camp\",\"depot\",\"graves\",\"wagon\",\"corral\",\"machine\",\"emplacement\",\"tent\",\"log\",\"well\",\"statue\",\"orga"
    "n\"],\"subject_says0\":{\"wall\":\"a ruined wall\",\"dais_brazier\":\"a stepped platform with a brazier\",\"dais_column\":\"a stepped platform with a column\",\"dais_altar\":\"a stepped platform with "
    "an altar\",\"skeleton\":\"the skeleton of something huge\",\"crater\":\"a rimmed crater or pit\",\"colonnade\":\"two rows of columns with a paved way\",\"hull\":\"a wrecked boat, wagon bed or pod\",\""
    "vehicle\":\"a wrecked tank or truck\",\"tree\":\"a great stump with roots\",\"circle\":\"a ritual circle with standing stones\",\"ruin\":\"a ruined building floor with broken walls\",\"pool\":\"a pool"
    " with stepping stones\",\"nest\":\"a great nest with eggs\",\"spires\":\"a cluster of crystal or rock spikes\",\"tentacles\":\"a pit with tentacles\",\"portal\":\"a doorway on the edge of the base\",\""
    "shaft\":\"a square shaft with a windlass\",\"camp\":\"a camp fire with bedrolls\",\"depot\":\"crates and barrels stacked on a pallet\",\"graves\":\"a row of graves, one open\",\"wagon\":\"a broken wag"
    "on\",\"corral\":\"a fenced ring with a gate\",\"machine\":\"a machine with a tank and pipes\",\"emplacement\":\"a sandbagged gun pit\",\"tent\":\"a ridge tent\",\"log\":\"a fallen giant tree\",\"well\""
    ":\"a big stone well\",\"statue\":\"a plinth with a fallen statue\",\"organ\":\"a great soft organ with veins\",\"bridge\":\"a stream with a crossing\"},\"prop_names\":[\"pick\",\"shovel\",\"axe\",\"sw"
    "ord\",\"spear\",\"rifle\",\"anchor\",\"wrench\",\"wheel\",\"ladder\",\"cart\",\"chest\",\"sign\",\"barrel\",\"keg\",\"crate\",\"crates\",\"timbers\",\"planks\",\"beam\",\"campfire\",\"brazier\",\"well"
    "head\",\"skull\",\"ribs\",\"bones\",\"carcass\",\"grate\",\"gear\",\"lid\",\"tiles\",\"slab\",\"steps\",\"drum\",\"stub\",\"runes\",\"crystals\",\"stump\",\"puddle\",\"nest\",\"fairy\",\"tentacle\",\""
    "pothole\",\"posts\",\"headstone\",\"coffin\",\"lantern\",\"teeth\",\"sandbags\",\"trapdoor\",\"vent\",\"sacks\",\"sack\",\"barrels\",\"log\",\"logpile\",\"urn\",\"pot\",\"oildrum\",\"tank\",\"rope\",\""
    "chain\",\"cables\",\"casings\",\"coins\",\"eggs\",\"pustules\",\"orepile\",\"holes\",\"rocks\",\"boulder\",\"pebbles\",\"rubble\",\"shrubs\",\"bush\",\"crack\",\"prints\"],\"special\":[\"anchor\",\"ca"
    "bles\",\"casings\",\"coffin\",\"coins\",\"eggs\",\"fairy\",\"gear\",\"grate\",\"headstone\",\"nest\",\"oildrum\",\"pustules\",\"rifle\",\"sandbags\",\"tank\",\"teeth\",\"tentacle\",\"trapdoor\",\"vent"
    "\",\"wrench\"],\"paths\":[\"ruts\",\"trail\",\"track\",\"boards\",\"stream\",\"stones\",\"way\",\"pipe\",\"cable\",\"veins\"],\"path_w\":{\"ruts\":1.6,\"trail\":2.2,\"track\":1.9,\"boards\":2.0,\"stre"
    "am\":2.2,\"stones\":1.9,\"way\":2.3,\"pipe\":1.2,\"cable\":1.5,\"veins\":1.2},\"path_says\":{\"ruts\":\"wagon ruts\",\"trail\":\"a worn footpath\",\"track\":\"a rail track\",\"boards\":\"a boardwalk\""
    ",\"stream\":\"a stream of the style's liquid\",\"stones\":\"stepping stones\",\"way\":\"a paved way\",\"pipe\":\"a pipe run\",\"cable\":\"cables or a fuse\",\"veins\":\"a thick vein or root\",\"none\""
    ":\"no path\"},\"minis\":{\"biped\":{\"pads\":[[0,-0.11,0.07],[0,0.11,0.07]],\"reach\":0.05,\"says\":\"a figure on two legs. It needs two flat, clear foot spots side by side\"},\"wide\":{\"pads\":[[0,-"
    "0.2,0.085],[0,0.2,0.085]],\"reach\":0.07,\"says\":\"a very fat or very broad figure on two legs set far apart. It needs two big flat foot spots wide apart, and its belly hangs low between them\"},\"wa"
    "lker\":{\"pads\":[[0,-0.14,0.085,0.065],[0,0.14,0.085,0.065]],\"reach\":0.06,\"says\":\"a two-legged robot or walker with big flat feet. It needs two large flat foot plates\"},\"tripod\":{\"pads\":[[0"
    ".17,0,0.06],[-0.085,-0.147,0.06],[-0.085,0.147,0.06]],\"reach\":0.05,\"says\":\"a creature or machine on three legs. It needs three flat foot spots in a triangle; low things may sit between them\"},\""
    "quad\":{\"pads\":[[0.16,-0.1,0.055],[0.16,0.1,0.055],[-0.16,-0.1,0.055],[-0.16,0.1,0.055]],\"reach\":0.05,\"says\":\"a four-legged beast (horse, wolf, bear). It needs four flat foot spots in a long re"
    "ctangle; low things may pass under its belly\"},\"many\":{\"pads\":[[0.21610730456464244,0.07394414079890914,0.035],[0.09803698581219777,0.15516986984428857,0.035],[-0.09768796959822372,0.155280271428"
    "0083,0.035],[-0.21594077069134593,0.07418784396722147,0.035],[-0.21627329027205325,-0.07370025006796202,0.035],[-0.09838575335095334,-0.15505907466522367,0.035],[0.09733870559432657,-0.155390279136344"
    "2,0.035],[0.21577368907458427,-0.07443135895473497,0.035]],\"reach\":0.04,\"says\":\"a creature on many legs (spider, insect, crab). It needs eight small flat spots in a ring; its body hangs over the "
    "middle, so only low things go there\"},\"scorpion\":{\"pads\":[[0.16506712298193568,0.0846963710092553,0.035],[0.06306447247905374,0.14234769290333793,0.035],[-0.06276211187177645,0.14242284222969456,"
    "0.035],[-0.16488705773544446,0.08489343456730541,0.035],[-0.16524676952825446,-0.0844990926150555,0.035],[-0.0633666731204637,-0.14227218250566384,0.035],[0.06245959206558316,-0.1424976302941141,0.035"
    "],[0.1647065742455244,-0.08509028278934529,0.035],[0.3,-0.13,0.045],[0.3,0.13,0.045]],\"reach\":0.05,\"says\":\"a giant scorpion: eight legs in a ring, two claws reaching forward, a tail arched over i"
    "ts back. It needs small flat spots for the legs and room in front for the claws\"},\"crawler\":{\"pads\":[[0,0,0.25,0.075]],\"reach\":0.05,\"says\":\"a long low animal on its belly (crocodile, lizard,"
    " snake, slug). It needs one long flat strip to lie on; nothing may stick up under it\"},\"tracked\":{\"pads\":[[0,-0.12,0.18,0.045],[0,0.12,0.18,0.045]],\"reach\":0.04,\"says\":\"a tracked or wheeled "
    "machine (tank, crawler robot). It needs two long flat strips side by side for its tracks\"},\"hover\":{\"pads\":[[0,0,0.045]],\"reach\":0.0,\"says\":\"a flying or hovering figure on a thin flight stem"
    ". It only needs one small flat spot for the stem, so the rest of the base can be as wild, tall and crowded as you like\"},\"none\":{\"pads\":[],\"reach\":0.0,\"says\":\"nothing in particular\"}},\"the"
    "mes0\":{\"mine\":[{\"theme\":\"tunnel mouth\",\"subject\":\"portal\",\"path\":\"track:to\",\"props\":\"path:cart wall:timbers*2 path:pick path:shovel yard:orepile front:lantern back:rubble*2\",\"also\""
    ":\"depot:path\"},{\"theme\":\"shaft head\",\"subject\":\"shaft\",\"path\":\"trail:to\",\"props\":\"front:ladder at:keg back:crates at:rope wall:timbers yard:orepile\",\"also\":\"camp:far\"},{\"theme\""
    ":\"blast site\",\"subject\":\"crater\",\"path\":\"cable:to\",\"props\":\"start:crate at:holes*2 at:rubble*3 path:pick far:keg\",\"also\":\"depot:far\",\"clutter\":{\"debris\":2.0}},{\"theme\":\"ore si"
    "ding\",\"subject\":\"depot\",\"path\":\"track:past\",\"props\":\"path:cart path:orepile*2 path:shovel wall:barrels start:beam\",\"also\":\"shaft:far\"},{\"theme\":\"miners' camp\",\"subject\":\"camp\""
    ",\"path\":\"trail:to\",\"props\":\"at:crates at:keg at:pick at:shovel at:lantern back:timbers\",\"also\":\"shaft:far\"}],\"swamp\":[{\"theme\":\"sunken shrine\",\"subject\":\"ruin\",\"path\":\"stones:"
    "to\",\"props\":\"at:drum*2 in:runes back:shrubs*2 front:skull at:stub\",\"also\":\"\",\"ban\":[\"crate\",\"iron\"]},{\"theme\":\"boat landing\",\"subject\":\"hull\",\"path\":\"boards:to\",\"props\":\""
    "path:posts at:rope path:crate at:spear back:shrubs*2\",\"also\":\"camp:far\"},{\"theme\":\"hanging tree\",\"subject\":\"tree\",\"path\":\"veins:past\",\"props\":\"at:fairy wall:bones at:stump at:skull"
    "\",\"also\":\"\",\"pal\":{\"vein\":\"root\"},\"ban\":[\"crate\",\"iron\"]},{\"theme\":\"serpent pool\",\"subject\":\"pool\",\"path\":\"stream:to\",\"props\":\"at:eggs*2 at:nest at:tentacle back:shrubs"
    "*2\",\"also\":\"\",\"ban\":[\"crate\",\"iron\"]},{\"theme\":\"hunters' hide\",\"subject\":\"camp\",\"path\":\"boards:to\",\"props\":\"at:spear back:carcass path:posts at:sacks\",\"also\":\"\"}],\"cynd"
    "er\":[{\"theme\":\"lava vent\",\"subject\":\"crater\",\"path\":\"stream:to\",\"props\":\"at:crystals*2 at:rubble*2 far:crack\",\"also\":\"\",\"pal\":{\"pit\":\"lava\"},\"clutter\":{\"debris\":1.5,\"gr"
    "owth\":0.0},\"ban\":[\"crate\",\"wood\"]},{\"theme\":\"forge altar\",\"subject\":\"dais_brazier\",\"path\":\"way:to\",\"props\":\"front:slab at:chain wall:pick at:rubble*2 path:brazier\",\"also\":\"\""
    "},{\"theme\":\"obsidian field\",\"subject\":\"spires\",\"path\":\"trail:to\",\"props\":\"path:pick path:crate path:crystals*2 at:crystals\",\"also\":\"\"},{\"theme\":\"ash burial\",\"subject\":\"skele"
    "ton\",\"path\":\"none\",\"props\":\"at:ribs front:runes at:brazier*2\",\"also\":\"\",\"ban\":[\"crate\",\"wood\",\"iron\"]},{\"theme\":\"rail to the deep\",\"subject\":\"portal\",\"path\":\"track:to\""
    ",\"props\":\"path:cart wall:timbers front:brazier path:pick\",\"also\":\"\"}],\"targa\":[{\"theme\":\"frozen gate\",\"subject\":\"portal\",\"path\":\"way:to\",\"props\":\"front:stub*2 wall:crystals*2 "
    "back:boulder\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"power core\",\"subject\":\"machine\",\"path\":\"pipe:to\",\"props\":\"at:vent*2 at:cables at:grate back:crystals\",\"also\":\"\"},{\"them"
    "e\":\"crashed pod\",\"subject\":\"vehicle\",\"path\":\"trail:to\",\"props\":\"at:tiles*2 wall:crystals*2 path:slab\",\"also\":\"\"},{\"theme\":\"ice garden\",\"subject\":\"spires\",\"path\":\"stones:t"
    "o\",\"props\":\"front:runes at:boulder*2 at:crystals\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"hall of pillars\",\"subject\":\"colonnade\",\"path\":\"way:past\",\"props\":\"at:drum at:slab*2 b"
    "ack:crystals\",\"also\":\"\",\"ban\":[\"crate\"]}],\"trederra\":[{\"theme\":\"gun pit\",\"subject\":\"emplacement\",\"path\":\"boards:to\",\"props\":\"at:casings*2 back:crates at:sandbags back:posts*2"
    "\",\"also\":\"\"},{\"theme\":\"shell hole\",\"subject\":\"crater\",\"path\":\"trail:past\",\"props\":\"path:rifle at:lid back:posts front:ladder\",\"also\":\"\",\"pal\":{\"pit\":\"water\"},\"clutter\""
    ":{\"debris\":1.5}},{\"theme\":\"dugout entrance\",\"subject\":\"portal\",\"path\":\"boards:to\",\"props\":\"wall:sandbags*2 path:sign wall:crates front:lantern\",\"also\":\"\"},{\"theme\":\"supply dum"
    "p\",\"subject\":\"depot\",\"path\":\"ruts:to\",\"props\":\"path:cart wall:barrels wall:oildrum*2 at:shovel at:crates at:sacks\",\"also\":\"tent:far\"},{\"theme\":\"field graves\",\"subject\":\"graves\""
    ",\"path\":\"trail:to\",\"props\":\"at:rifle*2 back:posts path:shovel\",\"also\":\"\",\"ban\":[\"crate\"]}],\"fortress\":[{\"theme\":\"garden shrine\",\"subject\":\"dais_altar\",\"path\":\"stones:to\","
    "\"props\":\"path:lantern*2 back:boulder*2 far:puddle\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"gatehouse\",\"subject\":\"portal\",\"path\":\"way:to\",\"props\":\"wall:spear*2 front:brazier pat"
    "h:posts\",\"also\":\"\"},{\"theme\":\"koi pond\",\"subject\":\"pool\",\"path\":\"stones:past\",\"props\":\"at:lantern at:boulder*2 at:planks\",\"also\":\"\",\"ban\":[\"crate\",\"iron\"]},{\"theme\":\""
    "armoury\",\"subject\":\"depot\",\"path\":\"way:to\",\"props\":\"at:posts wall:chest at:lid wall:sword wall:spear\",\"also\":\"\"},{\"theme\":\"dojo floor\",\"subject\":\"ruin\",\"path\":\"way:to\",\"p"
    "rops\":\"wall:sword wall:spear in:brazier in:urn*2\",\"also\":\"\",\"pal\":{\"floor\":\"tatami\",\"stone\":\"wood\"}}],\"beast\":[{\"theme\":\"feeding pit\",\"subject\":\"tentacles\",\"path\":\"veins:"
    "past\",\"props\":\"at:bones*2 at:carcass at:teeth\",\"also\":\"\",\"pal\":{\"pit\":\"bile\"}},{\"theme\":\"the heart\",\"subject\":\"organ\",\"path\":\"veins:to\",\"props\":\"at:pustules*2 at:eggs at:"
    "puddle*2\",\"also\":\"\"},{\"theme\":\"swallowed wreck\",\"subject\":\"vehicle\",\"path\":\"stream:past\",\"props\":\"at:crate*2 at:bones back:pustules\",\"also\":\"\",\"pal\":{\"wood\":\"iron\"}},{\""
    "theme\":\"brood nest\",\"subject\":\"nest\",\"path\":\"veins:to\",\"props\":\"at:eggs*3 at:pustules*2\",\"also\":\"\",\"pal\":{\"twig\":\"flesh\"}},{\"theme\":\"jaw line\",\"subject\":\"wall\",\"path\""
    ":\"none\",\"props\":\"wall:teeth*3 back:pustules*2 front:puddle\",\"also\":\"\",\"pal\":{\"stone\":\"tooth\"}}],\"badlands\":[{\"theme\":\"dry camp\",\"subject\":\"camp\",\"path\":\"trail:to\",\"props"
    "\":\"at:sacks at:rifle at:wheel at:keg\",\"also\":\"\"},{\"theme\":\"boot hill\",\"subject\":\"graves\",\"path\":\"trail:to\",\"props\":\"at:shovel back:posts start:sign\",\"also\":\"\",\"ban\":[\"cra"
    "te\"]},{\"theme\":\"broken wagon\",\"subject\":\"wagon\",\"path\":\"ruts:past\",\"props\":\"at:wheel path:crate*2 path:barrel far:bones\",\"also\":\"\"},{\"theme\":\"old diggings\",\"subject\":\"shaft"
    "\",\"path\":\"track:to\",\"props\":\"path:pick yard:orepile path:cart wall:timbers\",\"also\":\"\"},{\"theme\":\"carcass in the wash\",\"subject\":\"skeleton\",\"path\":\"stream:past\",\"props\":\"at:"
    "skull in:nest path:shrubs*2\",\"also\":\"\",\"pal\":{\"water\":\"wash\"},\"ban\":[\"crate\",\"wood\"]}],\"highland\":[{\"theme\":\"standing stones\",\"subject\":\"circle\",\"path\":\"trail:to\",\"prop"
    "s\":\"at:drum at:fairy far:shrubs*2\",\"also\":\"\",\"pal\":{\"stone\":\"standing\"},\"ban\":[\"crate\"]},{\"theme\":\"sheepfold\",\"subject\":\"corral\",\"path\":\"trail:to\",\"props\":\"in:keg start"
    ":posts path:cart\",\"also\":\"\",\"pal\":{\"wood\":\"slate\"}},{\"theme\":\"ruined tower\",\"subject\":\"ruin\",\"path\":\"stones:to\",\"props\":\"at:rubble*3 front:sword in:campfire\",\"also\":\"\"},"
    "{\"theme\":\"burn crossing\",\"subject\":\"bridge\",\"path\":\"stones:past\",\"props\":\"path:shrubs*2 at:boulder*2 start:boulder\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"hill camp\",\"subjec"
    "t\":\"camp\",\"path\":\"trail:to\",\"props\":\"at:axe at:stump at:logpile at:lid\",\"also\":\"\"}],\"maahl\":[{\"theme\":\"crystal well\",\"subject\":\"spires\",\"path\":\"stones:to\",\"props\":\"fron"
    "t:runes path:crystals*2 path:spear\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"buried temple\",\"subject\":\"colonnade\",\"path\":\"way:past\",\"props\":\"at:drum*2 back:slab at:stub at:pot\",\""
    "also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"altar of sand\",\"subject\":\"dais_altar\",\"path\":\"way:to\",\"props\":\"path:brazier*2 front:bones at:urn*2 at:tiles*2\",\"also\":\"\",\"ban\":[\"crate"
    "\"]},{\"theme\":\"bones of the giant\",\"subject\":\"skeleton\",\"path\":\"none\",\"props\":\"in:crystals*2 at:pothole at:shovel\",\"also\":\"\"},{\"theme\":\"ruin wall\",\"subject\":\"wall\",\"path\""
    ":\"trail:past\",\"props\":\"wall:rubble*3 back:shrubs*2 front:steps\",\"also\":\"\",\"ban\":[\"crate\"]}],\"corridor\":[{\"theme\":\"airlock\",\"subject\":\"portal\",\"path\":\"way:to\",\"props\":\"pa"
    "th:crates wall:crate front:stub at:cables\",\"also\":\"\"},{\"theme\":\"reactor bay\",\"subject\":\"machine\",\"path\":\"pipe:to\",\"props\":\"at:grate*2 at:vent at:wrench at:puddle back:tank\",\"also"
    "\":\"\"},{\"theme\":\"cargo hold\",\"subject\":\"depot\",\"path\":\"track:past\",\"props\":\"path:cart at:crates*2 at:ladder wall:oildrum*2 wall:tank\",\"also\":\"\"},{\"theme\":\"breach\",\"subject\""
    ":\"crater\",\"path\":\"cable:to\",\"props\":\"at:tiles*2 at:crack*2 at:slab*2\",\"also\":\"\",\"pal\":{\"pit\":\"slot\"},\"clutter\":{\"debris\":1.5}},{\"theme\":\"maintenance pit\",\"subject\":\"shaf"
    "t\",\"path\":\"pipe:past\",\"props\":\"front:ladder at:wrench at:gear at:grate\",\"also\":\"\"}],\"infestation\":[{\"theme\":\"hive mouth\",\"subject\":\"tentacles\",\"path\":\"veins:to\",\"props\":\""
    "at:eggs*2 at:pustules*2 path:rifle\",\"also\":\"\",\"pal\":{\"pit\":\"acid\"}},{\"theme\":\"egg chamber\",\"subject\":\"nest\",\"path\":\"veins:past\",\"props\":\"at:eggs*2 path:eggs*2 far:puddle*2\","
    "\"also\":\"\",\"pal\":{\"twig\":\"flesh\"}},{\"theme\":\"overrun cargo\",\"subject\":\"depot\",\"path\":\"veins:to\",\"props\":\"at:crates at:tentacle*2 front:skull\",\"also\":\"\"},{\"theme\":\"acid "
    "pool\",\"subject\":\"pool\",\"path\":\"stream:to\",\"props\":\"at:bones*2 at:crystals at:pustules\",\"also\":\"\"},{\"theme\":\"cocooned wreck\",\"subject\":\"vehicle\",\"path\":\"cable:to\",\"props\""
    ":\"wall:pustules*2 at:ribs in:eggs*2\",\"also\":\"\",\"pal\":{\"wood\":\"carapace\"}}],\"urban\":[{\"theme\":\"bomb site\",\"subject\":\"crater\",\"path\":\"cable:past\",\"props\":\"at:slab*2 at:rubbl"
    "e*3 path:sign\",\"also\":\"\",\"pal\":{\"pit\":\"pothole\"},\"clutter\":{\"debris\":2.0}},{\"theme\":\"barricade\",\"subject\":\"wall\",\"path\":\"none\",\"props\":\"wall:sandbags*2 back:oildrum*2 bac"
    "k:casings*2 wall:crates\",\"also\":\"\"},{\"theme\":\"tram stop\",\"subject\":\"depot\",\"path\":\"track:past\",\"props\":\"path:sign path:planks at:grate at:wheel\",\"also\":\"\"},{\"theme\":\"collap"
    "sed shopfront\",\"subject\":\"ruin\",\"path\":\"way:past\",\"props\":\"front:rubble*3 path:beam front:tiles\",\"also\":\"\",\"pal\":{\"stone\":\"brick\"}},{\"theme\":\"open manhole\",\"subject\":\"sha"
    "ft\",\"path\":\"pipe:to\",\"props\":\"at:lid front:ladder at:posts at:wrench\",\"also\":\"\",\"pal\":{\"wood\":\"iron\"}}],\"frontier\":[{\"theme\":\"water stop\",\"subject\":\"well\",\"path\":\"ruts:"
    "past\",\"props\":\"at:keg at:posts path:prints*2 at:barrel\",\"also\":\"\"},{\"theme\":\"broken wagon\",\"subject\":\"wagon\",\"path\":\"ruts:past\",\"props\":\"at:wheel path:crate*2 path:barrel at:ri"
    "fle\",\"also\":\"\"},{\"theme\":\"corral\",\"subject\":\"corral\",\"path\":\"trail:to\",\"props\":\"in:keg at:sacks start:posts at:rope\",\"also\":\"\"},{\"theme\":\"storefront\",\"subject\":\"portal\""
    ",\"path\":\"boards:past\",\"props\":\"wall:barrels wall:sacks wall:crates path:sign wall:planks\",\"also\":\"\",\"pal\":{\"stone\":\"board\"}},{\"theme\":\"prospector's camp\",\"subject\":\"camp\",\"p"
    "ath\":\"trail:to\",\"props\":\"at:pick at:lid at:sacks at:shovel\",\"also\":\"tent:at\"}],\"graveyard\":[{\"theme\":\"fresh grave\",\"subject\":\"graves\",\"path\":\"trail:to\",\"props\":\"at:shovel a"
    "t:coffin at:lantern\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"crypt door\",\"subject\":\"portal\",\"path\":\"way:to\",\"props\":\"front:urn*2 path:posts*2 wall:stub\",\"also\":\"\"},{\"theme\""
    ":\"fenced plot\",\"subject\":\"corral\",\"path\":\"stones:to\",\"props\":\"in:headstone*2 start:posts far:stump\",\"also\":\"\",\"pal\":{\"wood\":\"fence\"}},{\"theme\":\"hanging tree\",\"subject\":\""
    "tree\",\"path\":\"trail:to\",\"props\":\"at:coffin at:rope wall:fairy\",\"also\":\"\",\"pal\":{\"wood\":\"wood\"}},{\"theme\":\"robbed tomb\",\"subject\":\"dais_altar\",\"path\":\"trail:to\",\"props\""
    ":\"front:slab path:bones*2 at:pot at:spear\",\"also\":\"\"}],\"forest\":[{\"theme\":\"fallen giant\",\"subject\":\"log\",\"path\":\"trail:past\",\"props\":\"wall:fairy*2 wall:shrubs*2 back:bush\",\"al"
    "so\":\"\",\"ban\":[\"crate\",\"iron\"]},{\"theme\":\"woodcutters' clearing\",\"subject\":\"camp\",\"path\":\"trail:to\",\"props\":\"at:axe at:logpile*2 at:log at:stump*2 far:stump\",\"also\":\"\"},{\""
    "theme\":\"stream ford\",\"subject\":\"bridge\",\"path\":\"stones:past\",\"props\":\"path:shrubs*2 at:boulder*2 far:prints\",\"also\":\"\",\"ban\":[\"crate\",\"iron\"]},{\"theme\":\"old shrine\",\"subj"
    "ect\":\"circle\",\"path\":\"stones:to\",\"props\":\"in:lid at:fairy far:fairy wall:shrubs\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"bear den\",\"subject\":\"portal\",\"path\":\"trail:to\",\"pr"
    "ops\":\"front:bones*2 path:crack*2 front:carcass\",\"also\":\"\",\"pal\":{\"stone\":\"rock\"},\"ban\":[\"crate\",\"iron\"]}],\"dungeon\":[{\"theme\":\"cell door\",\"subject\":\"portal\",\"path\":\"way"
    ":to\",\"props\":\"at:chain*2 front:keg wall:bones*2 path:lid\",\"also\":\"\"},{\"theme\":\"pit trap\",\"subject\":\"shaft\",\"path\":\"none\",\"props\":\"at:trapdoor at:spear wall:bones far:puddle\",\""
    "also\":\"\",\"pal\":{\"wood\":\"flag\"}},{\"theme\":\"torture chamber\",\"subject\":\"dais_altar\",\"path\":\"way:to\",\"props\":\"at:brazier at:chain*2 at:sword path:grate\",\"also\":\"\"},{\"theme\""
    ":\"treasure hoard\",\"subject\":\"depot\",\"path\":\"way:to\",\"props\":\"at:coins*3 at:sword at:lid front:skull wall:chest at:urn\",\"also\":\"\"},{\"theme\":\"summoning circle\",\"subject\":\"circle"
    "\",\"path\":\"way:to\",\"props\":\"at:brazier*3 front:stub in:runes\",\"also\":\"\",\"ban\":[\"crate\"]}],\"temple\":[{\"theme\":\"high altar\",\"subject\":\"dais_altar\",\"path\":\"way:to\",\"props\""
    ":\"path:brazier*2 front:steps at:urn*2\",\"also\":\"\",\"clutter\":{\"growth\":0.2},\"ban\":[\"crate\"]},{\"theme\":\"fallen idol\",\"subject\":\"statue\",\"path\":\"way:to\",\"props\":\"path:slab*2 a"
    "t:coins at:drum\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"processional way\",\"subject\":\"colonnade\",\"path\":\"way:past\",\"props\":\"path:drum path:brazier*2\",\"also\":\"\",\"ban\":[\"cra"
    "te\"]},{\"theme\":\"sacred pool\",\"subject\":\"pool\",\"path\":\"stones:to\",\"props\":\"at:steps at:stub*4 at:shrubs\",\"also\":\"\",\"ban\":[\"crate\"]},{\"theme\":\"sealed vault\",\"subject\":\"po"
    "rtal\",\"path\":\"way:to\",\"props\":\"front:slab path:pick front:carcass wall:runes\",\"also\":\"\"}],\"coast\":[{\"theme\":\"shipwreck\",\"subject\":\"hull\",\"path\":\"trail:to\",\"props\":\"at:che"
    "st at:anchor at:barrel*2 at:rope\",\"also\":\"\"},{\"theme\":\"jetty\",\"subject\":\"portal\",\"path\":\"boards:to\",\"props\":\"path:posts*2 path:rope path:crates at:anchor\",\"also\":\"\",\"pal\":{\""
    "stone\":\"drift\"}},{\"theme\":\"tide pool\",\"subject\":\"pool\",\"path\":\"stream:to\",\"props\":\"at:pebbles*2 at:shrubs*2 at:nest\",\"also\":\"\",\"ban\":[\"crate\",\"iron\"]},{\"theme\":\"beach c"
    "amp\",\"subject\":\"camp\",\"path\":\"trail:to\",\"props\":\"at:timbers at:posts at:anchor at:barrel at:sacks\",\"also\":\"\"},{\"theme\":\"whale bones\",\"subject\":\"skeleton\",\"path\":\"none\",\"p"
    "rops\":\"wall:shrubs*2 back:pebbles*2 at:posts\",\"also\":\"\",\"ban\":[\"crate\",\"iron\"]}]},\"new\":[{\"NAME\":\"ashveil\",\"TITLE\":\"Ashveil Wastes\",\"SAYS\":\"grey ash plains of a dead world, w"
    "ith half-buried wrecked transports, broken pipelines on stilts, bleached bones and scrap\",\"GROUND\":\"#8a8580\",\"PAL\":{\"wood\":\"ashscrap\",\"iron\":\"rust\",\"rock\":\"ashcinder\",\"stone\":\"as"
    "hstone\",\"crate\":\"ashcargo\",\"water\":\"tar\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"ashstone\",\"plant\":null,\"soft\":\"ashdrift\",\"bone\":\"bone\",\"twig\":\"ashscrap\",\"vein\":\"ashpipe\"}"
    ",\"PATH_OK\":\"trail ruts pipe cable stones track\",\"EXTRA_PROPS\":\"oildrum tank gear grate wrench cables casings rifle\",\"BUILT\":false,\"ENCLOSED\":false,\"CLIFFS\":true,\"COLOURS\":[[\"ashdrift\""
    ",\"#a39e97\"],[\"ashstone\",\"#77736d\"],[\"ashcinder\",\"#4f4c49\"],[\"ashscrap\",\"#73625a\"],[\"ashhull\",\"#5f5a55\"],[\"ashcargo\",\"#6d6a5f\"],[\"ashpipe\",\"#7d7b76\"],[\"ashpylon\",\"#8e8a82\""
    "],[\"ashtarp\",\"#857a66\"],[\"ashpaint\",\"#b89a3c\"]],\"THEMES\":[{\"theme\":\"buried transport\",\"subject\":\"vehicle\",\"path\":\"ruts:past\",\"props\":\"at:wheel at:crate*2 at:tank back:rubble*2"
    " far:bones\",\"also\":\"depot:far\",\"clutter\":{\"debris\":2.2,\"growth\":0.0},\"ban\":[\"ashpipe\",\"ashpylon\",\"ashtarp\"]},{\"theme\":\"pipeline break\",\"subject\":\"machine\",\"path\":\"pipe:pa"
    "st\",\"props\":\"at:oildrum*2 at:puddle*2 at:wrench path:cables back:rubble*2\",\"also\":\"crater:far\",\"clutter\":{\"debris\":2.2,\"growth\":0.0},\"ban\":[\"ashhull\",\"ashpylon\",\"ashtarp\"]},{\"t"
    "heme\":\"scavengers' camp\",\"subject\":\"camp\",\"path\":\"trail:to\",\"props\":\"at:crates at:sacks at:oildrum at:rifle at:wrench back:timbers\",\"also\":\"tent:at\",\"clutter\":{\"debris\":2.2,\"gr"
    "owth\":0.0},\"ban\":[\"ashpipe\",\"ashpylon\"]},{\"theme\":\"marker pylon\",\"subject\":\"dais_column\",\"path\":\"stones:to\",\"props\":\"front:sign path:posts*2 at:cables at:skull far:bones*2\",\"al"
    "so\":\"\",\"clutter\":{\"debris\":2.2,\"growth\":0.0},\"ban\":[\"ashhull\",\"ashpipe\",\"ashtarp\"]},{\"theme\":\"dust-choked crater\",\"subject\":\"crater\",\"path\":\"trail:past\",\"props\":\"at:rub"
    "ble*3 at:slab*3 at:ribs far:bones at:skull back:boulder*2\",\"also\":\"\",\"clutter\":{\"debris\":2.6,\"growth\":0.0},\"ban\":[\"ashtarp\",\"ashpylon\"]}]},{\"NAME\":\"blight\",\"TITLE\":\"The Blight\""
    ",\"SAYS\":\"ground corrupted by something unnatural: twisted spikes and thorns of rock, heaps of bones, bubbling pools of ichor, cracked rune circles of an unknown script, fleshy lumpy rock and rifts "
    "torn open in the ground\",\"GROUND\":\"#6a6152\",\"PAL\":{\"wood\":\"bone\",\"iron\":\"iron\",\"rock\":\"blightrock\",\"stone\":\"blightrock\",\"crate\":\"crate\",\"water\":\"ichor\",\"pit\":\"rift\","
    "\"gem\":null,\"floor\":\"blightrock\",\"plant\":null,\"soft\":\"fleshrock\",\"bone\":\"bone\",\"twig\":\"bone\",\"vein\":\"tendril\"},\"PATH_OK\":\"trail stones way stream\",\"EXTRA_PROPS\":\"tentacle"
    " teeth pustules eggs\",\"BUILT\":false,\"ENCLOSED\":false,\"CLIFFS\":false,\"COLOURS\":[[\"blightrock\",\"#584a54\"],[\"thorn\",\"#2a2230\"],[\"fleshrock\",\"#8c5a60\"],[\"blister\",\"#b48a6a\"],[\"ru"
    "nestone\",\"#6a6270\"],[\"tendril\",\"#7a3b4a\"],[\"pustule\",\"#c9a35a\"]],\"THEMES\":[{\"theme\":\"spike garden\",\"subject\":\"blight_thorns\",\"path\":\"trail:to\",\"props\":\"at:rubble*3 front:sk"
    "ull at:bones*2 far:crack*2 back:pebbles\",\"also\":\"crater:far\",\"clutter\":{\"debris\":2.6,\"growth\":1.6,\"marks\":1.4},\"ban\":[\"crate\",\"wood\"]},{\"theme\":\"bone altar\",\"subject\":\"dais_a"
    "ltar\",\"path\":\"way:to\",\"props\":\"front:skull at:bones*3 at:ribs path:brazier*2 back:rubble*2\",\"also\":\"skeleton:far\",\"clutter\":{\"debris\":2.0,\"growth\":1.6,\"marks\":1.4},\"ban\":[\"crat"
    "e\"]},{\"theme\":\"boiling pool\",\"subject\":\"pool\",\"path\":\"stones:to\",\"props\":\"at:pustules*2 at:puddle*2 at:bones at:rubble far:crack\",\"also\":\"crater:far\",\"clutter\":{\"debris\":1.8,\""
    "growth\":2.0,\"marks\":1.6},\"ban\":[\"crate\",\"wood\"]},{\"theme\":\"rune circle\",\"subject\":\"blight_runes\",\"path\":\"stones:to\",\"props\":\"at:brazier*2 at:slab front:skull at:rubble*2 far:cr"
    "ack*2\",\"also\":\"\",\"pal\":{\"stone\":\"runestone\"},\"clutter\":{\"debris\":2.0,\"growth\":1.6,\"marks\":1.6},\"ban\":[\"crate\"]},{\"theme\":\"rift in the ground\",\"subject\":\"blight_rift\",\"p"
    "ath\":\"trail:past\",\"props\":\"at:crack*3 at:rubble*3 at:tentacle at:bones far:skull\",\"also\":\"\",\"pal\":{\"rock\":\"thorn\"},\"clutter\":{\"debris\":2.4,\"growth\":1.6,\"marks\":1.6},\"ban\":[\""
    "crate\",\"wood\"]}]},{\"NAME\":\"ferrum\",\"TITLE\":\"Ferrum Works\",\"SAYS\":\"a red, rusted factory world: riveted iron deck plates laid on red iron dust, gear wheels half sunk in it, cable bundles,"
    " vent stacks, pipe runs, broken machinery and heaps of black slag\",\"GROUND\":\"#8a3f26\",\"PAL\":{\"wood\":\"rust\",\"iron\":\"iron\",\"rock\":\"slag\",\"stone\":\"girder\",\"crate\":\"rust\",\"wate"
    "r\":\"tar\",\"pit\":\"pit\",\"gem\":null,\"floor\":\"plate\",\"plant\":null,\"soft\":\"dust\",\"bone\":\"bone\",\"twig\":\"cable\",\"vein\":\"cable\"},\"PATH_OK\":\"way cable pipe track ruts trail\",\""
    "EXTRA_PROPS\":\"oildrum tank gear grate wrench vent cables casings\",\"BUILT\":true,\"ENCLOSED\":false,\"CLIFFS\":true,\"COLOURS\":[[\"slag\",\"#3d3431\"],[\"girder\",\"#5e3526\"],[\"dust\",\"#9c4a2c\""
    "],[\"oxide\",\"#b35a2c\"]],\"THEMES\":[{\"theme\":\"gear pit\",\"subject\":\"crater\",\"path\":\"cable:to\",\"props\":\"at:gear*2 in:gear at:rubble*2 front:wrench back:oildrum far:crack\",\"also\":\"m"
    "achine:far\",\"clutter\":{\"debris\":1.5}},{\"theme\":\"cable run\",\"subject\":\"wall\",\"path\":\"cable:past\",\"props\":\"wall:cables*2 at:grate path:cables front:lid back:oildrum*2 at:wrench\",\"a"
    "lso\":\"depot:far\"},{\"theme\":\"vent stack\",\"subject\":\"machine\",\"path\":\"pipe:to\",\"props\":\"at:vent*2 at:grate back:tank at:cables front:lid\",\"also\":\"shaft:far\"},{\"theme\":\"fallen w"
    "ork-machine\",\"subject\":\"vehicle\",\"path\":\"ruts:to\",\"props\":\"at:gear*2 at:wheel path:wrench at:cables back:rubble*2 far:oildrum\",\"also\":\"crater:far\",\"clutter\":{\"debris\":1.4}},{\"the"
    "me\":\"slag heap\",\"subject\":\"spires\",\"path\":\"track:past\",\"props\":\"path:cart at:rubble*3 at:rocks wall:oildrum far:puddle\",\"also\":\"depot:far\",\"pal\":{\"gem\":\"slag\"},\"clutter\":{\""
    "debris\":1.8}}]},{\"NAME\":\"obelis\",\"TITLE\":\"Obelis Tombs\",\"SAYS\":\"an ancient alien tomb world: sleek black monoliths on stepped plinths, slabs cut with straight geometric glyph lines, half-b"
    "uried pylons, broken panels of smooth metal, sealed caskets, sand drifting over polished stone. Clean angular shapes only: chamfered blocks, slots, steps and rings\",\"GROUND\":\"#a89270\",\"PAL\":{\""
    "wood\":\"obmetal\",\"iron\":\"obmetal\",\"rock\":\"obsidian\",\"stone\":\"monolith\",\"crate\":\"casket\",\"water\":\"water\",\"pit\":\"slot\",\"gem\":\"obglow\",\"floor\":\"obfloor\",\"plant\":null,\""
    "soft\":\"sand\",\"bone\":\"bone\",\"twig\":\"obmetal\",\"vein\":\"obmetal\"},\"PATH_OK\":\"way stones trail cable\",\"EXTRA_PROPS\":\"grate vent cables coins\",\"BUILT\":true,\"ENCLOSED\":false,\"CLIF"
    "FS\":true,\"COLOURS\":[[\"monolith\",\"#1e1f26\"],[\"obfloor\",\"#4b4954\"],[\"obmetal\",\"#8d939e\"],[\"casket\",\"#34323c\"],[\"sand\",\"#cdb68a\"],[\"obglow\",\"#5fc4b8\"],[\"obinlay\",\"#b49a52\"]"
    "],\"THEMES\":[{\"theme\":\"tomb gate\",\"subject\":\"portal\",\"path\":\"way:to\",\"props\":\"front:steps wall:slab*2 back:tiles path:coins\",\"also\":\"graves:far\",\"clutter\":{\"growth\":0.0}},{\"t"
    "heme\":\"fallen monolith\",\"subject\":\"statue\",\"path\":\"stones:to\",\"props\":\"at:slab*2 at:rubble front:tiles far:grate\",\"also\":\"crater:far\",\"clutter\":{\"debris\":1.0,\"growth\":0.0}},{\""
    "theme\":\"pylon array\",\"subject\":\"colonnade\",\"path\":\"way:past\",\"props\":\"path:grate at:cables path:slab back:crystals\",\"also\":\"crater:far\",\"clutter\":{\"growth\":0.0}},{\"theme\":\"sa"
    "rcophagus field\",\"subject\":\"graves\",\"path\":\"way:to\",\"props\":\"at:slab at:chest back:tiles path:coins\",\"also\":\"statue:far\",\"clutter\":{\"growth\":0.0}},{\"theme\":\"glyph plaza\",\"sub"
    "ject\":\"dais_altar\",\"path\":\"way:to\",\"props\":\"front:tiles at:slab*2 front:steps back:grate\",\"also\":\"well:far\",\"clutter\":{\"growth\":0.0}}]},{\"NAME\":\"sundered\",\"TITLE\":\"Sundered C"
    "ity\",\"SAYS\":\"a gothic stone city shelled to rubble: cracked flagstones, broken fluted columns, fallen statues, shell craters\",\"GROUND\":\"#5a5249\",\"PAL\":{\"wood\":\"wood\",\"iron\":\"iron\",\""
    "rock\":\"rubble\",\"stone\":\"ashlar\",\"crate\":\"crate\",\"water\":\"water\",\"pit\":\"pothole\",\"floor\":\"flag\",\"plant\":null,\"soft\":\"spoil\",\"bone\":\"bone\"},\"PATH_OK\":\"way stones ruts"
    " cable pipe\",\"EXTRA_PROPS\":\"sandbags casings rifle oildrum grate cables\",\"BUILT\":true,\"ENCLOSED\":false,\"CLIFFS\":false,\"COLOURS\":[[\"ashlar\",\"#a6a196\"],[\"statue\",\"#c4bfb2\"],[\"spoil"
    "\",\"#77716a\"],[\"rubble\",\"#8d877e\"],[\"rust\",\"#7a4a32\"],[\"brass\",\"#b8923c\"]],\"THEMES\":[{\"theme\":\"fallen cathedral wall\",\"subject\":\"wall\",\"path\":\"way:past\",\"props\":\"front:r"
    "ubble*3 at:drum*2 path:slab back:stub front:tiles\",\"also\":\"statue:far\",\"clutter\":{\"debris\":1.8},\"ban\":[\"crate\"]},{\"theme\":\"statue toppled in the square\",\"subject\":\"statue\",\"path\""
    ":\"way:to\",\"props\":\"at:rubble*2 path:slab front:tiles*2 at:drum back:stub\",\"also\":\"crater:far\",\"clutter\":{\"debris\":1.4},\"ban\":[\"crate\"]},{\"theme\":\"shell crater in the street\",\"su"
    "bject\":\"crater\",\"path\":\"way:past\",\"props\":\"at:rubble*3 at:slab*2 path:casings far:tiles back:stub\",\"also\":\"wall:far\",\"pal\":{\"pit\":\"pothole\"},\"clutter\":{\"debris\":2.0}},{\"theme"
    "\":\"collapsed arcade\",\"subject\":\"colonnade\",\"path\":\"way:past\",\"props\":\"path:drum*2 at:rubble*2 at:slab front:stub far:tiles\",\"also\":\"crater:far\",\"clutter\":{\"debris\":1.6},\"ban\":"
    "[\"crate\"]},{\"theme\":\"barricaded doorway\",\"subject\":\"portal\",\"path\":\"way:to\",\"props\":\"front:sandbags*2 wall:crates path:casings at:beam front:rifle at:rubble*2\",\"also\":\"crater:far\""
    ",\"clutter\":{\"debris\":1.4}}]},{\"NAME\":\"thornwild\",\"TITLE\":\"Thornwild\",\"SAYS\":\"a deadly alien jungle: giant twisting roots, carnivorous plants with toothed jaws, spore pods, rings of glow"
    "ing fungi, thorn vines, and the bones and dropped gear of the unlucky\",\"GROUND\":\"#3c4629\",\"PAL\":{\"wood\":\"wood\",\"iron\":\"iron\",\"rock\":\"rock\",\"stone\":\"rock\",\"crate\":\"crate\",\"w"
    "ater\":\"water\",\"pit\":\"pit\",\"gem\":null,\"floor\":null,\"plant\":\"spikethorn\",\"soft\":\"moss\",\"vein\":\"root\",\"twig\":\"root\",\"bone\":\"bone\"},\"PATH_OK\":\"veins trail stream stones\""
    ",\"EXTRA_PROPS\":\"tentacle eggs fairy nest pustules rifle cables\",\"BUILT\":false,\"ENCLOSED\":false,\"CLIFFS\":false,\"COLOURS\":[[\"spikethorn\",\"#5b6130\"],[\"creeper\",\"#3b5a2a\"],[\"maw\",\"#"
    "9a3a55\"],[\"glowcap\",\"#8fdcc0\"],[\"sporepod\",\"#b39a3c\"],[\"pitcher\",\"#6c7e34\"],[\"hullplate\",\"#6b737a\"],[\"sundew\",\"#a04a4a\"],[\"seedburr\",\"#7a6a34\"],[\"rootbark\",\"#5a4430\"]],\"T"
    "HEMES\":[{\"theme\":\"root tangle\",\"subject\":\"tree\",\"path\":\"veins:past\",\"props\":\"at:tentacle*2 at:skull wall:shrubs*2 at:bones far:fairy\",\"also\":\"\",\"ban\":[\"crate\",\"metal\",\"hull"
    "plate\"]},{\"theme\":\"maw flower\",\"subject\":\"tentacles\",\"path\":\"trail:to\",\"props\":\"at:bones*2 at:skull front:rifle at:pustules back:shrubs*2\",\"also\":\"\",\"pal\":{\"pit\":\"bile\",\"ve"
    "in\":\"creeper\"},\"clutter\":{\"growth\":1.3}},{\"theme\":\"spore field\",\"subject\":\"nest\",\"path\":\"trail:past\",\"props\":\"at:pustules*2 at:eggs*2 path:prints far:puddle back:shrubs\",\"also\""
    ":\"\",\"pal\":{\"twig\":\"root\"},\"clutter\":{\"growth\":1.5},\"ban\":[\"crate\",\"hullplate\"]},{\"theme\":\"fungus ring\",\"subject\":\"circle\",\"path\":\"stones:to\",\"props\":\"in:bones at:fairy"
    "*2 far:fairy back:shrubs*2\",\"also\":\"\",\"pal\":{\"stone\":\"glowcap\"},\"clutter\":{\"growth\":1.3},\"ban\":[\"crate\",\"maw\",\"hullplate\"]},{\"theme\":\"overgrown wreck\",\"subject\":\"vehicle\""
    ",\"path\":\"trail:to\",\"props\":\"at:crates at:rifle at:tentacle*2 wall:shrubs*2 at:bones at:cables\",\"also\":\"nest:far\",\"pal\":{\"wood\":\"hullplate\"}}]}],\"features_install_col\":[[\"fire\",\""
    "#e2792b\"],[\"ash\",\"#4a4744\"],[\"twig\",\"#7b5a36\"],[\"rail\",\"#55585e\"],[\"hull\",\"#7a5a3c\"],[\"rope\",\"#b59b63\"],[\"sail\",\"#d9d1b8\"],[\"alien_p\",\"#5a7f8c\"],[\"flag_p\",\"#77726c\"],["
    "\"tflag_p\",\"#b8a888\"],[\"fleshfloor_p\",\"#8c4046\"],[\"board_p\",\"#94683c\"],[\"plate_p\",\"#56606e\"],[\"basalt_p\",\"#3b3534\"],[\"plank_p\",\"#8a5a32\"],[\"duck_p\",\"#7a5a36\"],[\"tatami_p\","
    "\"#b9b06a\"],[\"obfloor_p\",\"#4b4954\"],[\"shelf_p\",\"#6f6a64\"],[\"ice_p\",\"#bfe3ee\"]],\"details_install_col\":[[\"bone\",\"#e6dcc4\"],[\"glass\",\"#a9cfd6\"],[\"column\",\"#c9bfa8\"],[\"tendril\""
    ",\"#7a3b4a\"],[\"spine\",\"#d8c9a8\"],[\"egg\",\"#c7b37a\"],[\"pavers\",\"#b8ab94\"],[\"ruin\",\"#a89e8c\"],[\"obelisk\",\"#6e6a66\"],[\"slate\",\"#6f7377\"],[\"crate\",\"#a8743f\"],[\"post\",\"#6b6e7"
    "2\"],[\"metal\",\"#7d838c\"],[\"concrete\",\"#a3a09a\"],[\"brick\",\"#a5553a\"],[\"rubble\",\"#8f8a82\"],[\"wood\",\"#8a5a32\"],[\"iron\",\"#4d4f55\"],[\"quartz\",\"#e8e4dc\"],[\"moss\",\"#5d7a3a\"],["
    "\"heather\",\"#8a5a8a\"],[\"fern\",\"#52803c\"],[\"standing\",\"#80858b\"],[\"shrub\",\"#8c8248\"],[\"crystal\",\"#b9dcec\"],[\"salt\",\"#ece6d6\"],[\"pustule\",\"#c9a35a\"],[\"flesh\",\"#9c4a4a\"],[\""
    "carapace\",\"#4a3a2a\"],[\"sandstone\",\"#c9935a\"],[\"cable\",\"#3c3f45\"],[\"pipe\",\"#6f747c\"],[\"paint\",\"#e8e4d8\"],[\"plate\",\"#5b616b\"]],\"scenes_install_col\":[[\"sack\",\"#b8a47a\"],[\"sa"
    "il\",\"#d9d1b8\"],[\"fire\",\"#e2792b\"],[\"twig\",\"#7b5a36\"],[\"rope\",\"#b59b63\"],[\"chain\",\"#4a4c50\"],[\"gold\",\"#d4a72c\"],[\"earth\",\"#5a4634\"],[\"vein\",\"#5e2238\"],[\"root\",\"#4a3622"
    "\"],[\"egg\",\"#c7b37a\"],[\"cable\",\"#3c3f45\"],[\"pipe\",\"#6f747c\"],[\"sandbag\",\"#a8946a\"],[\"stump\",\"#7a5a3a\"],[\"clay\",\"#a8623c\"],[\"tendril\",\"#7a3b4a\"],[\"shroom\",\"#c9a27a\"],[\""
    "tooth\",\"#e8dcc0\"],[\"fence\",\"#33363a\"],[\"standing\",\"#80858b\"]],\"sd_install_col\":[[\"cactus\",\"#6f8a4a\"],[\"leather\",\"#7a5236\"],[\"cloth\",\"#b9ad92\"],[\"tin\",\"#9aa0a6\"],[\"brass\""
    ",\"#b8923c\"],[\"paper\",\"#e0d8c0\"],[\"glass\",\"#a9cfd6\"],[\"hide\",\"#8a6a4a\"],[\"wax\",\"#e6dfc8\"],[\"coral\",\"#d98a7a\"],[\"kelp\",\"#4f6b3a\"],[\"lacquer\",\"#8a2f2a\"],[\"bamboo\",\"#9aa65"
    "a\"],[\"jade\",\"#5fa07a\"],[\"slime\",\"#7fa04a\"],[\"web\",\"#d8d8d0\"],[\"chitin\",\"#3a3028\"],[\"resin\",\"#b07a2a\"],[\"rust\",\"#7a4a32\"],[\"tar\",\"#2a2826\"],[\"ash\",\"#4a4744\"],[\"ember\""
    ",\"#d2602a\"],[\"frost\",\"#dfeef4\"],[\"wire\",\"#55585e\"]],\"blight_cuts\":[[\"ichor\",\"#8fb03a\"],[\"rift\",\"#140a14\"],[\"runecut\",\"#a8452c\"]],\"obelis_floors\":[\"obfloor\"],\"obelis_loose\""
    ":[\"sand\"],\"amount\":{\"debris\":0.8,\"growth\":1.4,\"marks\":0.8}}";
/* ---- end of the generated part ---- */

static KVal *J;          /* KD_JSON parsed (kept: the strings below point into it) */
static void fill_set(SmStrSet *s, const KVal *l) { int i; if (l) for (i = 0; i < l->n; i++) sm_set_add(s, l->v[i]->s); }
static const char *keep_str(const char *s) { return s ? so_intern(s) : NULL; }
static void pal_from(ScPal *p, const KVal *d) {          /* scenes._P(**kw): the named roles, the rest as extras */
    int i; memset(p, 0, sizeof *p);
    p->wood = "wood"; p->iron = "iron"; p->rock = "rock"; p->stone = "rock"; p->crate = "crate"; p->water = "water"; p->pit = "pit";
    p->bone = "bone"; p->twig = "twig"; p->vein = "vein";
    for (i = 0; d && i < d->n; i++) kd_pal_set(p, d->k[i], d->v[i]->t == KV_STR ? d->v[i]->s : NULL);
}
static const char **pal_slot(ScPal *p, const char *key) {
    static const char *names[] = { "wood", "iron", "rock", "stone", "crate", "water", "pit", "gem", "floor", "plant", "soft", "bone", "twig", "vein" };
    const char **slots[14]; int i;
    slots[0] = &p->wood; slots[1] = &p->iron; slots[2] = &p->rock; slots[3] = &p->stone; slots[4] = &p->crate; slots[5] = &p->water; slots[6] = &p->pit;
    slots[7] = &p->gem; slots[8] = &p->floor; slots[9] = &p->plant; slots[10] = &p->soft; slots[11] = &p->bone; slots[12] = &p->twig; slots[13] = &p->vein;
    for (i = 0; i < 14; i++) if (strcmp(names[i], key) == 0) return slots[i];
    return NULL;
}
const char *kd_pal_get(const ScPal *p, const char *key, int *found) {
    const char **sl = pal_slot((ScPal *)p, key); int i;
    *found = 1;
    if (sl) return *sl;
    for (i = 0; i < p->nextra; i++) if (strcmp(p->extra_k[i], key) == 0) return p->extra_v[i];
    *found = 0; return NULL;
}
int kd_pal_set(ScPal *p, const char *key, const char *v) {
    const char **sl = pal_slot(p, key); int i;
    v = keep_str(v);
    if (sl) { *sl = v; return 0; }
    for (i = 0; i < p->nextra; i++) if (strcmp(p->extra_k[i], key) == 0) { p->extra_v[i] = v; return 0; }
    p->extra_k = (const char **)realloc((void *)p->extra_k, sizeof(char *) * (size_t)(p->nextra + 1));
    p->extra_v = (const char **)realloc((void *)p->extra_v, sizeof(char *) * (size_t)(p->nextra + 1));
    p->extra_k[p->nextra] = keep_str(key); p->extra_v[p->nextra] = v; p->nextra++;
    return 1;
}
void kd_pal_copy(ScPal *dst, const ScPal *src) {
    *dst = *src; dst->extra_k = dst->extra_v = NULL;
    if (src->nextra) {
        dst->extra_k = (const char **)malloc(sizeof(char *) * (size_t)src->nextra); dst->extra_v = (const char **)malloc(sizeof(char *) * (size_t)src->nextra);
        memcpy((void *)dst->extra_k, src->extra_k, sizeof(char *) * (size_t)src->nextra); memcpy((void *)dst->extra_v, src->extra_v, sizeof(char *) * (size_t)src->nextra);
    }
}
void kd_pal_free(ScPal *p) { free((void *)p->extra_k); free((void *)p->extra_v); p->extra_k = p->extra_v = NULL; p->nextra = 0; }

static KStyle *add_style(const char *name) {
    KD.styles = (KStyle *)realloc(KD.styles, sizeof(KStyle) * (size_t)(KD.nstyles + 1));
    memset(&KD.styles[KD.nstyles], 0, sizeof(KStyle)); KD.styles[KD.nstyles].name = keep_str(name); KD.styles[KD.nstyles].extra = "";
    return &KD.styles[KD.nstyles++];
}
const KStyle *kd_style(const char *name) { int i; if (!name) return NULL; for (i = 0; i < KD.nstyles; i++) if (strcmp(KD.styles[i].name, name) == 0) return &KD.styles[i]; return NULL; }
static void subj_setdefault(const char *name, const char *says) {
    int i;
    for (i = 0; i < KD.nsubjects; i++) if (strcmp(KD.subjects[i], name) == 0) break;
    if (i == KD.nsubjects) {
        if (KD.nsubjects == KD.capsub) { KD.capsub = KD.capsub ? 2 * KD.capsub : 64; KD.subjects = (const char **)realloc((void *)KD.subjects, sizeof(char *) * (size_t)KD.capsub); }
        KD.subjects[KD.nsubjects++] = keep_str(name);
    }
    if (says) kmap_setdefault(&KD.subject_says, name, says);
}
int kd_subject(const char *name) { int i; if (!name) return 0; for (i = 0; i < KD.nsubjects; i++) if (strcmp(KD.subjects[i], name) == 0) return 1; return 0; }
const struct KMini *kd_mini(const char *name) { int i; for (i = 0; i < KD.nminis; i++) if (strcmp(KD.minis[i].name, name) == 0) return &KD.minis[i]; return NULL; }
int kd_path(const char *kind) { int i; if (!kind) return -1; for (i = 0; i < KD.npaths; i++) if (strcmp(KD.paths[i], kind) == 0) return i; return -1; }
static const char **strlist(const KVal *l, int *n) {
    const char **v = (const char **)malloc(sizeof(char *) * (size_t)(l->n + 1)); int i;
    for (i = 0; i < l->n; i++) v[i] = keep_str(l->v[i]->s);
    *n = l->n; return v;
}

static void col_hook(void *ctx, const char *k2, const char *kind) {          /* kfix: gg.COL.setdefault(k2, gg.COL.get(kind, "#999999")) */
    const char *c = kmap_get(&KD.col, kind); (void)ctx;
    kmap_setdefault(&KD.col, k2, c ? c : "#999999");
}
void kinds_point_parts(void) {
    SM.bridge = KD.bridge; SM.terrk = KD.terrk; SM.soft = KD.soft; SM.pieces = &KD.pieces; SM.chamfer = KD.chamfer;
    SM.debris = &KD.debris; SM.growth = &KD.growth; SM.cutk = &KD.cutk;
    SM.col_setdefault = col_hook; SM.col_ctx = NULL;
}

static void blight_register(void) {          /* style_blight._register() */
    int i; KVal *cuts = kv_get(J, "blight_cuts");
    if (KD.blight_done) return;
    for (i = 0; i < cuts->n; i++) kmap_setdefault(&KD.cutc, cuts->v[i]->v[0]->s, cuts->v[i]->v[1]->s);          /* gg_.CUTC.setdefault(k, v) */
    if (!KD.scenes_loaded) return;          /* sc = sys.modules.get("scenes"); if sc is None: return */
    sm_set_add(KD.wetk, "ichor");
    for (i = 0; i < cuts->n; i++) sm_set_add(&KD.cutk, cuts->v[i]->v[0]->s);          /* CUTK.update(CUTS): its keys */
    sm_set_add(&KD.debris, "blightrock"); sm_set_add(&KD.debris, "thorn"); sm_set_add(&KD.debris, "runestone");
    sm_set_add(&KD.growth, "fleshrock"); sm_set_add(&KD.growth, "blister");
    subj_setdefault("blight_thorns", NULL); subj_setdefault("blight_rift", NULL); subj_setdefault("blight_runes", NULL);
    kmap_setdefault(&KD.subject_says, "blight_thorns", "a cluster of twisted rock thorns");
    kmap_setdefault(&KD.subject_says, "blight_rift", "a rift torn open in the ground");
    kmap_setdefault(&KD.subject_says, "blight_runes", "a cracked ring of unknown runes cut into the ground");
    sm_set_add(&KD.tall, "blight_thorns");
    KD.blight_done = 1;
}
static void obelis_register(int late) {          /* style_obelis._register(late) */
    int i; KVal *fl = kv_get(J, "obelis_floors"), *lo = kv_get(J, "obelis_loose");
    for (i = 0; i < fl->n; i++) sm_set_add(KD.bridge, fl->v[i]->s);
    if (late && KD.scenes_loaded) for (i = 0; i < lo->n; i++) sm_set_add(&KD.debris, lo->v[i]->s);
}
void kinds_layout_hook(const char *style) {
    if (!style) return;
    if (strcmp(style, "blight") == 0) blight_register();
    else if (strcmp(style, "obelis") == 0) obelis_register(1);
}
static void col_list(const char *key) {
    KVal *l = kv_get(J, key); int i;
    for (i = 0; i < l->n; i++) kmap_setdefault(&KD.col, l->v[i]->v[0]->s, l->v[i]->v[1]->s);
}
void kinds_install_features(void) { col_list("features_install_col"); }
void kinds_install_details(void) { col_list("details_install_col"); }
void kinds_install_scenes(void) { col_list("scenes_install_col"); }
void kinds_install_style_details(void) { col_list("sd_install_col"); }

void kinds_init(void) {
    static int done; KVal *x, *nw; int i, j;
    if (done) return;
    done = 1;
    J = kv_parse_json(KD_JSON);
    if (!J) { fprintf(stderr, "kinds.c: the tables do not parse\n"); abort(); }
    memset(&KD, 0, sizeof KD);
    /* ---- the colour tables as the whole import leaves them (gg, gg2, details, styles2, the hooks: in Python's order) ---- */
    x = kv_get(J, "col0"); for (i = 0; i < x->n; i++) kmap_setdefault(&KD.col, x->v[i]->v[0]->s, x->v[i]->v[1]->s);
    x = kv_get(J, "cutc0"); for (i = 0; i < x->n; i++) kmap_setdefault(&KD.cutc, x->v[i]->v[0]->s, x->v[i]->v[1]->s);
    KD.bridge = (SmStrSet *)&LY_BRIDGE; KD.soft = (SmStrSet *)&LY_SOFT; KD.terrk = (SmStrSet *)&LY_TERRK; KD.wetk = (SmStrSet *)&LY_WETK; KD.chamfer = (SmStrSet *)&LY_CHAMFER;
    LY_BRIDGE.n = LY_SOFT.n = LY_TERRK.n = LY_WETK.n = LY_CHAMFER.n = 0;
    fill_set(&KD.pieces, kv_get(J, "pieces0")); fill_set(KD.chamfer, kv_get(J, "chamfer0"));
    /* ---- import gg3 ---- */
    fill_set(KD.bridge, kv_get(J, "bridge0")); fill_set(KD.soft, kv_get(J, "soft0"));
    nw = kv_get(J, "new");
    {          /* newstyles.hook_gg3: the colours of every new style (the first one to name a material wins) */
        /* (gg.COL / CUTC came in above as the whole import leaves them, these colours among them: setdefault keeps them) */
        for (i = 0; i < nw->n; i++) { KVal *c = kv_get(nw->v[i], "COLOURS"); for (j = 0; j < c->n; j++) kmap_setdefault(&KD.col, c->v[j]->v[0]->s, c->v[j]->v[1]->s); }
        /* ... and m.tables(ns) in file order: style_blight._register (scenes not loaded: CUTC only), style_obelis._register() */
        for (i = 0; i < nw->n; i++) {
            const char *nm = kv_s(nw->v[i], "NAME");
            /* style_blight: its _register finds neither gg nor scenes in sys.modules yet (gg is loaded through
               sizefix): nothing happens until its layout */
            if (strcmp(nm, "obelis") == 0) obelis_register(0);
        }
    }
    /* ---- import scenes ---- */
    fill_set(KD.terrk, kv_get(J, "terrk0")); fill_set(KD.wetk, kv_get(J, "wetk0"));
    fill_set(&KD.debris, kv_get(J, "debris0")); fill_set(&KD.growth, kv_get(J, "growth0")); fill_set(&KD.cutk, kv_get(J, "cutk0"));
    fill_set(&KD.tall, kv_get(J, "tall0")); fill_set(&KD.also_ok, kv_get(J, "also_ok"));
    fill_set(&KD.built, kv_get(J, "built0")); fill_set(&KD.enclosed, kv_get(J, "enclosed0")); fill_set(&KD.cliffs, kv_get(J, "cliffs0"));
    fill_set(&KD.abstract, kv_get(J, "abstract")); fill_set(&KD.special, kv_get(J, "special"));
    x = kv_get(J, "subjects0"); for (i = 0; i < x->n; i++) subj_setdefault(x->v[i]->s, NULL);
    x = kv_get(J, "subject_says0"); for (i = 0; i < x->n; i++) kmap_setdefault(&KD.subject_says, x->k[i], x->v[i]->s);
    KD.prop_names = strlist(kv_get(J, "prop_names"), &KD.nprop_names);
    KD.paths = strlist(kv_get(J, "paths"), &KD.npaths);
    for (i = 0; i < KD.npaths; i++) { KD.path_w[i] = kv_num(kv_get(kv_get(J, "path_w"), KD.paths[i])); KD.path_says[i] = keep_str(kv_s(kv_get(J, "path_says"), KD.paths[i])); }
    KD.path_says[KD.npaths] = keep_str(kv_s(kv_get(J, "path_says"), "none"));
    KD.relations = strlist(kv_get(J, "relations"), &KD.nrelations);
    KD.edge_ok = strlist(kv_get(J, "edge_ok"), &KD.nedge_ok);
    x = kv_get(J, "amount"); KD.amount_debris = kv_num(kv_get(x, "debris")); KD.amount_growth = kv_num(kv_get(x, "growth")); KD.amount_marks = kv_num(kv_get(x, "marks"));
    x = kv_get(J, "minis");
    for (i = 0; i < x->n && i < 16; i++) {
        struct KMini *m = &KD.minis[KD.nminis++]; KVal *pads = kv_get(x->v[i], "pads");
        m->name = keep_str(x->k[i]); m->says = keep_str(kv_s(x->v[i], "says")); m->reach = kv_num(kv_get(x->v[i], "reach"));
        for (j = 0; j < pads->n && j < 12; j++) { int q; m->padn[j] = pads->v[j]->n; for (q = 0; q < pads->v[j]->n && q < 4; q++) m->pad[j][q] = kv_num(pads->v[j]->v[q]); }
        m->npads = pads->n < 12 ? pads->n : 12;
    }
    {          /* PAL, PATH_OK, STYLE_SAYS, _EXTRA of the older styles (PAL's order), themes.THEMES */
        KVal *pal = kv_get(J, "pal0"), *th = kv_get(J, "themes0");
        for (i = 0; i < pal->n; i++) {
            KStyle *st = add_style(pal->k[i]); ScPal p; int f;
            memset(&p, 0, sizeof p);
            {   /* PAL holds _P's full dict: take every key as given */
                KVal *d = pal->v[i]; int k; pal_from(&p, NULL);
                for (k = 0; k < d->n; k++) kd_pal_set(&p, d->k[k], d->v[k]->t == KV_STR ? d->v[k]->s : NULL);
            }
            st->pal = p; (void)f;
            st->path_ok = keep_str(kv_s(kv_get(J, "path_ok0"), st->name)); st->says = keep_str(kv_s(kv_get(J, "says0"), st->name));
            st->extra = kv_s(kv_get(J, "extra0"), st->name) ? keep_str(kv_s(kv_get(J, "extra0"), st->name)) : "";
            st->themes = kv_get(th, st->name);
        }
    }
    for (i = 0; i < nw->n; i++) {          /* newstyles.hook_scenes / hook_themes / styles2.TITLES */
        KVal *m = nw->v[i]; KStyle *st = add_style(kv_s(m, "NAME"));
        pal_from(&st->pal, kv_get(m, "PAL"));
        st->path_ok = keep_str(kv_s(m, "PATH_OK")); st->says = keep_str(kv_s(m, "SAYS")); st->extra = kv_s(m, "EXTRA_PROPS") ? keep_str(kv_s(m, "EXTRA_PROPS")) : "";
        st->title = keep_str(kv_s(m, "TITLE")); st->ground = keep_str(kv_s(m, "GROUND")); st->is_new = 1; st->themes = kv_get(m, "THEMES");
        if (kv_get(m, "BUILT")->i) sm_set_add(&KD.built, st->name);
        if (kv_get(m, "ENCLOSED")->i) sm_set_add(&KD.enclosed, st->name);
        if (kv_get(m, "CLIFFS")->i) sm_set_add(&KD.cliffs, st->name);
    }
    KD.scenes_loaded = 1;
    kinds_point_parts();
}

static char **copy_set(const SmStrSet *s, int *n) {
    char **v = (char **)malloc(sizeof(char *) * (size_t)(s->n + 1)); int i;
    for (i = 0; i < s->n; i++) v[i] = _strdup(s->v[i]);
    *n = s->n; return v;
}
void kinds_bc(BcCfg *c) {
    c->chamfer_prisms = copy_set(KD.chamfer, &c->nchamfer);
    c->pieces = copy_set(&KD.pieces, &c->npieces);
}

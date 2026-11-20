/* builders.c: see builders.h. A small table, searched by mod + name; a few thousand entries at most. */
#include <stdlib.h>
#include <string.h>
#include "builders.h"

typedef struct { char *mod, *name, *sig; BldFactory f; void *any; } BldEntry;
static BldEntry *tab; static int ntab, cap;
static struct { void *p; void (*f)(void *); } *kept; static int nkept, capk;

static char *dup(const char *s) { size_t n = strlen(s) + 1; char *d = malloc(n); if (d) memcpy(d, s, n); return d; }
static int find(const char *mod, const char *name) {
    if (!mod || !name) return -1;
    for (int i = 0; i < ntab; i++) if (!strcmp(tab[i].mod, mod) && !strcmp(tab[i].name, name)) return i;
    return -1;
}
static BldEntry *slot(const char *mod, const char *name) {
    int i = find(mod, name);
    if (i >= 0) return &tab[i];
    if (ntab == cap) { int nc = cap ? cap * 2 : 256; BldEntry *t = realloc(tab, sizeof *t * nc); if (!t) return NULL; tab = t; cap = nc; }
    memset(&tab[ntab], 0, sizeof tab[ntab]); tab[ntab].mod = dup(mod); tab[ntab].name = dup(name);
    return &tab[ntab++];
}
void bld_add(const char *mod, const char *name, BldFactory f) { BldEntry *e = slot(mod, name); if (e) { e->f = f; e->any = NULL; free(e->sig); e->sig = NULL; } }
BldFactory bld_get(const char *mod, const char *name) { int i = find(mod, name); return i >= 0 ? tab[i].f : NULL; }
int bld_make(const FtCall *call, FtBuildFn *fn, void **ctx) {
    if (!call || !call->mod || !call->name) return -3;          /* an FT_NEW / FT_LMS call has no module: use bld_ft_make (cport\bld_features.h), which knows builder from landmark */
    BldFactory f = bld_get(call->mod, call->name);
    if (!f) return -2;
    return f(call, fn, ctx);
}
void bld_keep(void *ctx, void (*free_fn)(void *)) {
    if (!ctx) return;
    if (nkept == capk) { int nc = capk ? capk * 2 : 256; void *t = realloc(kept, sizeof *kept * nc); if (!t) return; kept = t; capk = nc; }
    kept[nkept].p = ctx; kept[nkept].f = free_fn ? free_fn : free; nkept++;
}
void bld_free_all(void) { for (int i = nkept - 1; i >= 0; i--) kept[i].f(kept[i].p); nkept = 0; }
void bld_add_any(const char *mod, const char *name, const char *sig, void *fn) { BldEntry *e = slot(mod, name); if (e) { e->any = fn; e->f = NULL; free(e->sig); e->sig = dup(sig); } }
void *bld_any(const char *mod, const char *name, const char *sig) { int i = find(mod, name); return i >= 0 && tab[i].sig && !strcmp(tab[i].sig, sig) ? tab[i].any : NULL; }
int bld_count(void) { return ntab; }

/* each part's registration; a part that is not built yet is a weak symbol and is skipped */
#define PART(nm) void nm(void) __attribute__((weak));
#define RUN(nm) if (nm) nm();
#define PARTS(X) X(bld_register_features) X(bld_register_scenes) X(bld_register_details) X(bld_register_gg3) X(bld_register_styles2) \
                 X(bld_register_sd_abc) X(bld_register_sd_de) X(bld_register_sd_r) X(bld_register_sd_rx) X(bld_register_newstyles)
PARTS(PART)
void bld_register_all(void) { static int done; if (done) return; done = 1; PARTS(RUN) }

/* plate3mf.c (2026-11-22, the user: "a print plate with 3MF output; it puts several bases into one file for the
   slicer"): after a build, every STL in the picked folder goes into plate.3mf, one object per base, laid out on the
   printer's bed (FDM 256 x 256 mm, resin 200 x 125 mm; the Printer setting) at least PL_GAP mm apart; what does not fit
   goes on plate_2.3mf and so on. The STLs themselves are not touched. A 3MF is a zip (stored here: no compression code)
   holding [Content_Types].xml, _rels/.rels and 3D/3dmodel.model (the 3MF core spec, unit millimeter); each object is the
   base's own mesh at its own coordinates, and its build item moves it to its place on the plate. Settings "print_plate"
   or --plate on|off (OFF by default). Included by runner.c (it uses Mesh, stl_read, mesh_merge, edge_counts, Sb, ev). */
enum { PL_GAP = 3 };
static uint32_t z_crc_tab[256];
static uint32_t z_crc(const unsigned char *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    if (!z_crc_tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t k = i; for (int j = 0; j < 8; j++) k = k & 1 ? 0xEDB88320u ^ (k >> 1) : k >> 1; z_crc_tab[i] = k; }
    for (size_t i = 0; i < n; i++) c = z_crc_tab[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void z_u16(Sb *b, unsigned v) { unsigned char x[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; sb_add(b, (const char *)x, 2); }
static void z_u32(Sb *b, uint32_t v) { unsigned char x[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; sb_add(b, (const char *)x, 4); }
/* a stored zip of n entries (names, data, sizes) into path; 1 = written */
static int z_write(const char *path, const char **names, const char **data, const size_t *len, int n) {
    Sb z = { 0 }, cd = { 0 }; uint32_t *off = calloc((size_t)n, sizeof(uint32_t)), *crc = calloc((size_t)n, sizeof(uint32_t)); int ok = 0;
    for (int i = 0; i < n; i++) {
        size_t nl = strlen(names[i]); off[i] = (uint32_t)z.n; crc[i] = z_crc((const unsigned char *)data[i], len[i]);
        z_u32(&z, 0x04034b50u); z_u16(&z, 20); z_u16(&z, 0); z_u16(&z, 0); z_u16(&z, 0); z_u16(&z, 0x21);          /* version, flags, stored, time, date (1980-01-01) */
        z_u32(&z, crc[i]); z_u32(&z, (uint32_t)len[i]); z_u32(&z, (uint32_t)len[i]); z_u16(&z, (unsigned)nl); z_u16(&z, 0);
        sb_add(&z, names[i], nl); sb_add(&z, data[i], len[i]);
    }
    for (int i = 0; i < n; i++) {
        size_t nl = strlen(names[i]);
        z_u32(&cd, 0x02014b50u); z_u16(&cd, 20); z_u16(&cd, 20); z_u16(&cd, 0); z_u16(&cd, 0); z_u16(&cd, 0); z_u16(&cd, 0x21);
        z_u32(&cd, crc[i]); z_u32(&cd, (uint32_t)len[i]); z_u32(&cd, (uint32_t)len[i]); z_u16(&cd, (unsigned)nl); z_u16(&cd, 0); z_u16(&cd, 0);
        z_u16(&cd, 0); z_u16(&cd, 0); z_u32(&cd, 0); z_u32(&cd, off[i]); sb_add(&cd, names[i], nl);
    }
    {   uint32_t cdo = (uint32_t)z.n; sb_add(&z, cd.s, cd.n);
        z_u32(&z, 0x06054b50u); z_u16(&z, 0); z_u16(&z, 0); z_u16(&z, (unsigned)n); z_u16(&z, (unsigned)n); z_u32(&z, (uint32_t)cd.n); z_u32(&z, cdo); z_u16(&z, 0); }
    {   WCHAR w[MAX_PATH * 3]; FILE *f; wpath(path, w, MAX_PATH * 3); f = _wfopen(w, L"wb");
        if (f) { ok = fwrite(z.s, 1, z.n, f) == z.n; if (fclose(f) != 0) ok = 0; } }
    free(z.s); free(cd.s); free(off); free(crc);
    return ok;
}
typedef struct { char name[MAX_PATH]; Mesh m; double lo[3], hi[3], tx, ty, tz; int plate, wt; } PlBase;
static int pl_cmp_h(const void *a, const void *b) {          /* deeper footprints first (shelf packing), then by name */
    const PlBase *x = a, *y = b; double dx = x->hi[1] - x->lo[1], dy = y->hi[1] - y->lo[1];
    if (dx != dy) return dx > dy ? -1 : 1;
    return strcmp(x->name, y->name);
}
/* the plates for the STLs in folder; returns how many plates were written (0: none, or off) */
static int plate_write(const char *folder, int resin) {
    double bw = resin ? 200.0 : 256.0, bd = resin ? 125.0 : 256.0;
    WCHAR wf[MAX_PATH * 3], pat[MAX_PATH * 3]; WIN32_FIND_DATAW fd; HANDLE h; PlBase *B = NULL; int nb = 0, cap = 0, np = 0, i;
    wpath(folder, wf, MAX_PATH * 3); swprintf(pat, MAX_PATH * 3, L"%ls\\*.stl", wf);
    if ((h = FindFirstFileW(pat, &fd)) == INVALID_HANDLE_VALUE) return 0;
    do {
        char nm[MAX_PATH], full[MAX_PATH * 3]; WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nm, MAX_PATH, NULL, NULL);
        snprintf(full, sizeof full, "%s\\%s", folder, nm);
        if (nb == cap) { cap = cap ? 2 * cap : 16; B = realloc(B, sizeof(PlBase) * (size_t)cap); }
        memset(&B[nb], 0, sizeof(PlBase)); snprintf(B[nb].name, sizeof B[nb].name, "%.*s", (int)strlen(nm) - 4, nm);
        if (!stl_read(full, &B[nb].m)) continue;
        mesh_merge(&B[nb].m);          /* the corners shared again (an STL repeats them per face) */
        { int o, p; edge_counts(&B[nb].m, &o, &p, &B[nb].wt); }
        mesh_bounds(&B[nb].m, B[nb].lo, B[nb].hi);
        nb++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!nb) { free(B); return 0; }
    qsort(B, (size_t)nb, sizeof(PlBase), pl_cmp_h);
    {   double x = PL_GAP, y = PL_GAP, rowd = 0; int plate = 1;          /* shelves: left to right, then a new row; full: the next plate */
        for (i = 0; i < nb; i++) {
            double w = B[i].hi[0] - B[i].lo[0], d = B[i].hi[1] - B[i].lo[1];
            if (x > PL_GAP && x + w + PL_GAP > bw) { x = PL_GAP; y += rowd + PL_GAP; rowd = 0; }
            if (y > PL_GAP && y + d + PL_GAP > bd) { plate++; x = PL_GAP; y = PL_GAP; rowd = 0; }
            B[i].plate = plate; B[i].tx = x - B[i].lo[0]; B[i].ty = y - B[i].lo[1]; B[i].tz = -B[i].lo[2];
            x += w + PL_GAP; if (d > rowd) rowd = d;
        }
        np = plate;
    }
    for (int p = 1; p <= np; p++) {          /* one 3MF a plate */
        Sb md = { 0 }; int id = 0; char path[MAX_PATH * 3];
        static const char CT[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                                 "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                                 "<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/></Types>\n";
        static const char RL[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                                 "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/></Relationships>\n";
        sb_puts(&md, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n");
        sb_puts(&md, " <metadata name=\"Application\">Balderding's Ballin Base Kit</metadata>\n <resources>\n");
        for (i = 0; i < nb; i++) {
            if (B[i].plate != p) continue;
            const Mesh *m = &B[i].m; id++;
            sb_printf(&md, "  <object id=\"%d\" type=\"model\" name=\"%s\">\n   <mesh>\n    <vertices>\n", id, B[i].name);
            for (int v = 0; v < m->nv; v++) sb_printf(&md, "     <vertex x=\"%.6f\" y=\"%.6f\" z=\"%.6f\"/>\n", m->v[3 * v], m->v[3 * v + 1], m->v[3 * v + 2]);
            sb_puts(&md, "    </vertices>\n    <triangles>\n");
            for (int t = 0; t < m->nt; t++) sb_printf(&md, "     <triangle v1=\"%d\" v2=\"%d\" v3=\"%d\"/>\n", m->f[3 * t], m->f[3 * t + 1], m->f[3 * t + 2]);
            sb_puts(&md, "    </triangles>\n   </mesh>\n  </object>\n");
        }
        sb_puts(&md, " </resources>\n <build>\n");
        id = 0;
        for (i = 0; i < nb; i++) if (B[i].plate == p) { id++; sb_printf(&md, "  <item objectid=\"%d\" transform=\"1 0 0 0 1 0 0 0 1 %.4f %.4f %.4f\"/>\n", id, B[i].tx, B[i].ty, B[i].tz); }
        sb_puts(&md, " </build>\n</model>\n");
        if (p == 1) snprintf(path, sizeof path, "%s\\plate.3mf", folder); else snprintf(path, sizeof path, "%s\\plate_%d.3mf", folder, p);
        {   const char *nm[3] = { "[Content_Types].xml", "_rels/.rels", "3D/3dmodel.model" }, *dt[3] = { CT, RL, md.s }; size_t ln[3] = { strlen(CT), strlen(RL), md.n };
            int ok = z_write(path, nm, dt, ln, 3); char t[400]; int nbad = 0;
            for (i = 0; i < nb; i++) if (B[i].plate == p && !B[i].wt) nbad++;
            snprintf(t, sizeof t, "print plate %d: %d base%s on the %s bed (%.0f x %.0f mm)%s%s: %s", p, id, id == 1 ? "" : "s", resin ? "resin" : "FDM", bw, bd,
                     nbad ? ", not watertight: " : "", nbad ? "some (see their notes)" : "", ok ? path : "could not be written");
            ev("log", "s", "text", t);
        }
        free(md.s);
    }
    for (i = 0; i < nb; i++) mesh_free(&B[i].m);
    free(B);
    return np;
}

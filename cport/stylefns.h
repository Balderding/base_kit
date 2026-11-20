/* stylefns.h: the detail-function lists gg3.make reads for a style, filled from the builder parts' tables (stylefns.c;
   2026-11-20, agent "integrate"). Python:
     gg3.TIERS[style]      the three tier lists: gg3.py's own (bld_gg3.h BS_TIERS), styles2.TIERS2 (bld_details.h
                           DT_TIERS2) or the newer styles' tables() (bld_ns.h NS_STYLE "tiers")
     gg3.LATE / SPARSE     gg3's b_washbed b_scree b_spiral / c_cable c_lights c_chevron (by identity)
     details.DETAILS[style] details.py's pool (DT_DETAILS), replaced by styles2.DETAILS2 (DT_DETAILS2) or the newer
                           styles' pool (NS_STYLE "details"); then + features.NEW[style] (features.extend: FT_NEW, each a
                           features.comp of its builder); then + style_details.NEW20[style] (style_details.extend: every
                           sd_reg.REG[style] entry as features.comp(d.f, r, ground) with cls / kind / shape / sd set)
     style_details.NEW20[style], features.LANDMARKS[style] (FT_LMS: lm(L_x(...), r_frac, gap, ground))
   Every Python function object is ONE ScFn here, made once and kept for the whole process (identity as in Python:
   the same object in TIERS and DETAILS is the same ScFn). The newer styles' entries are interpreted from their value
   tree (NS_V F nodes: details.scatter / single / sunk / trail / path with their sh_* / pat_* arguments, styles2's
   f_column / f_post_row / f_vent / f_*, features.comp of a style builder, style_ashveil._kind / _sd,
   style_thornwild._sd, the style files' own tier functions, gg3's).
   Registered (sf_register, called by pipeline.c after bld_register_all):
       bld_add_any("gg3", "style_fns", "MkStyleFn", sf_style_fns)
       bld_add_any("details", "anchors", "MkDetailsFn", ...) and ("details", "apply", "MkDetailsFn", ...)
   Proof: work\cscene\integrate (dump_lists.py: the Python's lists, qualnames, tags and identity, against
   sf_describe's text). */
#ifndef STYLEFNS_H
#define STYLEFNS_H
#include "make.h"

/* the comps made here put true 3D solids (--supports on) into this list (pipeline.c points it at the CpOut's s3d) */
extern FtS3dList *sf_s3d;
/* CFG new_details / style_details (features.extend and style_details.extend run only when they are on) */
extern int sf_new_details, sf_style_details;

void sf_register(void);
/* compose.h CpSubjectFn: SUBJECTS[name](pal) with the subjects style files add (style_blight's three); others go to
   sf_subject_base (bld_scenes.c's). pipeline.c sets CPH.subject = sf_subject after cp_config(). */
extern CpSubjectFn sf_subject_base;
int sf_subject(const char *name, const ScPal *pal, FtBuildFn *fn, void **ctx);
int sf_style_fns(const char *style, MkStyleFns *out);           /* MkStyleFn: 0, SC_ERR (KeyError: no such style) */
/* the lists as text, one line per entry: "<list> N<i> <qualname> <tmpl>|<kind>|<shape>|<cls> <L><S>" or "<list> E<i>"
   (lists T0 T1 T2 D W, identity as first-seen numbers); malloc'd. tagged: after scenes._tag_all (as make leaves them) */
char *sf_describe(const char *style, int tagged);
/* how many entries could not be made (a maker missing from the builder table); their names in sf_missing_text */
extern int sf_nmissing; extern char sf_missing_text[4096];
#endif

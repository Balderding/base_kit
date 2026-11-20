/* audit.h: the "does this base make sense" check of a finished layout (audit.c). Turned on by the environment:
   BBB_AUDIT=<file> appends one JSON line per laid-out base to that file. */
#ifndef AUDIT_H
#define AUDIT_H
#include <stdio.h>
#include "scene.h"
typedef struct { double step, minw, minh, mina, cutw, cutd, tall; } AuCfg;          /* mm: step under a detail, thinnest part, lowest detail, smallest plan area (mm^2), narrowest / shallowest cut */
extern AuCfg AU;
int au_scene(Scene *s, const char *style, int n, double mm, const char *theme, FILE *out);
#endif

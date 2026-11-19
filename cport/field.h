/* field.h: mesher.Field in C (mesher.py: Field, Flat, Slope, Step, Band, stand_flat), for the CPU threads switch.
   Python still reads the command groups into outlines (mesher.field_ops); this file does the cell cutting, on all
   threads: every ground tile does its own steps, in order. With one thread it does the steps one after another over
   the tiles they reach, exactly as Field._apply. The result goes straight to mesh_from_field. */
#ifndef FIELD_H
#define FIELD_H
#include "mesh.h"
/* buf: the "BBF1" stream (cmesh.pack_ops). in: filled for mesh_from_field (free with mesh_in_free).
   skipped: the steps that failed, one per line ("name: GEOSException"). 1 = ok. */
int field_build(const unsigned char *buf, size_t n, int nthreads, MeshIn *in, char *skipped, int sklen, char *err, int errlen);
#endif

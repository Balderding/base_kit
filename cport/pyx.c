/* pyx.c: the UCRT's cos/sin/atan2 (what CPython's math module calls on Windows) */
#include <windows.h>
#include <stdio.h>
#include "pyx.h"
pyx_f1 py_cos, py_sin, py_log, py_sqrt_u, py_exp, py_tan, py_acos, py_asin, py_atan; pyx_f2 py_atan2, py_pow, py_fmod;
int pyx_load(void) {
    HMODULE m = LoadLibraryA("ucrtbase.dll");
    if (!m) { fprintf(stderr, "ucrtbase.dll not found\n"); return 0; }
    py_cos = (pyx_f1)(void *)GetProcAddress(m, "cos"); py_sin = (pyx_f1)(void *)GetProcAddress(m, "sin"); py_atan2 = (pyx_f2)(void *)GetProcAddress(m, "atan2");
#define GETF(v, name) v = (void *)GetProcAddress(m, name)
    GETF(py_log, "log"); GETF(py_sqrt_u, "sqrt"); GETF(py_exp, "exp"); GETF(py_tan, "tan"); GETF(py_acos, "acos"); GETF(py_asin, "asin"); GETF(py_atan, "atan"); GETF(py_pow, "pow"); GETF(py_fmod, "fmod");
#undef GETF
    return py_cos && py_sin && py_atan2 && py_log && py_exp && py_tan && py_acos && py_asin && py_atan && py_pow && py_fmod;
}

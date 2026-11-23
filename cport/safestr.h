/* safestr.h (2026-11-23): bounded text copies for the whole kit. The user's freeze came from wcscpy putting a 335-character
   hint into a 300-character buffer, which overwrote the window handle next to it. Every copy into a fixed buffer goes
   through these: the text is cut to fit and always ends with its 0. gui\build.cmd refuses to build if a plain wcscpy,
   wcscat, strcpy, strcat or sprintf is used anywhere in gui\ or cport\ (a line marked safestr-ok is let through).
     WCOPY(dst, src) / WCAT(dst, src)   dst a WCHAR / wchar_t ARRAY (its size is known); a pointer does not compile
     SCOPY(dst, src) / SCAT(dst, src)   the same for char arrays
     wcs_bcopy(dst, cap, src), wcs_bcat(dst, cap, src), str_bcopy(dst, cap, src), str_bcat(dst, cap, src): a pointer
     with its size in characters */
#ifndef SAFESTR_H
#define SAFESTR_H
#include <string.h>
#include <wchar.h>
static inline wchar_t *wcs_bcopy(wchar_t *d, size_t cap, const wchar_t *s) {
    size_t n = s ? wcslen(s) : 0; if (!cap) return d; if (n > cap - 1) n = cap - 1; if (n) memmove(d, s, n * sizeof(wchar_t)); d[n] = 0; return d;
}
static inline wchar_t *wcs_bcat(wchar_t *d, size_t cap, const wchar_t *s) {
    size_t have = 0; while (have < cap && d[have]) have++; if (have >= cap) return d; wcs_bcopy(d + have, cap - have, s); return d;
}
static inline char *str_bcopy(char *d, size_t cap, const char *s) {
    size_t n = s ? strlen(s) : 0; if (!cap) return d; if (n > cap - 1) n = cap - 1; if (n) memmove(d, s, n); d[n] = 0; return d;
}
static inline char *str_bcat(char *d, size_t cap, const char *s) {
    size_t have = 0; while (have < cap && d[have]) have++; if (have >= cap) return d; str_bcopy(d + have, cap - have, s); return d;
}
/* an array, not a pointer: its size is sizeof(a) / sizeof(a[0]) (a pointer here is a compile error, not a silent overflow) */
#define SAFESTR_ARRAY(a) (sizeof(char[1 - 2 * __builtin_types_compatible_p(__typeof__(a), __typeof__(&(a)[0]))]) * 0 + sizeof(a) / sizeof((a)[0]))
#define WCOPY(d, s) wcs_bcopy((d), SAFESTR_ARRAY(d), (s))
#define WCAT(d, s)  wcs_bcat((d), SAFESTR_ARRAY(d), (s))
#define SCOPY(d, s) str_bcopy((d), SAFESTR_ARRAY(d), (s))
#define SCAT(d, s)  str_bcat((d), SAFESTR_ARRAY(d), (s))
#endif

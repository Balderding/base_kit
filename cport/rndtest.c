/* rndtest: the random-number proof, C side (see rnd_case.py: same script, same output) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pyrandom.h"

static void hx(double x) { unsigned char b[8]; memcpy(b, &x, 8); for (int i = 0; i < 8; i++) printf("%02x", b[i]); }
static void sep(void) { printf(" "); }
static void list(const int64_t *a, int n) { for (int i = 0; i < n; i++) printf(i ? ",%lld" : "%lld", (long long)a[i]); }

static void script(PyRandom *r) {
    hx(py_random(r)); sep(); hx(py_random(r)); sep(); hx(py_uniform(r, -2.5, 7.0)); sep(); hx(py_uniform(r, 3, 1)); sep();
    printf("%lld", (long long)py_randint(r, 1, 6)); sep(); printf("%lld", (long long)py_randint(r, 0, 1000000000)); sep(); printf("%lld", (long long)py_randint(r, -5, 5)); sep();
    printf("%lld", (long long)py_randrange(r, 0, 100, 7)); sep(); printf("%lld", (long long)py_randrange(r, 10, 0, -3)); sep(); printf("%lld", (long long)py_randrange(r, 3, 40, 1)); sep();
    printf("%lld", (long long)py_choice(r, 17)); sep();
    int64_t x[12]; for (int i = 0; i < 12; i++) x[i] = i; py_shuffle_idx(r, x, 12); list(x, 12); sep();
    int64_t s[20]; py_sample_idx(r, 30, 5, s); list(s, 5); sep(); py_sample_idx(r, 1000, 20, s); list(s, 20); sep(); py_sample_idx(r, 8, 8, s); list(s, 8); sep(); py_sample_idx(r, 12, 6, s); list(s, 6); sep();
    double cum[4] = { 1, 3, 6.5, 6.75 }; for (int i = 0; i < 5; i++) printf(i ? ",%lld" : "%lld", (long long)py_choices1(r, cum, 4)); sep();
    for (int i = 0; i < 4; i++) printf(i ? ",%lld" : "%lld", (long long)py_choices1_unweighted(r, 9)); sep();
    printf("%llu", (unsigned long long)py_getrandbits(r, 40)); sep(); printf("%llu", (unsigned long long)py_getrandbits(r, 64)); sep(); printf("%llu", (unsigned long long)py_getrandbits(r, 1)); sep();
    for (int i = 0; i < 700; i++) py_random(r);
    hx(py_random(r));
}

/* the same seeds as rnd_case.py, and its line heads (repr cut to 40 characters) */
int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    if (!pyx_load()) return 2;
    static const char *S[] = { "", "a", "badlands12tier", "mine3mix", "ground_bigbadlandsbadlands_12proof1", "\xc3\xbc-\xc3\xab\xce\xa9", NULL };
    static const char *SR[] = { "''", "'a'", "'badlands12tier'", "'mine3mix'", "'ground_bigbadlandsbadlands_12proof1'", "'\xc3\xbc-\xc3\xab\xce\xa9'" };
    PyRandom r;
    for (int i = 0; S[i]; i++) { py_seed_str(&r, S[i]); printf("s %s: ", SR[i]); script(&r); printf("\n"); }
    { char big[201]; memset(big, 'x', 200); big[200] = 0; py_seed_str(&r, big); printf("s '%.39s: ", big); script(&r); printf("\n"); }
    static const uint64_t I[] = { 0, 1, 12345, (1ULL << 40) + 7, 9223372036854775807ULL };
    for (int i = 0; i < 5; i++) { py_seed_u64(&r, I[i]); printf("i %llu: ", (unsigned long long)I[i]); script(&r); printf("\n"); }
    static const double F[] = { 0.0, 0.5, 0.123456789, 0.9999999999999999, 1e-300, 123.456, 1152921504606846976.0, 7.0 };
    static const char *FR[] = { "0.0", "0.5", "0.123456789", "0.9999999999999999", "1e-300", "123.456", "1.152921504606847e+18", "7.0" };
    for (int i = 0; i < 8; i++) { py_seed_float(&r, F[i]); printf("f %s: ", FR[i]); script(&r); printf("\n"); }
    return 0;
}

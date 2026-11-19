/* pyrandom.c: sha512 for the str seeds (Windows' own bcrypt) */
#include <windows.h>
#include <bcrypt.h>
#include "pyrandom.h"
int pr_sha512(const unsigned char *data, size_t n, unsigned char out[64]) {
    BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE h; int ok = 0;
    if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA512_ALGORITHM, NULL, 0)) return 0;
    if (!BCryptCreateHash(a, &h, NULL, 0, NULL, 0, 0)) {
        ok = !BCryptHashData(h, (PUCHAR)data, (ULONG)n, 0) && !BCryptFinishHash(h, out, 64, 0);
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(a, 0); return ok;
}

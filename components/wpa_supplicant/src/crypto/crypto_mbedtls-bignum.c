// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef ESP_PLATFORM
#include "esp_system.h"
#include "mbedtls/bignum.h"
#endif

#include "utils/includes.h"
#include "utils/common.h"
#include "crypto.h"
#include "random.h"
#include "sha256.h"
#include "mbedtls/pk.h"

struct crypto_bignum *crypto_bignum_init(void)
{
    mbedtls_mpi *bn = os_zalloc(sizeof(mbedtls_mpi));
    if (bn == NULL) {
        return NULL;
    }

    mbedtls_mpi_init(bn);

    return (struct crypto_bignum *)bn;
}


struct crypto_bignum *crypto_bignum_init_set(const u8 *buf, size_t len)
{
    int ret = 0;
    mbedtls_mpi *bn = os_zalloc(sizeof(mbedtls_mpi));
    if (bn == NULL) {
        return NULL;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(bn, buf, len));
    return (struct crypto_bignum *) bn;

cleanup:
    os_free(bn);
    return NULL;
}


void crypto_bignum_deinit(struct crypto_bignum *n, int clear)
{
    mbedtls_mpi_free((mbedtls_mpi *)n);
    os_free((mbedtls_mpi *)n);
}


int crypto_bignum_to_bin(const struct crypto_bignum *a,
                         u8 *buf, size_t buflen, size_t padlen)
{
    int num_bytes, offset;

    if (padlen > buflen) {
        return -1;
    }

    num_bytes = mbedtls_mpi_size((mbedtls_mpi *) a);

    if ((size_t) num_bytes > buflen) {
        return -1;
    }
    if (padlen > (size_t) num_bytes) {
        offset = padlen - num_bytes;
    } else {
        offset = 0;
    }

    os_memset(buf, 0, offset);
    mbedtls_mpi_write_binary((mbedtls_mpi *) a, buf + offset, mbedtls_mpi_size((mbedtls_mpi *)a) );

    return num_bytes + offset;
}


int crypto_bignum_add(const struct crypto_bignum *a,
                      const struct crypto_bignum *b,
                      struct crypto_bignum *c)
{
    return mbedtls_mpi_add_mpi((mbedtls_mpi *) c, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ?
           -1 : 0;
}


int crypto_bignum_mod(const struct crypto_bignum *a,
                      const struct crypto_bignum *b,
                      struct crypto_bignum *c)
{
    return mbedtls_mpi_mod_mpi((mbedtls_mpi *) c, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ? -1 : 0;
}


int crypto_bignum_exptmod(const struct crypto_bignum *a,
                          const struct crypto_bignum *b,
                          const struct crypto_bignum *c,
                          struct crypto_bignum *d)
{
    return  mbedtls_mpi_exp_mod((mbedtls_mpi *) d, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b, (const mbedtls_mpi *) c, NULL) ? -1 : 0;

}


int crypto_bignum_inverse(const struct crypto_bignum *a,
                          const struct crypto_bignum *b,
                          struct crypto_bignum *c)
{
    return mbedtls_mpi_inv_mod((mbedtls_mpi *) c, (const mbedtls_mpi *) a,
                               (const mbedtls_mpi *) b) ? -1 : 0;
}


int crypto_bignum_sub(const struct crypto_bignum *a,
                      const struct crypto_bignum *b,
                      struct crypto_bignum *c)
{
    return mbedtls_mpi_sub_mpi((mbedtls_mpi *) c, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ?
           -1 : 0;
}


int crypto_bignum_div(const struct crypto_bignum *a,
                      const struct crypto_bignum *b,
                      struct crypto_bignum *c)
{
    return mbedtls_mpi_div_mpi((mbedtls_mpi *) c, NULL, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b) ?
           -1 : 0;
}


/* espurna 20.11: a * b mod p of P-256 (the prime of SAE group 19) without a division - the remainder of mbedtls is a
 * long division in software on the lx106 (MBEDTLS_NO_UDBL_DIVISION), several ms per call, and SAE
 * hunting-and-pecking calls it 4 times per candidate, 40+ candidates per attempt. The product of 8 x 8 words, one row
 * of d += s * b per word of b in lx106 assembly (MUL16U in 16-bit columns: every column sum fits in 32 bits, no carry
 * compare), the reduction by the NIST formula (FIPS 186-4 D.2.3) */
static const uint32_t p256_words[8] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xffffffff,
};

/* d[0..7] += s[0..7] * b, the carry out */
static uint32_t p256_mla(uint32_t *d, const uint32_t *s, uint32_t b)
{
    uint32_t c = 0;
#if defined(__XTENSA__) && defined(__XTENSA_CALL0_ABI__)
    uint32_t b_lo = b & 0xffff, b_hi = b >> 16;
    uint32_t t_a, t_h, t_d, t_0, t_1, t_2;
    uint32_t n = 8;
    asm volatile (
        "1:                             \n\t"
        "l32i   %[a], %[s], 0           \n\t"
        "l32i   %[dv], %[d], 0          \n\t"
        "addi   %[s], %[s], 4           \n\t"
        "extui  %[h], %[a], 16, 16      \n\t"
        "extui  %[a], %[a], 0, 16       \n\t"
        "mul16u %[t0], %[a], %[bl]      \n\t"
        "extui  %[t1], %[c], 0, 16      \n\t"
        "add    %[t0], %[t0], %[t1]     \n\t"
        "extui  %[t1], %[dv], 0, 16     \n\t"
        "add    %[t0], %[t0], %[t1]     \n\t"
        "mul16u %[t1], %[a], %[bh]      \n\t"
        "srli   %[t2], %[t0], 16        \n\t"
        "add    %[t1], %[t1], %[t2]     \n\t"
        "srli   %[t2], %[c], 16         \n\t"
        "add    %[t1], %[t1], %[t2]     \n\t"
        "mul16u %[t2], %[h], %[bl]      \n\t"
        "extui  %[a], %[t1], 0, 16      \n\t"
        "add    %[t2], %[t2], %[a]      \n\t"
        "srli   %[a], %[dv], 16         \n\t"
        "add    %[t2], %[t2], %[a]      \n\t"
        "extui  %[t0], %[t0], 0, 16     \n\t"
        "slli   %[a], %[t2], 16         \n\t"
        "or     %[a], %[a], %[t0]       \n\t"
        "s32i   %[a], %[d], 0           \n\t"
        "addi   %[d], %[d], 4           \n\t"
        "mul16u %[c], %[h], %[bh]       \n\t"
        "srli   %[t1], %[t1], 16        \n\t"
        "add    %[c], %[c], %[t1]       \n\t"
        "srli   %[t2], %[t2], 16        \n\t"
        "add    %[c], %[c], %[t2]       \n\t"
        "addi   %[n], %[n], -1          \n\t"
        "bnez   %[n], 1b                \n\t"
        : [s] "+r" (s), [d] "+r" (d), [c] "+r" (c), [n] "+r" (n),
          [a] "=&r" (t_a), [h] "=&r" (t_h), [dv] "=&r" (t_d),
          [t0] "=&r" (t_0), [t1] "=&r" (t_1), [t2] "=&r" (t_2)
        : [bl] "r" (b_lo), [bh] "r" (b_hi)
        : "memory");
#else
    for (size_t i = 0; i < 8; ++i) {
        const uint64_t r = (uint64_t) s[i] * b + d[i] + c;
        d[i] = (uint32_t) r;
        c = (uint32_t) (r >> 32);
    }
#endif
    return c;
}

/* r = a * b mod p256, a and b below 2^256 */
static void p256_mulmod_words(uint32_t r[8], const uint32_t a[8], const uint32_t b[8])
{
    uint32_t c[16] = { 0 };
    for (size_t i = 0; i < 8; ++i) {
        c[i + 8] = p256_mla(c + i, a, b[i]);
    }

    /* s1 + 2 s2 + 2 s3 + s4 + s5 - d1 - d2 - d3 - d4, word by word from the least significant (FIPS 186-4 D.2.3) */
    const int64_t w[8] = {
        (int64_t) c[0] + c[8] + c[9] - c[11] - c[12] - c[13] - c[14],
        (int64_t) c[1] + c[9] + c[10] - c[12] - c[13] - c[14] - c[15],
        (int64_t) c[2] + c[10] + c[11] - c[13] - c[14] - c[15],
        (int64_t) c[3] + 2 * ((int64_t) c[11] + c[12]) + c[13] - c[15] - c[8] - c[9],
        (int64_t) c[4] + 2 * ((int64_t) c[12] + c[13]) + c[14] - c[9] - c[10],
        (int64_t) c[5] + 2 * ((int64_t) c[13] + c[14]) + c[15] - c[10] - c[11],
        (int64_t) c[6] + 3 * (int64_t) c[14] + 2 * (int64_t) c[15] + c[13] - c[8] - c[9],
        (int64_t) c[7] + 3 * (int64_t) c[15] + c[8] - c[10] - c[11] - c[12] - c[13],
    };

    int64_t carry = 0;
    for (size_t i = 0; i < 8; ++i) {
        carry += w[i];
        r[i] = (uint32_t) carry;
        carry >>= 32;
    }

    /* r + carry * 2^256 into [0, p): the carry is small (-5..5), p added or taken that many times at most */
    while (carry < 0) {
        int64_t k = 0;
        for (size_t i = 0; i < 8; ++i) {
            k += (int64_t) r[i] + p256_words[i];
            r[i] = (uint32_t) k;
            k >>= 32;
        }
        carry += k;
    }
    for (;;) {
        int ge = carry > 0;
        if (!ge) {
            ge = 1;
            for (size_t i = 8; i-- > 0;) {
                if (r[i] != p256_words[i]) {
                    ge = r[i] > p256_words[i];
                    break;
                }
            }
        }
        if (!ge) {
            break;
        }
        int64_t k = 0;
        for (size_t i = 0; i < 8; ++i) {
            k += (int64_t) r[i] - p256_words[i];
            r[i] = (uint32_t) k;
            k >>= 32;
        }
        carry += k;
    }
}

/* the words of x when it is in [0, 2^256) */
static int p256_words_of(const mbedtls_mpi *x, uint32_t out[8])
{
    if ((mbedtls_mpi_cmp_int(x, 0) < 0) || (mbedtls_mpi_size(x) > 32)) {
        return 0;
    }
    return mbedtls_mpi_write_binary_le(x, (unsigned char *) out, 32) == 0;
}

/* c is the prime of P-256 */
static int p256_is(const mbedtls_mpi *c)
{
    uint32_t words[8];
    return p256_words_of(c, words) && (memcmp(words, p256_words, sizeof(words)) == 0);
}

int crypto_bignum_mulmod(const struct crypto_bignum *a,
                         const struct crypto_bignum *b,
                         const struct crypto_bignum *c,
                         struct crypto_bignum *d)
{
    int res;

    if (p256_is((const mbedtls_mpi *) c)) {
        uint32_t x[8], y[8], r[8];
        if (p256_words_of((const mbedtls_mpi *) a, x) && p256_words_of((const mbedtls_mpi *) b, y)) {
            p256_mulmod_words(r, x, y);
            return mbedtls_mpi_read_binary_le((mbedtls_mpi *) d, (const unsigned char *) r, 32) ? -1 : 0;
        }
    }

#if ALLOW_EVEN_MOD || !CONFIG_MBEDTLS_HARDWARE_MPI // Must enable ALLOW_EVEN_MOD if c is even
    mbedtls_mpi temp;
    mbedtls_mpi_init(&temp);

    res = mbedtls_mpi_mul_mpi(&temp, (const mbedtls_mpi *) a, (const mbedtls_mpi *) b);
    if (res) {
        return -1;
    }

    res = mbedtls_mpi_mod_mpi((mbedtls_mpi *) d, &temp, (mbedtls_mpi *) c);

    mbedtls_mpi_free(&temp);
#else
    // Works with odd modulus only, but it is faster with HW acceleration
    res = esp_mpi_mul_mpi_mod((mbedtls_mpi *) d, (mbedtls_mpi *) a, (mbedtls_mpi *) b, (mbedtls_mpi *) c);
#endif
    return res ? -1 : 0;
}


int crypto_bignum_cmp(const struct crypto_bignum *a,
                      const struct crypto_bignum *b)
{
    return mbedtls_mpi_cmp_mpi((const mbedtls_mpi *) a, (const mbedtls_mpi *) b);
}


int crypto_bignum_bits(const struct crypto_bignum *a)
{
    return mbedtls_mpi_bitlen((const mbedtls_mpi *) a);
}


int crypto_bignum_is_zero(const struct crypto_bignum *a)
{
    return (mbedtls_mpi_cmp_int((const mbedtls_mpi *) a, 0) == 0);
}


int crypto_bignum_is_one(const struct crypto_bignum *a)
{
    return (mbedtls_mpi_cmp_int((const mbedtls_mpi *) a, 1) == 0);
}


/* espurna: the Legendre symbol as the Jacobi symbol (p is an odd prime here), binary: subtractions and shifts of
 * fixed words, no division. SAE hunting-and-pecking takes 40+ of them per attempt (k = 40): a^((p-1)/2) mod p took
 * ~10 s of an attempt on the ESP8266 at 160 MHz (19.11), the Jacobi symbol on mbedtls_mpi ~2 s (its remainders are
 * long divisions in software, MBEDTLS_NO_UDBL_DIVISION, and allocations; 20.11). The time depends on the input; the
 * callers of SAE pass it blinded by a random value (is_quadratic_residue_blind() of sae.c), as hostapd does for this
 * very reason. The word loops in lx106 assembly (owner 20.11) */
#define JACOBI_WORDS_MAX 16


/* a -= b over len words, a >= b */
static void jacobi_sub(uint32_t *a, const uint32_t *b, size_t len)
{
#if defined(__XTENSA__) && defined(__XTENSA_CALL0_ABI__)
    uint32_t x, y, t, borrow = 0;
    asm volatile (
        "1:                             \n\t"
        "l32i   %[x], %[a], 0           \n\t"
        "l32i   %[y], %[b], 0           \n\t"
        "movi   %[t], 0                 \n\t"
        "bgeu   %[x], %[y], 2f          \n\t"
        "movi   %[t], 1                 \n\t"
        "2:                             \n\t"
        "sub    %[x], %[x], %[y]        \n\t"
        "bgeu   %[x], %[bw], 3f         \n\t"
        "movi   %[t], 1                 \n\t"
        "3:                             \n\t"
        "sub    %[x], %[x], %[bw]       \n\t"
        "s32i   %[x], %[a], 0           \n\t"
        "mov    %[bw], %[t]             \n\t"
        "addi   %[a], %[a], 4           \n\t"
        "addi   %[b], %[b], 4           \n\t"
        "addi   %[n], %[n], -1          \n\t"
        "bnez   %[n], 1b                \n\t"
        : [a] "+r" (a), [b] "+r" (b), [n] "+r" (len), [bw] "+r" (borrow),
          [x] "=&r" (x), [y] "=&r" (y), [t] "=&r" (t)
        :
        : "memory");
#else
    uint32_t borrow = 0;
    for (size_t i = 0; i < len; ++i) {
        const uint32_t x = a[i], y = b[i];
        const uint32_t r = x - y;
        const uint32_t out = (x < y) | (r < borrow);
        a[i] = r - borrow;
        borrow = out;
    }
#endif
}

/* a >>= bits over len words, 0 < bits < 32 */
static void jacobi_shr(uint32_t *a, size_t len, unsigned bits)
{
#if defined(__XTENSA__) && defined(__XTENSA_CALL0_ABI__)
    uint32_t lo, hi;
    if (len > 1) {
        size_t n = len - 1;
        asm volatile (
            "ssr    %[s]                    \n\t"
            "l32i   %[lo], %[a], 0          \n\t"
            "1:                             \n\t"
            "l32i   %[hi], %[a], 4          \n\t"
            "src    %[lo], %[hi], %[lo]     \n\t"
            "s32i   %[lo], %[a], 0          \n\t"
            "mov    %[lo], %[hi]            \n\t"
            "addi   %[a], %[a], 4           \n\t"
            "addi   %[n], %[n], -1          \n\t"
            "bnez   %[n], 1b                \n\t"
            "srl    %[lo], %[lo]            \n\t"
            "s32i   %[lo], %[a], 0          \n\t"
            : [a] "+r" (a), [n] "+r" (n), [lo] "=&r" (lo), [hi] "=&r" (hi)
            : [s] "r" (bits)
            : "memory");
    } else {
        a[0] >>= bits;
    }
#else
    for (size_t i = 0; i + 1 < len; ++i) {
        a[i] = (a[i] >> bits) | (a[i + 1] << (32 - bits));
    }
    a[len - 1] >>= bits;
#endif
}

/* -1, 0, 1 as a < b, a == b, a > b */
static int jacobi_cmp(const uint32_t *a, const uint32_t *b, size_t len)
{
    while (len--) {
        if (a[len] != b[len]) {
            return (a[len] > b[len]) ? 1 : -1;
        }
    }
    return 0;
}

int crypto_bignum_legendre(const struct crypto_bignum *a,
                           const struct crypto_bignum *p)
{
    uint32_t x_words[JACOBI_WORDS_MAX], n_words[JACOBI_WORDS_MAX];
    mbedtls_mpi x;
    int ret, res = -2;

    const size_t bytes = mbedtls_mpi_size((const mbedtls_mpi *) p);
    const size_t len = (bytes + 3) / 4;
    if (!len || (len > JACOBI_WORDS_MAX) || !mbedtls_mpi_get_bit((const mbedtls_mpi *) p, 0)) {
        return -2;
    }

    mbedtls_mpi_init(&x);
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&x, (const mbedtls_mpi *) a, (const mbedtls_mpi *) p));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&x, (unsigned char *) x_words, len * 4));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le((const mbedtls_mpi *) p, (unsigned char *) n_words, len * 4));

    uint32_t *u = x_words, *v = n_words;
    int t = 1;
    for (;;) {
        /* u = 0: the end */
        size_t zero_words = 0;
        while ((zero_words < len) && !u[zero_words]) {
            ++zero_words;
        }
        if (zero_words == len) {
            break;
        }

        /* u = 2^k * u': (2/v) = -1 for v = 3, 5 mod 8, k times */
        unsigned k = zero_words * 32;
        if (zero_words) {
            memmove(u, u + zero_words, (len - zero_words) * 4);
            memset(u + len - zero_words, 0, zero_words * 4);
        }
        const unsigned bits = __builtin_ctz(u[0]);
        if (bits) {
            jacobi_shr(u, len, bits);
        }
        k += bits;
        if ((k & 1) && (((v[0] & 7) == 3) || ((v[0] & 7) == 5))) {
            t = -t;
        }

        /* both odd: u < v swaps them (reciprocity: both 3 mod 4 flips the sign), then u -= v keeps (u/v) */
        if (jacobi_cmp(u, v, len) < 0) {
            uint32_t *w = u;
            u = v;
            v = w;
            if (((u[0] & 3) == 3) && ((v[0] & 3) == 3)) {
                t = -t;
            }
        }
        jacobi_sub(u, v, len);
    }

    /* v = 1: coprime */
    res = (v[0] == 1) ? t : 0;
    for (size_t i = 1; i < len; ++i) {
        if (v[i]) {
            res = 0;
        }
    }

cleanup:
    mbedtls_mpi_free(&x);
    return res;
}

/* the SDK's own, by exponentiation (kept for the comparison of the host test) */
int crypto_bignum_legendre_exp(const struct crypto_bignum *a,
                               const struct crypto_bignum *p)
{
    mbedtls_mpi exp, tmp;
    int res = -2, ret;

    mbedtls_mpi_init(&exp);
    mbedtls_mpi_init(&tmp);

    /* exp = (p-1) / 2 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&exp, (const mbedtls_mpi *) p, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&exp, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&tmp, (const mbedtls_mpi *) a, &exp, (const mbedtls_mpi *) p, NULL));

    if (mbedtls_mpi_cmp_int(&tmp, 1) == 0) {
        res = 1;
    } else if (mbedtls_mpi_cmp_int(&tmp, 0) == 0
            /* The below check is workaround for the case where HW
             * does not behave properly for X ^ A mod M when X is
             * power of M. Instead of returning value 0, value M is
             * returned.*/
            || mbedtls_mpi_cmp_mpi(&tmp, (const mbedtls_mpi *)p) == 0) {
        res = 0;
    } else {
        res = -1;
    }

cleanup:
    mbedtls_mpi_free(&tmp);
    mbedtls_mpi_free(&exp);
    return res;
}

int crypto_bignum_to_string(const struct crypto_bignum *a,
                         u8 *buf, size_t buflen, size_t padlen)
{
    int num_bytes, offset;
    size_t outlen;

    if (padlen > buflen) {
        return -1;
    }

    num_bytes = mbedtls_mpi_size((mbedtls_mpi *) a);

    if (padlen > (size_t) num_bytes) {
        offset = padlen - num_bytes;
    } else {
        offset = 0;
    }

    os_memset(buf, 0, offset);
    mbedtls_mpi_write_string((mbedtls_mpi *) a, 16, (char *)(buf + offset),
                                mbedtls_mpi_size((mbedtls_mpi *)a), &outlen);

    return outlen;
}

int crypto_bignum_addmod(struct crypto_bignum *a,
                      struct crypto_bignum *b,
                      struct crypto_bignum *c,
                      struct crypto_bignum *d)
{
    struct crypto_bignum *tmp = crypto_bignum_init();
    int ret = -1;

    if (mbedtls_mpi_add_mpi((mbedtls_mpi *) tmp, (const mbedtls_mpi *) b, (const mbedtls_mpi *) c) < 0)
        goto fail;

    if (mbedtls_mpi_mod_mpi( (mbedtls_mpi *) a, (const mbedtls_mpi *) tmp, (const mbedtls_mpi *) d) < 0)
        goto fail;

    ret = 0;
fail:
    crypto_bignum_deinit(tmp, 0);
    return ret;
}

void crypto_free_buffer(unsigned char *buf)
{
    os_free(buf);
}

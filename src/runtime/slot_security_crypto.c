/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot security crypto primitives.
 */

#include "slot_security.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* AES-256 block cipher with CTR mode and HMAC-SHA256 authentication. */

#ifdef PGY_AES_SBOX_TABLE_FOR_TESTS
static const uint8_t aes_sbox_table[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};
#endif

static uint8_t
gf256_mul(uint8_t a, uint8_t b)
{
    uint8_t result = 0;
    int i;

    for (i = 0; i < 8; i++) {
        result ^= a & (uint8_t)(-(int8_t)(b & 1));
        uint8_t hi = (uint8_t)(-(int8_t)((a >> 7) & 1));
        a = (uint8_t)((a << 1) ^ (hi & 0x1B));
        b >>= 1;
    }
    return result;
}

static uint8_t
gf256_inv(uint8_t x)
{
    uint8_t x2 = gf256_mul(x, x);
    uint8_t x3 = gf256_mul(x2, x);
    uint8_t x6 = gf256_mul(x3, x3);
    uint8_t x12 = gf256_mul(x6, x6);
    uint8_t x15 = gf256_mul(x12, x3);
    uint8_t x30 = gf256_mul(x15, x15);
    uint8_t x60 = gf256_mul(x30, x30);
    uint8_t x63 = gf256_mul(x60, x3);
    uint8_t x126 = gf256_mul(x63, x63);
    uint8_t x252 = gf256_mul(x126, x126);
    uint8_t x254 = gf256_mul(x252, x2);
    return x254;
}

static uint8_t
aes_sbox_compute(uint8_t x)
{
    uint8_t inv = gf256_inv(x);
    uint8_t s = inv;
    s ^= (uint8_t)((inv << 1) | (inv >> 7));
    s ^= (uint8_t)((inv << 2) | (inv >> 6));
    s ^= (uint8_t)((inv << 3) | (inv >> 5));
    s ^= (uint8_t)((inv << 4) | (inv >> 4));
    s ^= 0x63;
    return s;
}

static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static inline uint8_t
aes_xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static void
aes256_key_expand(const uint8_t key[32], uint8_t rk[240])
{
    uint8_t temp[4];
    int i;

    memcpy(rk, key, 32);

    for (i = 8; i < 60; i++) {
        memcpy(temp, rk + (i - 1) * 4, 4);

        if (i % 8 == 0) {
            uint8_t t = temp[0];
            temp[0] = aes_sbox_compute(temp[1]) ^ aes_rcon[i / 8];
            temp[1] = aes_sbox_compute(temp[2]);
            temp[2] = aes_sbox_compute(temp[3]);
            temp[3] = aes_sbox_compute(t);
        } else if (i % 8 == 4) {
            temp[0] = aes_sbox_compute(temp[0]);
            temp[1] = aes_sbox_compute(temp[1]);
            temp[2] = aes_sbox_compute(temp[2]);
            temp[3] = aes_sbox_compute(temp[3]);
        }

        rk[i * 4 + 0] = rk[(i - 8) * 4 + 0] ^ temp[0];
        rk[i * 4 + 1] = rk[(i - 8) * 4 + 1] ^ temp[1];
        rk[i * 4 + 2] = rk[(i - 8) * 4 + 2] ^ temp[2];
        rk[i * 4 + 3] = rk[(i - 8) * 4 + 3] ^ temp[3];
    }
}

static void
aes256_encrypt_block(const uint8_t rk[240], uint8_t block[16])
{
    uint8_t s[16], t[4];
    int r, i;

    memcpy(s, block, 16);

    for (i = 0; i < 16; i++)
        s[i] ^= rk[i];

    for (r = 1; r <= 14; r++) {
        for (i = 0; i < 16; i++)
            s[i] = aes_sbox_compute(s[i]);

        t[0] = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t[0];
        t[0] = s[2]; t[1] = s[6]; s[2] = s[10]; s[6] = s[14]; s[10] = t[0]; s[14] = t[1];
        t[0] = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t[0];

        if (r < 14) {
            for (i = 0; i < 4; i++) {
                int c = i * 4;
                uint8_t a0 = s[c], a1 = s[c + 1], a2 = s[c + 2], a3 = s[c + 3];
                uint8_t x0 = aes_xtime(a0), x1 = aes_xtime(a1);
                uint8_t x2 = aes_xtime(a2), x3 = aes_xtime(a3);
                s[c] = x0 ^ a1 ^ x1 ^ a2 ^ a3;
                s[c + 1] = a0 ^ x1 ^ a2 ^ x2 ^ a3;
                s[c + 2] = a0 ^ a1 ^ x2 ^ a3 ^ x3;
                s[c + 3] = a0 ^ x0 ^ a1 ^ a2 ^ x3;
            }
        }

        for (i = 0; i < 16; i++)
            s[i] ^= rk[r * 16 + i];
    }

    memcpy(block, s, 16);
}

static void
aes256_ctr(const uint8_t key[32], const uint8_t iv[16],
           const uint8_t *in, uint8_t *out, size_t len)
{
    uint8_t rk[240];
    uint8_t ctr[16], keystream[16];
    size_t i, block_idx;

    aes256_key_expand(key, rk);
    memcpy(ctr, iv, 16);

    for (block_idx = 0; block_idx < len; block_idx += 16) {
        memcpy(keystream, ctr, 16);
        aes256_encrypt_block(rk, keystream);

        size_t chunk = (len - block_idx < 16) ? (len - block_idx) : 16;
        for (i = 0; i < chunk; i++)
            out[block_idx + i] = in[block_idx + i] ^ keystream[i];

        for (int j = 15; j >= 12; j--) {
            if (++ctr[j] != 0)
                break;
        }
    }

    SecureMemoryWipe(rk, sizeof(rk));
    SecureMemoryWipe(keystream, sizeof(keystream));
}

#define HMAC_BLOCK_SIZE 64
#define HMAC_HASH_SIZE  32

static SecurityError
hmac_sha256(const uint8_t *key, size_t keyLen,
            const uint8_t *message, size_t messageLen,
            uint8_t output[32])
{
    uint8_t key_prime[HMAC_BLOCK_SIZE];
    uint8_t ipad_key[HMAC_BLOCK_SIZE];
    uint8_t opad_key[HMAC_BLOCK_SIZE];
    uint8_t inner_hash[HMAC_HASH_SIZE];
    uint8_t *buf = NULL;
    SecurityError err;
    size_t i;

    memset(key_prime, 0, HMAC_BLOCK_SIZE);

    if (keyLen > HMAC_BLOCK_SIZE) {
        err = SecureHashSHA256(key, keyLen, key_prime);
        if (err != SECURITY_SUCCESS)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    } else {
        memcpy(key_prime, key, keyLen);
    }

    for (i = 0; i < HMAC_BLOCK_SIZE; i++) {
        ipad_key[i] = key_prime[i] ^ 0x36;
        opad_key[i] = key_prime[i] ^ 0x5c;
    }

    buf = (uint8_t *)malloc(HMAC_BLOCK_SIZE + messageLen);
    if (buf == NULL) {
        SecureMemoryWipe(key_prime, sizeof(key_prime));
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    memcpy(buf, ipad_key, HMAC_BLOCK_SIZE);
    memcpy(buf + HMAC_BLOCK_SIZE, message, messageLen);
    err = SecureHashSHA256(buf, HMAC_BLOCK_SIZE + messageLen, inner_hash);
    SecureMemoryWipe(buf, HMAC_BLOCK_SIZE + messageLen);
    free(buf);
    if (err != SECURITY_SUCCESS) {
        SecureMemoryWipe(key_prime, sizeof(key_prime));
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    uint8_t outer_buf[HMAC_BLOCK_SIZE + HMAC_HASH_SIZE];
    memcpy(outer_buf, opad_key, HMAC_BLOCK_SIZE);
    memcpy(outer_buf + HMAC_BLOCK_SIZE, inner_hash, HMAC_HASH_SIZE);
    err = SecureHashSHA256(outer_buf, sizeof(outer_buf), output);

    SecureMemoryWipe(key_prime, sizeof(key_prime));
    SecureMemoryWipe(ipad_key, sizeof(ipad_key));
    SecureMemoryWipe(opad_key, sizeof(opad_key));
    SecureMemoryWipe(inner_hash, sizeof(inner_hash));
    SecureMemoryWipe(outer_buf, sizeof(outer_buf));

    return (err == SECURITY_SUCCESS) ? SECURITY_SUCCESS
                                     : SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
}

SecurityError
AES256Encrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *plaintext, size_t plaintextSize,
              uint8_t *ciphertext, uint8_t authTag[16])
{
    uint8_t digest[32];

    if (key == NULL || iv == NULL || plaintext == NULL || ciphertext == NULL ||
        authTag == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    aes256_ctr(key, iv, plaintext, ciphertext, plaintextSize);

    uint8_t *auth_data = (uint8_t *)malloc(16 + plaintextSize);
    if (auth_data == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    memcpy(auth_data, iv, 16);
    memcpy(auth_data + 16, ciphertext, plaintextSize);
    SecurityError err = hmac_sha256(key, 32, auth_data,
                                    16 + plaintextSize, digest);
    SecureMemoryWipe(auth_data, 16 + plaintextSize);
    free(auth_data);
    if (err != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    memcpy(authTag, digest, 16);
    return SECURITY_SUCCESS;
}

SecurityError
AES256Decrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *ciphertext, size_t ciphertextSize,
              const uint8_t authTag[16], uint8_t *plaintext)
{
    uint8_t digest[32];

    if (key == NULL || iv == NULL || ciphertext == NULL || authTag == NULL ||
        plaintext == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    uint8_t *auth_data = (uint8_t *)malloc(16 + ciphertextSize);
    if (auth_data == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    memcpy(auth_data, iv, 16);
    memcpy(auth_data + 16, ciphertext, ciphertextSize);
    SecurityError err = hmac_sha256(key, 32, auth_data,
                                    16 + ciphertextSize, digest);
    SecureMemoryWipe(auth_data, 16 + ciphertextSize);
    free(auth_data);
    if (err != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (!SecureCompareConstantTime(authTag, digest, 16))
        return SECURITY_ERROR_INVALID_TOKEN;

    aes256_ctr(key, iv, ciphertext, plaintext, ciphertextSize);
    return SECURITY_SUCCESS;
}

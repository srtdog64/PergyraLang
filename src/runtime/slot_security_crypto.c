/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot security crypto provider bindings.
 */

#include "slot_security.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

#ifdef _WIN32
static bool
slot_crypto_nt_success(NTSTATUS status)
{
    return status >= 0;
}
#endif

static void
slot_crypto_ctr_increment(uint8_t ctr[16])
{
    for (int j = 15; j >= 12; j--) {
        if (++ctr[j] != 0)
            break;
    }
}

#ifdef _WIN32
static SecurityError
slot_crypto_hmac_sha256(const uint8_t *key, size_t keyLen,
                        const uint8_t *message, size_t messageLen,
                        uint8_t output[32])
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    uint8_t *hashObject = NULL;
    DWORD objectSize = 0;
    DWORD hashSize = 0;
    DWORD cbData = 0;
    SecurityError result = SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (key == NULL || (message == NULL && messageLen > 0) || output == NULL
        || keyLen > (size_t)ULONG_MAX || messageLen > (size_t)ULONG_MAX)
        return SECURITY_ERROR_INVALID_TOKEN;

    if (!slot_crypto_nt_success(
            BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
                                        NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
        goto cleanup;
    if (!slot_crypto_nt_success(
            BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                              (PUCHAR)&objectSize, sizeof(objectSize),
                              &cbData, 0)))
        goto cleanup;
    if (!slot_crypto_nt_success(
            BCryptGetProperty(alg, BCRYPT_HASH_LENGTH,
                              (PUCHAR)&hashSize, sizeof(hashSize),
                              &cbData, 0))
        || hashSize != 32)
        goto cleanup;

    hashObject = (uint8_t *)malloc(objectSize);
    if (hashObject == NULL)
        goto cleanup;
    if (!slot_crypto_nt_success(
            BCryptCreateHash(alg, &hash, hashObject, objectSize,
                             (PUCHAR)key, (ULONG)keyLen, 0)))
        goto cleanup;
    if (messageLen > 0
        && !slot_crypto_nt_success(
            BCryptHashData(hash, (PUCHAR)message, (ULONG)messageLen, 0)))
        goto cleanup;
    if (!slot_crypto_nt_success(BCryptFinishHash(hash, output, 32, 0)))
        goto cleanup;

    result = SECURITY_SUCCESS;

cleanup:
    if (hash != NULL)
        BCryptDestroyHash(hash);
    if (hashObject != NULL) {
        SecureMemoryWipe(hashObject, objectSize);
        free(hashObject);
    }
    if (alg != NULL)
        BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

static SecurityError
slot_crypto_aes256_ctr(const uint8_t key[32], const uint8_t iv[16],
                       const uint8_t *in, uint8_t *out, size_t len)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE aesKey = NULL;
    uint8_t *keyObject = NULL;
    DWORD objectSize = 0;
    DWORD cbData = 0;
    DWORD outSize = 0;
    uint8_t ctr[16];
    uint8_t keystream[16];
    SecurityError result = SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (key == NULL || iv == NULL || (in == NULL && len > 0) || out == NULL)
        return SECURITY_ERROR_INVALID_TOKEN;

    if (!slot_crypto_nt_success(
            BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0)))
        goto cleanup;
    if (!slot_crypto_nt_success(
            BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                              (PUCHAR)BCRYPT_CHAIN_MODE_ECB,
                              sizeof(BCRYPT_CHAIN_MODE_ECB), 0)))
        goto cleanup;
    if (!slot_crypto_nt_success(
            BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                              (PUCHAR)&objectSize, sizeof(objectSize),
                              &cbData, 0)))
        goto cleanup;

    keyObject = (uint8_t *)malloc(objectSize);
    if (keyObject == NULL)
        goto cleanup;
    if (!slot_crypto_nt_success(
            BCryptGenerateSymmetricKey(alg, &aesKey, keyObject, objectSize,
                                       (PUCHAR)key, 32, 0)))
        goto cleanup;

    memcpy(ctr, iv, sizeof(ctr));
    for (size_t block = 0; block < len; block += 16) {
        size_t chunk = (len - block < 16) ? (len - block) : 16;
        if (!slot_crypto_nt_success(
                BCryptEncrypt(aesKey, ctr, sizeof(ctr), NULL, NULL, 0,
                              keystream, sizeof(keystream), &outSize, 0))
            || outSize != sizeof(keystream))
            goto cleanup;
        for (size_t i = 0; i < chunk; i++)
            out[block + i] = in[block + i] ^ keystream[i];
        slot_crypto_ctr_increment(ctr);
    }

    result = SECURITY_SUCCESS;

cleanup:
    SecureMemoryWipe(ctr, sizeof(ctr));
    SecureMemoryWipe(keystream, sizeof(keystream));
    if (aesKey != NULL)
        BCryptDestroyKey(aesKey);
    if (keyObject != NULL) {
        SecureMemoryWipe(keyObject, objectSize);
        free(keyObject);
    }
    if (alg != NULL)
        BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}
#else
static SecurityError
slot_crypto_hmac_sha256(const uint8_t *key, size_t keyLen,
                        const uint8_t *message, size_t messageLen,
                        uint8_t output[32])
{
    unsigned int outLen = 0;

    if (key == NULL || (message == NULL && messageLen > 0) || output == NULL
        || keyLen > (size_t)INT_MAX || messageLen > (size_t)INT_MAX)
        return SECURITY_ERROR_INVALID_TOKEN;

    if (HMAC(EVP_sha256(), key, (int)keyLen, message, messageLen,
             output, &outLen) == NULL
        || outLen != 32)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    return SECURITY_SUCCESS;
}

static SecurityError
slot_crypto_aes256_ctr(const uint8_t key[32], const uint8_t iv[16],
                       const uint8_t *in, uint8_t *out, size_t len)
{
    EVP_CIPHER_CTX *ctx;
    int outLen = 0;
    int total = 0;

    if (key == NULL || iv == NULL || (in == NULL && len > 0) || out == NULL)
        return SECURITY_ERROR_INVALID_TOKEN;
    if (len > (size_t)INT_MAX)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv) != 1
        || EVP_EncryptUpdate(ctx, out, &outLen, in, (int)len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    total = outLen;
    if (EVP_EncryptFinal_ex(ctx, out + total, &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    EVP_CIPHER_CTX_free(ctx);
    return SECURITY_SUCCESS;
}
#endif

SecurityError
AES256Encrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *plaintext, size_t plaintextSize,
              uint8_t *ciphertext, uint8_t authTag[16])
{
    uint8_t digest[32];
    uint8_t *authData = NULL;
    SecurityError err;

    if (key == NULL || iv == NULL || plaintext == NULL || ciphertext == NULL ||
        authTag == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }
    if (plaintextSize > SIZE_MAX - 16)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    err = slot_crypto_aes256_ctr(key, iv, plaintext, ciphertext, plaintextSize);
    if (err != SECURITY_SUCCESS)
        return err;

    authData = (uint8_t *)malloc(16 + plaintextSize);
    if (authData == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    memcpy(authData, iv, 16);
    memcpy(authData + 16, ciphertext, plaintextSize);
    err = slot_crypto_hmac_sha256(key, 32, authData, 16 + plaintextSize, digest);
    SecureMemoryWipe(authData, 16 + plaintextSize);
    free(authData);
    if (err != SECURITY_SUCCESS) {
        SecureMemoryWipe(digest, sizeof(digest));
        return err;
    }

    memcpy(authTag, digest, 16);
    SecureMemoryWipe(digest, sizeof(digest));
    return SECURITY_SUCCESS;
}

SecurityError
AES256Decrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *ciphertext, size_t ciphertextSize,
              const uint8_t authTag[16], uint8_t *plaintext)
{
    uint8_t digest[32];
    uint8_t *authData = NULL;
    SecurityError err;

    if (key == NULL || iv == NULL || ciphertext == NULL || authTag == NULL ||
        plaintext == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }
    if (ciphertextSize > SIZE_MAX - 16)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    authData = (uint8_t *)malloc(16 + ciphertextSize);
    if (authData == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    memcpy(authData, iv, 16);
    memcpy(authData + 16, ciphertext, ciphertextSize);
    err = slot_crypto_hmac_sha256(key, 32, authData, 16 + ciphertextSize, digest);
    SecureMemoryWipe(authData, 16 + ciphertextSize);
    free(authData);
    if (err != SECURITY_SUCCESS) {
        SecureMemoryWipe(digest, sizeof(digest));
        return err;
    }

    if (!SecureCompareConstantTime(authTag, digest, 16)) {
        SecureMemoryWipe(digest, sizeof(digest));
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    SecureMemoryWipe(digest, sizeof(digest));
    return slot_crypto_aes256_ctr(key, iv, ciphertext, plaintext, ciphertextSize);
}

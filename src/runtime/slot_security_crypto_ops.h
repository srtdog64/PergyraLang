#ifndef PERGYRA_SLOT_SECURITY_CRYPTO_OPS_H
#define PERGYRA_SLOT_SECURITY_CRYPTO_OPS_H

/*
 * Cryptographic utilities.  This owner uses platform crypto providers:
 * Windows CNG/BCrypt on Windows, OpenSSL EVP/RAND elsewhere.
 */

#ifdef _WIN32
static bool
slot_security_nt_success(NTSTATUS status)
{
    return status >= 0;
}
#endif

SecurityError
SecureRandomGenerate(uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size == 0)
        return SECURITY_ERROR_INVALID_TOKEN;

#ifdef _WIN32
    if (size > (size_t)ULONG_MAX)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    return slot_security_nt_success(
               BCryptGenRandom(NULL, buffer, (ULONG)size,
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG))
        ? SECURITY_SUCCESS
        : SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#else
    if (size > (size_t)INT_MAX)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    return RAND_bytes(buffer, (int)size) == 1
        ? SECURITY_SUCCESS
        : SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#endif
}

SecurityError
SecureHashSHA256(const uint8_t *input, size_t inputSize, uint8_t output[32])
{
    if ((input == NULL && inputSize > 0) || output == NULL)
        return SECURITY_ERROR_INVALID_TOKEN;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    uint8_t *hashObject = NULL;
    DWORD objectSize = 0;
    DWORD hashSize = 0;
    DWORD cbData = 0;
    SecurityError result = SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (inputSize > (size_t)ULONG_MAX)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (!slot_security_nt_success(
            BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
                                        NULL, 0)))
        goto cleanup;
    if (!slot_security_nt_success(
            BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                              (PUCHAR)&objectSize, sizeof(objectSize),
                              &cbData, 0)))
        goto cleanup;
    if (!slot_security_nt_success(
            BCryptGetProperty(alg, BCRYPT_HASH_LENGTH,
                              (PUCHAR)&hashSize, sizeof(hashSize),
                              &cbData, 0))
        || hashSize != 32)
        goto cleanup;

    hashObject = (uint8_t *)malloc(objectSize);
    if (hashObject == NULL)
        goto cleanup;
    if (!slot_security_nt_success(
            BCryptCreateHash(alg, &hash, hashObject, objectSize,
                             NULL, 0, 0)))
        goto cleanup;
    if (inputSize > 0
        && !slot_security_nt_success(
            BCryptHashData(hash, (PUCHAR)input, (ULONG)inputSize, 0)))
        goto cleanup;
    if (!slot_security_nt_success(BCryptFinishHash(hash, output, 32, 0)))
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
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outLen = 0;

    if (ctx == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1
        || (inputSize > 0 && EVP_DigestUpdate(ctx, input, inputSize) != 1)
        || EVP_DigestFinal_ex(ctx, output, &outLen) != 1
        || outLen != 32) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    EVP_MD_CTX_free(ctx);
    return SECURITY_SUCCESS;
#endif
}

SecurityError
TokenEncrypt(SecurityContext *context, const SecureToken *plainToken,
             EncryptedToken *encryptedToken)
{
    uint8_t iv[16] = {0};

    if (context == NULL || plainToken == NULL || encryptedToken == NULL ||
        context->masterKey == NULL) {
        slot_security_warn("token-encrypt", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                           "context, token, output, or master key is null");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    if (SecureRandomGenerate(iv, SECURITY_IV_SIZE) != SECURITY_SUCCESS) {
        slot_security_warn("token-encrypt", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "iv generation failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    memcpy(encryptedToken->encryptedToken, iv, SECURITY_IV_SIZE);
    if (AES256Encrypt(context->masterKey, iv, (const uint8_t *)plainToken,
                      sizeof(*plainToken), encryptedToken->encryptedToken + SECURITY_IV_SIZE,
                      encryptedToken->authTag) != SECURITY_SUCCESS) {
        slot_security_warn("token-encrypt", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "aes-256 encryption failed");
        SecureMemoryWipe(iv, sizeof(iv));
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    SecureMemoryWipe(iv, sizeof(iv));
    encryptedToken->keyVersion = SECURITY_VERSION;
    return SECURITY_SUCCESS;
}

SecurityError
TokenDecrypt(SecurityContext *context, const EncryptedToken *encryptedToken,
             SecureToken *plainToken)
{
    uint8_t iv[16] = {0};
    SecurityError result;

    if (context == NULL || encryptedToken == NULL || plainToken == NULL ||
        context->masterKey == NULL) {
        slot_security_warn("token-decrypt", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                           "context, encrypted token, output, or master key is null");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    memcpy(iv, encryptedToken->encryptedToken, SECURITY_IV_SIZE);
    result = AES256Decrypt(context->masterKey, iv,
                           encryptedToken->encryptedToken + SECURITY_IV_SIZE,
                           sizeof(*plainToken), encryptedToken->authTag,
                           (uint8_t *)plainToken);
    SecureMemoryWipe(iv, sizeof(iv));
    if (result != SECURITY_SUCCESS) {
        slot_security_warn("token-decrypt", result,
                           "aes-256 decryption or auth verification failed");
    }
    return result;
}

#endif /* PERGYRA_SLOT_SECURITY_CRYPTO_OPS_H */

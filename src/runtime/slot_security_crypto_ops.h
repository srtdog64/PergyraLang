#ifndef PERGYRA_SLOT_SECURITY_CRYPTO_OPS_H
#define PERGYRA_SLOT_SECURITY_CRYPTO_OPS_H

/*
 * Cryptographic utilities
 */
SecurityError
SecureRandomGenerate(uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size == 0)
        return SECURITY_ERROR_INVALID_TOKEN;

#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    BOOL success = CryptGenRandom(hCryptProv, (DWORD)size, buffer);
    CryptReleaseContext(hCryptProv, 0);

    return success ? SECURITY_SUCCESS : SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#elif defined(__linux__)
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    ssize_t bytesRead = read(fd, buffer, size);
    close(fd);

    return (bytesRead == (ssize_t)size) ? SECURITY_SUCCESS :
                                        SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#else
    /* Fallback to OpenSSL */
    return (RAND_bytes(buffer, (int)size) == 1) ? SECURITY_SUCCESS :
                                                 SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#endif
}

SecurityError
SecureHashSHA256(const uint8_t *input, size_t inputSize, uint8_t output[32])
{
    EVP_MD_CTX *ctx;
    unsigned int digestLen = 0;

    if (input == NULL || output == NULL || inputSize == 0)
        return SECURITY_ERROR_INVALID_TOKEN;

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (EVP_DigestUpdate(ctx, input, inputSize) != 1) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (EVP_DigestFinal_ex(ctx, output, &digestLen) != 1 || digestLen != 32) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    EVP_MD_CTX_free(ctx);
    return SECURITY_SUCCESS;
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
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

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
    if (result != SECURITY_SUCCESS) {
        slot_security_warn("token-decrypt", result,
                           "aes-256 decryption or auth verification failed");
    }
    return result;
}

#endif /* PERGYRA_SLOT_SECURITY_CRYPTO_OPS_H */

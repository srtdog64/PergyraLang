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

#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_EP1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_SIG0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

static void sha256_transform(SHA256_CTX *ctx, const uint8_t data[])
{
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) | ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SHA256_SIG1(m[i - 2]) + m[i - 7] + SHA256_SIG0(m[i - 15]) + m[i - 16];

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + sha256_k[i] + m[i];
        t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(SHA256_CTX *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t hash[])
{
    uint32_t i;

    i = ctx->datalen;

    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        hash[i]      = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 4]  = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 8]  = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0x000000ff);
    }
}

SecurityError
SecureHashSHA256(const uint8_t *input, size_t inputSize, uint8_t output[32])
{
    if (input == NULL || output == NULL || inputSize == 0)
        return SECURITY_ERROR_INVALID_TOKEN;

    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, input, inputSize);
    sha256_final(&ctx, output);

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

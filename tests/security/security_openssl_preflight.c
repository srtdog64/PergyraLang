#include <openssl/evp.h>

int
main(void)
{
    return EVP_sha256() == NULL;
}

#include <windows.h>
#include <bcrypt.h>

int
main(void)
{
    unsigned char b[1];
    return BCryptGenRandom(NULL, b, 1, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0;
}

#include <stdlib.h>

#if defined(_MSC_VER)
#define PGY_NOINLINE __declspec(noinline)
#else
#define PGY_NOINLINE __attribute__((noinline))
#endif

static PGY_NOINLINE int read_after_free(volatile int *value)
{
    return *value;
}

int main(void)
{
    volatile int *value = (volatile int *)malloc(sizeof(int));
    if (!value)
        return 2;
    *value = 42;
    free((void *)value);
    return read_after_free(value) == 42 ? 0 : 1;
}

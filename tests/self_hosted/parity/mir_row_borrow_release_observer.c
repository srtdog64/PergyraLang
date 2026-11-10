/* The probe's first matching String allocation is its borrowed input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *__real_malloc(size_t);
void __real_free(void *);
static const char expected[] = "borrow-retained-row";
static void *input_string;
static unsigned input_frees;

void *__wrap_malloc(size_t bytes)
{
    void *value = __real_malloc(bytes);
    if (bytes == sizeof(expected) && input_string == NULL) input_string = value;
    return value;
}

void __wrap_free(void *value)
{
    if (value != NULL && value == input_string) {
        if (memcmp(value, expected, sizeof(expected)) != 0) _Exit(88);
#ifdef PGY_SUPPRESS_INPUT_FREE
        return;
#else
        ++input_frees;
#endif
    }
    __real_free(value);
}

__attribute__((destructor)) static void verify_input_release(void)
{
    fprintf(stderr, "borrow_input_frees=%u\n", input_frees);
    if (input_string == NULL || input_frees != 1) _Exit(89);
}

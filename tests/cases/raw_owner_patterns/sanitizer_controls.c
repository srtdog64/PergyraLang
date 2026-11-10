#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
int32_t PatternIterations(void);
void PatternStart(void);
void PatternFinish(int32_t);

__attribute__((noinline)) static int32_t deliberately_invalid(void)
{
    volatile unsigned char *pointer = malloc(64);
    if (pointer == NULL) abort();
    pointer[0] = 37;
#ifdef PATTERN_CONTROL_UAF
    free((void *)pointer);
    return pointer[0];
#else
    /* Keep the real allocation observable, then lose its only pointer. */
    return pointer[0];
#endif
}
static void *worker(void *unused)
{
    (void)unused;
    return (void *)(uintptr_t)deliberately_invalid();
}
int main(void)
{
    (void)PatternIterations();
    PatternStart();
    /* Retire the worker stack and registers before LSan inspects roots. A
     * stale pointer in a still-live conservative root can conceal a leak. */
    pthread_t thread;
    void *result;
    if (pthread_create(&thread, NULL, worker, NULL) != 0) abort();
    if (pthread_join(thread, &result) != 0) abort();
    PatternFinish((int32_t)(uintptr_t)result);
    return 0;
}

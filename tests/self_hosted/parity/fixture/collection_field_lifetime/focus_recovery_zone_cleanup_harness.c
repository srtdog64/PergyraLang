/* Observe actual pthread-backed zone cleanup, not a replacement lock body. */
#ifndef PGY_FOCUS_GENERATED_C
#error "PGY_FOCUS_GENERATED_C must name the admitted generated C"
#endif
#include <pthread.h>
#include <stdio.h>
static int focus_init_count;
static int focus_destroy_count;
static int focus_lock_init(pthread_rwlock_t *lock, const pthread_rwlockattr_t *attr)
{
    int rc = pthread_rwlock_init(lock, attr);
    if (rc == 0) ++focus_init_count;
    return rc;
}
static int focus_lock_destroy(pthread_rwlock_t *lock)
{
    int rc = pthread_rwlock_destroy(lock);
    if (rc == 0) ++focus_destroy_count;
    return rc;
}
#define pthread_rwlock_init focus_lock_init
#define pthread_rwlock_destroy focus_lock_destroy
#define main pgy_generated_main
#include PGY_FOCUS_GENERATED_C
#undef main
#undef pthread_rwlock_init
#undef pthread_rwlock_destroy
int main(void)
{
    int rc = pgy_generated_main();
    if (rc != 0 || focus_init_count != 1 || focus_destroy_count != 1) {
        fprintf(stderr, "focus zone cleanup mismatch: init=%d destroy=%d rc=%d\n",
                focus_init_count, focus_destroy_count, rc);
        return 1;
    }
    puts("FOCUS ZONE CLEANUP PASS");
    return 0;
}

#ifndef PGY_RUNTIME_ZONE_SYNC_ABI_H
#define PGY_RUNTIME_ZONE_SYNC_ABI_H

#include "pgy_runtime_panic_contract.h"

#include <stdint.h>
#include <stdatomic.h>

/*
 * Zone storage and synchronization are a shared C ABI between the native and
 * self-hosted emitters.  Keep this header independent from the broader runtime
 * facade: generated units may already own their scalar/string runtime bodies.
 */
#ifdef PGY_ZONE_THREADSAFE
#include <pthread.h>

typedef pthread_rwlock_t PgyZoneLock;

#define PGY_ZONE_LOCK_FIELD    PgyZoneLock __zone_lock;
#define PGY_ZONE_LOCK_INIT(z) do { \
    if (pthread_rwlock_init(&(z)->__zone_lock, NULL) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone lock initialization failed"); \
} while (0)
#define PGY_ZONE_LOCK_DESTROY(z) do { \
    if (pthread_rwlock_destroy(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone lock destroy failed"); \
} while (0)
#define PGY_ZONE_RDLOCK(z) do { \
    if (pthread_rwlock_rdlock(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone read lock failed"); \
} while (0)
#define PGY_ZONE_WRLOCK(z) do { \
    if (pthread_rwlock_wrlock(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone write lock failed"); \
} while (0)
#define PGY_ZONE_UNLOCK(z) do { \
    if (pthread_rwlock_unlock(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone unlock failed"); \
} while (0)

#else

#define PGY_ZONE_LOCK_FIELD      /* no lock field */
#define PGY_ZONE_LOCK_INIT(z)    ((void)(z))
#define PGY_ZONE_LOCK_DESTROY(z) ((void)(z))
#define PGY_ZONE_RDLOCK(z)       ((void)(z))
#define PGY_ZONE_WRLOCK(z)       ((void)(z))
#define PGY_ZONE_UNLOCK(z)       ((void)(z))

#endif

_Static_assert(sizeof(_Atomic uint32_t) == sizeof(uint32_t),
               "Pergyra zone generation atomic must preserve uint32_t size");
_Static_assert(_Alignof(_Atomic uint32_t) == _Alignof(uint32_t),
               "Pergyra zone generation atomic must preserve uint32_t alignment");

#define PGY_ZONE_GENERATION_FIELD  _Atomic uint32_t __sync_generation;
#define PGY_ZONE_GENERATION_INC(z) \
    ((void)atomic_fetch_add_explicit(&(z)->__sync_generation, 1u, \
                                     memory_order_release))
#define PGY_ZONE_GENERATION_LOAD(z) \
    atomic_load_explicit(&(z)->__sync_generation, memory_order_acquire)

#endif /* PGY_RUNTIME_ZONE_SYNC_ABI_H */

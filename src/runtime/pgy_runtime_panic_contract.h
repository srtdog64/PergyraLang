#ifndef PGY_RUNTIME_PANIC_CONTRACT_H
#define PGY_RUNTIME_PANIC_CONTRACT_H

#include <stdio.h>
#include <stdlib.h>

#define PGY_RUNTIME_PANIC_PREFIX "[PGY PANIC]"

#define PGY_RUNTIME_PANIC_CLASS_OOM "oom"
#define PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO "divide-by-zero"
#define PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW "arithmetic-overflow"
#define PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS "out-of-bounds"
#define PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT "released-slot"
#define PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE "double-release"
#define PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN "invalid-secure-token"
#define PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH "authority-mismatch"
#define PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE "invalid-lifecycle-state"
#define PGY_RUNTIME_PANIC_CLASS_CAPABILITY_DENIED "capability-denied"
#define PGY_RUNTIME_PANIC_CLASS_BUDGET_EXCEEDED "budget-exceeded"
#define PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT "internal-invariant"

#define PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE \
    "slot write after release"
#define PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ \
    "slot read after release"
#define PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE \
    "device slot write after release"
#define PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ \
    "device slot read after release"
#define PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE \
    "secure slot write after release"
#define PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ \
    "secure slot read after release"
#define PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_RELEASE \
    "secure slot release after release"
#define PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT \
    "double release of slot"
#define PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT \
    "double release of device slot"
#define PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE \
    "invalid secure token on write"
#define PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ \
    "invalid secure token on read"
#define PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE \
    "invalid secure token on release"
#define PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE \
    "secure token denies write"
#define PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ \
    "secure token denies read"
#define PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH \
    "zone authority validation failed"
#define PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS \
    "array index out of bounds"
#define PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS \
    "slice out of bounds"
#define PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED \
    "allocation failed"
#define PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY \
    "arena out of memory"
#define PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY \
    "pool allocator out of memory"
#define PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO \
    "integer division or modulo by zero"
#define PGY_RUNTIME_PANIC_REASON_DIVISION_OVERFLOW \
    "signed division overflow (INT_MIN / -1 has no representable result)"
#define PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW \
    "signed integer addition overflow"
#define PGY_RUNTIME_PANIC_REASON_MULTIPLICATION_OVERFLOW \
    "signed integer multiplication overflow"
#define PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR \
    "Result unwrap on Err value"
#define PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE \
    "Option unwrap on None value"
#define PGY_RUNTIME_PANIC_REASON_INVALID_LIFECYCLE_STATE \
    "lifecycle operation applied in a state that forbids it"
#define PGY_RUNTIME_PANIC_REASON_CAPABILITY_DENIED \
    "operation requires a capability the content manifest did not grant"
#define PGY_RUNTIME_PANIC_REASON_BUDGET_EXCEEDED \
    "operation would exceed the resource budget the content manifest imposed"
#define PGY_RUNTIME_PANIC_REASON_FLOAT_TO_INT_OUT_OF_RANGE \
    "float-to-int conversion out of range (NaN or beyond the target bounds)"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define PGY_RUNTIME_NORETURN _Noreturn
#elif defined(__GNUC__) || defined(__clang__)
#  define PGY_RUNTIME_NORETURN __attribute__((noreturn))
#else
#  define PGY_RUNTIME_NORETURN
#endif

static inline PGY_RUNTIME_NORETURN void
pgy_runtime_panic_emit(const char *panic_class, const char *reason,
                       const char *file, int line)
{
    fprintf(stderr, "%s %s:%d class=%s reason=%s\n",
            PGY_RUNTIME_PANIC_PREFIX,
            file != NULL ? file : "<runtime>",
            line,
            panic_class != NULL ? panic_class
                                : PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
            reason != NULL ? reason : "runtime invariant failed");
    abort();
}

#define PGY_RUNTIME_PANIC_AT(panic_class, reason, file, line) \
    do { \
        fprintf(stderr, "%s %s:%d class=%s reason=%s\n", \
                PGY_RUNTIME_PANIC_PREFIX, \
                (file) != NULL ? (file) : "<runtime>", \
                (line), \
                (panic_class) != NULL ? (panic_class) \
                    : PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                (reason) != NULL ? (reason) : "runtime invariant failed"); \
        abort(); \
    } while (0)

#define PGY_RUNTIME_PANIC(panic_class, reason) \
    PGY_RUNTIME_PANIC_AT((panic_class), (reason), __FILE__, __LINE__)

#endif /* PGY_RUNTIME_PANIC_CONTRACT_H */

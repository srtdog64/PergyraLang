#ifndef PGY_RUNTIME_SLOT_STATUS_H
#define PGY_RUNTIME_SLOT_STATUS_H

#include <stdbool.h>

typedef enum
{
    PGY_RUNTIME_SLOT_STATUS_OK = 0,
    PGY_RUNTIME_SLOT_STATUS_NULL_SLOT,
    PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT,
    PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT,
    PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE,
    PGY_RUNTIME_SLOT_STATUS_NULL_TOKEN,
    PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN,
    PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_READ,
    PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_WRITE,
    PGY_RUNTIME_SLOT_STATUS_OUT_OF_MEMORY,
    PGY_RUNTIME_SLOT_STATUS_INVALID_HANDLE,
    PGY_RUNTIME_SLOT_STATUS_TYPE_MISMATCH,
    PGY_RUNTIME_SLOT_STATUS_SLOT_NOT_FOUND,
    PGY_RUNTIME_SLOT_STATUS_PERMISSION_DENIED,
    PGY_RUNTIME_SLOT_STATUS_TTL_EXPIRED,
    PGY_RUNTIME_SLOT_STATUS_THREAD_VIOLATION,
    PGY_RUNTIME_SLOT_STATUS_PINNED,
    PGY_RUNTIME_SLOT_STATUS_INVALID_PIN,
    PGY_RUNTIME_SLOT_STATUS_ID_EXHAUSTED
} PgyRuntimeSlotStatus;

typedef struct
{
    PgyRuntimeSlotStatus status;
    const char *name;
    const char *stage;
    const char *operation;
    bool recoverable;
} PgyRuntimeSlotFailure;

typedef enum
{
    PGY_RUNTIME_SLOT_RESULT_OK = 0,
    PGY_RUNTIME_SLOT_RESULT_ERR
} PgyRuntimeSlotResultTag;

#define PGY_RUNTIME_SLOT_RESULT_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    PgyRuntimeSlotResultTag tag; \
    union { \
        CType ok; \
        PgyRuntimeSlotFailure err; \
    }; \
} PgyRuntimeSlotResult_##SuffixName; \
\
static inline PgyRuntimeSlotResult_##SuffixName \
pgy_runtime_slot_result_ok_##SuffixName(CType value) \
{ \
    PgyRuntimeSlotResult_##SuffixName result; \
    result.tag = PGY_RUNTIME_SLOT_RESULT_OK; \
    result.ok = value; \
    return result; \
} \
\
static inline PgyRuntimeSlotResult_##SuffixName \
pgy_runtime_slot_result_err_##SuffixName(PgyRuntimeSlotFailure failure) \
{ \
    PgyRuntimeSlotResult_##SuffixName result; \
    result.tag = PGY_RUNTIME_SLOT_RESULT_ERR; \
    result.err = failure; \
    return result; \
} \
\
static inline bool \
pgy_runtime_slot_result_is_ok_##SuffixName( \
    const PgyRuntimeSlotResult_##SuffixName *result) \
{ \
    return result != NULL && result->tag == PGY_RUNTIME_SLOT_RESULT_OK; \
}

static inline bool
pgy_runtime_slot_status_ok(PgyRuntimeSlotStatus status)
{
    return status == PGY_RUNTIME_SLOT_STATUS_OK;
}

static inline const char *
pgy_runtime_slot_status_name(PgyRuntimeSlotStatus status)
{
    switch (status) {
    case PGY_RUNTIME_SLOT_STATUS_OK:
        return "ok";
    case PGY_RUNTIME_SLOT_STATUS_NULL_SLOT:
        return "null-slot";
    case PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT:
        return "null-output";
    case PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT:
        return "released-slot";
    case PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE:
        return "double-release";
    case PGY_RUNTIME_SLOT_STATUS_NULL_TOKEN:
        return "null-token";
    case PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN:
        return "invalid-token";
    case PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_READ:
        return "token-denies-read";
    case PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_WRITE:
        return "token-denies-write";
    case PGY_RUNTIME_SLOT_STATUS_OUT_OF_MEMORY:
        return "out-of-memory";
    case PGY_RUNTIME_SLOT_STATUS_INVALID_HANDLE:
        return "invalid-handle";
    case PGY_RUNTIME_SLOT_STATUS_TYPE_MISMATCH:
        return "type-mismatch";
    case PGY_RUNTIME_SLOT_STATUS_SLOT_NOT_FOUND:
        return "slot-not-found";
    case PGY_RUNTIME_SLOT_STATUS_PERMISSION_DENIED:
        return "permission-denied";
    case PGY_RUNTIME_SLOT_STATUS_TTL_EXPIRED:
        return "ttl-expired";
    case PGY_RUNTIME_SLOT_STATUS_THREAD_VIOLATION:
        return "thread-violation";
    case PGY_RUNTIME_SLOT_STATUS_PINNED:
        return "pinned";
    case PGY_RUNTIME_SLOT_STATUS_INVALID_PIN:
        return "invalid-pin";
    case PGY_RUNTIME_SLOT_STATUS_ID_EXHAUSTED:
        return "id-exhausted";
    default:
        return "unknown";
    }
}

static inline bool
pgy_runtime_slot_status_boundary_recoverable(PgyRuntimeSlotStatus status)
{
    switch (status) {
    case PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT:
    case PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE:
    case PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN:
    case PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_READ:
    case PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_WRITE:
    case PGY_RUNTIME_SLOT_STATUS_INVALID_HANDLE:
    case PGY_RUNTIME_SLOT_STATUS_TYPE_MISMATCH:
    case PGY_RUNTIME_SLOT_STATUS_SLOT_NOT_FOUND:
    case PGY_RUNTIME_SLOT_STATUS_PERMISSION_DENIED:
    case PGY_RUNTIME_SLOT_STATUS_TTL_EXPIRED:
    case PGY_RUNTIME_SLOT_STATUS_THREAD_VIOLATION:
    case PGY_RUNTIME_SLOT_STATUS_PINNED:
    case PGY_RUNTIME_SLOT_STATUS_INVALID_PIN:
        return true;
    case PGY_RUNTIME_SLOT_STATUS_OK:
    case PGY_RUNTIME_SLOT_STATUS_NULL_SLOT:
    case PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT:
    case PGY_RUNTIME_SLOT_STATUS_NULL_TOKEN:
    case PGY_RUNTIME_SLOT_STATUS_OUT_OF_MEMORY:
    case PGY_RUNTIME_SLOT_STATUS_ID_EXHAUSTED:
    default:
        return false;
    }
}

static inline PgyRuntimeSlotFailure
pgy_runtime_slot_failure_from_status(PgyRuntimeSlotStatus status,
                                     const char *stage,
                                     const char *operation)
{
    PgyRuntimeSlotFailure failure;

    failure.status = status;
    failure.name = pgy_runtime_slot_status_name(status);
    failure.stage = stage != NULL ? stage : "<stage>";
    failure.operation = operation != NULL ? operation : "<operation>";
    failure.recoverable = pgy_runtime_slot_status_boundary_recoverable(status);
    return failure;
}

#endif /* PGY_RUNTIME_SLOT_STATUS_H */

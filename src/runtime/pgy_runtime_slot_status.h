#ifndef PGY_RUNTIME_SLOT_STATUS_H
#define PGY_RUNTIME_SLOT_STATUS_H

#include <stdbool.h>

typedef enum
{
    PGY_RUNTIME_SLOT_STATUS_OK = 0,
    PGY_RUNTIME_SLOT_STATUS_NULL_SLOT,
    PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT,
    PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT,
    PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE
} PgyRuntimeSlotStatus;

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
    default:
        return "unknown";
    }
}

#endif /* PGY_RUNTIME_SLOT_STATUS_H */

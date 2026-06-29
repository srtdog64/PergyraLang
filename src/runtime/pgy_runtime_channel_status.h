#ifndef PGY_RUNTIME_CHANNEL_STATUS_H
#define PGY_RUNTIME_CHANNEL_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "../common/execution_lane_kind.h"

typedef enum
{
    PGY_RUNTIME_CHANNEL_STATUS_OK = 0,
    PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL,
    PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED,
    PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY,
    PGY_RUNTIME_CHANNEL_STATUS_EMPTY,
    PGY_RUNTIME_CHANNEL_STATUS_FULL,
    PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT,
    PGY_RUNTIME_CHANNEL_STATUS_ALLOC_FAILED,
    PGY_RUNTIME_CHANNEL_STATUS_REJECTED
} PgyRuntimeChannelStatus;

typedef struct
{
    PgyRuntimeChannelStatus status;
    const char *name;
    const char *stage;
    const char *operation;
    bool recoverable;
} PgyRuntimeChannelFailure;

typedef enum
{
    PGY_RUNTIME_CHANNEL_RESULT_OK = 0,
    PGY_RUNTIME_CHANNEL_RESULT_ERR
} PgyRuntimeChannelResultTag;

typedef struct
{
    PgyRuntimeChannelResultTag tag;
    union {
        int32_t ok;
        PgyRuntimeChannelFailure err;
    };
} PgyRuntimeChannelIntResult;

typedef struct
{
    PgyRuntimeChannelResultTag tag;
    union {
        char *ok;
        PgyRuntimeChannelFailure err;
    };
} PgyRuntimeChannelStringResult;

static inline const char *
pgy_runtime_channel_status_name(PgyRuntimeChannelStatus status)
{
    switch (status) {
    case PGY_RUNTIME_CHANNEL_STATUS_OK:
        return "ok";
    case PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL:
        return "null-channel";
    case PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED:
        return "uninitialized";
    case PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY:
        return "closed-empty";
    case PGY_RUNTIME_CHANNEL_STATUS_EMPTY:
        return "empty";
    case PGY_RUNTIME_CHANNEL_STATUS_FULL:
        return "full";
    case PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT:
        return "timeout";
    case PGY_RUNTIME_CHANNEL_STATUS_ALLOC_FAILED:
        return "alloc-failed";
    case PGY_RUNTIME_CHANNEL_STATUS_REJECTED:
        return "rejected";
    default:
        return "unknown";
    }
}

static inline bool
pgy_runtime_channel_lane_accepts_boundary(PgyExecutionLane lane)
{
    return lane == PGY_LANE_PINNED_ZONE;
}

static inline bool
pgy_runtime_channel_status_boundary_recoverable(
    PgyRuntimeChannelStatus status)
{
    switch (status) {
    case PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY:
    case PGY_RUNTIME_CHANNEL_STATUS_EMPTY:
    case PGY_RUNTIME_CHANNEL_STATUS_FULL:
    case PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT:
        return true;
    case PGY_RUNTIME_CHANNEL_STATUS_OK:
    case PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL:
    case PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED:
    case PGY_RUNTIME_CHANNEL_STATUS_ALLOC_FAILED:
    case PGY_RUNTIME_CHANNEL_STATUS_REJECTED:
    default:
        return false;
    }
}

static inline PgyRuntimeChannelFailure
pgy_runtime_channel_failure_from_status(PgyRuntimeChannelStatus status,
                                        const char *operation)
{
    PgyRuntimeChannelFailure failure;

    failure.status = status;
    failure.name = pgy_runtime_channel_status_name(status);
    failure.stage = "channel-boundary";
    failure.operation = operation != NULL ? operation : "<operation>";
    failure.recoverable =
        pgy_runtime_channel_status_boundary_recoverable(status);
    return failure;
}

#endif /* PGY_RUNTIME_CHANNEL_STATUS_H */

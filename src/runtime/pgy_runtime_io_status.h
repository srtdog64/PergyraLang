#ifndef PGY_RUNTIME_IO_STATUS_H
#define PGY_RUNTIME_IO_STATUS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PGY_RUNTIME_IO_STATUS_OK = 0,
    PGY_RUNTIME_IO_STATUS_NULL_PATH,
    PGY_RUNTIME_IO_STATUS_NULL_MODE,
    PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED,
    PGY_RUNTIME_IO_STATUS_OPEN_FAILED,
    PGY_RUNTIME_IO_STATUS_NO_FREE_HANDLE,
    PGY_RUNTIME_IO_STATUS_INVALID_HANDLE,
    PGY_RUNTIME_IO_STATUS_EOF,
    PGY_RUNTIME_IO_STATUS_SEEK_FAILED,
    PGY_RUNTIME_IO_STATUS_TELL_FAILED,
    PGY_RUNTIME_IO_STATUS_TOO_LARGE,
    PGY_RUNTIME_IO_STATUS_ALLOC_FAILED,
    PGY_RUNTIME_IO_STATUS_READ_FAILED,
    PGY_RUNTIME_IO_STATUS_WRITE_FAILED
} PgyRuntimeIoStatus;

typedef struct
{
    PgyRuntimeIoStatus status;
    const char *name;
    const char *stage;
    const char *operation;
    bool recoverable;
} PgyRuntimeIoFailure;

typedef enum
{
    PGY_RUNTIME_IO_RESULT_OK = 0,
    PGY_RUNTIME_IO_RESULT_ERR
} PgyRuntimeIoResultTag;

typedef struct
{
    PgyRuntimeIoResultTag tag;
    union {
        int32_t ok;
        PgyRuntimeIoFailure err;
    };
} PgyRuntimeIoIntResult;

typedef struct
{
    PgyRuntimeIoResultTag tag;
    union {
        char *ok;
        PgyRuntimeIoFailure err;
    };
} PgyRuntimeIoStringResult;

typedef struct
{
    PgyRuntimeIoResultTag tag;
    PgyRuntimeIoFailure err;
} PgyRuntimeIoVoidResult;

static inline const char *
pgy_runtime_io_status_name(PgyRuntimeIoStatus status)
{
    switch (status) {
    case PGY_RUNTIME_IO_STATUS_OK:
        return "ok";
    case PGY_RUNTIME_IO_STATUS_NULL_PATH:
        return "null-path";
    case PGY_RUNTIME_IO_STATUS_NULL_MODE:
        return "null-mode";
    case PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED:
        return "resolve-failed";
    case PGY_RUNTIME_IO_STATUS_OPEN_FAILED:
        return "open-failed";
    case PGY_RUNTIME_IO_STATUS_NO_FREE_HANDLE:
        return "no-free-handle";
    case PGY_RUNTIME_IO_STATUS_INVALID_HANDLE:
        return "invalid-handle";
    case PGY_RUNTIME_IO_STATUS_EOF:
        return "eof";
    case PGY_RUNTIME_IO_STATUS_SEEK_FAILED:
        return "seek-failed";
    case PGY_RUNTIME_IO_STATUS_TELL_FAILED:
        return "tell-failed";
    case PGY_RUNTIME_IO_STATUS_TOO_LARGE:
        return "too-large";
    case PGY_RUNTIME_IO_STATUS_ALLOC_FAILED:
        return "alloc-failed";
    case PGY_RUNTIME_IO_STATUS_READ_FAILED:
        return "read-failed";
    case PGY_RUNTIME_IO_STATUS_WRITE_FAILED:
        return "write-failed";
    default:
        return "unknown";
    }
}

static inline bool
pgy_runtime_io_status_boundary_recoverable(PgyRuntimeIoStatus status)
{
    switch (status) {
    case PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED:
    case PGY_RUNTIME_IO_STATUS_OPEN_FAILED:
    case PGY_RUNTIME_IO_STATUS_NO_FREE_HANDLE:
    case PGY_RUNTIME_IO_STATUS_INVALID_HANDLE:
    case PGY_RUNTIME_IO_STATUS_EOF:
    case PGY_RUNTIME_IO_STATUS_SEEK_FAILED:
    case PGY_RUNTIME_IO_STATUS_TELL_FAILED:
    case PGY_RUNTIME_IO_STATUS_TOO_LARGE:
    case PGY_RUNTIME_IO_STATUS_READ_FAILED:
    case PGY_RUNTIME_IO_STATUS_WRITE_FAILED:
        return true;
    case PGY_RUNTIME_IO_STATUS_OK:
    case PGY_RUNTIME_IO_STATUS_NULL_PATH:
    case PGY_RUNTIME_IO_STATUS_NULL_MODE:
    case PGY_RUNTIME_IO_STATUS_ALLOC_FAILED:
    default:
        return false;
    }
}

static inline PgyRuntimeIoFailure
pgy_runtime_io_failure_from_status(PgyRuntimeIoStatus status,
                                   const char *stage,
                                   const char *operation)
{
    PgyRuntimeIoFailure failure;

    failure.status = status;
    failure.name = pgy_runtime_io_status_name(status);
    failure.stage = stage != NULL ? stage : "<stage>";
    failure.operation = operation != NULL ? operation : "<operation>";
    failure.recoverable = pgy_runtime_io_status_boundary_recoverable(status);
    return failure;
}

static inline PgyRuntimeIoIntResult
pgy_runtime_io_int_ok(int32_t value)
{
    PgyRuntimeIoIntResult result;
    result.tag = PGY_RUNTIME_IO_RESULT_OK;
    result.ok = value;
    return result;
}

static inline PgyRuntimeIoIntResult
pgy_runtime_io_int_err(PgyRuntimeIoFailure failure)
{
    PgyRuntimeIoIntResult result;
    result.tag = PGY_RUNTIME_IO_RESULT_ERR;
    result.err = failure;
    return result;
}

static inline PgyRuntimeIoStringResult
pgy_runtime_io_string_ok(char *value)
{
    PgyRuntimeIoStringResult result;
    result.tag = PGY_RUNTIME_IO_RESULT_OK;
    result.ok = value;
    return result;
}

static inline PgyRuntimeIoStringResult
pgy_runtime_io_string_err(PgyRuntimeIoFailure failure)
{
    PgyRuntimeIoStringResult result;
    result.tag = PGY_RUNTIME_IO_RESULT_ERR;
    result.err = failure;
    return result;
}

static inline PgyRuntimeIoVoidResult
pgy_runtime_io_void_ok(void)
{
    PgyRuntimeIoVoidResult result;
    result.tag = PGY_RUNTIME_IO_RESULT_OK;
    result.err = pgy_runtime_io_failure_from_status(
        PGY_RUNTIME_IO_STATUS_OK, "io-boundary", "ok");
    return result;
}

static inline PgyRuntimeIoVoidResult
pgy_runtime_io_void_err(PgyRuntimeIoFailure failure)
{
    PgyRuntimeIoVoidResult result;
    result.tag = PGY_RUNTIME_IO_RESULT_ERR;
    result.err = failure;
    return result;
}

#endif /* PGY_RUNTIME_IO_STATUS_H */

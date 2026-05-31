/* =================================================================
 * Device Slot operations - extern wrappers for LLVM linker
 * ================================================================= */

#include "pgy_runtime_slot_status.h"

#define PGY_DEFINE_DEVICE_SLOT_EXPORTS(Suffix, CType, ZeroExpr)                 \
typedef struct {                                                                \
    CType value;                                                                \
    bool  claimed;                                                              \
} PgyDeviceSlot_##Suffix;                                                       \
                                                                                \
typedef struct {                                                                \
    PgyDeviceSlot_##Suffix *slot;                                               \
} PgyDeviceReadTaskArg_##Suffix;                                                \
                                                                                \
PgyDeviceSlot_##Suffix pgy_claim_device_##Suffix(void)                          \
{                                                                               \
    PgyDeviceSlot_##Suffix s;                                                   \
    s.value = (ZeroExpr);                                                       \
    s.claimed = true;                                                           \
    return s;                                                                   \
}                                                                               \
                                                                                \
void pgy_device_write_##Suffix(PgyDeviceSlot_##Suffix *s, CType v)              \
{                                                                               \
    if (s == NULL || !s->claimed) {                                             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,                \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE); \
    }                                                                           \
    s->value = v;                                                               \
}                                                                               \
                                                                                \
PgyRuntimeSlotStatus pgy_try_device_write_##Suffix(PgyDeviceSlot_##Suffix *s,   \
                                                   CType v)                     \
{                                                                               \
    if (s == NULL)                                                              \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT;                               \
    if (!s->claimed)                                                            \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT;                           \
    s->value = v;                                                               \
    return PGY_RUNTIME_SLOT_STATUS_OK;                                          \
}                                                                               \
                                                                                \
CType pgy_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)                       \
{                                                                               \
    if (s == NULL || !s->claimed) {                                             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,                \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ);  \
    }                                                                           \
    return s->value;                                                            \
}                                                                               \
                                                                                \
PgyRuntimeSlotStatus pgy_try_device_read_##Suffix(PgyDeviceSlot_##Suffix *s,    \
                                                  CType *out)                   \
{                                                                               \
    if (out == NULL)                                                            \
        return PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT;                             \
    if (s == NULL)                                                              \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT;                               \
    if (!s->claimed)                                                            \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT;                           \
    *out = s->value;                                                            \
    return PGY_RUNTIME_SLOT_STATUS_OK;                                          \
}                                                                               \
                                                                                \
PgyRuntimeSlotResult_##Suffix                                                   \
pgy_try_device_read_result_##Suffix(PgyDeviceSlot_##Suffix *s)                  \
{                                                                               \
    CType value;                                                                \
    PgyRuntimeSlotStatus status;                                                \
    memset(&value, 0, sizeof(value));                                           \
    status = pgy_try_device_read_##Suffix(s, &value);                           \
    if (status == PGY_RUNTIME_SLOT_STATUS_OK)                                   \
        return pgy_runtime_slot_result_ok_##Suffix(value);                      \
    return pgy_runtime_slot_result_err_##Suffix(                                \
        pgy_runtime_slot_failure_from_status(status,                            \
                                            "device-slot-boundary", "read"));  \
}                                                                               \
                                                                                \
void pgy_release_device_##Suffix(PgyDeviceSlot_##Suffix *s)                     \
{                                                                               \
    if (s == NULL) {                                                            \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,           \
                          "device slot release on null slot");                 \
    }                                                                           \
    if (!s->claimed) {                                                          \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,               \
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT); \
    }                                                                           \
    s->value = (ZeroExpr);                                                      \
    s->claimed = false;                                                         \
}                                                                               \
                                                                                \
PgyRuntimeSlotStatus pgy_try_release_device_##Suffix(PgyDeviceSlot_##Suffix *s) \
{                                                                               \
    if (s == NULL)                                                              \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT;                               \
    if (!s->claimed)                                                            \
        return PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE;                          \
    s->value = (ZeroExpr);                                                      \
    s->claimed = false;                                                         \
    return PGY_RUNTIME_SLOT_STATUS_OK;                                          \
}                                                                               \
                                                                                \
static void *pgy_device_read_task_##Suffix(void *raw)                           \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)raw;                                   \
    PgyRuntimeSlotResult_##Suffix read_result;                                  \
    if (arg == NULL)                                                            \
        return NULL;                                                            \
    read_result = pgy_try_device_read_result_##Suffix(arg->slot);               \
    if (read_result.tag != PGY_RUNTIME_SLOT_RESULT_OK) {                        \
        free(arg);                                                              \
        return NULL;                                                            \
    }                                                                           \
    CType *payload = (CType *)malloc(sizeof(CType));                            \
    if (payload == NULL) {                                                      \
        free(arg);                                                              \
        return NULL;                                                            \
    }                                                                           \
    *payload = read_result.ok;                                                  \
    free(arg);                                                                  \
    return payload;                                                             \
}                                                                               \
                                                                                \
PgyTaskHandle pgy_submit_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)        \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)malloc(sizeof(PgyDeviceReadTaskArg_##Suffix)); \
    if (arg == NULL) {                                                          \
        PgyTaskHandle empty = {0};                                              \
        return empty;                                                           \
    }                                                                           \
    arg->slot = s;                                                              \
    return pgy_spawn(pgy_device_read_task_##Suffix, arg);                       \
}

PGY_DEFINE_DEVICE_SLOT_EXPORTS(Int, int32_t, 0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Long, int64_t, 0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Float, float, 0.0f)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Double, double, 0.0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Bool, bool, false)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(String, char *, NULL)

/* =================================================================
 * Device Slot operations - extern wrappers for LLVM linker
 * ================================================================= */

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
CType pgy_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)                       \
{                                                                               \
    if (s == NULL || !s->claimed) {                                             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,                \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ);  \
    }                                                                           \
    return s->value;                                                            \
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
static void *pgy_device_read_task_##Suffix(void *raw)                           \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)raw;                                   \
    CType *result = (CType *)malloc(sizeof(CType));                             \
    if (result == NULL) {                                                       \
        free(arg);                                                              \
        return NULL;                                                            \
    }                                                                           \
    *result = pgy_device_read_##Suffix(arg->slot);                              \
    free(arg);                                                                  \
    return result;                                                              \
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

/* =================================================================
 * Secure slot operations - extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_SECURE_SLOT_EXPORTS(Suffix, CType, ZeroExpr)                \
static uint64_t pgy_secure_token_counter_##Suffix = 0x9e3779b97f4a7c15ULL;     \
                                                                               \
typedef struct {                                                               \
    CType    value;                                                            \
    bool     occupied;                                                         \
    uint64_t token;                                                            \
} PgySecureSlot_##Suffix;                                                      \
                                                                               \
typedef struct {                                                               \
    uint64_t id;                                                               \
    bool     can_write;                                                        \
    bool     can_read;                                                         \
} PgyToken_##Suffix;                                                           \
                                                                               \
PgySecureSlot_##Suffix pgy_claim_secure_##Suffix(PgyToken_##Suffix *out_token) \
{                                                                              \
    PgySecureSlot_##Suffix s;                                                  \
    uint64_t id = ++pgy_secure_token_counter_##Suffix;                         \
    if (id == 0) {                                                             \
        id = ++pgy_secure_token_counter_##Suffix;                              \
    }                                                                          \
    s.value = (ZeroExpr);                                                      \
    s.occupied = true;                                                         \
    s.token = id;                                                              \
    if (out_token != NULL) {                                                   \
        out_token->id = id;                                                    \
        out_token->can_write = true;                                           \
        out_token->can_read = true;                                            \
    }                                                                          \
    return s;                                                                  \
}                                                                              \
                                                                               \
void pgy_secure_write_##Suffix(PgySecureSlot_##Suffix *s, CType v,             \
                               const PgyToken_##Suffix *t)                     \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot write operand");                   \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    if (!t->can_write)                                                          \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE);  \
    s->value = v;                                                              \
}                                                                              \
                                                                               \
CType pgy_secure_read_##Suffix(PgySecureSlot_##Suffix *s,                      \
                               const PgyToken_##Suffix *t)                     \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot read operand");                    \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ);  \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ);  \
    if (!t->can_read)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ);   \
    return s->value;                                                           \
}                                                                              \
                                                                               \
void pgy_secure_release_##Suffix(PgySecureSlot_##Suffix *s,                    \
                                 const PgyToken_##Suffix *t)                   \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot release operand");                 \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,              \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_RELEASE); \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE); \
    s->occupied = false;                                                       \
    s->token = 0;                                                              \
}                                                                              \
                                                                               \
typedef struct {                                                               \
    PgySecureSlot_##Suffix  *slot;                                             \
    const PgyToken_##Suffix *token;                                            \
    bool                     active;                                           \
    bool                     can_write;                                        \
} PgyPinnedSecureSlotView_##Suffix;                                            \
                                                                               \
PgyPinnedSecureSlotView_##Suffix                                               \
pgy_secure_pin_read_##Suffix(PgySecureSlot_##Suffix *s,                        \
                             const PgyToken_##Suffix *t)                       \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot pin read operand");                \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ);  \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ);  \
    if (!t->can_read)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ);   \
    PgyPinnedSecureSlotView_##Suffix view;                                      \
    view.slot = s;                                                             \
    view.token = t;                                                            \
    view.active = true;                                                        \
    view.can_write = false;                                                    \
    return view;                                                               \
}                                                                              \
                                                                               \
PgyPinnedSecureSlotView_##Suffix                                               \
pgy_secure_pin_write_##Suffix(PgySecureSlot_##Suffix *s,                       \
                              const PgyToken_##Suffix *t)                      \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot pin write operand");               \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    if (!t->can_write)                                                          \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE);  \
    PgyPinnedSecureSlotView_##Suffix view;                                      \
    view.slot = s;                                                             \
    view.token = t;                                                            \
    view.active = true;                                                        \
    view.can_write = true;                                                     \
    return view;                                                               \
}                                                                              \
                                                                               \
void pgy_secure_pin_read_init_##Suffix(PgyPinnedSecureSlotView_##Suffix *out,  \
                                       PgySecureSlot_##Suffix *s,               \
                                       const PgyToken_##Suffix *t)              \
{                                                                              \
    if (out == NULL)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot pin read out");                    \
    *out = pgy_secure_pin_read_##Suffix(s, t);                                  \
}                                                                              \
                                                                               \
void pgy_secure_pin_write_init_##Suffix(PgyPinnedSecureSlotView_##Suffix *out, \
                                        PgySecureSlot_##Suffix *s,              \
                                        const PgyToken_##Suffix *t)             \
{                                                                              \
    if (out == NULL)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot pin write out");                   \
    *out = pgy_secure_pin_write_##Suffix(s, t);                                 \
}                                                                              \
                                                                               \
void pgy_secure_unpin_##Suffix(PgyPinnedSecureSlotView_##Suffix *view)         \
{                                                                              \
    if (view == NULL)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot unpin");                           \
    if (!view->active || view->slot == NULL || view->token == NULL)             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "inactive secure slot unpin");                       \
    view->active = false;                                                      \
    view->slot = NULL;                                                         \
    view->token = NULL;                                                        \
}

PGY_DEFINE_SECURE_SLOT_EXPORTS(Int, int32_t, 0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Long, int64_t, 0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Float, float, 0.0f)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Double, double, 0.0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Bool, bool, false)
PGY_DEFINE_SECURE_SLOT_EXPORTS(String, char *, NULL)

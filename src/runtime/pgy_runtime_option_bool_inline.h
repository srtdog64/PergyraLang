#ifndef PGY_RUNTIME_OPTION_BOOL_INLINE_H
#define PGY_RUNTIME_OPTION_BOOL_INLINE_H

#include <stdbool.h>
#include <stdint.h>

#ifndef PGY_RUNTIME_OPTION_TAG_DEFINED
#define PGY_RUNTIME_OPTION_TAG_DEFINED
typedef enum {
    PgyOptionSome,
    PgyOptionNone
} PgyOptionTag;
#endif

typedef struct {
    PgyOptionTag tag;
    bool value;
} PgyOption_Bool;

static inline PgyOption_Bool
pgy_option_some_Bool(bool value)
{
    PgyOption_Bool o;
    o.tag = PgyOptionSome;
    o.value = value;
    return o;
}

static inline PgyOption_Bool
pgy_option_none_Bool(void)
{
    PgyOption_Bool o;
    o.tag = PgyOptionNone;
    o.value = false;
    return o;
}

static inline bool
pgy_option_is_some_Bool(PgyOption_Bool *o)
{
    return o->tag == PgyOptionSome;
}

#define Some_Bool(...)          pgy_option_some_Bool(__VA_ARGS__)
#define None_Bool()             pgy_option_none_Bool()
#define IsSome_Bool(o)          ((o).tag == PgyOptionSome)
#define IsNone_Bool(o)          ((o).tag == PgyOptionNone)

#endif /* PGY_RUNTIME_OPTION_BOOL_INLINE_H */

#if defined(PGY_RUNTIME_OPTION_BOOL_ENABLE_UNWRAP) \
    && !defined(PGY_RUNTIME_OPTION_BOOL_UNWRAP_DEFINED)
#define PGY_RUNTIME_OPTION_BOOL_UNWRAP_DEFINED

static inline bool
pgy_option_unwrap_Bool(PgyOption_Bool *o)
{
    if (o->tag != PgyOptionSome) {
        PGY_PANIC(PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE);
    }
    return o->value;
}

#define UnwrapOption_Bool(o) \
    pgy_option_unwrap_Bool(&(PgyOption_Bool){(o).tag, (o).value})

#endif /* PGY_RUNTIME_OPTION_BOOL_ENABLE_UNWRAP */

#ifndef PGY_RUNTIME_OPTION_BOOL_INLINE_H
#define PGY_RUNTIME_OPTION_BOOL_INLINE_H

#include "pgy_runtime_linkage.h"

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

PGY_RT_DECL PgyOption_Bool
pgy_option_some_Bool(bool value)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyOption_Bool o;
    o.tag = PgyOptionSome;
    o.value = value;
    return o;
}
#else
;
#endif


PGY_RT_DECL PgyOption_Bool
pgy_option_none_Bool(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyOption_Bool o;
    o.tag = PgyOptionNone;
    o.value = false;
    return o;
}
#else
;
#endif


PGY_RT_DECL bool
pgy_option_is_some_Bool(PgyOption_Bool *o)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return o->tag == PgyOptionSome;
}
#else
;
#endif


#define Some_Bool(...)          pgy_option_some_Bool(__VA_ARGS__)
#define None_Bool()             pgy_option_none_Bool()
#define IsSome_Bool(o)          ((o).tag == PgyOptionSome)
#define IsNone_Bool(o)          ((o).tag == PgyOptionNone)

#endif /* PGY_RUNTIME_OPTION_BOOL_INLINE_H */

#if defined(PGY_RUNTIME_OPTION_BOOL_ENABLE_UNWRAP) \
    && !defined(PGY_RUNTIME_OPTION_BOOL_UNWRAP_DEFINED)
#define PGY_RUNTIME_OPTION_BOOL_UNWRAP_DEFINED

PGY_RT_DECL bool
pgy_option_unwrap_Bool(PgyOption_Bool *o)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (o->tag != PgyOptionSome) {
        PGY_PANIC(PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE);
    }
    return o->value;
}
#else
;
#endif


#define UnwrapOption_Bool(o) \
    pgy_option_unwrap_Bool(&(PgyOption_Bool){(o).tag, (o).value})

#endif /* PGY_RUNTIME_OPTION_BOOL_ENABLE_UNWRAP */

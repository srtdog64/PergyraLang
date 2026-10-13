#ifndef PGY_RUNTIME_STRING_VALUE_INLINE_H
#define PGY_RUNTIME_STRING_VALUE_INLINE_H

#include "pgy_runtime_linkage.h"

/* The compiler's admitted lifecycle fact owns permission to retire a value.
 * These operations only normalize live String bytes into system storage;
 * neither a char pointer nor calling copy proves source ownership. */
#ifdef PGY_RUNTIME_LIB_INTERNAL
#define PGY_STRING_VALUE_DECL
#else
#define PGY_STRING_VALUE_DECL PGY_RT_DECL
#endif

PGY_STRING_VALUE_DECL char *
pgy_string_result_copy(const char *source)
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (source == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "String result copy requires a nonnull source");
    size_t length = strlen(source);
    if (length == SIZE_MAX)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    char *owned = (char *)pgy_alloc(NULL, length + 1, 1);
    memcpy(owned, source, length + 1);
    return owned;
}
#else
;
#endif

PGY_STRING_VALUE_DECL void
pgy_string_result_retire(char *owned)
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (owned == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "String result retirement requires a nonnull owner");
    size_t length = strlen(owned);
    if (length == SIZE_MAX)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "String result retirement size overflow");
    pgy_free(NULL, owned, length + 1);
}
#else
;
#endif

#undef PGY_STRING_VALUE_DECL
#endif

#ifndef PERGYRA_PGY_BUILTIN_TYPE_TABLE_H
#define PERGYRA_PGY_BUILTIN_TYPE_TABLE_H

#include <stdbool.h>

typedef enum PgyBuiltinFlags {
    PGY_BUILTIN_FLAG_NONE = 0,
    PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY = 1 << 0
} PgyBuiltinFlags;

typedef struct PgyBuiltinInfo {
    const char *name;
    const char *type_name;
    PgyBuiltinFlags flags;
} PgyBuiltinInfo;

const PgyBuiltinInfo *pgy_builtin_lookup(const char *name);
const char *pgy_builtin_simple_return_type(const char *name);
bool pgy_builtin_is_intent_observability(const char *name);

#endif /* PERGYRA_PGY_BUILTIN_TYPE_TABLE_H */

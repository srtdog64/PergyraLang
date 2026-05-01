#include "codegen_hashmap_key_policy.h"

#include <stdio.h>
#include <string.h>

PgyHashMapKeyKind
pgy_hashmap_key_kind_from_name(const char *name)
{
    if (name == NULL || strcmp(name, "String") == 0)
        return PGY_HASHMAP_KEY_STRING;
    if (strcmp(name, "Int") == 0)
        return PGY_HASHMAP_KEY_INT;
    if (strcmp(name, "Long") == 0)
        return PGY_HASHMAP_KEY_LONG;
    if (strcmp(name, "Bool") == 0)
        return PGY_HASHMAP_KEY_BOOL;
    return PGY_HASHMAP_KEY_UNKNOWN;
}

const char *
pgy_hashmap_key_c_infix(const char *key_name)
{
    switch (pgy_hashmap_key_kind_from_name(key_name)) {
    case PGY_HASHMAP_KEY_INT:
        return "_i32";
    case PGY_HASHMAP_KEY_LONG:
        return "_i64";
    case PGY_HASHMAP_KEY_BOOL:
        return "_bool";
    case PGY_HASHMAP_KEY_STRING:
    case PGY_HASHMAP_KEY_UNKNOWN:
        return "";
    }
    return "";
}

bool
pgy_hashmap_key_raw_export_name(const char *operation,
                                const char *key_name,
                                char *out,
                                size_t out_size)
{
    int written;

    if (operation == NULL || out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "pgy_map_%s_raw%s_export",
                       operation, pgy_hashmap_key_c_infix(key_name));
    return written >= 0 && (size_t)written < out_size;
}

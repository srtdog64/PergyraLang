#include "codegen_hashmap_key_policy.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    const char *name;
    PgyHashMapKeyKind kind;
    const char *c_infix;
} PgyHashMapKeySpec;

static const PgyHashMapKeySpec pgy_hashmap_key_specs[] = {
    { "String", PGY_HASHMAP_KEY_STRING, "" },
    { "Int", PGY_HASHMAP_KEY_INT, "_i32" },
    { "Long", PGY_HASHMAP_KEY_LONG, "_i64" },
    { "Bool", PGY_HASHMAP_KEY_BOOL, "_bool" },
};

static const PgyHashMapKeySpec *
pgy_hashmap_key_find_spec(const char *name)
{
    const char *effective_name = name != NULL ? name : "String";
    for (size_t i = 0; i < sizeof(pgy_hashmap_key_specs) / sizeof(pgy_hashmap_key_specs[0]); i++) {
        if (strcmp(pgy_hashmap_key_specs[i].name, effective_name) == 0)
            return &pgy_hashmap_key_specs[i];
    }
    return NULL;
}

PgyHashMapKeyKind
pgy_hashmap_key_kind_from_name(const char *name)
{
    const PgyHashMapKeySpec *spec = pgy_hashmap_key_find_spec(name);
    if (spec != NULL)
        return spec->kind;
    return PGY_HASHMAP_KEY_UNKNOWN;
}

const char *
pgy_hashmap_key_c_infix(const char *key_name)
{
    const PgyHashMapKeySpec *spec = pgy_hashmap_key_find_spec(key_name);
    if (spec != NULL)
        return spec->c_infix;
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

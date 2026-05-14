#ifndef PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H
#define PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H

#include "transpiler_type_mapping_helpers.h"

static bool
collection_runtime_suffix_copy(const char *inner_type,
                               char *out,
                               size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    if (inner_type == NULL || strcmp(inner_type, "Int") == 0) {
        copy_capped_string(out, out_size, "int");
        return out[0] != '\0';
    }
    if (strcmp(inner_type, "String") == 0) {
        copy_capped_string(out, out_size, "string");
        return out[0] != '\0';
    }

    sanitize_c_suffix(inner_type, out, out_size);
    return out[0] != '\0';
}

#endif /* PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H */

/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend collection runtime suffix rendering.
 */

#include "transpiler_collection_runtime_suffix.h"

#include <string.h>

#include "codegen_type_mapping.h"

bool
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

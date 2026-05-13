#ifndef PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H
#define PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H

#include "transpiler_type_mapping_helpers.h"

static const char *
collection_runtime_suffix(const char *inner_type)
{
    static char suffix[128];

    if (inner_type == NULL)
        return "int";
    if (strcmp(inner_type, "Int") == 0)
        return "int";
    if (strcmp(inner_type, "String") == 0)
        return "string";

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    return suffix;
}

#endif /* PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H */

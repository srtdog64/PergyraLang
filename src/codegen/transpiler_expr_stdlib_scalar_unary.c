/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend scalar unary builtin catalog.
 */

#include "transpiler_expr_stdlib_scalar_unary.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct TranspilerScalarUnarySpec {
    const char *name;
} TranspilerScalarUnarySpec;

static int
transpiler_scalar_unary_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerScalarUnarySpec *spec =
        (const TranspilerScalarUnarySpec *)entry;

    return strcmp(name, spec->name);
}

bool
transpiler_scalar_unary_builtin_name(const char *fn)
{
    static const TranspilerScalarUnarySpec specs[] = {
        { "Acos" },
        { "Asin" },
        { "Atan" },
        { "Ceil" },
        { "Cos" },
        { "Exp" },
        { "Floor" },
        { "Log10" },
        { "Log2" },
        { "MathLog" },
        { "Round" },
        { "Sin" },
        { "Tan" },
    };

    if (fn == NULL)
        return false;

    return bsearch(&fn, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), transpiler_scalar_unary_spec_compare) != NULL;
}

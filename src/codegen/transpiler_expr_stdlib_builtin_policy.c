#include "transpiler_expr_stdlib_builtin_policy.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    size_t argc;
    TranspilerArrayStdlibOp op;
} TranspilerArrayStdlibSpec;

typedef struct {
    const char *name;
    size_t argc;
    TranspilerStdlibOp op;
} TranspilerStdlibSpec;

typedef struct {
    const char *type_name;
    TranspilerToStringKind kind;
} TranspilerToStringSpec;

static const TranspilerArrayStdlibSpec kTranspilerArraySpecs[] = {
    {"ArrayFilter", 2, TRANSPILER_ARRAY_OP_FILTER},
    {"ArrayLength", 1, TRANSPILER_ARRAY_OP_LENGTH},
    {"ArrayMap", 2, TRANSPILER_ARRAY_OP_MAP},
    {"ArrayPop", 1, TRANSPILER_ARRAY_OP_POP},
    {"ArrayPush", 2, TRANSPILER_ARRAY_OP_PUSH},
    {"ArrayReverse", 1, TRANSPILER_ARRAY_OP_REVERSE},
    {"ArraySet", 3, TRANSPILER_ARRAY_OP_SET},
    {"ArraySort", 1, TRANSPILER_ARRAY_OP_SORT},
    {"SliceCopy", 1, TRANSPILER_ARRAY_OP_SLICE_COPY},
};

static const TranspilerStdlibSpec kTranspilerStdlibSpecs[] = {
    {"Clone", 1, TRANSPILER_STDLIB_OP_CLONE},
    {"Print", 1, TRANSPILER_STDLIB_OP_PRINT},
    {"ToString", 1, TRANSPILER_STDLIB_OP_TO_STRING},
};

static const TranspilerToStringSpec kTranspilerToStringSpecs[] = {
    {"Bool", TRANSPILER_TO_STRING_KIND_BOOL},
    {"Double", TRANSPILER_TO_STRING_KIND_DOUBLE},
    {"Float", TRANSPILER_TO_STRING_KIND_FLOAT},
    {"Long", TRANSPILER_TO_STRING_KIND_LONG},
    {"String", TRANSPILER_TO_STRING_KIND_STRING},
};

static int
transpiler_array_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerArrayStdlibSpec *spec =
        (const TranspilerArrayStdlibSpec *)entry;
    return strcmp(name, spec->name);
}

static int
transpiler_stdlib_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerStdlibSpec *spec = (const TranspilerStdlibSpec *)entry;
    return strcmp(name, spec->name);
}

static int
transpiler_to_string_spec_compare(const void *key, const void *entry)
{
    const char *type_name = (const char *)key;
    const TranspilerToStringSpec *spec = (const TranspilerToStringSpec *)entry;
    return strcmp(type_name, spec->type_name);
}

TranspilerArrayStdlibOp
transpiler_array_lookup(const char *fn, size_t argc)
{
    const TranspilerArrayStdlibSpec *spec;

    if (fn == NULL)
        return TRANSPILER_ARRAY_OP_NONE;
    spec = (const TranspilerArrayStdlibSpec *)bsearch(
        fn,
        kTranspilerArraySpecs,
        sizeof(kTranspilerArraySpecs) / sizeof(kTranspilerArraySpecs[0]),
        sizeof(kTranspilerArraySpecs[0]),
        transpiler_array_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return TRANSPILER_ARRAY_OP_NONE;
    return spec->op;
}

TranspilerStdlibOp
transpiler_stdlib_lookup(const char *fn, size_t argc)
{
    const TranspilerStdlibSpec *spec;

    if (fn == NULL)
        return TRANSPILER_STDLIB_OP_NONE;
    spec = (const TranspilerStdlibSpec *)bsearch(
        fn,
        kTranspilerStdlibSpecs,
        sizeof(kTranspilerStdlibSpecs) / sizeof(kTranspilerStdlibSpecs[0]),
        sizeof(kTranspilerStdlibSpecs[0]),
        transpiler_stdlib_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return TRANSPILER_STDLIB_OP_NONE;
    return spec->op;
}

TranspilerToStringKind
transpiler_to_string_kind(const char *type_name)
{
    const TranspilerToStringSpec *spec;

    if (type_name == NULL)
        return TRANSPILER_TO_STRING_KIND_DEFAULT;
    spec = (const TranspilerToStringSpec *)bsearch(
        type_name,
        kTranspilerToStringSpecs,
        sizeof(kTranspilerToStringSpecs) / sizeof(kTranspilerToStringSpecs[0]),
        sizeof(kTranspilerToStringSpecs[0]),
        transpiler_to_string_spec_compare);
    return spec != NULL ? spec->kind : TRANSPILER_TO_STRING_KIND_DEFAULT;
}

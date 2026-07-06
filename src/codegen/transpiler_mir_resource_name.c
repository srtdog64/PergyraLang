#include "codegen_mir_resource_name_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TranspilerMIRResourceOpSpec {
    const char *op_name;
    TranspilerMIRResourceOp op;
} TranspilerMIRResourceOpSpec;

static int
transpiler_mir_resource_op_spec_compare(const void *key, const void *entry)
{
    return strcmp((const char *)key,
                  ((const TranspilerMIRResourceOpSpec *)entry)->op_name);
}

const char *
transpiler_extract_type_suffix_from_fn(const char *fn_name)
{
    if (fn_name == NULL)
        return NULL;

    const char *last_underscore = NULL;
    for (const char *p = fn_name; *p; p++) {
        if (*p == '_')
            last_underscore = p;
    }
    if (last_underscore == NULL)
        return NULL;

    return last_underscore + 1;
}

TranspilerMIRResourceOp
transpiler_mir_resource_op_lookup(const char *op_name)
{
    static const TranspilerMIRResourceOpSpec specs[] = {
        { "BorrowRead", TRANS_MIR_RESOURCE_OP_BORROW_READ },
        { "BorrowWrite", TRANS_MIR_RESOURCE_OP_BORROW_WRITE },
        { "Claim", TRANS_MIR_RESOURCE_OP_CLAIM },
        { "Move", TRANS_MIR_RESOURCE_OP_MOVE },
        { "Read", TRANS_MIR_RESOURCE_OP_READ },
        { "Release", TRANS_MIR_RESOURCE_OP_RELEASE },
        { "Write", TRANS_MIR_RESOURCE_OP_WRITE },
    };
    const TranspilerMIRResourceOpSpec *spec;

    if (op_name == NULL)
        return TRANS_MIR_RESOURCE_OP_NONE;

    spec = (const TranspilerMIRResourceOpSpec *)bsearch(op_name,
        specs,
        sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]),
        transpiler_mir_resource_op_spec_compare);
    return spec != NULL ? spec->op : TRANS_MIR_RESOURCE_OP_NONE;
}

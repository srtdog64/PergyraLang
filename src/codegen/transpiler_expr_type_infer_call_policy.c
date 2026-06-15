#include "transpiler_expr_type_infer_call_policy.h"

#include <stdlib.h>
#include <string.h>

#include "codegen_match_variant_policy.h"

typedef struct TranspilerInferCallSpec
{
    const char *name;
    TranspilerInferCallOp op;
} TranspilerInferCallSpec;

static int
transpiler_infer_call_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerInferCallSpec *spec =
        (const TranspilerInferCallSpec *)entry;

    return strcmp(name, spec->name);
}

TranspilerInferCallOp
transpiler_infer_call_lookup(const char *name)
{
    static const TranspilerInferCallSpec kTranspilerInferCallSpecs[] = {
        { "Abs", TRANS_INFER_CALL_ABS },
        { "Clamp", TRANS_INFER_CALL_CLAMP },
        { "Clone", TRANS_INFER_CALL_CLONE },
        { "DeviceRead", TRANS_INFER_CALL_DEVICE_READ },
        { "IsNone", TRANS_INFER_CALL_IS_NONE },
        { "IsSome", TRANS_INFER_CALL_IS_SOME },
        { "ListGet", TRANS_INFER_CALL_LIST_GET },
        { "MapGet", TRANS_INFER_CALL_MAP_GET },
        { "MapKeys", TRANS_INFER_CALL_MAP_KEYS },
        { "Max", TRANS_INFER_CALL_MAX },
        { "Min", TRANS_INFER_CALL_MIN },
        { "RecvTimeout", TRANS_INFER_CALL_RECV_TIMEOUT },
        { "SendTimeoutStatus", TRANS_INFER_CALL_SEND_TIMEOUT_STATUS },
        { "SetValues", TRANS_INFER_CALL_SET_VALUES },
        { "SubmitDeviceRead", TRANS_INFER_CALL_SUBMIT_DEVICE_READ },
        { "ToObject", TRANS_INFER_CALL_TO_OBJECT },
        { "ToTObject", TRANS_INFER_CALL_TO_TOBJECT },
        { "TryRecv", TRANS_INFER_CALL_TRY_RECV },
        { "TrySendStatus", TRANS_INFER_CALL_TRY_SEND_STATUS },
        { "UnwrapOption", TRANS_INFER_CALL_UNWRAP_OPTION },
        { "ViewRead", TRANS_INFER_CALL_VIEW_READ },
        { "ViewWrite", TRANS_INFER_CALL_VIEW_WRITE },
    };
    const TranspilerInferCallSpec *match;
    PgyCodegenMatchVariantKind variant_kind;

    if (name == NULL)
        return TRANS_INFER_CALL_NONE;
    variant_kind = pgy_codegen_match_variant_lookup(name);
    if (variant_kind == PGY_MATCH_VARIANT_SOME)
        return TRANS_INFER_CALL_SOME;
    if (variant_kind == PGY_MATCH_VARIANT_NONE_CTOR)
        return TRANS_INFER_CALL_NONE_CTOR;

    match = (const TranspilerInferCallSpec *)bsearch(&name,
        kTranspilerInferCallSpecs,
        sizeof(kTranspilerInferCallSpecs)
            / sizeof(kTranspilerInferCallSpecs[0]),
        sizeof(kTranspilerInferCallSpecs[0]),
        transpiler_infer_call_compare);
    return match != NULL ? match->op : TRANS_INFER_CALL_NONE;
}

bool
transpiler_infer_call_is_numeric_passthrough(TranspilerInferCallOp op)
{
    return op == TRANS_INFER_CALL_ABS
        || op == TRANS_INFER_CALL_CLAMP
        || op == TRANS_INFER_CALL_CLONE
        || op == TRANS_INFER_CALL_MAX
        || op == TRANS_INFER_CALL_MIN;
}

bool
transpiler_infer_call_returns_channel_status(TranspilerInferCallOp op)
{
    return op == TRANS_INFER_CALL_TRY_SEND_STATUS
        || op == TRANS_INFER_CALL_SEND_TIMEOUT_STATUS;
}

bool
transpiler_infer_call_returns_channel_option(TranspilerInferCallOp op)
{
    return op == TRANS_INFER_CALL_TRY_RECV
        || op == TRANS_INFER_CALL_RECV_TIMEOUT
        || transpiler_infer_call_returns_channel_status(op);
}

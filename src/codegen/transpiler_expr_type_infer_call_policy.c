#include "transpiler_expr_type_infer_call_policy.h"

#include <stdlib.h>
#include <string.h>

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
        { "Concat", TRANS_INFER_CALL_RETURNS_STRING },
        { "DeviceRead", TRANS_INFER_CALL_DEVICE_READ },
        { "E", TRANS_INFER_CALL_E },
        { "IsNone", TRANS_INFER_CALL_IS_NONE },
        { "IsSome", TRANS_INFER_CALL_IS_SOME },
        { "ListGet", TRANS_INFER_CALL_LIST_GET },
        { "Lower", TRANS_INFER_CALL_RETURNS_STRING },
        { "MapGet", TRANS_INFER_CALL_MAP_GET },
        { "MapKeys", TRANS_INFER_CALL_MAP_KEYS },
        { "Max", TRANS_INFER_CALL_MAX },
        { "Measure", TRANS_INFER_CALL_MEASURE },
        { "Min", TRANS_INFER_CALL_MIN },
        { "None", TRANS_INFER_CALL_NONE_CTOR },
        { "PI", TRANS_INFER_CALL_PI },
        { "QubitState", TRANS_INFER_CALL_QUBIT_STATE },
        { "RecvTimeout", TRANS_INFER_CALL_RECV_TIMEOUT },
        { "Replace", TRANS_INFER_CALL_RETURNS_STRING },
        { "SendTimeoutStatus", TRANS_INFER_CALL_SEND_TIMEOUT_STATUS },
        { "Some", TRANS_INFER_CALL_SOME },
        { "StringReplace", TRANS_INFER_CALL_RETURNS_STRING },
        { "StringTrim", TRANS_INFER_CALL_RETURNS_STRING },
        { "SubmitDeviceRead", TRANS_INFER_CALL_SUBMIT_DEVICE_READ },
        { "Substring", TRANS_INFER_CALL_RETURNS_STRING },
        { "ToLower", TRANS_INFER_CALL_RETURNS_STRING },
        { "ToObject", TRANS_INFER_CALL_TO_OBJECT },
        { "ToString", TRANS_INFER_CALL_RETURNS_STRING },
        { "ToTObject", TRANS_INFER_CALL_TO_TOBJECT },
        { "ToUpper", TRANS_INFER_CALL_RETURNS_STRING },
        { "Trim", TRANS_INFER_CALL_RETURNS_STRING },
        { "TryRecv", TRANS_INFER_CALL_TRY_RECV },
        { "TrySendStatus", TRANS_INFER_CALL_TRY_SEND_STATUS },
        { "UnwrapOption", TRANS_INFER_CALL_UNWRAP_OPTION },
        { "Upper", TRANS_INFER_CALL_RETURNS_STRING },
        { "ViewRead", TRANS_INFER_CALL_VIEW_READ },
        { "ViewWrite", TRANS_INFER_CALL_VIEW_WRITE },
    };
    const TranspilerInferCallSpec *match;

    if (name == NULL)
        return TRANS_INFER_CALL_NONE;

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
transpiler_infer_call_returns_float_constant(TranspilerInferCallOp op)
{
    return op == TRANS_INFER_CALL_E
        || op == TRANS_INFER_CALL_PI;
}

bool
transpiler_infer_call_returns_string(TranspilerInferCallOp op)
{
    return op == TRANS_INFER_CALL_RETURNS_STRING;
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

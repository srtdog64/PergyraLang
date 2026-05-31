#include "transpiler_func_flow_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"

TranspilerReturnOptionCtorOp
transpiler_return_option_ctor_lookup(const char *callee_name)
{
    switch (pgy_codegen_match_variant_lookup(callee_name)) {
    case PGY_MATCH_VARIANT_NONE_CTOR:
        return TRANS_RETURN_OPTION_CTOR_NONE_VALUE;
    case PGY_MATCH_VARIANT_SOME:
        return TRANS_RETURN_OPTION_CTOR_SOME;
    default:
        return TRANS_RETURN_OPTION_CTOR_NONE;
    }
}

bool
transpiler_func_copy_current_return_type(TranspilerCtx *ctx,
                                         const char *type_name)
{
    size_t len;

    if (ctx == NULL || type_name == NULL)
        return false;

    len = strlen(type_name);
    if (len >= sizeof(ctx->current_return_type))
        return false;

    memcpy(ctx->current_return_type, type_name, len + 1);
    return true;
}

bool
transpiler_func_parameter_surface_desc(char *out, size_t out_size,
                                       const char *param_name,
                                       const char *func_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "function parameter '%s' of '%s'",
        param_name != NULL ? param_name : "(anonymous)",
        func_name != NULL ? func_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

void
transpiler_func_format_too_long(TranspilerCtx *ctx, const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "function generated string");
}

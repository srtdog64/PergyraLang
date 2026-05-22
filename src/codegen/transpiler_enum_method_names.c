#include "transpiler_enum_method_names.h"

#include <stdio.h>

#include "../semantic/diag_codes.h"
#include "transpiler_context.h"

bool
transpiler_enum_method_emit_name(char *out,
                                 size_t out_size,
                                 const char *enum_name,
                                 const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_%s",
        enum_name != NULL ? enum_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

bool
transpiler_enum_method_surface_desc(char *out,
                                    size_t out_size,
                                    const char *enum_name,
                                    const char *method_name,
                                    const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "enum method parameter '%s.%s(%s)'",
        enum_name != NULL ? enum_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)",
        param_name != NULL ? param_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

void
transpiler_enum_format_too_long(TranspilerCtx *ctx, const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "enum generated string");
}

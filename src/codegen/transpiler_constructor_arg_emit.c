#include "transpiler_domain_constructor_internal.h"

#include <string.h>

#include "../semantic/diag_codes.h"
#include "codegen_type_mapping.h"
#include "transpiler_constructor_channel_guard.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_type_require.h"

char *
transpiler_emit_ctor_arg_with_expected_type_name(TranspilerCtx *ctx,
                                                const char *field_type_name,
                                                const char *field_name,
                                                ASTNode *arg)
{
    const char *saved_expected_type;
    const char *arg_type;
    char *result;

    if (field_type_name == NULL || field_type_name[0] == '\0') {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing constructor field type-name metadata for '%s'",
            field_name != NULL ? field_name : "(anonymous-field)");
        return NULL;
    }

    if (transpiler_type_name_is_channel(field_type_name)) {
        transpiler_constructor_reject_channel_field(ctx, field_name);
        return NULL;
    }

    saved_expected_type = ctx->expected_type;
    char expected_type_buf[256];
    ctx->expected_type = transpiler_type_name_apply_generic_bindings(
        ctx, field_type_name, expected_type_buf, sizeof(expected_type_buf));
    arg_type = transpiler_expr_infer_type_name(ctx, arg);
    if (arg_type != NULL && strcmp(arg_type, "Void") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ARG_TYPE,
            "C constructor field '%s' cannot consume a Void expression value",
            field_name != NULL ? field_name : "<field>");
        ctx->expected_type = saved_expected_type;
        return NULL;
    }
    result = emit_expression(arg, ctx);
    ctx->expected_type = saved_expected_type;
    if (result == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C constructor field '%s' could not lower initializer expression",
            field_name != NULL ? field_name : "<field>");
        return NULL;
    }
    return result;
}

/* Constructor expected types are declaration ABI facts. The retained AST type
 * node is not an expected-type recovery source here; shared/domain constructor
 * lowering must stop when the hosted MIR view does not carry the name. */
char *
transpiler_emit_ctor_arg_from_field_abi(TranspilerCtx *ctx,
                                        const char *field_type_name,
                                        const char *field_name,
                                        ASTNode *arg,
                                        size_t index)
{
    if (field_type_name == NULL || field_type_name[0] == '\0') {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing constructor field type-name metadata for '%s' index %zu",
            field_name != NULL ? field_name : "(anonymous-field)", index);
        return NULL;
    }
    return transpiler_emit_ctor_arg_with_expected_type_name(
        ctx, field_type_name, field_name, arg);
}

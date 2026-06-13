#include "transpiler_expr_dispatch_operand.h"

#include "transpiler_expr_dispatch_emit.h"
#include "../semantic/diag_codes.h"

char *
transpiler_dispatch_emit_part(TranspilerCtx *ctx,
                              ASTNode *expr,
                              const char *owner,
                              const char *role)
{
    char *rendered = emit_expression(expr, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s could not lower %s expression",
        owner != NULL ? owner : "expression",
        role != NULL ? role : "operand");
    return NULL;
}

/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend projection conversion builtins.
 */

#include "transpiler_expr_projection_builtin.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_projection.h"

static char *
totobject_unsupported(TranspilerCtx *ctx, const char *message)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        message);
    return NULL;
}

char *
emit_builtin_to_dto(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    ASTNode *target_decl;
    ASTNode *source_decl;
    const char *target_name;
    const char *source_type_name;
    char *source_expr;
    char *result;

    if (ast_call_arg_count(call) != 2)
        return totobject_unsupported(ctx,
            "C backend: ToTObject requires target type and source subject");

    target_arg = ast_call_argument(call, 0);
    source_arg = ast_call_argument(call, 1);
    if (target_arg == NULL
        || target_arg->type != AST_IDENTIFIER
        || ast_identifier_name(target_arg) == NULL) {
        return totobject_unsupported(ctx,
            "C backend: ToTObject requires named target tobject type");
    }
    target_name = ast_identifier_name(target_arg);
    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER)
        return totobject_unsupported(ctx,
            "C backend: ToTObject requires named subject source");

    target_decl = transpiler_find_projection_nominal_decl_local(ctx, target_name);
    if (target_decl == NULL || !ast_class_is_struct(target_decl))
        return totobject_unsupported(ctx,
            "C backend: ToTObject target must be tobject/struct");

    source_type_name = transpiler_expr_infer_type_name(
        ctx, source_arg);
    source_decl = transpiler_find_projection_nominal_decl_local(
        ctx, source_type_name);
    if (source_decl == NULL)
        return totobject_unsupported(ctx,
            "C backend: ToTObject source subject type not found");

    source_expr = emit_expression(source_arg, ctx);
    result = emit_projection_literal(ctx, target_decl, source_decl, NULL,
        target_name, source_expr);
    free(source_expr);
    return result;
}

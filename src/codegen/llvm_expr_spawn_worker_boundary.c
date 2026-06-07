/*
 * LLVM spawn worker-boundary storage rejection owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_worker_boundary.h"

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal_api.h"
#include "transpiler_type_mapping.h"

static const char *
llvm_spawn_worker_storage_kind(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    if (llvm_lookup_array_var(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Array/Slice", true, true);
    if (llvm_lookup_list_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "List", true, false);
    if (llvm_lookup_queue_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Queue", true, false);
    if (llvm_lookup_set_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Set", true, false);
    if (llvm_lookup_map_value(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "HashMap", true, false);
    if (llvm_lookup_channel_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Channel", true, false);
    return NULL;
}

bool
llvm_spawn_reject_worker_storage_param(LLVMGenCtx *ctx, ASTNode *site,
                                       FuncParam *param,
                                       const char *param_type_name,
                                       size_t index,
                                       const char *callee_name)
{
    char *type_name;
    const char *kind;

    if (ctx == NULL || param == NULL || param->type == NULL)
        return false;

    type_name = NULL;
    if (param_type_name == NULL) {
        type_name = llvm_render_type_name_scratch_in_ctx(ctx, param->type,
                                                         &ctx->scratch);
        param_type_name = type_name;
    }
    kind = codegen_worker_boundary_storage_kind_from_type_name(
        param_type_name, true);
    if (kind == NULL)
        return false;

    llvm_set_error_at_with_hints(ctx, site,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
        "LLVM spawn parameter %zu for '%s' cannot transport %s<T> storage across a worker boundary; semantic/CFG should reject this path before lowering",
        index + 1,
        callee_name != NULL ? callee_name : "<function>",
        kind);
    return true;
}

bool
llvm_spawn_reject_worker_storage_arg(LLVMGenCtx *ctx, ASTNode *site,
                                     ASTNode *arg, size_t index,
                                     const char *callee_name)
{
    const char *name;
    const char *kind;

    if (arg == NULL || arg->type != AST_IDENTIFIER)
        return false;

    name = ast_identifier_name(arg);
    kind = llvm_spawn_worker_storage_kind(ctx, name);
    if (kind == NULL)
        return false;

    llvm_set_error_at_with_hints(ctx, site,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
        "LLVM spawn argument %zu for '%s' cannot transport %s<T> storage '%s' across a worker boundary; semantic/CFG should reject this path before lowering",
        index + 1,
        callee_name != NULL ? callee_name : "<function>",
        kind,
        name != NULL ? name : "<argument>");
    return true;
}

#endif

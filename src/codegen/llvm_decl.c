#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_boundary_slot_param.h"
#include "llvm_decl_authority.h"
#include "llvm_mir_signature.h"

static unsigned
llvm_function_emitted_param_count(LLVMGenCtx *ctx, ASTNode *node,
                                  const MIRRoutine *routine)
{
    unsigned count = 0;
    bool allow_ast_compat = routine == NULL;
    size_t param_count;

    if (allow_ast_compat)
        param_count = ast_func_param_count(node);
    else
        param_count = llvm_mir_routine_param_count(routine);

    for (size_t i = 0; i < param_count; i++) {
        bool is_secure = false;
        FuncParam *p;
        const char *param_type_name;
        const char *slot_inner = NULL;

        if (allow_ast_compat) {
            p = ast_func_param(node, i);
            param_type_name = NULL;
        } else {
            p = llvm_mir_routine_param(routine, i);
            param_type_name = llvm_mir_routine_param_type_name(routine, i);
        }
        if (p == NULL || p->name == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM function parameter requires a concrete name and type metadata");
            continue;
        }
        count++;
        slot_inner = param_type_name != NULL
            ? llvm_boundary_slot_inner_name_from_type_name(ctx,
                p,
                param_type_name,
                &is_secure)
            : llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        if (slot_inner != NULL && is_secure)
            count++;
    }
    return count;
}

static LLVMTypeRef
llvm_decl_required_implicit_self_type(LLVMGenCtx *ctx, ASTNode *func)
{
    const char *host_name = NULL;
    LLVMClassTypeEntry *cls = NULL;

    if (ctx == NULL)
        return NULL;

    host_name = llvm_current_host_class_name(ctx);
    cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    if (cls != NULL) {
        return cls->is_pointer_self_host
            ? LLVMPointerType(cls->struct_type, 0)
            : cls->struct_type;
    }

    llvm_set_error_at_with_hints(ctx, func,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REPORT_COMPILER_BUG,
        "LLVM implicit self parameter requires current host metadata; silent i32 placeholder is not allowed");
    return NULL;
}

static LLVMTypeRef
llvm_decl_required_param_type(LLVMGenCtx *ctx, ASTNode *func, FuncParam *param)
{
    if (ctx == NULL)
        return NULL;
    if (param != NULL && param->type != NULL)
        return ast_type_to_llvm(ctx, param->type);
    if (param != NULL && llvm_param_is_implicit_self(param))
        return llvm_decl_required_implicit_self_type(ctx, func);

    llvm_set_error_at_with_hints(ctx, func,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM function parameter requires explicit type metadata; silent i32 fallback is not allowed");
    return NULL;
}

static LLVMTypeRef
llvm_decl_required_param_type_name_first(LLVMGenCtx *ctx,
                                         ASTNode *func,
                                         FuncParam *param,
                                         const char *type_name)
{
    if (type_name != NULL) {
        LLVMTypeRef type = pergyra_type_to_llvm(ctx, type_name);
        if (type != NULL || (ctx != NULL && ctx->has_error))
            return type;
    }
    return llvm_decl_required_param_type(ctx, func, param);
}

/* =================================================================
 * Function declaration emission
 * ================================================================= */

static void
llvm_forward_declare_func_with_signature(ASTNode *node,
                                         const MIRRoutine *routine,
                                         LLVMGenCtx *ctx)
{
    const char *name = routine != NULL
        ? llvm_mir_routine_name(routine)
        : ast_declaration_name(node);
    bool allow_ast_compat = false;
    bool generic_func = false;
    bool extern_func = routine == NULL
        && llvm_decl_is_extern_function(ctx, node);
    if (routine == NULL) {
        generic_func = llvm_mir_or_ast_function_is_generic(NULL, node);
        if (llvm_active_has_mir(ctx) && !generic_func && !extern_func) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing function forward routine for '%s'",
                name != NULL ? name : "(anonymous)");
            return;
        }
    }
    if (!llvm_mir_routine_signature_metadata_complete(
            ctx,
            routine,
            node,
            "MIR-only LLVM path missing function forward signature metadata for '%s'",
            "MIR-only LLVM path missing function declaration return type-name metadata for '%s'",
            "MIR-only LLVM path missing function declaration parameter type-name metadata for '%s'")) {
        return;
    }
    allow_ast_compat = routine == NULL;
    size_t param_count;
    if (allow_ast_compat)
        param_count = ast_func_param_count(node);
    else
        param_count = llvm_mir_routine_param_count(routine);
    unsigned emitted_param_count =
        llvm_function_emitted_param_count(ctx, node, routine);

    /* Return type */
    LLVMTypeRef ret_type = ctx->type_void;
    const char *return_type_name = NULL;
    ASTNode *return_type = NULL;
    if (allow_ast_compat) {
        return_type = ast_func_return_type(node);
    } else {
        return_type_name = llvm_mir_routine_return_type_name(routine);
        return_type = llvm_mir_routine_return_type(routine);
    }
    if (return_type_name != NULL) {
        ret_type = pergyra_type_to_llvm(ctx, return_type_name);
    } else if (return_type != NULL) {
        ret_type = ast_type_to_llvm(ctx, return_type);
    }
    if (ctx->has_error || ret_type == NULL)
        return;

    /* Parameter types */
    LLVMTypeRef *param_types = NULL;
    if (emitted_param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
                                       emitted_param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_OOM,
                PGY_CAUSE_LLVM_MEMORY_EXHAUSTED,
                PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT,
                "LLVM function declaration parameter allocation failed for '%s'",
                name != NULL ? name : "(anonymous)");
            return;
        }
        unsigned pidx = 0;
        for (size_t i = 0; i < param_count; i++) {
            bool is_secure = false;
            FuncParam *p;
            const char *param_type_name;
            const char *slot_inner = NULL;

            if (allow_ast_compat) {
                p = ast_func_param(node, i);
                param_type_name = NULL;
            } else {
                p = llvm_mir_routine_param(routine, i);
                param_type_name = llvm_mir_routine_param_type_name(routine, i);
            }
            if (p == NULL || p->name == NULL)
                continue;
            LLVMTypeRef pt = llvm_decl_required_param_type_name_first(
                ctx, node, p, param_type_name);
            if (ctx->has_error || pt == NULL)
                return;
            if (param_type_name != NULL
                ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
                : (p != NULL
                    && p->type != NULL
                    && ast_type_name(p->type) != NULL
                    && llvm_type_name_uses_pointer_self(ctx,
                        ast_type_name(p->type)))) {
                pt = LLVMPointerType(pt, 0);
            }
            if (p != NULL && p->mode == PARAM_MODE_MUT_REF) {
                pt = LLVMPointerType(pt, 0);
            }
            slot_inner = param_type_name != NULL
                ? llvm_boundary_slot_inner_name_from_type_name(ctx,
                    p,
                    param_type_name,
                    &is_secure)
                : llvm_boundary_slot_inner_name(ctx, p, &is_secure);
            if (slot_inner != NULL) {
                param_types[pidx++] = LLVMPointerType(pt, 0);
                if (is_secure) {
                    param_types[pidx++] =
                        llvm_secure_token_type(ctx, slot_inner);
                }
            } else {
                param_types[pidx++] = pt;
            }
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types,
                                            emitted_param_count, 0);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, name, fn_type);
    llvm_register_function(ctx, name, fn, fn_type, ret_type);

}

void
llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx)
{
    llvm_forward_declare_func_with_signature(node, NULL, ctx);
}

void
llvm_forward_declare_func_from_mir(const MIRRoutine *routine,
                                   ASTNode *node,
                                   LLVMGenCtx *ctx)
{
    llvm_forward_declare_func_with_signature(node, routine, ctx);
}

#endif /* PGY_LLVM_ENABLED */

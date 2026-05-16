/*
 * LLVM generic callee specialization for calls and spawn targets.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_spawn_names.h"

#include <string.h>

#include "llvm_boundary_slot_param.h"
#include "llvm_expr_boundary_projection_helpers.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static LLVMTypeRef
llvm_spawn_required_param_type(LLVMGenCtx *ctx,
                               ASTNode *owner,
                               FuncParam *param,
                               const char *callee_name)
{
    if (ctx == NULL)
        return NULL;
    if (param != NULL && param->type != NULL)
        return ast_type_to_llvm(ctx, param->type);

    llvm_set_error_at_with_hints(ctx, owner,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM generic call '%s' parameter requires explicit type metadata; silent i32 fallback is not allowed",
        callee_name != NULL ? callee_name : "<anonymous>");
    return NULL;
}

static const char *
llvm_generic_call_required_suffix(LLVMGenCtx *ctx,
                                  ASTNode *owner,
                                  const char *callee_name,
                                  LLVMValueRef value,
                                  size_t arg_index)
{
    const char *suffix;

    if (ctx == NULL || value == NULL) {
        llvm_set_error_at_with_hints(ctx, owner,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM generic call '%s' requires a lowered argument %zu for specialization",
            callee_name != NULL ? callee_name : "<anonymous>",
            arg_index + 1);
        return NULL;
    }

    suffix = llvm_type_to_suffix(ctx, LLVMTypeOf(value));
    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
        return suffix;

    llvm_set_error_at_with_hints(ctx, owner,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM generic call '%s' requires concrete argument %zu type metadata for specialization",
        callee_name != NULL ? callee_name : "<anonymous>",
        arg_index + 1);
    return NULL;
}

LLVMFuncEntry *
llvm_resolve_callee_entry(LLVMGenCtx *ctx, const char *callee_name,
                          LLVMValueRef *args, size_t argc)
{
    ASTNode *generic_ast = llvm_lookup_generic_template(ctx, callee_name);
    char mangled[256];

    if (generic_ast == NULL)
        return llvm_lookup_function(ctx, callee_name);

    if (!llvm_spawn_copy_name(ctx, generic_ast, mangled, sizeof(mangled),
            callee_name, "generic callee"))
        return NULL;
    for (size_t i = 0; i < argc; i++) {
        const char *suf = llvm_generic_call_required_suffix(ctx, generic_ast,
            callee_name, args != NULL ? args[i] : NULL, i);
        if (suf == NULL)
            return NULL;
        llvm_append_mangled_suffix(mangled, sizeof(mangled), suf);
    }

    if (!llvm_mono_already_emitted(ctx, mangled)) {
        GenericParams *gp;
        int saved_subst;
        LLVMBasicBlockRef saved_bb;
        LLVMValueRef saved_fn;
        LLVMTypeRef saved_ret;
        LLVMTypeRef ret = ctx->type_void;
        size_t pc;
        LLVMTypeRef *ptypes;
        size_t real_pc = 0;
        LLVMTypeRef ft;
        LLVMValueRef mono_fn;
        LLVMBasicBlockRef entry;

        llvm_register_mono(ctx, mangled);

        gp = ast_func_generic_params(generic_ast);
        saved_subst = ctx->type_subst_count;
        ctx->type_subst_count = 0;
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count && gi < 8; gi++) {
            GenericParam *generic_param = ast_generic_param_at(gp, gi);
            const char *param_name = ast_generic_param_name(generic_param);
            const char *suffix;
            LLVMTypeRef concrete;
            if (param_name == NULL || gi >= argc || args == NULL
                || args[gi] == NULL) {
                llvm_set_error_at_with_hints(ctx, generic_ast,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM generic call '%s' requires argument %zu to bind generic parameter '%s'",
                    callee_name != NULL ? callee_name : "<anonymous>",
                    gi + 1,
                    param_name != NULL ? param_name : "<anonymous>");
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            concrete = LLVMTypeOf(args[gi]);
            suffix = llvm_generic_call_required_suffix(ctx, generic_ast,
                callee_name, args[gi], gi);
            if (suffix == NULL) {
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            ctx->type_subst[ctx->type_subst_count].param_name =
                param_name;
            ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
            ctx->type_subst[ctx->type_subst_count].type_name = suffix;
            ctx->type_subst_count++;
        }

        saved_bb = LLVMGetInsertBlock(ctx->builder);
        saved_fn = ctx->current_function;
        saved_ret = ctx->current_ret_type;

        ASTNode *return_type = ast_func_return_type(generic_ast);
        if (return_type != NULL)
            ret = ast_type_to_llvm(ctx, return_type);

        pc = ast_func_param_count(generic_ast);
        ptypes = pgy_arena_calloc(&ctx->scratch,
            ((pc * 2) > 0 ? (pc * 2) : 1) * sizeof(LLVMTypeRef));
        if (ptypes == NULL) {
            llvm_set_error_at_with_hints(ctx, generic_ast,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM generic spawn specialization parameter type allocation failed for '%s'",
                callee_name != NULL ? callee_name : "<anonymous>");
            ctx->type_subst_count = saved_subst;
            return NULL;
        }
        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = ast_func_param(generic_ast, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            if (p == NULL || p->name == NULL)
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p,
                    &is_secure);
                if (inner != NULL) {
                    LLVMTypeRef slot_ty = ast_type_to_llvm(ctx, p->type);
                    ptypes[real_pc++] = LLVMPointerType(slot_ty, 0);
                    if (is_secure)
                        ptypes[real_pc++] = llvm_secure_token_type(ctx, inner);
                } else {
                    LLVMTypeRef pt = llvm_spawn_required_param_type(
                        ctx, generic_ast, p, callee_name);
                    if (pt == NULL) {
                        ctx->type_subst_count = saved_subst;
                        return NULL;
                    }
                    ptypes[real_pc++] = pt;
                }
            }
        }
        ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
        mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
        llvm_register_function(ctx, mangled, mono_fn, ft, ret);
        ctx->current_function = mono_fn;
        ctx->current_ret_type = ret;
        entry = LLVMAppendBasicBlockInContext(ctx->context, mono_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = ast_func_param(generic_ast, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            if (p == NULL || p->name == NULL)
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p,
                    &is_secure);
                LLVMTypeRef pt = llvm_spawn_required_param_type(
                    ctx, generic_ast, p, callee_name);
                if (pt == NULL) {
                    llvm_scope_pop(ctx);
                    ctx->type_subst_count = saved_subst;
                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    if (saved_bb != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
                    return NULL;
                }
                if (inner != NULL) {
                    LLVMValueRef slot_ptr = LLVMGetParam(mono_fn,
                        (unsigned)real_pc++);
                    llvm_scope_declare(ctx, p->name, slot_ptr, pt);
                    llvm_register_slot_var(ctx, p->name, inner, is_secure);
                    if (is_secure) {
                        LLVMTypeRef token_ty = llvm_secure_token_type(ctx,
                            inner);
                        char token_name[256];
                        LLVMValueRef token_alloca;
                        if (!llvm_spawn_format_name(ctx, generic_ast,
                                token_name, sizeof(token_name), p->name,
                                "_token", "secure token")) {
                            llvm_scope_pop(ctx);
                            ctx->type_subst_count = saved_subst;
                            ctx->current_function = saved_fn;
                            ctx->current_ret_type = saved_ret;
                            if (saved_bb != NULL)
                                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
                            return NULL;
                        }
                        token_alloca = llvm_create_entry_alloca(ctx, token_ty,
                            token_name);
                        LLVMBuildStore(ctx->builder,
                            LLVMGetParam(mono_fn, (unsigned)real_pc++),
                            token_alloca);
                        llvm_scope_declare(ctx, pergyra_strdup(token_name),
                            token_alloca, token_ty);
                    }
                } else {
                    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt,
                        p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(mono_fn, (unsigned)real_pc), alloca);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    if (p->type != NULL && p->type->type == AST_TYPE
                        && ast_type_name(p->type) != NULL
                        && llvm_lookup_class(ctx,
                            ast_type_name(p->type)) != NULL) {
                        llvm_register_var_class(ctx, p->name,
                            ast_type_name(p->type));
                    }
                    real_pc++;
                }
            }
        }

        ASTNode *body = ast_func_body(generic_ast);
        if (body != NULL)
            llvm_emit_block(body, ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder, LLVMConstInt(ret, 0, 0));
        }

        llvm_scope_pop(ctx);
        ctx->type_subst_count = saved_subst;
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    }

    return llvm_lookup_function(ctx, mangled);
}

#endif

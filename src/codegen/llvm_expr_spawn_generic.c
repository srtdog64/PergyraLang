/*
 * LLVM generic callee specialization for calls and spawn targets.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_spawn_names.h"

#include <string.h>

#include "llvm_boundary_slot_param.h"
#include "llvm_backend_generic.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_internal_api.h"
#include "llvm_mir_signature.h"
#include "../compiler/mir_decl_headers.h"
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
    const LLVMGenericTemplate *generic_template =
        llvm_lookup_generic_template_entry(ctx, callee_name);
    ASTNode *generic_ast = generic_template != NULL
        ? generic_template->ast
        : NULL;
    const MIRRoutine *generic_routine = generic_template != NULL
        ? generic_template->routine
        : NULL;
    const MIRDeclHeader *generic_header = generic_routine != NULL
        ? llvm_find_decl_header_in_context_of_type(ctx, AST_FUNC_DECL,
            callee_name)
        : NULL;
    char mangled[256];

    if (generic_template == NULL)
        return llvm_lookup_function(ctx, callee_name);

    if (!llvm_spawn_copy_name(ctx, generic_ast, mangled, sizeof(mangled),
            callee_name, "generic callee"))
        return NULL;
    for (size_t i = 0; i < argc; i++) {
        const char *suf = llvm_generic_call_required_suffix(ctx, generic_ast,
            callee_name, args != NULL ? args[i] : NULL, i);
        if (suf == NULL)
            return NULL;
        llvm_spawn_append_mangled_suffix(mangled, sizeof(mangled), suf);
    }

    if (!llvm_mono_already_emitted(ctx, mangled)) {
        GenericParams *gp;
        int saved_subst;

        gp = generic_header == NULL
            ? ast_declaration_generic_params(generic_ast)
            : NULL;
        saved_subst = ctx->type_subst_count;
        ctx->type_subst_count = 0;
        size_t generic_count = generic_header != NULL
            ? mir_decl_header_generic_param_count(generic_header)
            : ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count && gi < 8; gi++) {
            const MIRDeclGenericParam *generic_meta = generic_header != NULL
                ? mir_decl_header_generic_param(generic_header, gi)
                : NULL;
            GenericParam *generic_param = generic_header == NULL
                ? ast_generic_param_at(gp, gi)
                : NULL;
            const char *param_name = generic_meta != NULL
                ? mir_decl_generic_param_name(generic_meta)
                : ast_generic_param_name(generic_param);
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

        if (generic_routine != NULL) {
            LLVMTypeRef ret = ctx->type_void;
            const char *return_type_name =
                llvm_mir_routine_return_type_name(generic_routine);
            ASTNode *return_type =
                llvm_mir_routine_return_type(generic_routine);
            size_t pc;
            LLVMTypeRef *ptypes;
            size_t real_pc = 0;
            LLVMTypeRef ft;
            LLVMValueRef mono_fn;
            MIRRoutine specialized;
            LLVMValueRef emitted;

            if (generic_header == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing generic function header metadata for '%s'",
                    callee_name != NULL ? callee_name : "(anonymous)");
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            if (!llvm_mir_routine_signature_metadata_complete(ctx,
                    generic_routine,
                    generic_ast,
                    "MIR-only LLVM path missing generic function signature metadata for '%s'",
                    "MIR-only LLVM path missing generic function return type-name metadata for '%s'",
                    "MIR-only LLVM path missing generic function parameter type-name metadata for '%s'")) {
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            if (return_type_name != NULL)
                ret = pergyra_type_to_llvm(ctx, return_type_name);
            else if (return_type != NULL)
                ret = ast_type_to_llvm(ctx, return_type);
            if (ctx->has_error || ret == NULL) {
                ctx->type_subst_count = saved_subst;
                return NULL;
            }

            pc = llvm_mir_routine_param_count(generic_routine);
            ptypes = pgy_arena_calloc(&ctx->scratch,
                ((pc * 2) > 0 ? (pc * 2) : 1) * sizeof(LLVMTypeRef));
            if (ptypes == NULL) {
                llvm_set_error_at_with_hints(ctx, generic_ast,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM generic specialization parameter type allocation failed for '%s'",
                    callee_name != NULL ? callee_name : "<anonymous>");
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = llvm_mir_routine_param(generic_routine, k);
                const char *param_type_name =
                    llvm_mir_routine_param_type_name(generic_routine, k);
                MIRParamResourceKind resource_kind = MIR_PARAM_RESOURCE_NONE;
                const char *inner;
                LLVMTypeRef pt;

                if (llvm_param_is_implicit_self(p))
                    continue;
                if (p == NULL || p->name == NULL)
                    continue;
                inner = llvm_mir_boundary_resource_inner_name(
                    ctx, generic_routine, k, &resource_kind);
                pt = param_type_name != NULL
                    ? pergyra_type_to_llvm(ctx, param_type_name)
                    : llvm_spawn_required_param_type(ctx, generic_ast, p,
                        callee_name);
                if (ctx->has_error || pt == NULL) {
                    ctx->type_subst_count = saved_subst;
                    return NULL;
                }
                ptypes[real_pc++] =
                    inner != NULL ? LLVMPointerType(pt, 0) : pt;
                if (inner != NULL
                    && resource_kind == MIR_PARAM_RESOURCE_SECURE_SLOT)
                    ptypes[real_pc++] = llvm_secure_token_type(ctx, inner);
            }

            ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
            llvm_register_mono(ctx, mangled);
            mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
            llvm_register_function(ctx, mangled, mono_fn, ft, ret);

            specialized = *generic_routine;
            specialized.name = mangled;
            emitted = llvm_emit_func_from_mir(&specialized, ctx);
            ctx->type_subst_count = saved_subst;
            if (emitted == NULL || ctx->has_error)
                return NULL;
            return llvm_lookup_function(ctx, mangled);
        }

        LLVMBasicBlockRef saved_bb;
        LLVMLexicalRegistrySnapshot lexical_snapshot;
        LLVMValueRef saved_fn;
        LLVMTypeRef saved_ret;
        LLVMTypeRef saved_function_ret;
        const char *saved_return_type_name;
        ASTNode *saved_return_callable_type;
        LLVMTypeRef ret = ctx->type_void;
        size_t pc;
        LLVMTypeRef *ptypes;
        size_t real_pc = 0;
        LLVMTypeRef ft;
        LLVMValueRef mono_fn;
        LLVMBasicBlockRef entry;

        saved_bb = LLVMGetInsertBlock(ctx->builder);
        saved_fn = ctx->current_function;
        saved_ret = ctx->current_ret_type;
        saved_function_ret = ctx->current_function_ret_type;
        saved_return_type_name = ctx->current_return_type_name;
        saved_return_callable_type = ctx->current_return_callable_type;
        lexical_snapshot = llvm_lexical_registry_snapshot(ctx);

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
        llvm_register_mono(ctx, mangled);
        mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
        llvm_register_function(ctx, mangled, mono_fn, ft, ret);
        ctx->current_function = mono_fn;
        ctx->current_ret_type = ret;
        ctx->current_function_ret_type = ret;
        ctx->current_return_type_name = return_type != NULL
            ? llvm_stmt_render_type_annotation_copy(ctx, return_type)
            : NULL;
        ctx->current_return_callable_type =
            return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE
                ? return_type
                : NULL;
        entry = LLVMAppendBasicBlockInContext(ctx->context, mono_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);
        if (ctx->has_error) {
            llvm_lexical_registry_restore(ctx, lexical_snapshot);
            ctx->type_subst_count = saved_subst;
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            ctx->current_function_ret_type = saved_function_ret;
            ctx->current_return_type_name = saved_return_type_name;
            ctx->current_return_callable_type = saved_return_callable_type;
            if (saved_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
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
                LLVMTypeRef pt = llvm_spawn_required_param_type(
                    ctx, generic_ast, p, callee_name);
                if (pt == NULL) {
                    llvm_scope_pop(ctx);
                    llvm_lexical_registry_restore(ctx, lexical_snapshot);
                    ctx->type_subst_count = saved_subst;
                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    ctx->current_function_ret_type = saved_function_ret;
                    ctx->current_return_type_name = saved_return_type_name;
                    ctx->current_return_callable_type =
                        saved_return_callable_type;
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
                            llvm_lexical_registry_restore(ctx, lexical_snapshot);
                            ctx->type_subst_count = saved_subst;
                            ctx->current_function = saved_fn;
                            ctx->current_ret_type = saved_ret;
                            ctx->current_function_ret_type =
                                saved_function_ret;
                            ctx->current_return_type_name =
                                saved_return_type_name;
                            ctx->current_return_callable_type =
                                saved_return_callable_type;
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
            else {
                if (!ctx->has_error) {
                    llvm_set_error_at_with_hints(ctx, generic_ast,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_CFG_MISSING_RETURN,
                        PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                        "LLVM generic specialization '%s' reached backend without an all-path return terminator",
                        mangled);
                }
                LLVMBuildUnreachable(ctx->builder);
            }
        }

        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);
        ctx->type_subst_count = saved_subst;
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        ctx->current_function_ret_type = saved_function_ret;
        ctx->current_return_type_name = saved_return_type_name;
        ctx->current_return_callable_type = saved_return_callable_type;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    }

    return llvm_lookup_function(ctx, mangled);
}

#endif

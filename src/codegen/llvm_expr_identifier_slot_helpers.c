/*
 * LLVM identifier and slot-source expression lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_identifier_slot_helpers.h"

#include <stdio.h>
#include <string.h>

#include "codegen_match_variant_policy.h"
#include "codegen_slot_type_policy.h"
#include "llvm_expr_host_spawn_literal_helpers.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

LLVMValueRef
llvm_emit_boolean(ASTNode *node, LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, ast_boolean_value(node) ? 1 : 0, 0);
}

static LLVMValueRef
llvm_identifier_error(ASTNode *node, LLVMGenCtx *ctx, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_SYMBOL_UNDEFINED,
            PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            "%s",
            message != NULL ? message : "LLVM identifier could not be resolved");
    }
    return NULL;
}

static const char *
llvm_derive_slot_inner_from_current_decl(LLVMGenCtx *ctx,
                                         const char *source_name,
                                         bool *secure_out)
{
    ASTNode *current_decl;

    if (secure_out != NULL)
        *secure_out = false;
    if (ctx == NULL || source_name == NULL || ctx->current_function == NULL)
        return NULL;

    current_decl = ctx->current_func_decl;
    if (current_decl == NULL || current_decl->type != AST_FUNC_DECL)
        return NULL;

    for (size_t i = 0; i < ast_func_param_count(current_decl); i++) {
        FuncParam *p = ast_func_param(current_decl, i);
        const char *type_name;
        GenericParams *generic_args;
        const char *inner_name;

        if (p == NULL || p->name == NULL || strcmp(p->name, source_name) != 0
            || p->type == NULL || p->type->type != AST_TYPE
            || ast_type_name(p->type) == NULL) {
            continue;
        }

        type_name = ast_type_name(p->type);
        if (!pgy_codegen_type_name_is_slot(type_name)
            && !pgy_codegen_type_name_is_secure_slot(type_name)) {
            continue;
        }

        generic_args = ast_type_generic_args(p->type);
        GenericParam *inner_param = ast_generic_param_at(generic_args, 0);
        if (inner_param == NULL) {
            continue;
        }

        inner_name = llvm_keep_rendered_persistent(ctx,
            llvm_stmt_render_type_arg(inner_param),
            "out of memory copying LLVM slot source type");
        if (inner_name == NULL)
            continue;

        if (secure_out != NULL)
            *secure_out = pgy_codegen_type_name_is_secure_slot(type_name);
        return inner_name;
    }

    return NULL;
}

LLVMVarEntry *
llvm_resolve_slot_target(LLVMGenCtx *ctx, ASTNode *slot_arg,
                         const char **inner_out,
                         const char **source_name_out,
                         bool *secure_out)
{
    const char *inner = NULL;
    const char *source_name = NULL;
    bool is_secure = false;

    if (slot_arg == NULL)
        return NULL;

    if (slot_arg->type == AST_IDENTIFIER) {
        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx,
            ast_identifier_name(slot_arg));
        if (view != NULL) {
            source_name = view->source_slot;
            inner = view->inner_type;
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        } else {
            source_name = ast_identifier_name(slot_arg);
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    } else if (slot_arg->type == AST_CALL
               && ast_call_callee(slot_arg) != NULL
               && ast_call_callee(slot_arg)->type == AST_IDENTIFIER
               && ast_call_arg_count(slot_arg) >= 1
               && ast_call_argument(slot_arg, 0) != NULL
               && ast_call_argument(slot_arg, 0)->type == AST_IDENTIFIER) {
        const char *callee = ast_identifier_name(ast_call_callee(slot_arg));
        if (pgy_codegen_call_name_is_slot_source(callee)) {
            source_name =
                ast_identifier_name(ast_call_argument(slot_arg, 0));
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    }

    if (inner == NULL && source_name != NULL) {
        const char *derived_inner =
            llvm_derive_slot_inner_from_current_decl(ctx, source_name,
                &is_secure);
        if (derived_inner != NULL)
            inner = derived_inner;
    }

    if (source_name == NULL) {
        llvm_set_error_at_with_hints(ctx, slot_arg,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_BINDING_MISSING,
            PGY_FIX_USE_SLOT_BOUND_IDENTIFIER,
            "LLVM slot operation requires an identifier/view/move source with a known slot binding");
        return NULL;
    }
    if (inner == NULL) {
        llvm_set_error_at_with_hints(ctx, slot_arg,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM slot operation on '%s' requires a concrete slot inner type; silent Int fallback is no longer allowed",
            source_name);
        return NULL;
    }
    LLVMVarEntry *slot_var = source_name != NULL
        ? llvm_scope_lookup(ctx, source_name)
        : NULL;
    if (slot_var == NULL) {
        llvm_set_error_at_with_hints(ctx, slot_arg,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_BINDING_MISSING,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot operation on '%s' requires a registered slot local",
            source_name);
        return NULL;
    }
    if (inner_out != NULL)
        *inner_out = inner;
    if (source_name_out != NULL)
        *source_name_out = source_name;
    if (secure_out != NULL)
        *secure_out = is_secure;
    return slot_var;
}

LLVMValueRef
llvm_emit_identifier(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = ast_identifier_name(node);
    LLVMProjectionBorrowEntry *projection_borrow;
    LLVMVarEntry *entry;
    LLVMFuncEntry *fn;

    if (!ctx->suppress_slot_auto_read) {
        const char *inner = llvm_lookup_slot_inner(ctx, name);
        if (inner != NULL) {
            LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
            if (var != NULL) {
                bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
                char fn_name[64];
                snprintf(fn_name, sizeof(fn_name),
                    is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
                fn = llvm_lookup_function(ctx, fn_name);
                if (fn != NULL) {
                    if (is_secure) {
                        LLVMVarEntry *token_var =
                            llvm_require_secure_token_var(ctx, node, name,
                                "auto-read");
                        if (token_var != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, var),
                                token_var->alloca
                            };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type,
                                fn->fn, args, 2, llvm_tmp_name(ctx));
                        }
                        return NULL;
                    }
                    {
                        LLVMValueRef args[] = { llvm_slot_runtime_arg(ctx, var) };
                        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 1, llvm_tmp_name(ctx));
                    }
                }
                if (pgy_classify_type(inner) != PGY_TK_UNKNOWN) {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "LLVM slot auto-read requires registered runtime function '%s'",
                        fn_name);
                    return NULL;
                }
                if (is_secure)
                    return llvm_emit_structural_secure_slot_read(ctx, var, inner);
                return llvm_direct_slot_read(ctx, var, inner);
            }
        }
    }

    projection_borrow = llvm_lookup_projection_borrow(ctx, name);
    if (projection_borrow != NULL) {
        return llvm_emit_projection_from_binding(node, ctx,
            projection_borrow->class_name,
            projection_borrow->source_name);
    }

    entry = llvm_scope_lookup(ctx, name);
    if (entry != NULL)
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));

    if (llvm_current_host_class_name(ctx) != NULL && strcmp(name, "self") != 0) {
        LLVMClassTypeEntry *cls =
            llvm_lookup_class(ctx, llvm_current_host_class_name(ctx));
        if (cls != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, cls);
                LLVMValueRef gep;
                LLVMTypeRef field_type;
                if (base_ptr == NULL)
                    return llvm_identifier_error(node, ctx,
                        "LLVM host field access requires a self receiver");
                gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, base_ptr, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                field_type = llvm_class_field_type_at_index(cls, field_idx);
                if (field_type == NULL)
                    return NULL;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }
        }
    }

    fn = llvm_lookup_function(ctx, name);
    if (fn != NULL)
        return fn->fn;

    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
        if (variant != NULL) {
            /*
             * Construct enum struct only when the enum has data-bearing
             * variants (tagged union shape). Plain enums (all variants
             * payload-less) lower to i32 tag, so returning struct would
             * mismatch function signatures.
             */
            ASTNode *enum_decl = llvm_find_enum_decl(ctx, variant->enum_name);
            bool has_data = false;
            if (enum_decl != NULL) {
                size_t vc = 0;
                (void)ast_enum_variants(enum_decl, &vc);
                for (size_t i = 0; i < vc; i++) {
                    if (ast_enum_variant_param_count(enum_decl, i) > 0) {
                        has_data = true;
                        break;
                    }
                }
            }
            if (has_data) {
                LLVMClassTypeEntry *enum_cls = llvm_lookup_class(ctx,
                    variant->enum_name);
                if (enum_cls != NULL
                    && enum_cls->struct_type != NULL
                    && LLVMGetTypeKind(enum_cls->struct_type)
                           == LLVMStructTypeKind) {
                    LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
                    enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                        LLVMConstInt(ctx->type_i32,
                            (unsigned long long)variant->value, 0),
                        0, llvm_tmp_name(ctx));
                    return enum_val;
                }
            }
            return LLVMConstInt(ctx->type_i32,
                (unsigned long long)variant->value, 0);
        }
    }

    if (pgy_codegen_match_variant_lookup(name) == PGY_MATCH_VARIANT_NONE_CTOR) {
        LLVMTypeRef ctx_ty = ctx->current_ret_type;
        if (ctx_ty != NULL
            && LLVMGetTypeKind(ctx_ty) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx_ty) == 2
            && LLVMStructGetTypeAtIndex(ctx_ty, 0) == ctx->type_i32) {
            LLVMTypeRef value_ty = LLVMStructGetTypeAtIndex(ctx_ty, 1);
            LLVMValueRef o = LLVMGetUndef(ctx_ty);
            o = LLVMBuildInsertValue(ctx->builder, o,
                LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
            o = LLVMBuildInsertValue(ctx->builder, o,
                LLVMConstNull(value_ty), 1, llvm_tmp_name(ctx));
            return o;
        }
    }

    return llvm_identifier_error(node, ctx,
        "LLVM identifier is not declared in the active scope, host, function registry, or enum inventory");
}

#endif

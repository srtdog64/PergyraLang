#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"
#include "llvm_stmt_let_names.h"

#include <string.h>

bool
llvm_stmt_emit_view_or_move_let(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init = node->data.let_decl.initializer;

    if (type_ann == NULL || type_ann->type != AST_TYPE
        || type_ann->data.type.name == NULL
        || init == NULL || init->type != AST_CALL
        || init->data.call.callee == NULL
        || init->data.call.callee->type != AST_IDENTIFIER
        || init->data.call.arg_count < 1
        || init->data.call.arguments[0] == NULL
        || init->data.call.arguments[0]->type != AST_IDENTIFIER)
        return false;

    const char *ann_name = type_ann->data.type.name;
    const char *callee = init->data.call.callee->data.identifier.name;
    const char *source_name = init->data.call.arguments[0]->data.identifier.name;
    bool alias_decl =
        (pgy_codegen_type_name_is_read_view(ann_name)
         && pgy_codegen_call_name_is_view_read(callee))
        || (pgy_codegen_type_name_is_write_view(ann_name)
            && pgy_codegen_call_name_is_view_write(callee))
        || ((strcmp(ann_name, "MoveToken") == 0
             || strncmp(ann_name, "MoveToken<", 10) == 0)
            && strcmp(callee, "Move") == 0);
    if (!alias_decl)
        return false;

    char *inner = NULL;
    if (type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0
        && type_ann->data.type.generic_args->params[0] != NULL)
        inner = llvm_stmt_render_type_arg(
            type_ann->data.type.generic_args->params[0]);
    if (inner == NULL || inner[0] == '\0') {
        llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
        free(inner);
        return true;
    }

    LLVMVarEntry *source = llvm_scope_lookup(ctx, source_name);
    if (source == NULL) {
        free(inner);
        return true;
    }

    bool is_move = (strcmp(callee, "Move") == 0);
    if (is_move) {
        LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
        if (ctx->has_error || slot_ty == NULL) {
            free(inner);
            return true;
        }
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);
        LLVMValueRef moved = LLVMBuildLoad2(ctx->builder, source->type,
            source->alloca, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, moved, alloca_val);
        llvm_scope_declare(ctx, name, alloca_val, slot_ty);
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].var_name, source_name) == 0) {
                ctx->slot_vars[i].released = true;
                break;
            }
        }
    } else {
        llvm_scope_declare(ctx, name, source->alloca, source->type);
    }
    llvm_register_view_var(ctx, name, source_name, inner, is_move);
    free(inner);
    return true;
}

static bool
llvm_stmt_emit_slot_token_alloca(ASTNode *node,
                                 LLVMGenCtx *ctx,
                                 LLVMTypeRef token_ty,
                                 const char *slot_name,
                                 LLVMValueRef slot_alloca)
{
    char token_name[256];
    LLVMValueRef token_alloca;

    if (!llvm_let_with_token_name(ctx, node, token_name,
            sizeof(token_name), slot_name))
        return false;
    token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
    LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

    if (slot_alloca != NULL) {
        LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
            slot_alloca, ctx->type_i64, llvm_tmp_name(ctx));
        LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
            LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
            llvm_tmp_name(ctx));
        LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
            token_ty, token_alloca, 0, llvm_tmp_name(ctx));
        LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
            token_ty, token_alloca, 1, llvm_tmp_name(ctx));
        LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
            token_ty, token_alloca, 2, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_write_ptr);
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_read_ptr);
    }
    llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
    return true;
}

bool
llvm_stmt_emit_slot_sugar_let(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init = node->data.let_decl.initializer;

    if (type_ann == NULL || type_ann->type != AST_TYPE
        || type_ann->data.type.name == NULL)
        return false;

    const char *ann_name = type_ann->data.type.name;
    bool is_slot_sugar = (strcmp(ann_name, "Slot") == 0
                       || strncmp(ann_name, "Slot<", 5) == 0);
    bool is_secure_slot_sugar = (strcmp(ann_name, "SecureSlot") == 0
                              || strncmp(ann_name, "SecureSlot<", 11) == 0);
    if (!is_slot_sugar && !is_secure_slot_sugar)
        return false;

    char *inner = NULL;
    bool is_secure = is_secure_slot_sugar;
    if (type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0
        && type_ann->data.type.generic_args->params[0] != NULL)
        inner = llvm_stmt_render_type_arg(
            type_ann->data.type.generic_args->params[0]);
    if (inner == NULL || inner[0] == '\0') {
        llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
        free(inner);
        return true;
    }

    if (init != NULL && init->type == AST_IDENTIFIER) {
        LLVMViewVarEntry *move_entry = llvm_lookup_view_var(ctx,
            init->data.identifier.name);
        if (move_entry != NULL && move_entry->is_move_token) {
            LLVMTypeRef slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            if (ctx->has_error || slot_ty == NULL) {
                free(inner);
                return true;
            }
            LLVMValueRef alloca_val =
                llvm_stmt_create_slot_alloca(ctx, slot_ty, name);
            LLVMVarEntry *source = llvm_scope_lookup(ctx,
                init->data.identifier.name);
            if (source == NULL) {
                free(inner);
                return true;
            }
            LLVMValueRef moved = LLVMBuildLoad2(ctx->builder, source->type,
                source->alloca, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, moved, alloca_val);
            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner, is_secure);
            if (is_secure) {
                LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                if (!llvm_stmt_emit_slot_token_alloca(node, ctx, token_ty,
                        name, NULL)) {
                    free(inner);
                    return true;
                }
            }
            free(inner);
            return true;
        }
    }

    LLVMTypeRef slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    if (ctx->has_error || slot_ty == NULL) {
        free(inner);
        return true;
    }
    LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0), claimed_ptr);

    if (is_secure) {
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        char token_name[256];
        if (!llvm_stmt_emit_slot_token_alloca(node, ctx, token_ty,
                name, alloca_val)) {
            free(inner);
            return true;
        }
        if (!llvm_let_with_token_name(ctx, node, token_name,
                sizeof(token_name), name)) {
            free(inner);
            return true;
        }
        LLVMVarEntry *token_var = llvm_scope_lookup(ctx, token_name);
        LLVMValueRef token_id = token_var != NULL
            ? LLVMBuildLoad2(ctx->builder, ctx->type_i64,
                LLVMBuildStructGEP2(ctx->builder, token_ty, token_var->alloca,
                    0, llvm_tmp_name(ctx)), llvm_tmp_name(ctx))
            : LLVMConstInt(ctx->type_i64, 0, 0);
        LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
            slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
    }

    llvm_scope_declare(ctx, name, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, name, inner, is_secure);

    if (init != NULL) {
        LLVMValueRef val = llvm_emit_expression(init, ctx);
        if (val != NULL) {
            char fn_name[64];
            if (!llvm_let_with_slot_write_name(ctx, init, fn_name,
                    sizeof(fn_name), inner, is_secure)) {
                free(inner);
                return true;
            }
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                if (is_secure) {
                    char token_name[256];
                    LLVMVarEntry *token_var;
                    if (!llvm_let_with_token_name(ctx, node, token_name,
                            sizeof(token_name), name)) {
                        free(inner);
                        return true;
                    }
                    token_var = llvm_scope_lookup(ctx, token_name);
                    if (token_var != NULL) {
                        LLVMValueRef args[] = {
                            alloca_val, val, token_var->alloca
                        };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 3, "");
                    }
                } else {
                    LLVMValueRef args[] = { alloca_val, val };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, "");
                }
            } else if (pgy_classify_type(inner) != PGY_TK_UNKNOWN) {
                llvm_set_error_at_with_hints(ctx, init,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM slot initializer requires registered runtime function '%s'",
                    fn_name);
            } else {
                LLVMValueRef value_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 0, llvm_tmp_name(ctx));
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, value_ptr);
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    occ_ptr);
            }
        }
    }
    free(inner);
    return true;
}

#endif /* PGY_LLVM_ENABLED */

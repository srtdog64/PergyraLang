/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM intent declaration entry binding / subject-array setup.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

#include <string.h>

const char *
llvm_intent_involves_type_name(ASTNode *involves)
{
    ASTNode *subject_type = NULL;

    if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
        return NULL;
    subject_type = ast_intent_involves_subject_type(involves);
    if (subject_type == NULL) {
        return NULL;
    }
    return ast_type_name(subject_type);
}

static bool
llvm_intent_type_is_subject_participant(LLVMGenCtx *ctx, const char *type_name)
{
    LLVMClassTypeEntry *cls;

    if (ctx == NULL || type_name == NULL)
        return false;
    cls = llvm_lookup_class(ctx, type_name);
    return cls != NULL && cls->is_subject;
}

void
llvm_emit_intent_entry_bindings(LLVMGenCtx *ctx,
                                ASTNode *node,
                                LLVMValueRef fn,
                                const IntentBindingMetadataView *bindings_view,
                                size_t param_count,
                                bool mir_only_intent,
                                LLVMValueRef *subjects_ptr_out,
                                size_t *subject_count_out)
{
    size_t mir_binding_count =
        bindings_view != NULL ? bindings_view->count : 0;
    size_t subject_count = 0;
    LLVMValueRef subjects_ptr;

    if (subjects_ptr_out != NULL)
        *subjects_ptr_out = LLVMConstPointerNull(LLVMPointerType(ctx->type_i8ptr, 0));
    if (subject_count_out != NULL)
        *subject_count_out = 0;
    if (ctx == NULL || node == NULL || fn == NULL)
        return;

    size_t binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    ASTNode **bindings = NULL;
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    if (!mir_only_intent) {
        bindings = ast_intent_decl_bindings(node, &binding_count);
        involves_nodes = ast_intent_decl_involves(node, &involve_count);
        values = ast_intent_decl_values(node, &value_count);
    }

    if (mir_only_intent) {
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    bindings_view, i)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete ordered intent entry binding metadata");
                return;
            }
            if (!intent_binding_metadata_kind_is_supported(
                    intent_binding_metadata_view_kind_at(
                        bindings_view, i))) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has invalid ordered intent entry binding metadata");
                return;
            }
        }
    }

    for (size_t i = 0; i < param_count; i++) {
        LLVMTypeRef pt = NULL;
        const char *alias = NULL;
        const char *type_name = NULL;
        ASTNode *binding = mir_only_intent
            ? NULL
            : (binding_count > 0
                ? (i < binding_count ? bindings[i] : NULL)
                : (i < involve_count
                    ? involves_nodes[i]
                    : (i - involve_count < value_count
                        ? values[i - involve_count]
                        : NULL)));
        if (mir_only_intent
            && intent_binding_metadata_view_row_is_kind(
                bindings_view, i, "participant")) {
            alias = intent_binding_metadata_view_alias_at(bindings_view, i);
            type_name = intent_binding_metadata_view_type_at(bindings_view, i);
            pt = pergyra_type_to_llvm(ctx, type_name);
            if (ctx->has_error || pt == NULL)
                return;
            if (llvm_type_name_uses_pointer_self(ctx, type_name))
                pt = LLVMPointerType(pt, 0);
        } else if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
            ASTNode *involves = binding;
            alias = ast_intent_involves_alias(involves);
            type_name = llvm_intent_involves_type_name(involves);
            if (type_name != NULL) {
                pt = pergyra_type_to_llvm(ctx, type_name);
                if (ctx->has_error || pt == NULL)
                    return;
                if (llvm_type_name_uses_pointer_self(ctx, type_name))
                    pt = LLVMPointerType(pt, 0);
            } else if (!mir_only_intent
                       && involves != NULL
                       && ast_intent_involves_subject_type(involves) != NULL) {
                pt = ast_type_to_llvm(ctx, ast_intent_involves_subject_type(involves));
                if (ctx->has_error || pt == NULL)
                    return;
                if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                    pt = LLVMPointerType(pt, 0);
            }
        } else if (mir_only_intent
                   && intent_binding_metadata_view_row_is_kind(
                       bindings_view, i, "value")) {
            alias = intent_binding_metadata_view_alias_at(bindings_view, i);
            type_name = intent_binding_metadata_view_type_at(bindings_view, i);
            pt = pergyra_type_to_llvm(ctx, type_name);
            if (ctx->has_error || pt == NULL)
                return;
        } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
            ASTNode *value = binding;
            ASTNode *value_type = ast_intent_value_type(value);
            alias = ast_intent_value_alias(value);
            if (value_type != NULL) {
                type_name = ast_type_name(value_type);
                pt = ast_type_to_llvm(ctx, value_type);
                if (ctx->has_error || pt == NULL)
                    return;
            }
        }
        if (alias == NULL || pt == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_BINDING_TYPE,
                "LLVM intent entry binding %zu requires alias and type metadata; silent i8ptr fallback is not allowed",
                i + 1);
            return;
        }
        LLVMValueRef a = llvm_create_entry_alloca(ctx, pt, alias);
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), a);
        llvm_scope_declare(ctx, alias, a, pt);
        if (type_name != NULL)
            llvm_register_var_class(ctx, alias, type_name);
    }

    if (mir_only_intent) {
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (intent_binding_metadata_view_row_is_kind(
                    bindings_view, i, "participant")
                && llvm_intent_type_is_subject_participant(
                    ctx, intent_binding_metadata_view_type_at(
                        bindings_view, i))) {
                subject_count++;
            }
        }
    } else {
        for (size_t i = 0; i < involve_count; i++) {
            ASTNode *involves = i < involve_count
                ? involves_nodes[i]
                : NULL;
            const char *type_name = llvm_intent_involves_type_name(involves);
            if (llvm_intent_type_is_subject_participant(ctx, type_name))
                subject_count++;
        }
    }

    if (subject_count > 0) {
        LLVMTypeRef subject_array_type = LLVMArrayType(ctx->type_i8ptr,
            (unsigned)subject_count);
        LLVMValueRef subjects_alloca = llvm_create_entry_alloca(ctx,
            subject_array_type, "__intent_subjects");
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        unsigned subject_index = 0;

        size_t participant_loop_count = mir_only_intent
            ? mir_binding_count : involve_count;
        for (size_t i = 0; i < participant_loop_count; i++) {
            bool is_mir_participant = mir_only_intent
                && intent_binding_metadata_view_row_is_kind(
                    bindings_view, i, "participant");
            ASTNode *involves = mir_only_intent || i >= involve_count
                ? NULL : involves_nodes[i];
            const char *alias = mir_only_intent
                ? intent_binding_metadata_view_alias_at(bindings_view, i)
                : (involves != NULL ? ast_intent_involves_alias(involves) : NULL);
            const char *type_name = mir_only_intent
                ? intent_binding_metadata_view_type_at(bindings_view, i)
                : (involves != NULL ? llvm_intent_involves_type_name(involves) : NULL);
            LLVMVarEntry participant_var;
            bool has_participant_var = llvm_scope_lookup_snapshot(ctx,
                alias != NULL ? alias : "participant", &participant_var);
            LLVMValueRef indices[] = {
                zero,
                LLVMConstInt(ctx->type_i32, subject_index, 0)
            };
            LLVMValueRef participant_ptr = has_participant_var
                ? LLVMBuildLoad2(ctx->builder, participant_var.type,
                    participant_var.alloca, llvm_tmp_name(ctx))
                : LLVMConstPointerNull(ctx->type_i8ptr);
            if (mir_only_intent && !is_mir_participant)
                continue;
            if (!llvm_intent_type_is_subject_participant(ctx, type_name))
                continue;
            if (LLVMGetTypeKind(LLVMTypeOf(participant_ptr)) != LLVMPointerTypeKind)
                continue;
            LLVMValueRef cast_participant = participant_ptr;
            if (LLVMTypeOf(participant_ptr) != ctx->type_i8ptr) {
                cast_participant = LLVMBuildBitCast(ctx->builder, participant_ptr,
                    ctx->type_i8ptr, llvm_tmp_name(ctx));
            }
            LLVMValueRef elem_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast_participant, elem_ptr);
            subject_index++;
        }

        {
            LLVMValueRef indices[] = { zero, zero };
            subjects_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
        }
    } else {
        subjects_ptr = LLVMConstPointerNull(LLVMPointerType(ctx->type_i8ptr, 0));
    }

    if (subjects_ptr_out != NULL)
        *subjects_ptr_out = subjects_ptr;
    if (subject_count_out != NULL)
        *subject_count_out = subject_count;
}

#endif

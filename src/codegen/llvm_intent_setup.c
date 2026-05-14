/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM intent declaration entry binding / subject-array setup.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

const char *
llvm_intent_involves_type_name(ASTNode *involves)
{
    ASTNode *subject_type = ast_intent_involves_subject_type(involves);
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || subject_type == NULL || subject_type->type != AST_TYPE) {
        return NULL;
    }
    return subject_type->data.type.name;
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
                                const char **participant_aliases,
                                const char **participant_types,
                                size_t participant_count,
                                size_t param_count,
                                bool mir_only_intent,
                                LLVMValueRef *subjects_ptr_out,
                                size_t *subject_count_out)
{
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
    ASTNode **bindings = ast_intent_decl_bindings(node, &binding_count);
    ASTNode **involves_nodes = ast_intent_decl_involves(node, &involve_count);
    ASTNode **values = ast_intent_decl_values(node, &value_count);

    for (size_t i = 0, participant_index = 0; i < param_count; i++) {
        LLVMTypeRef pt = ctx->type_i8ptr;
        const char *alias = NULL;
        const char *type_name = NULL;
        ASTNode *binding = binding_count > 0
            ? (i < binding_count ? bindings[i] : NULL)
            : (i < involve_count
                ? involves_nodes[i]
                : (i - involve_count < value_count
                    ? values[i - involve_count]
                    : NULL));
        if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
            ASTNode *involves = binding;
            alias = (mir_only_intent && participant_aliases != NULL && participant_index < participant_count)
                ? participant_aliases[participant_index]
                : (participant_aliases != NULL && participant_index < participant_count
                    ? participant_aliases[participant_index]
                    : ast_intent_involves_alias(involves));
            type_name = (mir_only_intent && participant_types != NULL && participant_index < participant_count)
                ? participant_types[participant_index]
                : (participant_types != NULL && participant_index < participant_count
                    ? participant_types[participant_index]
                    : llvm_intent_involves_type_name(involves));
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
            participant_index++;
        } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
            ASTNode *value = binding;
            ASTNode *value_type = ast_intent_value_type(value);
            alias = ast_intent_value_alias(value);
            type_name = (value_type != NULL && value_type->type == AST_TYPE)
                ? value_type->data.type.name : NULL;
            if (value_type != NULL) {
                pt = ast_type_to_llvm(ctx, value_type);
                if (ctx->has_error || pt == NULL)
                    return;
            }
        }
        LLVMValueRef a = llvm_create_entry_alloca(ctx, pt, alias != NULL ? alias : "param");
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), a);
        llvm_scope_declare(ctx, alias != NULL ? alias : "param", a, pt);
        if (type_name != NULL)
            llvm_register_var_class(ctx, alias, type_name);
    }

    for (size_t i = 0; i < participant_count; i++) {
        ASTNode *involves = i < involve_count
            ? involves_nodes[i]
            : NULL;
        const char *type_name = (mir_only_intent && participant_types != NULL && i < participant_count)
            ? participant_types[i]
            : (participant_types != NULL && i < participant_count
                ? participant_types[i]
                : llvm_intent_involves_type_name(involves));
        if (llvm_intent_type_is_subject_participant(ctx, type_name))
            subject_count++;
    }

    if (subject_count > 0) {
        LLVMTypeRef subject_array_type = LLVMArrayType(ctx->type_i8ptr,
            (unsigned)subject_count);
        LLVMValueRef subjects_alloca = llvm_create_entry_alloca(ctx,
            subject_array_type, "__intent_subjects");
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        unsigned subject_index = 0;

        for (size_t i = 0; i < participant_count; i++) {
            ASTNode *involves = i < involve_count
                ? involves_nodes[i]
                : NULL;
            const char *alias = (mir_only_intent && participant_aliases != NULL && i < participant_count)
                ? participant_aliases[i]
                : (participant_aliases != NULL && i < participant_count
                    ? participant_aliases[i]
                    : ast_intent_involves_alias(involves));
            const char *type_name = (mir_only_intent && participant_types != NULL && i < participant_count)
                ? participant_types[i]
                : (participant_types != NULL && i < participant_count
                    ? participant_types[i]
                    : llvm_intent_involves_type_name(involves));
            LLVMVarEntry *participant_var = llvm_scope_lookup(ctx, alias != NULL ? alias : "participant");
            LLVMValueRef indices[] = {
                zero,
                LLVMConstInt(ctx->type_i32, subject_index, 0)
            };
            LLVMValueRef participant_ptr = participant_var != NULL
                ? LLVMBuildLoad2(ctx->builder, participant_var->type, participant_var->alloca, llvm_tmp_name(ctx))
                : LLVMConstPointerNull(ctx->type_i8ptr);
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

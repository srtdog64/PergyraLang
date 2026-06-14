/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR match-condition lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "codegen_match_subject_lookup.h"
#include "codegen_match_variant_policy.h"
#include "llvm_internal.h"
#include "llvm_mir_match_pattern.h"
#include "llvm_mir_match_region.h"
#include "llvm_mir_signature.h"
#include "parser/ast_api.h"

#include <stdint.h>
#include <string.h>

static LLVMValueRef
llvm_mir_emit_guarded_match_condition(LLVMGenCtx *ctx,
                                      ASTNode *case_node,
                                      uint32_t case_stable_id,
                                      LLVMValueRef tag_cmp,
                                      LLVMValueRef subject,
                                      unsigned payload_index,
                                      const char *binding);

static void
llvm_mir_match_lower_error(ASTNode *node,
                           LLVMGenCtx *ctx,
                           const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM MIR match lowering failed");
    }
}

LLVMValueRef
llvm_mir_emit_match_case_condition(const MIRInstruction *inst,
                                   LLVMGenCtx *ctx)
{
    ASTNode *case_node;
    ASTNode *subject_node;
    LLVMValueRef subject;
    LLVMValueRef cmp = NULL;
    uint32_t case_stable_id;

    if (inst == NULL || ctx == NULL)
        return NULL;
    case_node = mir_instruction_source_payload(inst);
    case_stable_id = mir_instruction_source_stable_id(inst);
    if (case_node == NULL
        || case_node->type != AST_MATCH_CASE) {
        return NULL;
    }

    subject_node = pgy_codegen_match_subject_for_branch(inst);
    if (subject_node == NULL)
        return NULL;
    subject = llvm_emit_expression(subject_node, ctx);
    if (subject == NULL) {
        llvm_mir_match_lower_error(subject_node, ctx,
            "LLVM MIR match lowering could not lower subject expression");
        return NULL;
    }

    if (ast_match_case_patterns(case_node, NULL) != NULL
        && ast_match_case_pattern_count(case_node) > 1) {
        for (size_t i = 0; i < ast_match_case_pattern_count(case_node); i++) {
            ASTNode *pattern_node = ast_match_case_pattern_at(case_node, i);
            LLVMValueRef pattern = llvm_emit_expression(
                pattern_node, ctx);
            LLVMValueRef one_cmp;
            if (pattern == NULL) {
                llvm_mir_match_lower_error(pattern_node, ctx,
                    "LLVM MIR match lowering could not lower case pattern");
                return NULL;
            }
            one_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, subject, pattern,
                                    llvm_tmp_name(ctx));
            cmp = cmp == NULL ? one_cmp
                              : LLVMBuildOr(ctx->builder, cmp, one_cmp,
                                            llvm_tmp_name(ctx));
        }
        if (cmp == NULL) {
            llvm_mir_match_lower_error(case_node, ctx,
                "LLVM MIR match lowering requires at least one case pattern");
            return NULL;
        }
        return cmp;
    }

    if (ast_match_case_pattern(case_node) == NULL)
        return NULL;

    {
        const char *option_kind = NULL;
        const char *result_kind = NULL;
        const char *binding = NULL;
        ASTNode *pattern_node = ast_match_case_pattern(case_node);
        if (llvm_mir_is_option_destructor(pattern_node,
                                          &option_kind, &binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            LLVMValueRef tag_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    pgy_codegen_match_variant_llvm_tag(
                        pgy_codegen_match_variant_lookup(option_kind)), 0),
                llvm_tmp_name(ctx));
            return llvm_mir_emit_guarded_match_condition(
                ctx, case_node, case_stable_id, tag_cmp, subject, 1, binding);
        }
        if (llvm_mir_is_result_destructor(pattern_node,
                                          &result_kind, &binding)) {
            PgyCodegenMatchVariantKind result_variant =
                pgy_codegen_match_variant_lookup(result_kind);
            unsigned payload_index =
                pgy_codegen_match_variant_result_payload_index(result_variant);
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            LLVMValueRef tag_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    pgy_codegen_match_variant_llvm_tag(
                        result_variant), 0),
                llvm_tmp_name(ctx));
            return llvm_mir_emit_guarded_match_condition(
                ctx, case_node, case_stable_id, tag_cmp, subject,
                payload_index, binding);
        }
        /* General enum variant destructor: Circle(r), Empty, etc */
        {
            const char *variant_name = NULL;
            size_t variant_argc = 0;
            ASTNode *callee = NULL;
            if (pattern_node->type == AST_CALL) {
                callee = ast_call_callee(pattern_node);
                if (callee != NULL && callee->type == AST_IDENTIFIER) {
                    variant_name = ast_identifier_name(callee);
                    variant_argc = ast_call_arg_count(pattern_node);
                } else if (callee != NULL
                    && callee->type == AST_MEMBER_ACCESS) {
                    variant_name = ast_member_name(callee);
                    variant_argc = ast_call_arg_count(pattern_node);
                }
            } else if (pattern_node->type == AST_IDENTIFIER) {
                variant_name = ast_identifier_name(pattern_node);
                variant_argc = 0;
            } else if (pattern_node->type == AST_MEMBER_ACCESS) {
                variant_name = ast_member_name(pattern_node);
                variant_argc = 0;
            }
            if (variant_name != NULL) {
                LLVMEnumVariantEntry *variant =
                    llvm_lookup_enum_variant(ctx, variant_name);
                if (variant != NULL) {
                    LLVMClassTypeEntry *enum_cls =
                        llvm_lookup_class(ctx, variant->enum_name);
                    /*
                     * Plain enums (no payload variants) lower the subject as
                     * a bare i32 tag rather than a tagged-union struct, so
                     * compare directly without ExtractValue.
                     */
                    LLVMTypeKind subject_kind =
                        LLVMGetTypeKind(LLVMTypeOf(subject));
                    if (subject_kind != LLVMStructTypeKind) {
                        return LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                            subject,
                            LLVMConstInt(LLVMTypeOf(subject),
                                (unsigned long long)variant->value, 0),
                            llvm_tmp_name(ctx));
                    }
                    LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder,
                        subject, 0, llvm_tmp_name(ctx));
                    (void)variant_argc;
                    (void)enum_cls;
                    return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                        LLVMConstInt(ctx->type_i32,
                            (unsigned long long)variant->value, 0),
                        llvm_tmp_name(ctx));
                }
            }
        }
        LLVMValueRef pattern = llvm_emit_expression(pattern_node, ctx);
        if (pattern == NULL) {
            llvm_mir_match_lower_error(pattern_node, ctx,
                "LLVM MIR match lowering could not lower case pattern");
            return NULL;
        }
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, subject, pattern,
                             llvm_tmp_name(ctx));
    }
}

static bool
llvm_mir_remap_payload_binding(LLVMGenCtx *ctx,
                               uint32_t case_stable_id,
                               const char *binding,
                               LLVMTypeRef payload_ty)
{
    char alloca_name[256];
    LLVMVarEntry entry;
    bool has_entry;
    LLVMClassTypeEntry *payload_cls;

    if (ctx == NULL || binding == NULL || strcmp(binding, "_") == 0)
        return true;
    llvm_mir_match_payload_alloca_name(case_stable_id, binding,
                                       alloca_name, sizeof(alloca_name));
    has_entry = llvm_scope_lookup_snapshot(ctx, alloca_name, &entry);
    if ((!has_entry || entry.alloca == NULL) && payload_ty != NULL) {
        LLVMValueRef payload_alloca =
            llvm_create_entry_alloca(ctx, payload_ty, alloca_name);
        llvm_scope_declare(ctx, pergyra_strdup(alloca_name), payload_alloca,
                           payload_ty);
        has_entry = llvm_scope_lookup_snapshot(ctx, alloca_name, &entry);
    }
    if (!has_entry || entry.alloca == NULL)
        return true;
    {
        LLVMValueRef binding_alloca = entry.alloca;
        LLVMTypeRef binding_type = entry.type;
        llvm_scope_declare(ctx, pergyra_strdup(binding), binding_alloca,
                           binding_type);
        payload_cls = llvm_lookup_class_by_type(ctx, binding_type);
    }
    if (payload_cls != NULL)
        llvm_register_var_class(ctx, binding, payload_cls->class_name);
    return true;
}

static LLVMTypeRef
llvm_mir_case_payload_type(LLVMGenCtx *ctx,
                           const MIRInstruction *inst,
                           const char *kind)
{
    ASTNode *case_node;
    ASTNode *subject_node;
    LLVMTypeRef subject_ty;
    unsigned payload_index;
    bool saved_has_error;
    char saved_error_msg[sizeof(ctx->error_msg)];
    uint32_t saved_error_line;
    uint32_t saved_error_column;
    const char *saved_error_code;
    const char *saved_error_cause_ir;
    const char *saved_error_fix_source;

    if (ctx == NULL || inst == NULL || kind == NULL)
        return NULL;
    case_node = mir_instruction_source_payload(inst);
    if (case_node == NULL || case_node->type != AST_MATCH_CASE)
        return NULL;
    subject_node = pgy_codegen_match_subject_for_branch(inst);
    if (subject_node == NULL)
        return NULL;
    saved_has_error = ctx->has_error;
    memcpy(saved_error_msg, ctx->error_msg, sizeof(saved_error_msg));
    saved_error_line = ctx->error_line;
    saved_error_column = ctx->error_column;
    saved_error_code = ctx->error_code;
    saved_error_cause_ir = ctx->error_cause_ir;
    saved_error_fix_source = ctx->error_fix_source;
    subject_ty = NULL;
    if (subject_node->type == AST_CALL
        && ast_call_callee(subject_node) != NULL
        && ast_call_callee(subject_node)->type == AST_IDENTIFIER) {
        const char *callee = ast_identifier_name(ast_call_callee(subject_node));
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx, callee);
        ASTNode *ret = NULL;
        const char *return_type_name = NULL;
        bool decl_is_extern = decl != NULL
            && llvm_decl_is_extern_function(ctx, decl);
        const MIRRoutine *routine = NULL;
        bool decl_is_generic = false;
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && llvm_active_has_mir(ctx)
            && !decl_is_extern) {
            routine = llvm_active_function_routine_by_name(ctx, callee);
        }
        decl_is_generic = llvm_mir_or_ast_function_is_generic(routine, decl);
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && llvm_active_has_mir(ctx)
            && !decl_is_generic
            && !decl_is_extern) {
            if (routine == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing match subject routine for '%s'",
                    callee != NULL ? callee : "(anonymous-function)");
                return NULL;
            }
            if (!llvm_mir_routine_signature_metadata_complete_for(ctx,
                    routine, decl,
                    LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME,
                    "MIR-only LLVM path missing match subject signature metadata for '%s'",
                    "MIR-only LLVM path missing match subject return type-name metadata for '%s'",
                    NULL)) {
                return NULL;
            }
            ret = llvm_mir_routine_return_type(routine);
            return_type_name = llvm_mir_routine_return_type_name(routine);
            if (return_type_name != NULL) {
                subject_ty = pergyra_type_to_llvm(ctx, return_type_name);
                if (ctx->has_error)
                    return NULL;
            }
        } else {
            ret = ast_func_return_type(decl);
        }
        if (subject_ty == NULL && ret != NULL)
            subject_ty = ast_type_to_llvm(ctx, ret);
    }
    if (subject_ty == NULL
        || LLVMGetTypeKind(subject_ty) != LLVMStructTypeKind) {
        ctx->has_error = saved_has_error;
        memcpy(ctx->error_msg, saved_error_msg, sizeof(ctx->error_msg));
        ctx->error_line = saved_error_line;
        ctx->error_column = saved_error_column;
        ctx->error_code = saved_error_code;
        ctx->error_cause_ir = saved_error_cause_ir;
        ctx->error_fix_source = saved_error_fix_source;
        return NULL;
    }
    if (pgy_codegen_match_variant_lookup(kind) == PGY_MATCH_VARIANT_SOME) {
        payload_index = 1;
    } else {
        payload_index = pgy_codegen_match_variant_result_payload_index(
            pgy_codegen_match_variant_lookup(kind));
    }
    if (LLVMCountStructElementTypes(subject_ty) <= payload_index)
        return NULL;
    return LLVMStructGetTypeAtIndex(subject_ty, payload_index);
}

static bool
llvm_mir_remap_case_bindings(LLVMGenCtx *ctx,
                             const MIRInstruction *inst)
{
    ASTNode *case_node;
    ASTNode *pattern_node;
    const char *kind = NULL;
    const char *binding = NULL;
    uint32_t case_stable_id;

    if (ctx == NULL || inst == NULL)
        return true;
    case_node = mir_instruction_source_payload(inst);
    case_stable_id = mir_instruction_source_stable_id(inst);
    if (case_node == NULL || case_node->type != AST_MATCH_CASE)
        return true;
    pattern_node = ast_match_case_pattern(case_node);
    if (pattern_node == NULL)
        return true;
    if (llvm_mir_is_option_destructor(pattern_node, &kind, &binding)
        || llvm_mir_is_result_destructor(pattern_node, &kind, &binding)) {
        LLVMTypeRef payload_ty =
            llvm_mir_case_payload_type(ctx, inst, kind);
        return llvm_mir_remap_payload_binding(ctx, case_stable_id, binding,
                                              payload_ty);
    }

    if (pattern_node->type == AST_CALL) {
        size_t argc = ast_call_arg_count(pattern_node);
        for (size_t i = 0; i < argc; i++) {
            ASTNode *arg = ast_call_argument(pattern_node, i);
            if (arg == NULL || arg->type != AST_IDENTIFIER)
                continue;
            if (!llvm_mir_remap_payload_binding(ctx, case_stable_id,
                                                ast_identifier_name(arg),
                                                NULL)) {
                return false;
            }
        }
    }
    return true;
}

bool
llvm_mir_remap_active_match_bindings(const MIRRoutine *routine,
                                     const MIRBasicBlock *block,
                                     LLVMGenCtx *ctx)
{
    size_t target_id;

    if (routine == NULL || block == NULL || ctx == NULL)
        return true;
    target_id = block->id;
    if (target_id >= routine->block_count)
        return true;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *candidate = &routine->blocks[i];
        if (!llvm_mir_case_true_region_contains(routine, candidate,
                                                target_id)) {
            continue;
        }
        for (size_t j = 0; j < candidate->instruction_count; j++) {
            const MIRInstruction *inst = &candidate->instructions[j];
            if (inst->kind != MIR_INST_BRANCH
                || inst->branch_shape != MIR_BRANCH_MATCH_CASE) {
                continue;
            }
            if (!llvm_mir_remap_case_bindings(ctx, inst))
                return false;
        }
    }
    return true;
}

static bool
llvm_mir_emit_payload_binding(LLVMGenCtx *ctx,
                              uint32_t case_stable_id,
                              LLVMValueRef payload,
                              const char *binding)
{
    LLVMTypeRef payload_ty;
    LLVMValueRef payload_alloca;
    char alloca_name[256];
    LLVMClassTypeEntry *payload_cls;

    if (ctx == NULL || payload == NULL || binding == NULL)
        return true;
    payload_ty = LLVMTypeOf(payload);
    llvm_mir_match_payload_alloca_name(case_stable_id, binding,
                                       alloca_name, sizeof(alloca_name));
    {
        LLVMVarEntry existing;
        bool has_existing =
            llvm_scope_lookup_snapshot(ctx, alloca_name, &existing);
        payload_alloca = has_existing && existing.alloca != NULL
            ? existing.alloca
            : llvm_create_entry_alloca(ctx, payload_ty, alloca_name);
    }
    LLVMBuildStore(ctx->builder, payload, payload_alloca);
    llvm_scope_declare(ctx, pergyra_strdup(binding),
                       payload_alloca, payload_ty);
    llvm_scope_declare(ctx, pergyra_strdup(alloca_name),
                       payload_alloca, payload_ty);
    payload_cls = llvm_lookup_class_by_type(ctx, payload_ty);
    if (payload_cls != NULL)
        llvm_register_var_class(ctx, binding, payload_cls->class_name);
    return true;
}

static LLVMValueRef
llvm_mir_emit_guarded_match_condition(LLVMGenCtx *ctx,
                                      ASTNode *case_node,
                                      uint32_t case_stable_id,
                                      LLVMValueRef tag_cmp,
                                      LLVMValueRef subject,
                                      unsigned payload_index,
                                      const char *binding)
{
    LLVMBasicBlockRef tag_block;
    LLVMBasicBlockRef guard_block;
    LLVMBasicBlockRef guard_tail;
    LLVMBasicBlockRef cont_block;
    LLVMValueRef guard;
    LLVMValueRef phi;
    LLVMValueRef false_value;
    LLVMValueRef incoming[2];
    LLVMBasicBlockRef incoming_blocks[2];
    LLVMValueRef fn;

    if (ctx == NULL || case_node == NULL || tag_cmp == NULL)
        return tag_cmp;
    if (ast_match_case_guard(case_node) == NULL)
        return tag_cmp;

    tag_block = LLVMGetInsertBlock(ctx->builder);
    if (tag_block == NULL)
        return tag_cmp;
    fn = LLVMGetBasicBlockParent(tag_block);
    if (fn == NULL)
        return tag_cmp;

    guard_block = LLVMAppendBasicBlockInContext(ctx->context, fn,
                                                "mir.match.guard");
    cont_block = LLVMAppendBasicBlockInContext(ctx->context, fn,
                                               "mir.match.guard.cont");
    LLVMBuildCondBr(ctx->builder, tag_cmp, guard_block, cont_block);

    LLVMPositionBuilderAtEnd(ctx->builder, guard_block);
    if (binding != NULL && subject != NULL) {
        LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, subject,
            payload_index, llvm_tmp_name(ctx));
        if (!llvm_mir_emit_payload_binding(ctx, case_stable_id,
                                           payload, binding))
            return NULL;
    }
    guard = llvm_emit_expression(ast_match_case_guard(case_node), ctx);
    if (guard == NULL) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, ast_match_case_guard(case_node),
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR match lowering could not lower guard expression");
        }
        return NULL;
    }
    guard_tail = LLVMGetInsertBlock(ctx->builder);
    LLVMBuildBr(ctx->builder, cont_block);

    LLVMPositionBuilderAtEnd(ctx->builder, cont_block);
    phi = LLVMBuildPhi(ctx->builder, LLVMInt1TypeInContext(ctx->context),
                       llvm_tmp_name(ctx));
    false_value = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);
    incoming[0] = false_value;
    incoming[1] = guard;
    incoming_blocks[0] = tag_block;
    incoming_blocks[1] = guard_tail;
    LLVMAddIncoming(phi, incoming, incoming_blocks, 2);
    return phi;
}

bool
llvm_mir_emit_match_case_body_binding(const MIRRoutine *routine,
                                      const MIRBasicBlock *block,
                                      LLVMGenCtx *ctx)
{
    const MIRInstruction *branch_inst;
    ASTNode *case_node;
    ASTNode *subject_node;
    ASTNode *pattern_node;
    LLVMValueRef subject;
    const char *option_kind = NULL;
    const char *result_kind = NULL;
    const char *binding = NULL;
    uint32_t case_stable_id;

    if (routine == NULL || block == NULL || ctx == NULL)
        return true;
    branch_inst = llvm_mir_find_incoming_match_branch(routine, block);
    if (branch_inst == NULL)
        return true;
    case_node = mir_instruction_source_payload(branch_inst);
    case_stable_id = mir_instruction_source_stable_id(branch_inst);
    if (case_node == NULL || case_node->type != AST_MATCH_CASE)
        return true;
    pattern_node = ast_match_case_pattern(case_node);
    if (pattern_node == NULL)
        return true;
    subject_node = pgy_codegen_match_subject_for_branch(branch_inst);
    if (subject_node == NULL)
        return true;
    subject = llvm_emit_expression(subject_node, ctx);
    if (subject == NULL) {
        llvm_mir_match_lower_error(subject_node, ctx,
            "LLVM MIR match body binding could not lower subject expression");
        return false;
    }

    if (llvm_mir_is_option_destructor(pattern_node, &option_kind, &binding)) {
        if (binding != NULL) {
            if (ast_match_case_guard(case_node) != NULL)
                return true;
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                subject, 1, llvm_tmp_name(ctx));
            return llvm_mir_emit_payload_binding(ctx, case_stable_id,
                                                 payload, binding);
        }
        return true;
    }

    if (llvm_mir_is_result_destructor(pattern_node, &result_kind, &binding)) {
        if (binding != NULL) {
            unsigned payload_index =
                pgy_codegen_match_variant_result_payload_index(
                    pgy_codegen_match_variant_lookup(result_kind));
            if (ast_match_case_guard(case_node) != NULL)
                return true;
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                subject, payload_index, llvm_tmp_name(ctx));
            return llvm_mir_emit_payload_binding(ctx, case_stable_id,
                                                 payload, binding);
        }
        return true;
    }

    if (pattern_node->type == AST_CALL) {
        ASTNode *callee = ast_call_callee(pattern_node);
        const char *variant_name = NULL;
        if (callee != NULL && callee->type == AST_IDENTIFIER)
            variant_name = ast_identifier_name(callee);
        else if (callee != NULL && callee->type == AST_MEMBER_ACCESS)
            variant_name = ast_member_name(callee);
        LLVMEnumVariantEntry *variant = variant_name != NULL
            ? llvm_lookup_enum_variant(ctx, variant_name)
            : NULL;
        LLVMClassTypeEntry *enum_cls = variant != NULL
            ? llvm_lookup_class(ctx, variant->enum_name)
            : NULL;
        if (variant != NULL && enum_cls != NULL) {
            int field_idx = llvm_class_field_index(enum_cls, variant_name);
            size_t argc = ast_call_arg_count(pattern_node);
            if (field_idx > 0 && argc > 0) {
                LLVMValueRef payload = LLVMBuildExtractValue(
                    ctx->builder, subject, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                for (size_t i = 0; i < argc; i++) {
                    ASTNode *arg = ast_call_argument(pattern_node, i);
                    LLVMValueRef binding_val;
                    if (arg == NULL || arg->type != AST_IDENTIFIER)
                        continue;
                    binding = ast_identifier_name(arg);
                    if (binding == NULL)
                        continue;
                    binding_val = LLVMBuildExtractValue(ctx->builder,
                        payload, (unsigned)i, llvm_tmp_name(ctx));
                    if (!llvm_mir_emit_payload_binding(ctx, case_stable_id,
                                                       binding_val, binding)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

#endif /* PGY_LLVM_ENABLED */

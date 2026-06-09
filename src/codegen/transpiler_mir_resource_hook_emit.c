/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR resource/cleanup hook emission owner for the C backend.
 */

#include "transpiler_mir_resource_hook_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"
#include "../parser/ast_api.h"
#include "../compiler/mir_type_helpers.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_resource_op_core.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_stmt_emit.h"
#include "transpiler_symbols.h"

static bool
transpiler_mir_name_matches_slot(const char *name, const char *slot)
{
    char base[128];
    size_t version = 0;

    if (name == NULL || slot == NULL)
        return false;
    if (strcmp(name, slot) == 0)
        return true;
    return transpiler_parse_versioned_name(name, base, sizeof(base), &version)
        && strcmp(base, slot) == 0;
}

static const MIRTypeLayout *
transpiler_mir_layout_from_type_annotation(TranspilerCtx *ctx,
                                           ASTNode *type_node)
{
    const MIRTypeLayout *layout = NULL;
    char *type_name = NULL;

    if (ctx == NULL || type_node == NULL)
        return NULL;

    type_name = transpiler_render_effective_local_type_name(ctx, type_node);
    if (type_name == NULL)
        type_name = mir_render_type_name(type_node);
    if (type_name != NULL)
        layout = mir_abi_lookup(type_name);
    free(type_name);
    return layout;
}

static ASTNode *
transpiler_mir_def_type_annotation(const MIRInstruction *inst)
{
    ASTNode *source;

    if (inst == NULL || inst->kind != MIR_INST_DEF)
        return NULL;
    if (inst->expr1 != NULL)
        return inst->expr1;
    source = mir_instruction_source_payload(inst);
    if (mir_instruction_source_is_local_decl(inst) && source != NULL)
        return ast_let_type(source);
    return NULL;
}

static const MIRTypeLayout *
transpiler_mir_find_prior_resource_layout_for_slot(TranspilerCtx *ctx,
                                                   const MIRInstruction *before,
                                                   const char *slot)
{
    TranspilerMIRRoutineInventory inventory;

    if (ctx == NULL || before == NULL || slot == NULL || slot[0] == '\0')
        return NULL;

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t ri = 0; ri < inventory.count; ri++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, ri);
        const MIRTypeLayout *layout = NULL;
        if (routine == NULL)
            continue;
        for (size_t bi = 0; bi < routine->block_count; bi++) {
            const MIRBasicBlock *block = &routine->blocks[bi];
            for (size_t ii = 0; ii < block->instruction_count; ii++) {
                const MIRInstruction *candidate = &block->instructions[ii];
                if (candidate == before)
                    return layout;
                if (candidate->kind == MIR_INST_RESOURCE_OP
                    && candidate->type_layout != NULL
                    && transpiler_mir_name_matches_slot(candidate->slot_anchor, slot)) {
                    layout = candidate->type_layout;
                } else if (candidate->kind == MIR_INST_DEF
                           && candidate->type_layout != NULL
                           && transpiler_mir_name_matches_slot(candidate->result_name, slot)) {
                    layout = candidate->type_layout;
                } else if (candidate->kind == MIR_INST_DEF
                           && transpiler_mir_name_matches_slot(candidate->result_name, slot)) {
                    layout = transpiler_mir_layout_from_type_annotation(
                        ctx, transpiler_mir_def_type_annotation(candidate));
                }
            }
        }
    }
    return NULL;
}

static const char *
transpiler_mir_find_prior_borrow_source_for_view(TranspilerCtx *ctx,
                                                 const MIRInstruction *before,
                                                 const char *view_name)
{
    TranspilerMIRRoutineInventory inventory;

    if (ctx == NULL || before == NULL || view_name == NULL
        || view_name[0] == '\0') {
        return NULL;
    }

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t ri = 0; ri < inventory.count; ri++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, ri);
        const char *source_slot = NULL;
        if (routine == NULL)
            continue;
        for (size_t bi = 0; bi < routine->block_count; bi++) {
            const MIRBasicBlock *block = &routine->blocks[bi];
            for (size_t ii = 0; ii < block->instruction_count; ii++) {
                const MIRInstruction *candidate = &block->instructions[ii];
                TranspilerMIRResourceOp candidate_op =
                    transpiler_mir_resource_op_lookup(candidate->name);
                if (candidate == before)
                    return source_slot;
                if (candidate->kind == MIR_INST_RESOURCE_OP
                    && (candidate_op == TRANS_MIR_RESOURCE_OP_BORROW_READ
                        || candidate_op == TRANS_MIR_RESOURCE_OP_BORROW_WRITE)
                    && transpiler_mir_name_matches_slot(candidate->arg1, view_name)
                    && candidate->arg0 != NULL) {
                    source_slot = candidate->arg0;
                }
            }
        }
    }
    return NULL;
}

bool
transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                  CodeBuf *out,
                                  int indent,
                                  const MIRBasicBlock *owning_block,
                                  const MIRInstruction *inst,
                                  const char *handle_expr,
                                  bool cleanup_hook)
{
    const char *slot_anchor = "";
    const char *arg_name = "";
    const MIRInstruction *emit_inst = inst;
    MIRInstruction inst_copy;
    char *write_value_expr = NULL;
    bool redirected_view_resource = false;
    const char *helper = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";

    if (out == NULL || inst == NULL)
        return false;

    if (inst->kind == MIR_INST_RESOURCE_OP) {
        TranspilerMIRResourceOp op =
            transpiler_mir_resource_op_lookup(inst->name);
        if (!cleanup_hook
            && ctx != NULL
            && ctx->active_ssa_map != NULL
            && op == TRANS_MIR_RESOURCE_OP_WRITE
            && inst->expr0 != NULL) {
            ASTNode *value_node = inst->expr0;
            if (value_node != NULL) {
                write_value_expr = emit_expression_with_ssa_map(
                    value_node,
                    ctx,
                    (TranspilerSSANameMap *)ctx->active_ssa_map);
                if (write_value_expr != NULL && write_value_expr[0] != '\0') {
                    inst_copy = *inst;
                    inst_copy.arg1 = write_value_expr;
                    emit_inst = &inst_copy;
                }
            }
        }
        bool claim_already_materialized_by_stmt = false;
        bool is_claim_op = op == TRANS_MIR_RESOURCE_OP_CLAIM;
        if (!cleanup_hook && is_claim_op) {
            if (transpiler_active_has_mir(ctx)) {
                claim_already_materialized_by_stmt = false;
            } else {
                const char *claim_name = inst->slot_anchor != NULL
                    ? inst->slot_anchor
                    : inst->arg0;
                const char *existing_type = claim_name != NULL
                    ? lookup_typed_var(ctx, claim_name)
                    : NULL;
                char result_base[128];
                size_t result_version = 0;
                bool claim_matches_result =
                    claim_name != NULL
                    && inst->result_name != NULL
                    && transpiler_parse_versioned_name(inst->result_name,
                        result_base, sizeof(result_base), &result_version)
                    && strcmp(claim_name, result_base) == 0;
                if (mir_instruction_source_is_local_decl(inst)
                    && claim_matches_result) {
                    claim_already_materialized_by_stmt = true;
                } else if (claim_name != NULL
                           && existing_type != NULL
                           && (strncmp(existing_type, "Slot<", 5) == 0
                               || strncmp(existing_type, "SecureSlot<", 11) == 0
                               || strncmp(existing_type, "DeviceSlot<", 11) == 0)) {
                    claim_already_materialized_by_stmt = true;
                }
            }
        }
        bool needs_concrete_emit = (cleanup_hook && !is_claim_op)
            || (is_claim_op && !claim_already_materialized_by_stmt && !cleanup_hook);
        if (!cleanup_hook
            && ctx != NULL
            && op == TRANS_MIR_RESOURCE_OP_WRITE
            && inst->slot_anchor != NULL) {
            TypedVarEntry *view_entry = lookup_typed_entry(ctx, inst->slot_anchor);
            const char *view_source_slot = inst->resource_owner_slot_anchor;
            const MIRTypeLayout *source_layout = inst->type_layout;

            if (transpiler_active_has_mir(ctx)
                && inst->resource_owner_requires_metadata
                && ((view_source_slot == NULL || view_source_slot[0] == '\0')
                    || source_layout == NULL)) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "MIR view-backed resource op '%s' is missing owner slot ABI metadata",
                    inst->name != NULL ? inst->name : "<op>");
                free(write_value_expr);
                return false;
            }
            if ((view_source_slot == NULL || view_source_slot[0] == '\0')
                && transpiler_active_has_mir(ctx)) {
                view_source_slot = transpiler_mir_find_prior_borrow_source_for_view(
                    ctx, inst, inst->slot_anchor);
            }
            if ((view_source_slot == NULL || view_source_slot[0] == '\0')
                && view_entry != NULL
                && view_entry->is_view
                && view_entry->source_slot[0] != '\0') {
                view_source_slot = view_entry->source_slot;
            }
            if (source_layout == NULL
                && view_source_slot != NULL
                && view_source_slot[0] != '\0'
                && transpiler_active_has_mir(ctx)) {
                source_layout = transpiler_mir_find_prior_resource_layout_for_slot(
                    ctx, inst, view_source_slot);
            }

            if (view_source_slot != NULL && view_source_slot[0] != '\0') {
                const char *source_type_name = lookup_typed_var(ctx, view_source_slot);
                if (emit_inst != &inst_copy) {
                    inst_copy = *emit_inst;
                    emit_inst = &inst_copy;
                }
                inst_copy.slot_anchor = view_source_slot;
                inst_copy.arg0 = view_source_slot;
                inst_copy.type_layout = source_layout != NULL
                    ? source_layout
                    : (source_type_name != NULL
                    ? mir_abi_lookup(source_type_name)
                    : NULL);
                redirected_view_resource = true;
            }
        }
        if (transpiler_active_has_mir(ctx) && !cleanup_hook) {
            bool slot_is_secure = emit_inst->slot_anchor != NULL
                && lookup_slot_is_secure(ctx, emit_inst->slot_anchor);
            /* Non-secure Write/Release/Move resource ops only become a concrete
             * runtime call when the paired MIR_INST_STMT lives in this same
             * block. When the SSA def-block carries a use-block's resource flow
             * (e.g. while-loop body Write attributed to the slot def block),
             * the canonical concrete emit belongs to the use-block stmt; this
             * block stays observability-only to keep C/LLVM backend parity. */
            bool has_local_mirror_stmt =
                transpiler_mir_resource_has_mirroring_stmt_in_block(
                    owning_block, inst);
            if (is_claim_op
                || redirected_view_resource
                || (has_local_mirror_stmt
                    && (op == TRANS_MIR_RESOURCE_OP_WRITE
                        || op == TRANS_MIR_RESOURCE_OP_RELEASE
                        || op == TRANS_MIR_RESOURCE_OP_MOVE)
                    && !slot_is_secure)) {
                needs_concrete_emit = true;
            }
        }
        if (needs_concrete_emit) {
            if (!transpiler_emit_mir_resource_op(ctx, out, indent, emit_inst, emit_inst->type_layout, NULL)) {
                if (inst->rir_op != NULL
                    && (inst->rir_op->kind == RIR_OP_PROJECT_REFRESH
                        || inst->rir_op->kind == RIR_OP_PROJECT_PUBLISH)) {
                    /* Direct projection expressions already emit the concrete value
                     * via MIR DEF/STMT paths; keep only the observability/export hook
                     * for projection resource ops that do not have a slot runtime ABI. */
                } else {
                    transpiler_set_backend_error_with_hints(
                        ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "cannot emit MIR resource op '%s' for slot '%s': missing typed runtime layout",
                        inst->name != NULL ? inst->name : "<op>",
                        inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
                    free(write_value_expr);
                    return false;
                }
            }
            if (!cleanup_hook) {
                free(write_value_expr);
                return true;
            }
        }
    }

    if (inst->name != NULL) {
        codebuf_write(out, "%*s%s(%s, ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
        codebuf_write(out, "\"%s\", ", inst->name);
    } else {
        codebuf_write(out, "%*s%s(%s, \"unknown\", ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
    }

    if (inst->rir_op != NULL && inst->rir_op->slot_anchor != NULL)
        slot_anchor = inst->rir_op->slot_anchor;
    else if (inst->arg0 != NULL)
        slot_anchor = inst->arg0;
    if (inst->arg1 != NULL)
        arg_name = inst->arg1;
    else if (inst->rir_op != NULL && inst->rir_op->arg0 != NULL)
        arg_name = inst->rir_op->arg0;

    codebuf_write(out, "\"%s\", \"%s\");\n",
        slot_anchor != NULL ? slot_anchor : "",
        arg_name != NULL ? arg_name : "");
    free(write_value_expr);
    return true;
}

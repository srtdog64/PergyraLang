/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR resource/cleanup hook emission owner for the C backend.
 */

#include "transpiler_mir_resource_hook_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../compiler/mir_abi_layout.h"
#include "../semantic/diag_codes.h"
#include "../compiler/mir_type_helpers.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_ssa_names.h"
#include "codegen_mir_resource_name_helpers.h"
#include "transpiler_mir_resource_op_core.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_stmt_emit.h"
#include "transpiler_symbols.h"

static TypedVarEntry *
transpiler_embedded_zone_cleanup_entry(TranspilerCtx *ctx,
                                       const char *versioned_name)
{
    if (ctx == NULL || versioned_name == NULL)
        return NULL;
    for (int i = ctx->typed_var_count - 1; i >= 0; i--) {
        TypedVarEntry *entry = &ctx->typed_vars[i];
        if (entry->requires_embedded_zone_cleanup
            && entry->embedded_zone_cleanup_routine
                == ctx->active_mir_routine
            && strcmp(entry->name, versioned_name) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool
transpiler_embedded_zone_view_for_type(
    TranspilerCtx *ctx,
    const char *type_name,
    TranspilerHostedWorldZoneSlotView *view_out)
{
    TranspilerHostedWorldZoneSlotView view;

    if (view_out == NULL)
        return false;
    memset(&view, 0, sizeof(view));
    if (ctx == NULL || type_name == NULL) {
        *view_out = view;
        return false;
    }
    view = transpiler_hosted_world_zone_slot_view_from_decl(
        ctx, type_name, NULL);
    *view_out = view;
    return view.uses_mir_metadata && view.count > 0;
}

static bool
transpiler_embedded_zone_field_name(
    TranspilerCtx *ctx,
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index,
    const char **field_name_out)
{
    const char *field_name =
        transpiler_hosted_world_zone_slot_view_name(view, index);

    if (field_name_out != NULL)
        *field_name_out = field_name;
    if (field_name != NULL && field_name[0] != '\0')
        return true;
    transpiler_set_mir_inventory_missing(
        ctx,
        "MIR embedded-zone lifecycle is missing world-zone field metadata at index %llu",
        (unsigned long long) index);
    return false;
}

bool
transpiler_emit_mir_embedded_zone_local_guard_decl(
    TranspilerCtx *ctx,
    CodeBuf *out,
    int indent,
    const char *versioned_name,
    const char *type_name,
    const char *c_name)
{
    TranspilerHostedWorldZoneSlotView view;
    TypedVarEntry *entry;

    if (!transpiler_embedded_zone_view_for_type(ctx, type_name, &view))
        return true;
    if (out == NULL || versioned_name == NULL || c_name == NULL)
        return false;
    entry = lookup_typed_entry(ctx, versioned_name);
    if (entry == NULL || strcmp(entry->name, versioned_name) != 0) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR embedded-zone lifecycle is missing SSA local '%s'",
            versioned_name);
        return false;
    }
    entry->requires_embedded_zone_cleanup = true;
    entry->embedded_zone_cleanup_routine = ctx->active_mir_routine;
    write_indent_to(out, indent);
    codebuf_write(out, "bool %s__embedded_zones_initialized = false;\n",
                  c_name);
    return true;
}

bool
transpiler_emit_mir_embedded_zone_local_init(
    TranspilerCtx *ctx,
    CodeBuf *out,
    int indent,
    const char *versioned_name,
    const char *type_name,
    const char *c_name)
{
    TranspilerHostedWorldZoneSlotView view;

    if (!transpiler_embedded_zone_view_for_type(ctx, type_name, &view))
        return true;
    if (out == NULL || c_name == NULL
        || transpiler_embedded_zone_cleanup_entry(ctx, versioned_name) == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR embedded-zone initialization for '%s' has no owned SSA lifetime",
            versioned_name != NULL ? versioned_name : "<local>");
        return false;
    }
    for (size_t i = 0; i < view.count; i++) {
        const char *field_name = NULL;
        if (!transpiler_embedded_zone_field_name(ctx, &view, i,
                                                 &field_name)) {
            return false;
        }
        write_indent_to(out, indent);
        codebuf_write(out, "PGY_ZONE_LOCK_INIT(&%s.%s);\n",
                      c_name, field_name);
    }
    write_indent_to(out, indent);
    codebuf_write(out, "%s__embedded_zones_initialized = true;\n", c_name);
    return true;
}

bool
transpiler_emit_mir_embedded_zone_local_cleanups(
    TranspilerCtx *ctx,
    CodeBuf *out,
    int indent)
{
    if (ctx == NULL || out == NULL)
        return false;
    for (int i = ctx->typed_var_count - 1; i >= 0; i--) {
        TypedVarEntry *entry = &ctx->typed_vars[i];
        TranspilerHostedWorldZoneSlotView view;
        char *c_name;

        if (!entry->requires_embedded_zone_cleanup
            || entry->embedded_zone_cleanup_routine
                != ctx->active_mir_routine) {
            continue;
        }
        if (!transpiler_embedded_zone_view_for_type(
                ctx, entry->type_name, &view)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR embedded-zone cleanup for '%s' lost its world-zone metadata",
                entry->name);
            return false;
        }
        c_name = transpiler_render_ssa_name(ctx, entry->name);
        if (c_name == NULL)
            return false;
        write_indent_to(out, indent);
        codebuf_write(out, "if (%s__embedded_zones_initialized) {\n", c_name);
        for (size_t j = view.count; j > 0; j--) {
            const char *field_name = NULL;
            if (!transpiler_embedded_zone_field_name(ctx, &view, j - 1,
                                                     &field_name)) {
                free(c_name);
                return false;
            }
            write_indent_to(out, indent + 1);
            codebuf_write(out, "PGY_ZONE_LOCK_DESTROY(&%s.%s);\n",
                          c_name, field_name);
        }
        write_indent_to(out, indent + 1);
        codebuf_write(out, "%s__embedded_zones_initialized = false;\n",
                      c_name);
        write_indent_to(out, indent);
        codebuf_write(out, "}\n");
        free(c_name);
    }
    return true;
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
            bool mir_active = transpiler_active_has_mir(ctx);
            TypedVarEntry *view_entry = mir_active
                ? NULL
                : lookup_typed_entry(ctx, inst->slot_anchor);
            const char *view_source_slot = inst->resource_owner_slot_anchor;
            const MIRTypeLayout *source_layout = inst->type_layout;
            const char *expected_owner_slot = mir_active
                ? inst->resource_owner_slot_anchor
                : NULL;

            if (mir_active
                && (inst->resource_owner_requires_metadata
                    || (expected_owner_slot != NULL && expected_owner_slot[0] != '\0'))
                && ((view_source_slot == NULL || view_source_slot[0] == '\0')
                    || source_layout == NULL
                    || (expected_owner_slot != NULL
                        && expected_owner_slot[0] != '\0'
                        && strcmp(view_source_slot, expected_owner_slot) != 0))) {
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
                && mir_active) {
                view_source_slot = expected_owner_slot;
            }
            if ((view_source_slot == NULL || view_source_slot[0] == '\0')
                && view_entry != NULL
                && view_entry->is_view
                && view_entry->source_slot[0] != '\0'
                && !mir_active) {
                view_source_slot = view_entry->source_slot;
            }
            if (view_source_slot != NULL && view_source_slot[0] != '\0') {
                const char *source_type_name = source_layout == NULL && !mir_active
                    ? lookup_typed_var(ctx, view_source_slot)
                    : NULL;
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
                inst_copy.abi_layout_id = mir_abi_layout_id(inst_copy.type_layout);
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
                || mir_instruction_source_is_with_slot_release(emit_inst)
                || (has_local_mirror_stmt
                    && (redirected_view_resource
                        || ((op == TRANS_MIR_RESOURCE_OP_WRITE
                             || op == TRANS_MIR_RESOURCE_OP_RELEASE
                             || op == TRANS_MIR_RESOURCE_OP_MOVE)
                            && !slot_is_secure)))) {
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

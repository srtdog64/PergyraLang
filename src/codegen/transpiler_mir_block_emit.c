#include "transpiler_mir_block_emit.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_defer_emit.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_mir_assignment_emit.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_block_schedule_emit.h"
#include "transpiler_mir_cfg_control_emit.h"
#include "transpiler_mir_destructure_emit.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_match_condition_emit.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_pending_uses.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_preserved_let_emit.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_resource_op_emit.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_mir_stmt_emit.h"
#include "transpiler_statement_dispatch.h"
#include "transpiler_symbols.h"

/* Emit a C preprocessor `#line N "path"` directive at column 0 so the C
 * compiler's debug line table (DWARF/CodeView) maps generated code back to the
 * Pergyra source. The path is escaped for backslash and quote. */
static void
transpiler_emit_c_line_directive(CodeBuf *buf, uint32_t line, const char *path)
{
    const char *p;

    if (buf == NULL || path == NULL || line == 0)
        return;
    codebuf_write(buf, "#line %u \"", (unsigned)line);
    for (p = path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '\"')
            codebuf_write(buf, "\\%c", *p);
        else
            codebuf_write(buf, "%c", *p);
    }
    codebuf_write(buf, "\"\n");
}

bool
transpiler_emit_mir_block_statements(CodeBuf *buf, const ASTNode *func_decl,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *out_ssa_map,
                                     char *reason,
                                     size_t reason_cap)
{
    TranspilerSSANameMap ssa_map = {0};
    TranspilerSSANameMap *ssa_map_out = &ssa_map;
    const void *saved_active_ssa_map;
    const MIRInstruction *saved_active_mir_instruction;
    bool source_order_mode = false;
    size_t *inst_order = NULL;
    bool ok = true;
    const char *routine_name = NULL;

    if (buf == NULL || mir_routine == NULL || block == NULL || ctx == NULL)
        return false;
    routine_name = transpiler_mir_routine_name(mir_routine);
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';

    if (out_ssa_map != NULL)
        ssa_map_out = out_ssa_map;
    if (!transpiler_emit_mir_block_with_ssa_map(ssa_map_out, block)) {
        if (reason != NULL && reason_cap > 0)
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR emission failed for block %llu in %s: missing SSA entry map",
                     (unsigned long long) block->id,
                     routine_name != NULL ? routine_name : "<routine>");
        return false;
    }
    if (!transpiler_mir_emit_for_in_body_binding(buf, mir_routine, block, ctx,
                                                 ssa_map_out)) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: unsupported for-in body binding",
                     (unsigned long long) block->id);
        }
        return false;
    }
    if (!transpiler_mir_seed_block_phi_names(block, ssa_map_out))
        return false;
    saved_active_ssa_map = ctx->active_ssa_map;
    saved_active_mir_instruction = ctx->active_mir_instruction;
    ctx->active_ssa_map = ssa_map_out;
    if (!transpiler_mir_remap_active_match_bindings(
            mir_routine, block, ctx, ssa_map_out)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        ctx->active_mir_instruction = saved_active_mir_instruction;
        return false;
    }
    if (!transpiler_mir_emit_match_case_body_binding(
            buf, mir_routine, block, ctx, ssa_map_out)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        ctx->active_mir_instruction = saved_active_mir_instruction;
        return false;
    }
    if (!transpiler_emit_mir_pin_enter_local(buf, ctx, block, reason, reason_cap)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        ctx->active_mir_instruction = saved_active_mir_instruction;
        return false;
    }
    if (!transpiler_mir_seed_pin_view_alias(block, ssa_map_out)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        ctx->active_mir_instruction = saved_active_mir_instruction;
        return false;
    }
    /* Seed function parameter names into the per-block SSA name map so that
     * resource-op emit-time identifier resolution (via
     * `transpiler_expr_identifiers_mapped`) treats a function parameter as
     * mapped. Without this seeding, a Write resource op whose value expression
     * references a parameter (e.g. `Write(slot, seed)`) silently fails the
     * mapping precheck, returns HANDLED, and is dropped from the C output --
     * while the paired residual stmt is also skipped under the mirrored-
     * resource rule, leaving the entire Write call missing. The per-block
     * map is regenerated from MIR SSA entry values by
     * `transpiler_emit_mir_block_with_ssa_map` above, so we must seed
     * parameters AFTER that regeneration and BEFORE instruction emission. */
    {
        size_t pcnt = mir_routine_param_count(mir_routine);
        for (size_t p = 0; p < pcnt; p++) {
            FuncParam *fp = mir_routine_param(mir_routine, p);
            if (fp != NULL && fp->name != NULL
                && transpiler_resolve_ssa_name(ssa_map_out, fp->name) == NULL) {
                transpiler_ssa_name_map_set(ssa_map_out, fp->name, fp->name);
            }
        }
        if (transpiler_resolve_ssa_name(ssa_map_out, "self") == NULL) {
            transpiler_ssa_name_map_set(ssa_map_out, "self", "self");
        }
    }
    source_order_mode = !transpiler_mir_routine_has_explicit_cfg(mir_routine)
        && transpiler_mir_block_has_source_order_metadata(block)
        && block->instruction_count > 0;
    if (source_order_mode) {
        if (!transpiler_mir_block_build_source_order(block, &inst_order,
                                                     reason, reason_cap)) {
            ctx->active_ssa_map = saved_active_ssa_map;
            ctx->active_mir_instruction = saved_active_mir_instruction;
            return false;
        }
    }

    if (!transpiler_emit_mir_claim_prepass(buf, block, ctx,
                                           reason, reason_cap)) {
        free(inst_order);
        ctx->active_ssa_map = saved_active_ssa_map;
        ctx->active_mir_instruction = saved_active_mir_instruction;
        return false;
    }

    uint32_t debug_last_line = 0;
    const char *debug_source_path = transpiler_active_source_path(ctx);
    for (size_t i = 0; i < block->instruction_count; i++) {
        size_t inst_index = source_order_mode ? inst_order[i] : i;
        const MIRInstruction *inst = &block->instructions[inst_index];
        ctx->active_mir_instruction = inst;

        if (debug_source_path != NULL) {
            uint32_t debug_line = mir_instruction_source_line(inst);
            if (debug_line != 0 && debug_line != debug_last_line) {
                transpiler_emit_c_line_directive(buf, debug_line,
                                                 debug_source_path);
                debug_last_line = debug_line;
            }
        }

        if (!transpiler_materialize_pending_inst_uses(buf, ctx,
                                                      mir_routine, block, inst,
                                                      ssa_map_out, ctx->indent,
                                                      true, reason, reason_cap)) {
            ok = false;
            break;
        }

        if (inst->kind == MIR_INST_RESOURCE_OP) {
            TranspilerMIRInstEmitResult resource_result =
                transpiler_emit_mir_resource_op_inst(
                    buf, mir_routine, block, inst, inst_index, ctx,
                    ssa_map_out, reason, reason_cap);
            if (resource_result == TRANSPILE_MIR_INST_FAILED) {
                ok = false;
                break;
            }
            if (resource_result == TRANSPILE_MIR_INST_HANDLED)
                continue;
        }

        {
            TranspilerMIRLocalLetEmitResult let_result =
                transpiler_emit_mir_source_local_let_def_inst(
                    buf, mir_routine, block, inst, ctx, ssa_map_out,
                    reason, reason_cap);
            if (let_result == TRANSPILE_MIR_LOCAL_LET_FAILED) {
                ok = false;
                break;
            }
            if (let_result == TRANSPILE_MIR_LOCAL_LET_HANDLED)
                continue;
        }

        {
            TranspilerMIRAssignmentEmitResult assignment_result =
                transpiler_emit_mir_assignment_def_inst(
                    buf, func_decl, mir_routine, block, inst, inst_index, ctx,
                    ssa_map_out, reason, reason_cap);
            if (assignment_result == TRANSPILE_MIR_ASSIGNMENT_FAILED) {
                ok = false;
                break;
            }
            if (assignment_result == TRANSPILE_MIR_ASSIGNMENT_HANDLED)
                continue;
        }

        if (inst->kind == MIR_INST_DEF
            && inst->expr0 != NULL
            && !mir_instruction_uses_source_statement_emit(inst)
            && inst->result_name != NULL) {
            ASTNode *def_expr = inst->expr0;
            char *lhs = NULL;
            char *rhs = NULL;
            const char *binding_name = inst->arg0;
            const char *value_type = NULL;

            if (binding_name != NULL)
                value_type = lookup_typed_var(ctx, binding_name);
            if ((value_type == NULL || strcmp(value_type, "Unknown") == 0)
                && mir_routine != NULL
                && binding_name != NULL
                && transpiler_mir_routine_has_source_local_binding(
                    mir_routine, binding_name)) {
                value_type = transpiler_mir_routine_source_local_type_name(
                    mir_routine, binding_name);
                if (value_type == NULL
                    || value_type[0] == '\0'
                    || strcmp(value_type, "Unknown") == 0) {
                    if (reason != NULL && reason_cap > 0) {
                        transpiler_mir_reasonf(reason, reason_cap,
                            "MIR block %llu emission failed: DEF '%s' is missing source-local type metadata",
                            (unsigned long long) block->id,
                            binding_name);
                    }
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR block %llu emission failed: DEF '%s' is missing source-local type metadata",
                        (unsigned long long) block->id,
                        binding_name);
                    ok = false;
                    break;
                }
            }
            if (value_type == NULL || strcmp(value_type, "Unknown") == 0)
                value_type = infer_expression_type_name(ctx, def_expr);

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            {
                const char *saved_expected_type = ctx->expected_type;
                ctx->expected_type = value_type;
                rhs = emit_expression_with_ssa_map(def_expr, ctx, ssa_map_out);
                ctx->expected_type = saved_expected_type;
            }
            if (lhs == NULL || rhs == NULL) {
                free(lhs);
                free(rhs);
                if (reason != NULL && reason_cap > 0) {
                    transpiler_mir_reasonf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render DEF for '%s'",
                             (unsigned long long) block->id,
                             binding_name != NULL ? binding_name : inst->result_name);
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (binding_name != NULL) {
                if (!transpiler_ssa_name_map_set(ssa_map_out,
                                                 binding_name,
                                                 inst->result_name)) {
                    ok = false;
                    break;
                }
                if (value_type != NULL
                    && value_type[0] != '\0'
                    && strcmp(value_type, "Unknown") != 0) {
                    register_typed_var(ctx, binding_name, value_type);
                }
            }
            continue;
        }

        if (inst->kind == MIR_INST_LOOP_INIT) {
            if (!transpiler_mir_emit_for_loop_init_inst(buf, inst, ctx,
                                                        ssa_map_out)) {
                if (reason != NULL && reason_cap > 0) {
                    transpiler_mir_reasonf(reason, reason_cap,
                             "MIR block %llu emission failed: unsupported for-in CFG lowering",
                             (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind == MIR_INST_DESTRUCTURE) {
            if (inst->expr0 == NULL
                || mir_instruction_destructure_binding_count(inst) == 0) {
                if (reason != NULL && reason_cap > 0) {
                    transpiler_mir_reasonf(reason, reason_cap,
                        "MIR block %llu emission failed: DESTRUCTURE instruction missing MIR destructure facts",
                        (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            if (!transpiler_emit_mir_let_destructure_stmt(
                    buf, block, inst, ctx, ssa_map_out,
                    reason, reason_cap)) {
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind == MIR_INST_ASSIGN) {
            if (inst->expr0 == NULL || inst->expr1 == NULL) {
                if (reason != NULL && reason_cap > 0) {
                    transpiler_mir_reasonf(reason, reason_cap,
                        "MIR block %llu emission failed: ASSIGN instruction missing MIR assignment facts",
                        (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            if (!transpiler_emit_mir_assignment_expr_stmt(
                    buf, block, inst, ctx, ssa_map_out,
                    reason, reason_cap)) {
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind != MIR_INST_STMT)
            continue;
        if (mir_instruction_source_is_defer_stmt(inst)) {
            transpiler_register_defer(inst->expr0, ctx);
            continue;
        }
        if (mir_instruction_source_stmt_reemit_is_redundant(inst)) {
            /* Closure #82: AST_LET_DESTRUCTURE is already lowered by
             * MIR_INST_DESTRUCTURE above (transpiler_emit_mir_let_destructure_stmt).
             * Falling through to emit_statement here re-runs the non-MIR
             * destructure emit which redeclares the same C locals with a
             * type prefix, producing gcc "previous definition" errors on
             * destructure_array, destructure_tuple_return, tuple_literal_local. */
            continue;
        }
        if (!mir_instruction_source_stmt_residual_emit_is_allowed(inst)) {
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: residual STMT emit outside allowed residual statement policy",
                    (unsigned long long) block->id);
            }
            ok = false;
            break;
        }
        if (transpiler_mir_stmt_is_mirrored_resource(ctx, block, inst))
            continue;
        if (transpiler_mir_routine_has_explicit_cfg(mir_routine)
            && transpiler_mir_inst_is_cfg_container(inst)) {
            continue;
        }
        if (inst->name != NULL && strcmp(inst->name, "bind") == 0) {
            if (inst->arg0 == NULL || inst->slot_anchor == NULL
                || inst->arg1 == NULL
                || !transpiler_emit_bind_statement_parts(
                    ctx, inst->arg0, inst->slot_anchor, inst->arg1)) {
                if (reason != NULL && reason_cap > 0) {
                    transpiler_mir_reasonf(reason, reason_cap,
                        "MIR block %llu emission failed: bind statement missing MIR bind facts",
                        (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            continue;
        }
        if (mir_instruction_source_stmt_call_emit_is_allowed(inst)) {
            if (!transpiler_emit_mir_call_statement(buf, block, inst,
                                                    inst->expr0, ctx,
                                                    ssa_map_out,
                                                    reason, reason_cap)) {
                ok = false;
                break;
            }
            continue;
        }
        if (mir_instruction_source_stmt_runtime_boundary_emit_is_allowed(inst)) {
            emit_statement(inst->expr0, ctx);
            continue;
        }
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                "MIR block %llu emission failed: STMT source-payload emission is retired; lower this statement to MIR facts",
                (unsigned long long) block->id);
        }
        ok = false;
        break;
    }
    ctx->active_ssa_map = saved_active_ssa_map;
    ctx->active_mir_instruction = saved_active_mir_instruction;
    return ok;
}

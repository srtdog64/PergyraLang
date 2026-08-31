#include "transpiler_mir_terminator_emit.h"

#include <stdlib.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_control_flow_emit.h"
#include "transpiler_defer_emit.h"
#include "transpiler_func_flow_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_cfg_control_emit.h"
#include "transpiler_mir_phi_emit.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "../semantic/diag_codes.h"

static void
transpiler_mir_set_pin_cleanup_error(TranspilerCtx *ctx,
                                     const char *name,
                                     const MIRBasicBlock *block,
                                     const char *block_reason)
{
    if (ctx == NULL || ctx->backend_error != NULL || block == NULL)
        return;
    transpiler_set_mir_topology_invalid(
        ctx,
        "MIR pin cleanup emission failed in function '%s' at block %llu: %s",
        name != NULL ? name : "<function>",
        (unsigned long long) block->id,
        block_reason != NULL && block_reason[0] != '\0'
            ? block_reason
            : "unknown reason");
}

static ASTNode *
transpiler_mir_return_callable_type(TranspilerCtx *ctx,
                                    const MIRRoutine *mir_routine)
{
    ASTNode *return_type = transpiler_mir_routine_return_type(mir_routine);

    if (return_type != NULL) {
        if (return_type->type == AST_EVENT_HANDLER_TYPE)
            return return_type;
        return NULL;
    }
    return transpiler_func_current_return_callable_type(ctx);
}

static bool
transpiler_emit_mir_branch_terminator(const MIRRoutine *mir_routine,
                                      const MIRBasicBlock *block,
                                      size_t block_index,
                                      const MIRInstruction *inst,
                                      const char *name,
                                      TranspilerCtx *ctx,
                                      TranspilerSSANameMap *block_ssa_map,
                                      char *block_reason,
                                      size_t block_reason_cap)
{
    char *cond = transpiler_mir_render_branch_condition(
        mir_routine, inst, block->succ_true, ctx, block_ssa_map);

    if (cond == NULL) {
        if (ctx != NULL && ctx->backend_error == NULL) {
            transpiler_set_mir_topology_invalid(ctx,
                "MIR branch condition emission failed in function '%s' at block %llu",
                name != NULL ? name : "<function>",
                (unsigned long long) block->id);
        }
        return false;
    }

    if (block->has_succ_true) {
        transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent, block_index,
            block, &mir_routine->blocks[block->succ_true]);
    }
    if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                            block_reason,
                                            block_reason_cap)) {
        free(cond);
        transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
        return false;
    }

    write_indent(ctx);
    transpiler_write_condition_head(ctx, "if", cond, " {\n");
    write_indent_to(ctx->out, ctx->indent + 1);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                  name, block->succ_true);
    write_indent(ctx);
    codebuf_write(ctx->out, "} else {\n");
    if (block->has_succ_false) {
        transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent + 1,
            block_index, block, &mir_routine->blocks[block->succ_false]);
    }
    write_indent_to(ctx->out, ctx->indent + 1);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                  name, block->succ_false);
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    free(cond);
    return true;
}

static bool
transpiler_emit_mir_return_terminator(const MIRBasicBlock *block,
                                      const MIRRoutine *mir_routine,
                                      const MIRInstruction *inst,
                                      const char *name,
                                      TranspilerCtx *ctx,
                                      TranspilerSSANameMap *block_ssa_map,
                                      char *block_reason,
                                      size_t block_reason_cap)
{
    char *ret_expr = NULL;
    char return_temp[64];
    const char *return_expr = NULL;

    if (inst->expr0 != NULL) {
        const char *saved_expected_type = ctx->expected_type;
        ASTNode *saved_expected_callable_type = ctx->expected_callable_type;
        ASTNode *return_callable_type =
            transpiler_mir_return_callable_type(ctx, mir_routine);
        if (return_callable_type != NULL)
            ctx->expected_callable_type = return_callable_type;
        if (ctx->current_return_type[0] != '\0'
            && strcmp(ctx->current_return_type, "Void") != 0
            && strcmp(ctx->current_return_type, "void") != 0) {
            ctx->expected_type = ctx->current_return_type;
        }
        ret_expr = emit_expression_with_ssa_map(inst->expr0, ctx, block_ssa_map);
        ctx->expected_callable_type = saved_expected_callable_type;
        ctx->expected_type = saved_expected_type;
        if (ret_expr == NULL) {
            if (ctx != NULL && ctx->backend_error == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "MIR return terminator in function '%s' could not lower return value",
                    name != NULL ? name : "<function>");
            }
            return false;
        }
        return_expr = transpiler_emit_mut_ref_return_capture(
            ctx, ret_expr, return_temp, sizeof(return_temp));
        if (return_expr == NULL) {
            free(ret_expr);
            return false;
        }
    } else if (strcmp(ctx->current_return_type, "Void") != 0) {
        if (ctx != NULL && ctx->backend_error == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_CFG_MISSING_RETURN,
                PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                "MIR non-Void return terminator in function '%s' requires a value",
                name != NULL ? name : "<function>");
        }
        return false;
    }
    transpiler_emit_defers_from(ctx, 0);
    if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                            block_reason,
                                            block_reason_cap)) {
        free(ret_expr);
        transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
        return false;
    }

    transpiler_emit_mut_ref_writebacks(ctx);
    if (!transpiler_emit_mir_embedded_zone_local_cleanups(
            ctx, ctx->out, ctx->indent)) {
        free(ret_expr);
        return false;
    }
    transpiler_region_scope_destroy(ctx);
    write_indent(ctx);
    if (inst->expr0 != NULL) {
        codebuf_write(ctx->out, "return %s;\n", return_expr);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
    free(ret_expr);
    return true;
}

bool
transpiler_emit_mir_explicit_terminator(const MIRRoutine *mir_routine,
                                        const MIRBasicBlock *block,
                                        size_t block_index,
                                        const char *name,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *block_ssa_map,
                                        bool *terminator_emitted,
                                        char *block_reason,
                                        size_t block_reason_cap)
{
    if (terminator_emitted != NULL)
        *terminator_emitted = false;
    for (size_t j = 0; j < block->instruction_count; j++) {
        const MIRInstruction *inst = &block->instructions[j];
        if (inst->kind == MIR_INST_RESOURCE_OP)
            continue;
        if (inst->kind == MIR_INST_BRANCH) {
            if (!transpiler_emit_mir_branch_terminator(
                    mir_routine, block, block_index, inst, name, ctx,
                    block_ssa_map, block_reason, block_reason_cap)) {
                return false;
            }
            if (terminator_emitted != NULL)
                *terminator_emitted = true;
            return true;
        }
        if (inst->kind == MIR_INST_RETURN) {
            if (!transpiler_emit_mir_return_terminator(
                    block, mir_routine, inst, name, ctx, block_ssa_map,
                    block_reason, block_reason_cap)) {
                return false;
            }
            if (terminator_emitted != NULL)
                *terminator_emitted = true;
            return true;
        }
    }
    return true;
}

bool
transpiler_emit_mir_fallthrough_terminator(const MIRRoutine *mir_routine,
                                           const MIRBasicBlock *block,
                                           size_t block_index,
                                           const char *name,
                                           TranspilerCtx *ctx,
                                           char *block_reason,
                                           size_t block_reason_cap)
{
    if (block->has_succ_true) {
        transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent,
            block_index, block, &mir_routine->blocks[block->succ_true]);
        if (!transpiler_mir_emit_loop_backedge_increment(ctx->out, ctx,
                                                         mir_routine,
                                                         block)) {
            return false;
        }
        if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                                block_reason,
                                                block_reason_cap)) {
            transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                      name, block->succ_true);
        return true;
    }

    if (!block->has_succ_false) {
        transpiler_emit_defers_from(ctx, 0);
        if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                                block_reason,
                                                block_reason_cap)) {
            transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
            return false;
        }
        transpiler_emit_mut_ref_writebacks(ctx);
        if (!transpiler_emit_mir_embedded_zone_local_cleanups(
                ctx, ctx->out, ctx->indent)) {
            return false;
        }
        transpiler_region_scope_destroy(ctx);
        write_indent(ctx);
        if (strcmp(ctx->current_return_type, "Void") == 0) {
            codebuf_write(ctx->out, "return;\n");
        } else {
            /* Non-void fall-through is unreachable for well-typed values, so
             * fail closed -- matching the AST-direct emitter
             * (transpiler_func_class_flow_emit.c) and the LLVM backend
             * (llvm_mir_block_emit.c) -- rather than leaving UB if an invalid
             * discriminant from the unsafe/FFI boundary reaches the
             * no-arm-matched end of an exhaustive match. */
            codebuf_write(ctx->out,
                "PGY_PANIC(\"non-void function reached end without return\");\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "__builtin_unreachable();\n");
        }
    }
    return true;
}

#include "transpiler_intent_cleanup_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "../parser/ast_api.h"
#include "transpiler_block_intent_helpers.h"
#include "transpiler_context.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_intent_query.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_ssa_map.h"

static bool
transpiler_emit_intent_missing_carrier(TranspilerCtx *ctx, const char *message)
{
    transpiler_set_mir_intent_carrier_missing(ctx, "%s", message);
    return false;
}

static bool
transpiler_emit_intent_mir_resource_block(ASTNode *node,
                                          TranspilerCtx *ctx,
                                          const MIRRoutine *mir_routine,
                                          const MIRBasicBlock *block)
{
    const char *intent_name = ast_intent_decl_name(node);

    if (node == NULL || ctx == NULL || mir_routine == NULL || block == NULL)
        return true;

    transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
        intent_name, mir_routine, block);
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP) {
            if (!transpiler_emit_mir_resource_hook(
                    ctx, ctx->out, ctx->indent, inst, "__intent_handle", true)) {
                return false;
            }
        }
    }
    return true;
}

bool
transpiler_emit_intent_cleanup_tail(ASTNode *node,
                                    TranspilerCtx *ctx,
                                    bool mir_only_intent,
                                    bool emit_cleanup_from_mir,
                                    bool has_compensate_steps,
                                    bool needs_cleanup_done_label,
                                    ASTNode **step_nodes,
                                    size_t step_count,
                                    const char **mir_step_names,
                                    const MIRRoutine *mir_routine,
                                    const char **participant_aliases,
                                    const char **participant_types,
                                    size_t participant_count)
{
    const char *intent_name = ast_intent_decl_name(node);
    IntentRollbackPolicy rollback_policy =
        ast_intent_decl_rollback_policy(node);

    if (node == NULL || ctx == NULL)
        return false;

    if (emit_cleanup_from_mir) {
        const MIRBasicBlock *cleanup_block = &mir_routine->blocks[mir_routine->cleanup_block];
        const MIRBasicBlock *rollback_block = mir_routine->has_rollback_block
            ? &mir_routine->blocks[mir_routine->rollback_block] : NULL;
        const MIRBasicBlock *invalidation_block = mir_routine->has_invalidation_block
            ? &mir_routine->blocks[mir_routine->invalidation_block] : NULL;
        write_indent(ctx);
        codebuf_write(ctx->out, "/* cleanup-emitted-from-mir */\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n",
            intent_name, mir_routine->cleanup_block);
        ctx->indent++;
        if (!transpiler_emit_intent_mir_resource_block(node, ctx, mir_routine, cleanup_block))
            return false;
        if (mir_routine->has_rollback_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_failed) {\n");
            write_indent_to(ctx->out, ctx->indent + 1);
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                intent_name, mir_routine->rollback_block);
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
        if (mir_routine->has_invalidation_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                intent_name, mir_routine->invalidation_block);
        } else {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "return __intent_result;\n");
        }
        ctx->indent--;
        if (mir_routine->has_rollback_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n",
                intent_name, mir_routine->rollback_block);
            ctx->indent++;
            if (!transpiler_emit_intent_mir_resource_block(node, ctx, mir_routine, rollback_block))
                return false;
            if (has_compensate_steps && step_count > 0) {
                for (size_t i = step_count; i-- > 0;) {
                    ASTNode *step = step_nodes[i];
                    const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
                    ASTNode **compensate_exprs = NULL;
                    size_t compensate_expr_count = 0;
                    const char *zone_type_name = NULL;
                    const char *zone_alias = NULL;
                    const char *from_alias = NULL;
                    const char **who_aliases = NULL;
                    size_t who_alias_count = 0;
                    bool has_compensate = false;
                    if (step != NULL && step->type == AST_INTENT_STEP) {
                        if (step_name == NULL)
                            step_name = ast_intent_step_name(step);
                        has_compensate = mir_only_intent
                            ? transpiler_mir_intent_has_stmt(
                                mir_routine, step_name,
                                "IntentEval", "compensate")
                            : ast_intent_step_compensate_expr_count(step) > 0;
                    }
                    if (step == NULL || step->type != AST_INTENT_STEP || !has_compensate)
                        continue;
                    if (mir_routine != NULL) {
                        compensate_expr_count = transpiler_collect_mir_intent_eval_exprs(
                            mir_routine, step_name, "compensate", &compensate_exprs);
                        zone_type_name = transpiler_find_mir_intent_meta_arg(
                            mir_routine, step_name, "IntentZoneWhere");
                        zone_alias = transpiler_find_mir_intent_meta_arg(
                            mir_routine, step_name, "IntentZoneAlias");
                        from_alias = transpiler_find_mir_intent_meta_arg(
                            mir_routine, step_name, "IntentZoneFrom");
                        who_alias_count = transpiler_collect_mir_intent_who_aliases(
                            mir_routine, step_name, &who_aliases);
                    }
                    if (mir_only_intent
                        && transpiler_mir_intent_has_stmt(
                            mir_routine, step_name,
                            "IntentEval", "compensate")
                        && compensate_expr_count == 0) {
                        return transpiler_emit_intent_missing_carrier(
                            ctx, "MIR-only C path missing intent compensate eval carrier");
                    }
                    if (!mir_only_intent && compensate_expr_count == 0) {
                        compensate_expr_count = ast_intent_step_compensate_expr_count(step);
                        compensate_exprs = ast_intent_step_compensate_exprs(step, NULL);
                    }
                    if (mir_only_intent) {
                        if (transpiler_mir_intent_has_stmt(
                                mir_routine, step_name, "IntentZoneWhere", NULL)
                            && zone_type_name == NULL)
                            return transpiler_emit_intent_missing_carrier(
                                ctx, "MIR-only C path missing intent zone where metadata");
                        if (transpiler_mir_intent_has_stmt(
                                mir_routine, step_name, "IntentZoneAlias", NULL)
                            && zone_alias == NULL)
                            return transpiler_emit_intent_missing_carrier(
                                ctx, "MIR-only C path missing intent zone alias metadata");
                        if (transpiler_mir_intent_has_stmt(
                                mir_routine, step_name, "IntentZoneFrom", NULL)
                            && from_alias == NULL)
                            return transpiler_emit_intent_missing_carrier(
                                ctx, "MIR-only C path missing intent transfer-from metadata");
                        if (transpiler_mir_intent_has_stmt(
                                mir_routine, step_name, "IntentWho", NULL)
                            && who_alias_count == 0)
                            return transpiler_emit_intent_missing_carrier(
                                ctx, "MIR-only C path missing intent who metadata");
                    }
                    write_indent(ctx);
                    codebuf_write(ctx->out, "if (__intent_step_completed[%zu]) {\n", i);
                    ctx->indent++;
                    for (size_t j = compensate_expr_count; j-- > 0;) {
                        char *expr = emit_expression(compensate_exprs[j], ctx);
                        if (expr != NULL && expr[0] != '\0') {
                            write_indent(ctx);
                            codebuf_write(ctx->out, "%s;\n", expr);
                        }
                        free(expr);
                    }
                    if (mir_only_intent) {
                        emit_intent_step_bind_bound_zone_with_metadata(
                            ctx->out, ctx, node, zone_type_name, zone_alias, from_alias,
                            who_aliases, who_alias_count,
                            participant_aliases, participant_types, participant_count);
                    } else {
                        emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
                    }
                    if (rollback_policy == INTENT_ROLLBACK_CURRENT) {
                        if (mir_routine->has_invalidation_block) {
                            write_indent(ctx);
                            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                                intent_name, mir_routine->invalidation_block);
                        } else {
                            write_indent(ctx);
                            codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
                            write_indent(ctx);
                            codebuf_write(ctx->out, "return __intent_result;\n");
                        }
                    }
                    ctx->indent--;
                    write_indent(ctx);
                    codebuf_write(ctx->out, "}\n");
                    if (mir_only_intent)
                        free(compensate_exprs);
                    if (mir_only_intent)
                        free((void *)who_aliases);
                }
            }
            if (mir_routine->has_invalidation_block) {
                write_indent(ctx);
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                    intent_name, mir_routine->invalidation_block);
            } else {
                write_indent(ctx);
                codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
                write_indent(ctx);
                codebuf_write(ctx->out, "return __intent_result;\n");
            }
            ctx->indent--;
        }
        if (mir_routine->has_invalidation_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n",
                intent_name, mir_routine->invalidation_block);
            ctx->indent++;
            if (!transpiler_emit_intent_mir_resource_block(node, ctx, mir_routine, invalidation_block))
                return false;
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "return __intent_result;\n");
            ctx->indent--;
        }
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_cleanup:\n");
        ctx->indent++;
        if (has_compensate_steps && step_count > 0) {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_failed) {\n");
            ctx->indent++;
            for (size_t i = step_count; i-- > 0;) {
                ASTNode *step = step_nodes[i];
                if (step == NULL || step->type != AST_INTENT_STEP
                    || ast_intent_step_compensate_expr_count(step) == 0)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "if (__intent_step_completed[%zu]) {\n", i);
                ctx->indent++;
                for (size_t j = ast_intent_step_compensate_expr_count(step); j-- > 0;) {
                    char *expr = emit_expression(ast_intent_step_compensate_exprs(step, NULL)[j], ctx);
                    if (expr != NULL && expr[0] != '\0') {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s;\n", expr);
                    }
                    free(expr);
                }
                emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
                if (rollback_policy == INTENT_ROLLBACK_CURRENT) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "goto __intent_cleanup_done;\n");
                }
                ctx->indent--;
                write_indent(ctx);
                codebuf_write(ctx->out, "}\n");
            }
            ctx->indent--;
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
        if (needs_cleanup_done_label) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__intent_cleanup_done:\n");
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "return __intent_result;\n");
    }
    ctx->indent--;
    return true;
}

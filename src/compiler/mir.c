#include "mir.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../common/arena.h"
#include "../runtime/pgy_abi_spec.h"
#include "../parser/ast_api.h"
#include "mir_lower_population.h"
#include "mir_public_surface.h"

static MIRBranchShape
mir_branch_shape_from_ast(const ASTNode *node)
{
    if (node == NULL)
        return MIR_BRANCH_EXPR;
    if (node->type == AST_FOR_LOOP)
        return ast_for_iterable(node) != NULL
            ? MIR_BRANCH_FOR_IN
            : MIR_BRANCH_FOR_RANGE;
    if (node->type == AST_MATCH_CASE)
        return MIR_BRANCH_MATCH_CASE;
    if (node->type == AST_BLOCK)
        return MIR_BRANCH_SELECT_DISPATCH;
    return MIR_BRANCH_EXPR;
}

#include "mir_base_helpers.h"
#include "mir_cleanup.h"
#include "mir_intent.h"
#include "mir_surface_usage.h"
#include "mir_stmt_population.h"
#include "mir_type_helpers.h"
#include "mir_validation.h"

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->source_phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.kind = MIR_INST_PHI;
        inst.name = block->source_phi_nodes[i].name;
        inst.slot_anchor = block->source_phi_nodes[i].name;
        inst.arg0 = "phi";
        if (!mir_commit_instruction(routine, block, &inst))
            return false;
    }
    return true;
}

static bool
mir_add_terminator_instruction(MIRRoutine *routine,
                               MIRBasicBlock *block,
                               HIRBlockTerminatorKind terminator_kind,
                               ASTNode *terminator_condition,
                               ASTNode *terminator_value)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL)
        return false;
    if (terminator_kind != HIR_BLOCK_BRANCH
        && terminator_kind != HIR_BLOCK_RETURN)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.kind = (terminator_kind == HIR_BLOCK_BRANCH)
                    ? MIR_INST_BRANCH
                    : MIR_INST_RETURN;
    inst.name = (terminator_kind == HIR_BLOCK_BRANCH) ? "branch" : "return";
    inst.source_terminator_kind = terminator_kind;
    inst.has_source_terminator_kind = true;
    inst.source_terminator_has_value = terminator_value != NULL;
    inst.ast = (terminator_kind == HIR_BLOCK_BRANCH)
                   ? terminator_condition
                   : terminator_value;
    if (inst.kind == MIR_INST_BRANCH) {
        inst.branch_shape = mir_branch_shape_from_ast(inst.ast);
        inst.requires_source_branch_emit =
            inst.branch_shape == MIR_BRANCH_MATCH_CASE
            || inst.branch_shape == MIR_BRANCH_SELECT_DISPATCH;
    }
    if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_EXPR)
        inst.expr0 = terminator_condition;
    else if (inst.kind == MIR_INST_RETURN)
        inst.expr0 = terminator_value;
    if (inst.kind == MIR_INST_BRANCH
        && inst.ast != NULL
        && inst.ast->type == AST_FOR_LOOP) {
        inst.arg0 = ast_for_variable(inst.ast);
        if (ast_for_iterable(inst.ast) != NULL) {
            inst.expr0 = ast_for_iterable(inst.ast);
            inst.expr1 = ast_for_iterable(inst.ast);
        } else {
            inst.expr0 = ast_for_range_start(inst.ast);
            inst.expr1 = ast_for_range_end(inst.ast);
        }
    }
    return mir_commit_instruction(routine, block, &inst);
}

static bool
mir_copy_ast_nodes(ASTNode ***dst, size_t *dst_count, ASTNode **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(ASTNode *));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(ASTNode *));
    *dst_count = src_count;
    return true;
}

static bool
mir_copy_names(const char ***dst, size_t *dst_count, const char **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(const char *));
    if (*dst == NULL)
        return false;
    memcpy((void *)*dst, src, src_count * sizeof(const char *));
    *dst_count = src_count;
    return true;
}

static bool
mir_copy_phi_nodes(MIRSourcePhiNode **dst, size_t *dst_count,
                   const HIRPhiNode *src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(MIRSourcePhiNode));
    if (*dst == NULL)
        return false;
    *dst_count = src_count;
    for (size_t i = 0; i < src_count; i++) {
        (*dst)[i].name = src[i].name;
        if (!copy_indices(&(*dst)[i].incoming_predecessors,
                          &(*dst)[i].incoming_predecessor_count,
                          src[i].incoming_predecessors,
                          src[i].incoming_predecessor_count)) {
            for (size_t j = 0; j < i; j++)
                free((*dst)[j].incoming_predecessors);
            free(*dst);
            *dst = NULL;
            *dst_count = 0;
            return false;
        }
    }
    return true;
}

static void
mir_block_record_source_location(MIRBasicBlock *block, const ASTNode *source_ast)
{
    if (block == NULL)
        return;
    block->has_source_location = source_ast != NULL;
    block->source_line = source_ast != NULL ? source_ast->line : 0;
    block->source_column = source_ast != NULL ? source_ast->column : 0;
}

#include "mir_ssa_rename.h"

#include "mir_liveness_dce.h"
#include "mir_dce.h"

#include "mir_fact_validate.h"

static bool
mir_build_blocks_from_hir(MIRRoutine *routine, const HIRRoutine *hir_routine)
{
    if (routine == NULL)
        return false;

    if (hir_routine == NULL || !hir_routine->has_cfg || hir_routine->cfg.block_count == 0) {
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        mir_block_record_source_location(&block, NULL);
        routine->entry_block = 0;
        return append_block(routine, block);
    }

    routine->entry_block = hir_routine->cfg.entry_block;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        const ASTNode *source_ast = NULL;
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = i;
        block.is_entry = (i == hir_routine->cfg.entry_block);
        block.is_reachable = src->is_reachable;
        block.is_pin_region = src->is_pin_region;
        block.is_select_case_body = src->is_select_case_body;
        block.pin_view_is_write = src->pin_view_is_write;
        block.pin_source_name = src->pin_source_name;
        block.pin_view_name = src->pin_view_name;
        block.pin_block_ast = src->pin_block_ast;
        block.source_hir_block_id = src->id;
        if (src->statement_count > 0)
            source_ast = src->statements[0];
        else if (src->terminator_condition != NULL)
            source_ast = src->terminator_condition;
        else if (src->terminator_value != NULL)
            source_ast = src->terminator_value;
        mir_block_record_source_location(&block, source_ast);
        block.succ_true = src->succ_true;
        block.succ_false = src->succ_false;
        block.has_succ_true = src->has_succ_true;
        block.has_succ_false = src->has_succ_false;
        if (!copy_indices(&block.predecessors,
                          &block.predecessor_count,
                          src->predecessors,
                          src->predecessor_count)) {
            free(block.predecessors);
            return false;
        }
        block.predecessor_capacity = block.predecessor_count;
        if (!mir_copy_ast_nodes(&block.source_statement_inventory.items,
                                &block.source_statement_inventory.count,
                                src->statements,
                                src->statement_count)
            || !mir_copy_names(&block.source_local_defs,
                               &block.source_local_def_count,
                               src->local_defs,
                               src->local_def_count)
            || !copy_indices(&block.source_dom_tree_children,
                             &block.source_dom_tree_child_count,
                             src->dom_tree_children,
                             src->dom_tree_child_count)
            || !mir_copy_phi_nodes(&block.source_phi_nodes,
                                   &block.source_phi_node_count,
                                   src->phi_nodes,
                                   src->phi_node_count)) {
            free(block.predecessors);
            free(block.source_statement_inventory.items);
            free((void *)block.source_local_defs);
            free(block.source_dom_tree_children);
            if (block.source_phi_nodes != NULL) {
                for (size_t j = 0; j < block.source_phi_node_count; j++)
                    free(block.source_phi_nodes[j].incoming_predecessors);
            }
            free(block.source_phi_nodes);
            return false;
        }
        if (!append_block(routine, block))
            return false;
    }

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        if (!mir_add_phi_placeholders(routine, &routine->blocks[i]))
            return false;
        if (!mir_add_terminator_instruction(routine,
                                            &routine->blocks[i],
                                            src->terminator_kind,
                                            src->terminator_condition,
                                            src->terminator_value))
            return false;
    }

    return true;
}

#include "mir_decl_headers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_abi_layout.h"

MIRProgram *
mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message)
{
    const char *debug_mir_lower;
    MIRProgram *mir;
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR lowering requires HIR");
        return NULL;
    }

    debug_mir_lower = getenv("PGY_DEBUG_MIR_LOWER");

    mir_abi_table_init();

    mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }

#define MIR_COPY_AST_LIST(field, count_field) \
    do { \
        mir->count_field = hir->count_field; \
        if (hir->count_field > 0) { \
            mir->field = calloc(hir->count_field, sizeof(ASTNode *)); \
            if (mir->field == NULL) { \
                if (error_message != NULL) \
                    *error_message = pergyra_strdup("out of memory"); \
                mir_destroy(mir); \
                return NULL; \
            } \
            memcpy(mir->field, hir->field, hir->count_field * sizeof(ASTNode *)); \
        } \
    } while (0)

    MIR_COPY_AST_LIST(externs, extern_count);
    MIR_COPY_AST_LIST(types, type_count);
    MIR_COPY_AST_LIST(abilities, ability_count);
    MIR_COPY_AST_LIST(roles, role_count);
    MIR_COPY_AST_LIST(parties, party_count);
    MIR_COPY_AST_LIST(rosters, roster_count);
    MIR_COPY_AST_LIST(worlds, world_count);
    MIR_COPY_AST_LIST(relations, relation_count);
    MIR_COPY_AST_LIST(effects, effect_count);
    MIR_COPY_AST_LIST(zones, zone_count);
    MIR_COPY_AST_LIST(events, event_count);
    MIR_COPY_AST_LIST(intents, intent_count);
    MIR_COPY_AST_LIST(functions, function_count);
    mir->has_top_level_exec = false;
    mir->has_main_function = false;
    mir->main_function_name = NULL;
    for (size_t i = 0; i < mir->function_count; i++) {
        ASTNode *fn = mir->functions[i];
        const char *fn_name = ast_declaration_name(fn);
        if (fn == NULL || fn->type != AST_FUNC_DECL
            || fn_name == NULL) {
            continue;
        }
        if (strcmp(fn_name, "__pgy_top_level_exec") == 0)
            mir->has_top_level_exec = true;
        if (strcmp(fn_name, "Main") == 0) {
            mir->has_main_function = true;
            mir->main_function_name = fn_name;
        } else if (strcmp(fn_name, "main") == 0) {
            mir->has_main_function = true;
            if (mir->main_function_name == NULL)
                mir->main_function_name = fn_name;
        }
    }
    mir_program_record_inventory_surface_usage(mir);

#undef MIR_COPY_AST_LIST

    for (size_t i = 0; i < hir->type_count; i++) {
        if (!mir_record_decl_header(mir, hir->types[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->party_count; i++) {
        if (!mir_record_decl_header(mir, hir->parties[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->role_count; i++) {
        if (!mir_record_decl_header(mir, hir->roles[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->roster_count; i++) {
        if (!mir_record_decl_header(mir, hir->rosters[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->world_count; i++) {
        if (!mir_record_decl_header(mir, hir->worlds[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->relation_count; i++) {
        if (!mir_record_decl_header(mir, hir->relations[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->effect_count; i++) {
        if (!mir_record_decl_header(mir, hir->effects[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->zone_count; i++) {
        if (!mir_record_decl_header(mir, hir->zones[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }

    HIRRoutineInventory hir_inventory;
    hir_routine_inventory_from_program(hir, &hir_inventory);
    for (size_t i = 0; i < hir_inventory.count; i++) {
        const HIRRoutine *hir_routine =
            hir_routine_inventory_get(&hir_inventory, i);
        MIRRoutine routine;
        const HIRBasicBlock *cfg_blocks_before = NULL;
        size_t cfg_block_count_before = 0;
        if (hir_routine == NULL) {
            if (error_message != NULL)
                *error_message =
                    pergyra_strdup("invalid HIR routine inventory");
            mir_destroy(mir);
            return NULL;
        }
        memset(&routine, 0, sizeof(routine));
        pgy_arena_init(&routine.scratch, 0);
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.name = hir_routine->name;
        routine.ast = hir_routine->ast;
        routine.is_action_like = hir_routine->is_action_like;
        routine.hir_routine = hir_routine;
        routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);
        routine.owner_name = routine.rir_scope != NULL
            ? routine.rir_scope->owner_name
            : hir_routine->owner_name;
        routine.owner_ast_type = hir_routine->owner_ast_type;
        cfg_blocks_before =
            hir_routine->has_cfg ? hir_routine->cfg.blocks : NULL;
        cfg_block_count_before =
            hir_routine->has_cfg ? hir_routine->cfg.block_count : 0;

        if (!mir_build_blocks_from_hir(&routine, hir_routine)
            || !mir_append_cleanup_block(&routine, routine.rir_scope)
            || !mir_populate_instructions(&routine)
            || !mir_apply_ssa_rename(&routine)
            || !mir_populate_stmt_instructions(&routine)
            || !mir_populate_use_edges(&routine)
            || !mir_materialize_cleanup_edges(&routine)
            || !mir_recompute_analysis(&routine)
            || !append_routine(mir, routine)) {
            pgy_arena_destroy(&routine.scratch);
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }

        if (hir_routine->has_cfg
            && (hir_routine->cfg.blocks != cfg_blocks_before
                || hir_routine->cfg.block_count != cfg_block_count_before)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "HIR CFG storage changed during MIR lowering for routine '%s' (before_count=%zu after_count=%zu)",
                    routine.name != NULL ? routine.name : "(anonymous)",
                    cfg_block_count_before,
                    hir_routine->cfg.block_count);
            }
            mir_destroy(mir);
            return NULL;
        }

        if (debug_mir_lower != NULL && debug_mir_lower[0] != '\0'
            && routine.kind == MIR_SCOPE_INTENT) {
            fprintf(stdout,
                "[MIR LOWER] Intent '%s' after build: has_cleanup=%d, blocks=%zu\n",
                routine.name ? routine.name : "(null)",
                routine.has_cleanup_block,
                routine.block_count);
            for (size_t b = 0; b < routine.block_count; b++) {
                fprintf(stdout,
                    "  block[%zu] has_cleanup_succ=%d has_rollback_succ=%d has_invalidation_succ=%d\n",
                    b,
                    routine.blocks[b].has_cleanup_succ,
                    routine.blocks[b].has_rollback_succ,
                    routine.blocks[b].has_invalidation_succ);
            }
        }
    }

    mir_link_decl_method_routines(mir);

    if (!mir_run_dce_pass(mir, error_message)) {
        mir_destroy(mir);
        return NULL;
    }
    mir_refresh_non_cfg_body_fallback_inventory(mir);

    return mir;
}

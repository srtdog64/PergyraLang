#include "hir_internal.h"

#include "hir_analysis.h"
#include "hir_cfg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/string_compat.h"

typedef struct {
    const ASTNode *node;
    uint32_t id;
} HIRExecutionStatement;

typedef struct {
    size_t id, statement_start, statement_count;
    HIRBlockTerminatorKind kind;
    const ASTNode *condition, *value;
    uint32_t condition_id, value_id;
    bool has_true, has_false;
    size_t true_target, false_target;
    uint32_t loop_id;
    size_t loop_exit;
} HIRExecutionBlock;

struct HIRCFGExecutionWitness {
    const ASTNode *routine, *body, *declaration;
    uint32_t routine_id, body_id, source_id, declaration_source_id;
    HIRTopLevelKind kind;
    ASTNodeType owner_type;
    bool hosted, action_like;
    size_t parameter_count;
    char *name, *owner_name;
    size_t declaration_id, entry, block_count, statement_count;
    bool has_cfg;
    const HIRBasicBlock *original_blocks;
    HIRExecutionBlock *blocks;
    HIRExecutionStatement *statements;
};

typedef struct {
    const ASTNode *ast, *body;
    uint32_t ast_id, body_id, routine_id, source_id;
    const HIRCFGExecutionWitness *layout;
} HIRProgramExecutionRoutine;

struct HIRProgramExecutionWitness {
    const HIRProgram *owner;
    const ASTNode *program;
    uint32_t program_id;
    size_t item_count, decl_count, function_count, intent_count, routine_count;
    HIRTopLevelItem *items;
    HIRDecl *decls;
    HIRExecutionStatement *item_sources, *decl_sources, *functions, *intents;
    HIRProgramExecutionRoutine *routines;
};

void hir_cfg_program_execution_release(HIRProgram *hir)
{
    if (hir == NULL || hir->execution_census == NULL) return;
    HIRProgramExecutionWitness *w = hir->execution_census;
    free(w->items); free(w->decls);
    free(w->item_sources); free(w->decl_sources);
    free(w->functions); free(w->intents); free(w->routines);
    free(w);
    hir->execution_census = NULL;
}

/* Called once after all inventories and fallible lowering consumers finish.
 * Only flat storage is owned here; AST and routine witnesses remain borrowed. */
bool hir_cfg_program_execution_seal(HIRProgram *hir, const ASTNode *program)
{
    if (hir == NULL || program == NULL || program->type != AST_PROGRAM ||
        hir->execution_census != NULL ||
        hir->source_program_syntax_id != ast_node_stable_id(program) ||
        hir->item_count > hir->item_capacity || hir->decl_count > hir->decl_capacity ||
        hir->routine_count > hir->routine_capacity ||
        hir->function_count > hir->function_capacity || hir->intent_count > hir->intent_capacity ||
        (hir->item_count != 0 && hir->items == NULL) ||
        (hir->decl_count != 0 && hir->decls == NULL) ||
        (hir->routine_count != 0 && hir->routines == NULL) ||
        (hir->function_count != 0 && hir->functions == NULL) ||
        (hir->intent_count != 0 && hir->intents == NULL) ||
        hir->item_count > SIZE_MAX / sizeof(HIRTopLevelItem) ||
        hir->item_count > SIZE_MAX / sizeof(HIRExecutionStatement) ||
        hir->decl_count > SIZE_MAX / sizeof(HIRDecl) ||
        hir->decl_count > SIZE_MAX / sizeof(HIRExecutionStatement) ||
        hir->function_count > SIZE_MAX / sizeof(HIRExecutionStatement) ||
        hir->intent_count > SIZE_MAX / sizeof(HIRExecutionStatement) ||
        hir->routine_count > SIZE_MAX / sizeof(HIRProgramExecutionRoutine)) return false;
    HIRProgramExecutionWitness *w = calloc(1, sizeof(*w));
    if (w == NULL) return false;
    w->items = hir->item_count == 0 ? NULL : calloc(hir->item_count, sizeof(*w->items));
    w->item_sources = hir->item_count == 0 ? NULL : calloc(hir->item_count, sizeof(*w->item_sources));
    w->decls = hir->decl_count == 0 ? NULL : calloc(hir->decl_count, sizeof(*w->decls));
    w->decl_sources = hir->decl_count == 0 ? NULL : calloc(hir->decl_count, sizeof(*w->decl_sources));
    w->functions = hir->function_count == 0 ? NULL : calloc(hir->function_count, sizeof(*w->functions));
    w->intents = hir->intent_count == 0 ? NULL : calloc(hir->intent_count, sizeof(*w->intents));
    w->routines = hir->routine_count == 0 ? NULL : calloc(hir->routine_count, sizeof(*w->routines));
    if ((hir->item_count != 0 && (w->items == NULL || w->item_sources == NULL)) ||
        (hir->decl_count != 0 && (w->decls == NULL || w->decl_sources == NULL)) ||
        (hir->function_count != 0 && w->functions == NULL) ||
        (hir->intent_count != 0 && w->intents == NULL) ||
        (hir->routine_count != 0 && w->routines == NULL)) goto failed;
    w->owner = hir; w->program = program; w->program_id = ast_node_stable_id(program);
    w->item_count = hir->item_count; w->decl_count = hir->decl_count;
    w->function_count = hir->function_count; w->intent_count = hir->intent_count;
    w->routine_count = hir->routine_count;
    for (size_t i = 0; i < w->item_count; i++) {
        if (hir->items[i].ast == NULL) goto failed;
        w->items[i] = hir->items[i];
        w->item_sources[i] = (HIRExecutionStatement){hir->items[i].ast, ast_node_stable_id(hir->items[i].ast)};
    }
    for (size_t i = 0; i < w->decl_count; i++) {
        if (hir->decls[i].ast == NULL) goto failed;
        w->decls[i] = hir->decls[i];
        w->decl_sources[i] = (HIRExecutionStatement){hir->decls[i].ast, ast_node_stable_id(hir->decls[i].ast)};
    }
    for (size_t i = 0; i < w->function_count; i++) {
        if (hir->functions[i] == NULL) goto failed;
        w->functions[i] = (HIRExecutionStatement){hir->functions[i], ast_node_stable_id(hir->functions[i])};
    }
    for (size_t i = 0; i < w->intent_count; i++) {
        if (hir->intents[i] == NULL) goto failed;
        w->intents[i] = (HIRExecutionStatement){hir->intents[i], ast_node_stable_id(hir->intents[i])};
    }
    for (size_t i = 0; i < w->routine_count; i++) {
        const HIRRoutine *r = &hir->routines[i];
        if (r->execution_layout == NULL) goto failed;
        w->routines[i] = (HIRProgramExecutionRoutine){r->ast, r->body,
            ast_node_stable_id(r->ast), ast_node_stable_id(r->body),
            r->routine_id, r->source_syntax_id, r->execution_layout};
    }
    hir->execution_census = w;
    return true;
failed:
    free(w->items); free(w->decls); free(w->item_sources); free(w->decl_sources);
    free(w->functions); free(w->intents); free(w->routines); free(w);
    return false;
}

bool hir_cfg_program_execution_current(const HIRProgram *hir, const ASTNode *expected_program)
{
    const HIRProgramExecutionWitness *w = hir == NULL ? NULL : hir->execution_census;
    if (w == NULL || w->owner != hir || w->program == NULL ||
        w->program->type != AST_PROGRAM || ast_node_stable_id(w->program) != w->program_id ||
        hir->source_program_syntax_id != w->program_id ||
        (expected_program != NULL && (w->program_id == 0 || expected_program != w->program ||
            ast_node_stable_id(expected_program) != w->program_id)) ||
        hir->item_count != w->item_count || hir->decl_count != w->decl_count ||
        hir->function_count != w->function_count || hir->intent_count != w->intent_count ||
        hir->routine_count != w->routine_count ||
        hir->item_count > hir->item_capacity || hir->decl_count > hir->decl_capacity ||
        hir->function_count > hir->function_capacity || hir->intent_count > hir->intent_capacity ||
        hir->routine_count > hir->routine_capacity ||
        (w->item_count != 0 && hir->items == NULL) ||
        (w->decl_count != 0 && hir->decls == NULL) ||
        (w->function_count != 0 && hir->functions == NULL) ||
        (w->intent_count != 0 && hir->intents == NULL) ||
        (w->routine_count != 0 && hir->routines == NULL)) return false;
    for (size_t i = 0; i < w->item_count; i++)
        if (hir->items[i].ast != w->item_sources[i].node ||
            ast_node_stable_id(hir->items[i].ast) != w->item_sources[i].id ||
            hir->items[i].kind != w->items[i].kind || hir->items[i].name != w->items[i].name) return false;
    for (size_t i = 0; i < w->decl_count; i++)
        if (hir->decls[i].ast != w->decl_sources[i].node ||
            ast_node_stable_id(hir->decls[i].ast) != w->decl_sources[i].id ||
            hir->decls[i].id != w->decls[i].id || hir->decls[i].kind != w->decls[i].kind ||
            hir->decls[i].phase != w->decls[i].phase || hir->decls[i].name != w->decls[i].name ||
            hir->decls[i].source_syntax_id != w->decls[i].source_syntax_id) return false;
    for (size_t i = 0; i < w->function_count; i++)
        if (hir->functions[i] != w->functions[i].node ||
            ast_node_stable_id(hir->functions[i]) != w->functions[i].id) return false;
    for (size_t i = 0; i < w->intent_count; i++)
        if (hir->intents[i] != w->intents[i].node ||
            ast_node_stable_id(hir->intents[i]) != w->intents[i].id) return false;
    for (size_t i = 0; i < w->routine_count; i++) {
        const HIRRoutine *r = &hir->routines[i];
        const HIRProgramExecutionRoutine *saved = &w->routines[i];
        if (r->ast != saved->ast || r->body != saved->body ||
            ast_node_stable_id(r->ast) != saved->ast_id || ast_node_stable_id(r->body) != saved->body_id ||
            r->routine_id != saved->routine_id || r->source_syntax_id != saved->source_id ||
            r->execution_layout != saved->layout ||
            (w->program_id != 0 && !hir_cfg_execution_current(r))) return false;
    }
    return true;
}

void hir_cfg_execution_release(HIRRoutine *routine)
{
    if (routine == NULL || routine->execution_layout == NULL) return;
    free(routine->execution_layout->blocks);
    free(routine->execution_layout->statements);
    free(routine->execution_layout->name);
    free(routine->execution_layout->owner_name);
    free(routine->execution_layout);
    routine->execution_layout = NULL;
}

/* The sole issuance point is the successful finish below. Queries never
 * rebuild a missing or changed layout from the current mutable CFG. */
static bool hir_cfg_execution_seal(HIRRoutine *routine)
{
    if (routine == NULL || routine->execution_layout != NULL ||
        routine->cfg.block_count > SIZE_MAX / sizeof(HIRExecutionBlock) ||
        (routine->cfg.block_count != 0 && routine->cfg.blocks == NULL)) return false;
    size_t statements = 0;
    for (size_t b = 0; b < routine->cfg.block_count; b++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[b];
        if (block->id != b || block->statement_count > SIZE_MAX - statements ||
            (block->statement_count != 0 && block->statements == NULL)) return false;
        statements += block->statement_count;
    }
    if (statements > SIZE_MAX / sizeof(HIRExecutionStatement)) return false;
    HIRCFGExecutionWitness *w = calloc(1, sizeof(*w));
    if (w == NULL) return false;
    w->blocks = routine->cfg.block_count == 0 ? NULL : calloc(routine->cfg.block_count, sizeof(*w->blocks));
    w->statements = statements == 0 ? NULL : calloc(statements, sizeof(*w->statements));
    w->name = routine->name == NULL ? NULL : pergyra_strdup(routine->name);
    w->owner_name = routine->owner_name == NULL ? NULL : pergyra_strdup(routine->owner_name);
    if ((routine->cfg.block_count != 0 && w->blocks == NULL) || (statements != 0 && w->statements == NULL) ||
        (routine->name != NULL && w->name == NULL) || (routine->owner_name != NULL && w->owner_name == NULL)) {
        free(w->blocks); free(w->statements); free(w->name); free(w->owner_name); free(w);
        return false;
    }
    w->routine = routine->ast; w->body = routine->body;
    w->routine_id = ast_node_stable_id(routine->ast); w->body_id = ast_node_stable_id(routine->body);
    w->source_id = routine->source_syntax_id; w->declaration_id = routine->decl_id;
    w->declaration = routine->declaration_ast;
    w->declaration_source_id = ast_node_stable_id(routine->declaration_ast);
    w->kind = routine->kind; w->owner_type = routine->owner_ast_type;
    w->hosted = routine->is_hosted; w->action_like = routine->is_action_like;
    w->parameter_count = routine->parameter_count;
    w->has_cfg = routine->has_cfg; w->entry = routine->cfg.entry_block;
    w->block_count = routine->cfg.block_count; w->statement_count = statements;
    w->original_blocks = routine->cfg.blocks;
    size_t at = 0;
    for (size_t b = 0; b < w->block_count; b++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[b];
        w->blocks[b] = (HIRExecutionBlock){block->id, at, block->statement_count,
            block->terminator_kind, block->terminator_condition, block->terminator_value,
            ast_node_stable_id(block->terminator_condition), ast_node_stable_id(block->terminator_value),
            block->has_succ_true, block->has_succ_false, block->succ_true, block->succ_false,
            block->source_loop_syntax_id, block->source_loop_exit};
        for (size_t s = 0; s < block->statement_count; s++) {
            w->statements[at++] = (HIRExecutionStatement){block->statements[s], ast_node_stable_id(block->statements[s])};
        }
    }
    routine->execution_layout = w;
    return true;
}

bool hir_cfg_execution_owner_current(const HIRRoutine *routine)
{
    const HIRCFGExecutionWitness *w = routine == NULL ? NULL : routine->execution_layout;
    if (w == NULL || w->routine != routine->ast || w->body != routine->body ||
        w->routine_id != ast_node_stable_id(routine->ast) || w->body_id != ast_node_stable_id(routine->body) ||
        w->source_id != w->routine_id ||
        (routine->ast != NULL && routine->ast->type == AST_FUNC_DECL && ast_func_body(routine->ast) != routine->body) ||
        w->source_id != routine->source_syntax_id || w->declaration_id != routine->decl_id ||
        w->declaration == NULL || w->declaration != routine->declaration_ast || w->declaration_source_id == 0 ||
        w->declaration_source_id != ast_node_stable_id(routine->declaration_ast) ||
        w->kind != routine->kind || w->owner_type != routine->owner_ast_type ||
        w->hosted != routine->is_hosted || w->action_like != routine->is_action_like ||
        w->parameter_count != routine->parameter_count ||
        (w->name == NULL) != (routine->name == NULL) ||
        (w->name != NULL && strcmp(w->name, routine->name) != 0) ||
        (w->owner_name == NULL) != (routine->owner_name == NULL) ||
        (w->owner_name != NULL && strcmp(w->owner_name, routine->owner_name) != 0)) return false;
    return true;
}

bool hir_cfg_execution_current(const HIRRoutine *routine)
{
    if (!hir_cfg_execution_owner_current(routine)) return false;
    const HIRCFGExecutionWitness *w = routine->execution_layout;
    if (w->has_cfg != routine->has_cfg || w->entry != routine->cfg.entry_block ||
        w->block_count != routine->cfg.block_count ||
        routine->cfg.block_count > routine->cfg.block_capacity ||
        w->original_blocks != routine->cfg.blocks ||
        (w->block_count != 0 && (w->blocks == NULL || routine->cfg.blocks == NULL)) ||
        (w->statement_count != 0 && w->statements == NULL)) return false;
    size_t at = 0;
    for (size_t b = 0; b < w->block_count; b++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[b];
        const HIRExecutionBlock *saved = &w->blocks[b];
        if (at > w->statement_count || saved->id != b || block->id != saved->id || saved->statement_start != at ||
            saved->statement_count != block->statement_count || block->statement_count > w->statement_count - at ||
            block->statement_count > block->statement_capacity ||
            (block->statement_count != 0 && block->statements == NULL) ||
            saved->kind != block->terminator_kind || saved->condition != block->terminator_condition ||
            saved->value != block->terminator_value || saved->condition_id != ast_node_stable_id(block->terminator_condition) ||
            saved->value_id != ast_node_stable_id(block->terminator_value) || saved->has_true != block->has_succ_true ||
            saved->has_false != block->has_succ_false || saved->true_target != block->succ_true ||
            saved->false_target != block->succ_false ||
            saved->loop_id != block->source_loop_syntax_id ||
            saved->loop_exit != block->source_loop_exit) return false;
        for (size_t s = 0; s < block->statement_count; s++, at++)
            if (w->statements[at].node != block->statements[s] ||
                w->statements[at].id != ast_node_stable_id(block->statements[s])) return false;
    }
    return at == w->statement_count;
}

static bool
hir_cfg_successor_in_range(const HIRRoutine *routine, size_t successor)
{
    return routine != NULL && successor < routine->cfg.block_count;
}

static bool
hir_cfg_block_targets(const HIRBasicBlock *block, size_t target)
{
    if (block == NULL)
        return false;
    return (block->has_succ_true && block->succ_true == target)
        || (block->has_succ_false && block->succ_false == target);
}

static bool
hir_cfg_predecessors_contain(const HIRBasicBlock *block, size_t predecessor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] == predecessor)
            return true;
    }
    return false;
}

static bool
hir_validate_cfg_shape(const HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg)
        return true;
    if (routine->cfg.blocks == NULL || routine->cfg.block_count == 0)
        return false;
    if (routine->cfg.entry_block >= routine->cfg.block_count)
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block->id != i)
            return false;

        switch (block->terminator_kind) {
        case HIR_BLOCK_FALLTHROUGH:
            return false;
        case HIR_BLOCK_GOTO:
            if (!block->has_succ_true || block->has_succ_false)
                return false;
            if (!hir_cfg_successor_in_range(routine, block->succ_true))
                return false;
            break;
        case HIR_BLOCK_BRANCH:
            if (block->terminator_condition == NULL)
                return false;
            if (!block->has_succ_true || !block->has_succ_false)
                return false;
            if (!hir_cfg_successor_in_range(routine, block->succ_true)
                || !hir_cfg_successor_in_range(routine, block->succ_false))
                return false;
            break;
        case HIR_BLOCK_RETURN:
        case HIR_BLOCK_UNREACHABLE:
            if (block->has_succ_true || block->has_succ_false)
                return false;
            break;
        default:
            return false;
        }
    }

    return true;
}

static bool
hir_validate_cfg_predecessors(const HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg)
        return true;
    if (!hir_validate_cfg_shape(routine))
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        for (size_t p = 0; p < block->predecessor_count; p++) {
            size_t predecessor = block->predecessors[p];
            if (predecessor >= routine->cfg.block_count)
                return false;
            if (!hir_cfg_block_targets(&routine->cfg.blocks[predecessor], i))
                return false;
        }

        if (block->has_succ_true
            && !hir_cfg_predecessors_contain(&routine->cfg.blocks[block->succ_true], i))
            return false;
        if (block->has_succ_false
            && !hir_cfg_predecessors_contain(&routine->cfg.blocks[block->succ_false], i))
            return false;
    }

    return true;
}

bool
hir_finish_cfg_routine(HIRRoutine *routine)
{
    if (!hir_validate_cfg_shape(routine))
        return false;
    if (!hir_finalize_cfg(routine))
        return false;
    if (!hir_validate_cfg_predecessors(routine))
        return false;
    if (!hir_compute_cfg_dominance(routine))
        return false;
    if (!hir_compute_cfg_dominance_frontier(routine))
        return false;
    if (!hir_compute_cfg_dom_tree(routine))
        return false;
    if (!hir_compute_cfg_loops(routine))
        return false;
    if (!hir_collect_cfg_local_defs(routine))
        return false;
    if (!hir_compute_cfg_phi_candidates(routine))
        return false;
    if (!hir_materialize_phi_nodes(routine))
        return false;
    hir_finalize_cfg_summary(routine);
    return hir_cfg_execution_seal(routine);
}

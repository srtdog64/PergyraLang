#include "mir_loop_collection_cleanup.h"

#include "hir_cfg.h"
#include "mir_base_helpers.h"
#include "mir_branch_source_facts.h"
#include "mir_call_fact.h"
#include "mir_json_expression_graph_materialize.h"
#include "mir_source_emit_predicates.h"
#include "mir_program_fact_validate.h"
#include "../semantic/slot_summary.h"
#include "../common/pgy_builtin_type_table.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/region_retention_summary.h"
#include "../semantic/builtin_kind.h"

#include <stdlib.h>
#include <string.h>

#define LOOP_RETIRE "local-collection-retire"

typedef struct { const char *name; size_t argument, arity; } BorrowRow;
#define PGY_RETENTION_ARGUMENT_ANY SIZE_MAX
#define PGY_BUILTIN_ARGUMENT_RETENTION(identity, name, argument, retention) { name, argument, SIZE_MAX },
#define PGY_STDLIB_ARGUMENT_RETENTION(identity, name, argument, retention, arity) { name, argument, arity },
static const BorrowRow borrow_rows[] = {
#include "../semantic/builtin_argument_retention_registry.def"
};
#undef PGY_STDLIB_ARGUMENT_RETENTION
#undef PGY_BUILTIN_ARGUMENT_RETENTION
#undef PGY_RETENTION_ARGUMENT_ANY

/* The retention row alone says nothing about a returned view. Require the
 * existing type owner to prove a scalar/Void result as well. */
static bool scalar_type(const char *type)
{
    return type != NULL && (strcmp(type, "Void") == 0 || strcmp(type, "Int") == 0 ||
        strcmp(type, "Long") == 0 || strcmp(type, "Bool") == 0 ||
        strcmp(type, "Float") == 0 || strcmp(type, "Double") == 0);
}

static bool declared_argument(const MIRRoutine *caller, uint32_t identity, size_t ordinal, size_t arity)
{
    const MIRProgram *program = caller->program;
    const MIRRoutine *callee = NULL;
    if (program == NULL || program->routine_count > program->routine_capacity ||
        (program->routine_count > 0 && program->routines == NULL)) return false;
    for (size_t r = 0; r < program->routine_count; r++)
        if (program->routines[r].source_syntax_id == identity) {
            if (callee != NULL) return false;
            callee = &program->routines[r];
        }
    if (callee == NULL || callee->hir_routine == NULL || callee->kind != MIR_SCOPE_FUNCTION ||
        callee->is_action_like || callee->owner_name != NULL || !callee->has_signature ||
        callee->generic_param_count != 0 || callee->param_count != arity || ordinal >= arity ||
        callee->param_type_names == NULL || callee->param_type_names[ordinal] == NULL ||
        strcmp(callee->param_type_names[ordinal], "Array<String>") != 0 || callee->param_abi_facts == NULL ||
        !scalar_type(callee->return_type_name) || callee->return_callable_sig.is_callable ||
        !mir_validate_function_param_flow_summaries(callee, NULL)) return false;
    const MIRParamAbiFact *abi = &callee->param_abi_facts[ordinal];
    if ((abi->carriage != MIR_PARAM_CARRIAGE_VALUE && abi->carriage != MIR_PARAM_CARRIAGE_READONLY_REF) ||
        abi->resource_kind != MIR_PARAM_RESOURCE_NONE) return false;
    for (size_t p = 0; p < callee->function_param_flow_summary_count; p++) {
        const MIRFunctionParamFlowSummary *row = &callee->function_param_flow_summaries[p];
        if (row->parameter_index == ordinal)
            /* Own/async formals have no preservation grant from the producer.
             * An inout formal is excluded by its ABI carriage above. */
            return row->array_preserved == PGY_FUNCTION_PARAM_ARRAY_PRESERVED_ALL &&
                (row->mask & ~(unsigned)SLOT_PARAM_SUMMARY_READ) == 0;
    }
    return false;
}

static bool borrowed_argument(const MIRRoutine *r, const MIRJsonExpressionGraph *g, size_t value)
{
    for (size_t p = value + 1; p < g->count; p++) {
        const MIRJsonExpressionGraphNode *arg = &g->nodes[p];
        if (strcmp(arg->kind, "call_argument") != 0 || arg->right != (int)value) continue;
        int call = arg->left;
        size_t ordinal = 0;
        while (call >= 0 && (size_t)call < g->count &&
               strcmp(g->nodes[call].kind, "call_argument") == 0) {
            ordinal++;
            call = g->nodes[call].left;
        }
        if (call < 0 || (size_t)call >= g->count) return false;
        const MIRJsonExpressionGraphNode *node = &g->nodes[call];
        if (strcmp(node->kind, "call") != 0 || strcmp(node->call_target_kind, "direct") != 0 ||
            node->left < 0 || (size_t)node->left >= g->count) return false;
        size_t arity = ordinal + 1;
        for (size_t q = p + 1; q < g->count; q++)
            if (strcmp(g->nodes[q].kind, "call_argument") == 0 && g->nodes[q].left == (int)p) {
                arity++;
                p = q;
            }
        if (node->call_target_syntax_id != 0)
            return declared_argument(r, node->call_target_syntax_id, ordinal, arity);
        BuiltinKind builtin = builtin_resolve(node->call_target_name);
        if (g->nodes[node->left].binding_syntax_id != 0 ||
            g->nodes[node->left].source_binding_syntax_id != 0 ||
            (!scalar_type(pgy_builtin_simple_return_type(node->call_target_name)) &&
             builtin != BUILTIN_ARRAY_PUSH_OWNED_STRING &&
             builtin != BUILTIN_CONCAT)) return false;
        for (size_t row = 0; row < sizeof(borrow_rows) / sizeof(borrow_rows[0]); row++)
            if (strcmp(borrow_rows[row].name, node->call_target_name) == 0 &&
                (borrow_rows[row].argument == SIZE_MAX || borrow_rows[row].argument == ordinal) &&
                (borrow_rows[row].arity == SIZE_MAX || borrow_rows[row].arity == arity)) return true;
        return false;
    }
    return false;
}

/* Array<String> indexing materializes a temporary String descriptor.  It is
 * still a synchronous read when that exact descriptor flows directly into an
 * argument whose retention contract ends with the call.  Any stored, returned,
 * deferred, multiply-used, or otherwise transformed indexed value remains
 * outside this permission. */
static bool borrowed_indexed_argument(const MIRRoutine *r,
    const MIRJsonExpressionGraph *g, size_t value)
{
    size_t index_node = SIZE_MAX;
    size_t matches = 0;

    for (size_t p = value + 1; p < g->count; p++) {
        const MIRJsonExpressionGraphNode *node = &g->nodes[p];
        if (strcmp(node->kind, "index") != 0 || node->left != (int)value)
            continue;
        index_node = p;
        matches++;
    }
    return matches == 1 && borrowed_argument(r, g, index_node);
}

static bool dominates(const HIRRoutine *hir, size_t dom, size_t block)
{
    for (size_t fuel = 0; fuel < hir->cfg.block_count; fuel++) {
        if (block == dom) return true;
        if (block >= hir->cfg.block_count || !hir->cfg.blocks[block].has_immediate_dominator) return false;
        size_t next = hir->cfg.blocks[block].immediate_dominator;
        if (next == block) return false;
        block = next;
    }
    return false;
}

static bool private_uses(const MIRRoutine *r, uint32_t binding, const char *value, const bool *region,
    uint32_t *kind)
{
    bool owned = false, borrowed = false;
    for (size_t b = 0; b < r->block_count; b++) {
        const MIRBasicBlock *block = &r->blocks[b];
        if (!block->is_reachable) continue;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (inst->kind == MIR_INST_CLEANUP_EDGE) continue;
            if (inst->has_collection_ownership_receipt &&
                inst->collection_ownership_receiver_binding_id == binding) {
                if (inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_DROP) return false;
                owned |= inst->collection_ownership_effect_kind ==
                        PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                    || inst->collection_ownership_effect_kind ==
                        PGY_COLLECTION_EFFECT_OWNED_STRING_SET;
                borrowed |= inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION;
            }
            ASTNode *roots[2];
            if (!mir_instruction_evaluation_roots(r, inst, roots)) return false;
            bool source_use = false;
            for (size_t e = 0; e < 2; e++) {
                if (roots[e] == NULL) continue;
                MIRJsonExpressionGraph graph = {0};
                bool valid = mir_json_expression_graph_build_for_routine(&graph, roots[e], r) >= 0;
                for (size_t n = 0; valid && n < graph.count; n++)
                    if (strcmp(graph.nodes[n].kind, "leaf") == 0 &&
                        graph.nodes[n].source_binding_syntax_id == binding) {
                        source_use = true;
                        if (!region[b] || mir_instruction_source_is_defer_stmt(inst) ||
                            (!borrowed_argument(r, &graph, n) &&
                             !borrowed_indexed_argument(r, &graph, n))) {
                            valid = false;
                        }
                    }
                mir_json_expression_graph_dispose(&graph);
                if (!valid) return false;
            }
            for (size_t u = 0; u < inst->use_count; u++)
                if (inst->uses[u] != NULL && strcmp(inst->uses[u], value) == 0 && !source_use)
                    return false;
        }
    }
    if (owned && borrowed) return false;
    *kind = owned ? 2u : 1u;
    return true;
}

static bool cleanup_matches(const MIRInstruction *inst, uint32_t binding,
    const char *value, uint32_t kind, size_t target)
{
    return inst->kind == MIR_INST_CLEANUP_EDGE && inst->name != NULL &&
        strcmp(inst->name, LOOP_RETIRE) == 0 && inst->binding_syntax_id == binding &&
        inst->collection_cleanup_kind == kind && inst->collection_cleanup_target == target &&
        inst->arg0 != NULL && strcmp(inst->arg0, value) == 0 &&
        inst->use_count == 1 && inst->uses != NULL && inst->uses[0] != NULL &&
        strcmp(inst->uses[0], value) == 0 &&
        inst->ast == NULL && inst->expr0 == NULL && inst->expr1 == NULL &&
        !inst->has_collection_ownership_receipt;
}

static bool issue_or_check(MIRRoutine *r, size_t b, uint32_t binding,
    const char *value, uint32_t kind, size_t target, bool issue, size_t *expected)
{
    MIRBasicBlock *block = &r->blocks[b];
    (*expected)++;
    if (!issue) {
        size_t found = 0;
        for (size_t i = 0; i < block->instruction_count; i++)
            found += cleanup_matches(&block->instructions[i], binding, value, kind, target);
        return found == 1;
    }
    MIRInstruction inst = {0};
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = LOOP_RETIRE;
    inst.binding_syntax_id = binding;
    inst.arg0 = value;
    inst.collection_cleanup_kind = kind;
    inst.collection_cleanup_target = target;
    char *use = pergyra_strdup(value);
    if (use == NULL || !append_owned_name(&inst.uses, &inst.use_count, &inst.use_capacity, use)) return false;
    if (mir_commit_instruction(r, block, &inst)) {
        r->cleanup_instruction_count++;
        return true;
    }
    free((void *)inst.uses[0]);
    free((void *)inst.uses);
    return false;
}

static bool loop_cleanup(MIRRoutine *r, bool issue, char **error)
{
    size_t expected = 0, actual = 0;
    const HIRRoutine *hir = r->hir_routine;
    if (r->block_count > r->block_capacity || (r->block_count > 0 && r->blocks == NULL) ||
        r->collection_ownership_fact_count > r->collection_ownership_fact_capacity ||
        (r->collection_ownership_fact_count > 0 && r->collection_ownership_facts == NULL)) goto invalid;
    for (size_t b = 0; b < r->block_count; b++) {
        if (r->blocks[b].instruction_count > r->blocks[b].instruction_capacity ||
            (r->blocks[b].instruction_count > 0 && r->blocks[b].instructions == NULL)) goto invalid;
        for (size_t i = 0; i < r->blocks[b].instruction_count; i++) {
            const MIRInstruction *inst = &r->blocks[b].instructions[i];
            if (inst->use_count > inst->use_capacity ||
                (inst->use_count > 0 && inst->uses == NULL)) goto invalid;
            actual += inst->collection_cleanup_kind != 0;
        }
    }
    if (issue && actual != 0) goto invalid;
    bool candidates = false;
    for (size_t f = 0; f < r->collection_ownership_fact_count; f++)
        candidates |= r->collection_ownership_facts[f].origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL;
    if (!candidates) {
        if (actual != 0) goto invalid;
        return true;
    }
    if (hir == NULL || !hir->has_cfg) {
        if (actual != 0) goto invalid;
        return true;
    }
    if (hir->cfg.block_count > r->block_count || !hir_cfg_execution_current(hir)) goto invalid;
    for (size_t f = 0; f < r->collection_ownership_fact_count; f++) {
        const MIRCollectionOwnershipFact *fact = &r->collection_ownership_facts[f];
        if (fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL) continue;
        size_t birth = SIZE_MAX, definitions = 0;
        const char *value = NULL;
        for (size_t b = 0; b < r->block_count; b++)
            for (size_t i = 0; i < r->blocks[b].instruction_count; i++) {
                const MIRInstruction *inst = &r->blocks[b].instructions[i];
                if (inst->kind != MIR_INST_DEF || inst->binding_syntax_id != fact->binding_syntax_id) continue;
                definitions++;
                birth = b;
                value = inst->result_name;
            }
        if (definitions != 1 || value == NULL || birth >= hir->cfg.block_count ||
            !r->blocks[birth].is_reachable) continue;
        size_t header = SIZE_MAX;
        for (size_t h = 0; h < hir->cfg.block_count; h++)
            if (h != birth && hir->cfg.blocks[h].source_loop_syntax_id != 0 &&
                hir->cfg.blocks[h].source_loop_exit < hir->cfg.block_count &&
                !dominates(hir, hir->cfg.blocks[h].source_loop_exit, birth) &&
                dominates(hir, h, birth) && (header == SIZE_MAX || dominates(hir, header, h))) header = h;
        size_t exit = header == SIZE_MAX
            ? SIZE_MAX : hir->cfg.blocks[header].source_loop_exit;
        if (header != SIZE_MAX && exit >= hir->cfg.block_count) goto invalid;
        bool *region = calloc(r->block_count, sizeof(*region));
        size_t *queue = calloc(r->block_count, sizeof(*queue));
        if (region == NULL || queue == NULL) { free(region); free(queue); goto invalid; }
        size_t count = 0;
        if (header == SIZE_MAX) {
            /* A fresh local outside a loop owns one function-scope lifetime.
             * Its region is every reachable block dominated by its exact
             * birth.  A use after a non-dominated join is rejected by
             * private_uses rather than being cleaned on only one path. */
            for (size_t b = 0; b < r->block_count; b++)
                if (r->blocks[b].is_reachable && dominates(hir, birth, b)) {
                    region[b] = true;
                    queue[count++] = b;
                }
        } else {
            /* A birth inside a loop owns one iteration.  Do not carry it
             * across the backedge or the loop exit. */
            queue[count++] = birth;
            region[birth] = true;
            for (size_t q = 0; q < count; q++) {
                const MIRBasicBlock *block = &r->blocks[queue[q]];
                size_t targets[2] = {block->has_succ_true ? block->succ_true : SIZE_MAX,
                                     block->has_succ_false ? block->succ_false : SIZE_MAX};
                for (size_t s = 0; s < 2; s++) {
                    size_t target = targets[s];
                    if (target >= hir->cfg.block_count || target == header || target == exit ||
                        region[target] || !r->blocks[target].is_reachable ||
                        !dominates(hir, birth, target)) continue;
                    region[target] = true;
                    queue[count++] = target;
                }
            }
        }
        uint32_t kind = 0;
        bool valid = true;
        if (private_uses(r, fact->binding_syntax_id, value, region, &kind))
            for (size_t q = 0; valid && q < count; q++) {
                size_t b = queue[q];
                MIRBasicBlock *block = &r->blocks[b];
                size_t targets[2] = {block->has_succ_true ? block->succ_true : SIZE_MAX,
                                     block->has_succ_false ? block->succ_false : SIZE_MAX};
                if (!block->has_succ_true && !block->has_succ_false) {
                    /* Abort/unreachable is not a continuing cleanup exit. */
                    for (size_t i = 0; i < block->instruction_count; i++)
                        if (block->instructions[i].kind == MIR_INST_RETURN)
                            valid = issue_or_check(r, b, fact->binding_syntax_id, value,
                                kind, SIZE_MAX, issue, &expected);
                } else for (size_t s = 0; valid && s < 2; s++)
                    if (targets[s] < r->block_count && !region[targets[s]] &&
                        (s == 0 || targets[s] != targets[0]))
                        valid = issue_or_check(r, b, fact->binding_syntax_id, value,
                            kind, targets[s], issue, &expected);
            }
        free(region);
        free(queue);
        if (!valid) goto invalid;
    }
    if (!issue && expected != actual) goto invalid;
    return true;
invalid:
    if (error != NULL && *error == NULL)
        *error = pergyra_strdup("MIR loop-local collection cleanup is missing, stale, or inconsistent");
    return false;
}

bool mir_synthesize_loop_collection_cleanup(MIRRoutine *r, char **error)
{
    return r != NULL && loop_cleanup(r, true, error);
}

bool mir_validate_loop_collection_cleanup(const MIRRoutine *r, char **error)
{
    return r != NULL && loop_cleanup((MIRRoutine *)r, false, error);
}

bool mir_loop_collection_cleanup_edge(const MIRBasicBlock *block, uint32_t binding, size_t target)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->collection_cleanup_kind != 0 && inst->binding_syntax_id == binding &&
            inst->collection_cleanup_target == target) return true;
    }
    return false;
}

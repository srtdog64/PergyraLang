#include "mir_loop_collection_cleanup.h"

#include "hir_cfg.h"
#include "mir_base_helpers.h"
#include "mir_abi_layout.h"
#include "mir_branch_source_facts.h"
#include "mir_call_fact.h"
#include "mir_json_expression_graph_materialize.h"
#include "mir_source_emit_predicates.h"
#include "mir_program_fact_validate.h"
#include "../semantic/slot_summary.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

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

static bool declared_cleanup_argument(const MIRRoutine *caller,
    uint32_t identity, size_t ordinal, size_t arity,
    bool require_string_array, bool allow_value_result)
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
        (require_string_array &&
            strcmp(callee->param_type_names[ordinal], "Array<String>") != 0) ||
        callee->param_abi_facts == NULL ||
        callee->return_type_name == NULL || callee->return_callable_sig.is_callable ||
        !mir_validate_function_param_flow_summaries(callee, NULL)) return false;
    const MIRParamAbiFact *abi = &callee->param_abi_facts[ordinal];
    bool restored_value_result = allow_value_result && require_string_array
        && abi->carriage == MIR_PARAM_CARRIAGE_VALUE_RESULT;
    if ((abi->carriage != MIR_PARAM_CARRIAGE_VALUE
            && abi->carriage != MIR_PARAM_CARRIAGE_READONLY_REF
            && !restored_value_result) ||
        abi->resource_kind != MIR_PARAM_RESOURCE_NONE) return false;
    for (size_t p = 0; p < callee->function_param_flow_summary_count; p++) {
        const MIRFunctionParamFlowSummary *row = &callee->function_param_flow_summaries[p];
        if (row->parameter_index == ordinal) {
            /* Own/async formals have no preservation grant from the producer.
             * A ref formal may hand the borrow through further synchronous
             * ref calls; its ABI still forbids mutation or retention beyond
             * this call. An exact value-result Array formal may read or write
             * only when the summary proves that the descriptor and all
             * elements return to this caller and publishes no outward effect.
             * The loop-local cleanup then keeps the sole retirement
             * responsibility after the call. */
            if (restored_value_result)
                return row->array_preserved
                        == PGY_FUNCTION_PARAM_ARRAY_PRESERVED_ALL
                    && row->mask == SLOT_PARAM_SUMMARY_NONE;
            unsigned allowed = SLOT_PARAM_SUMMARY_READ;
            if (abi->carriage == MIR_PARAM_CARRIAGE_READONLY_REF)
                allowed |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
            return (!require_string_array ||
                    row->array_preserved ==
                        PGY_FUNCTION_PARAM_ARRAY_PRESERVED_ALL) &&
                (row->mask & ~allowed) == 0;
        }
    }
    return false;
}

static bool declared_argument(const MIRRoutine *caller, uint32_t identity,
    size_t ordinal, size_t arity)
{
    return declared_cleanup_argument(
        caller, identity, ordinal, arity, true, true);
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
            return declared_argument(
                r, node->call_target_syntax_id, ordinal, arity);
        if (g->nodes[node->left].binding_syntax_id != 0 ||
            g->nodes[node->left].source_binding_syntax_id != 0) return false;
        /* The exact name/ordinal/arity row is the retention authority for a
         * stdlib or builtin call. Do not re-derive the same fact from its
         * return type: allocating readers such as StringJoin return a String
         * while retaining neither input. Missing rows still fail closed. */
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

/* Reading one String element for an immediate Bool comparison does not
 * publish its descriptor. The collection fact fixes the source as this
 * Array<String>; the graph must show one exact index and one scalar consumer.
 * Stored, returned, called, or otherwise transformed indexed values remain
 * outside this permission. */
static bool borrowed_indexed_scalar_observation(
    const MIRJsonExpressionGraph *g, size_t value)
{
    size_t index_node = SIZE_MAX;
    size_t index_count = 0;
    size_t consumer_count = 0;
    bool scalar_consumer = false;

    for (size_t p = value + 1; p < g->count; p++) {
        const MIRJsonExpressionGraphNode *node = &g->nodes[p];
        if (strcmp(node->kind, "index") != 0 || node->left != (int)value)
            continue;
        index_node = p;
        index_count++;
    }
    if (index_count != 1)
        return false;
    for (size_t p = index_node + 1; p < g->count; p++) {
        const MIRJsonExpressionGraphNode *node = &g->nodes[p];
        if (node->left != (int)index_node && node->right != (int)index_node)
            continue;
        consumer_count++;
        scalar_consumer = strcmp(node->kind, "equality") == 0
            || strcmp(node->kind, "inequality") == 0;
    }
    return consumer_count == 1 && scalar_consumer;
}

/* A local nominal carrier may hold one loop-local Array descriptor until the
 * end of the same iteration.  Every use of that carrier must stay in the
 * cleanup region and flow directly to a declared read-only formal.  A ref
 * formal may forward the borrow through other synchronous ref calls; RETURN,
 * channel escape, mutation, field extraction and untyped uses remain
 * fail-closed through the existing per-formal summary. */
static bool readonly_carrier_argument(const MIRRoutine *r,
    const MIRJsonExpressionGraph *g, size_t value)
{
    for (size_t p = value + 1; p < g->count; p++) {
        const MIRJsonExpressionGraphNode *arg = &g->nodes[p];
        if (strcmp(arg->kind, "call_argument") != 0 ||
            arg->right != (int)value)
            continue;
        int call = arg->left;
        size_t ordinal = 0;
        while (call >= 0 && (size_t)call < g->count &&
               strcmp(g->nodes[call].kind, "call_argument") == 0) {
            ordinal++;
            call = g->nodes[call].left;
        }
        if (call < 0 || (size_t)call >= g->count)
            return false;
        const MIRJsonExpressionGraphNode *node = &g->nodes[call];
        if (strcmp(node->kind, "call") != 0 ||
            strcmp(node->call_target_kind, "direct") != 0 ||
            node->call_target_syntax_id == 0)
            return false;
        size_t arity = ordinal + 1;
        for (size_t q = p + 1; q < g->count; q++) {
            if (strcmp(g->nodes[q].kind, "call_argument") == 0 &&
                g->nodes[q].left == (int)p) {
                arity++;
                p = q;
            }
        }
        return declared_cleanup_argument(r, node->call_target_syntax_id,
            ordinal, arity, false, false);
    }
    return false;
}

static bool local_nominal_carrier_uses(const MIRRoutine *r,
    const MIRInstruction *definition, const bool *region)
{
    if (r == NULL || definition == NULL || region == NULL ||
        definition->kind != MIR_INST_DEF || definition->result_name == NULL ||
        definition->binding_syntax_id == 0)
        return false;
    for (size_t b = 0; b < r->block_count; b++) {
        const MIRBasicBlock *block = &r->blocks[b];
        if (!block->is_reachable)
            continue;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            bool uses_carrier = false;
            for (size_t u = 0; u < inst->use_count; u++)
                uses_carrier |= inst->uses[u] != NULL &&
                    strcmp(inst->uses[u], definition->result_name) == 0;
            if (!uses_carrier)
                continue;
            if (!region[b] || mir_instruction_source_is_defer_stmt(inst))
                return false;
            ASTNode *roots[2];
            if (!mir_instruction_evaluation_roots(r, inst, roots))
                return false;
            bool accounted = false;
            for (size_t e = 0; e < 2; e++) {
                if (roots[e] == NULL)
                    continue;
                MIRJsonExpressionGraph graph = {0};
                bool valid = mir_json_expression_graph_build_for_routine(
                    &graph, roots[e], r) >= 0;
                for (size_t n = 0; valid && n < graph.count; n++) {
                    if (strcmp(graph.nodes[n].kind, "leaf") != 0 ||
                        graph.nodes[n].source_binding_syntax_id !=
                            definition->binding_syntax_id)
                        continue;
                    if (accounted || !readonly_carrier_argument(r, &graph, n))
                        valid = false;
                    else
                        accounted = true;
                }
                mir_json_expression_graph_dispose(&graph);
                if (!valid)
                    return false;
            }
            if (!accounted)
                return false;
        }
    }
    return true;
}

static bool nominal_constructor_target(const MIRRoutine *r,
    const MIRJsonExpressionGraphNode *call)
{
    if (r == NULL || r->program == NULL || call == NULL ||
        strcmp(call->kind, "call") != 0 ||
        strcmp(call->call_target_kind, "direct") != 0 ||
        call->call_target_syntax_id == 0 || r->program->types == NULL)
        return false;
    size_t matches = 0;
    for (size_t i = 0; i < r->program->type_count; i++) {
        const ASTNode *type = r->program->types[i];
        if (type == NULL ||
            ast_node_stable_id(type) != call->call_target_syntax_id)
            continue;
        if (++matches != 1 || type->type != AST_CLASS_DECL ||
            ast_class_nominal_kind(type) != NOMINAL_DECL_STRUCT)
            return false;
    }
    return matches == 1;
}

static bool local_nominal_carrier_argument(const MIRRoutine *r,
    const MIRInstruction *definition, const MIRJsonExpressionGraph *g,
    size_t value, const bool *region)
{
    if (definition == NULL || definition->kind != MIR_INST_DEF ||
        definition->result_name == NULL || definition->binding_syntax_id == 0)
        return false;
    for (size_t p = value + 1; p < g->count; p++) {
        const MIRJsonExpressionGraphNode *arg = &g->nodes[p];
        if (strcmp(arg->kind, "call_argument") != 0 ||
            arg->right != (int)value || arg->left < 0 ||
            (size_t)arg->left >= g->count)
            continue;
        int call_index = arg->left;
        while (call_index >= 0 && (size_t)call_index < g->count &&
               strcmp(g->nodes[call_index].kind, "call_argument") == 0)
            call_index = g->nodes[call_index].left;
        if (call_index < 0 || (size_t)call_index >= g->count)
            return false;
        const MIRJsonExpressionGraphNode *call = &g->nodes[call_index];
        return nominal_constructor_target(r, call) &&
            local_nominal_carrier_uses(r, definition, region);
    }
    return false;
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
    bool owned_elements, uint32_t *kind)
{
    bool owned = owned_elements, borrowed = false;
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
                        PGY_COLLECTION_EFFECT_OWNED_STRING_SET
                    || inst->collection_ownership_effect_kind ==
                        PGY_COLLECTION_EFFECT_OWNED_STRING_POP;
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
                             !borrowed_indexed_argument(r, &graph, n) &&
                             !borrowed_indexed_scalar_observation(&graph, n) &&
                             !local_nominal_carrier_argument(
                                 r, inst, &graph, n, region))) {
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

typedef struct { uint32_t binding; const char *value; } ScalarView;

static bool uses_value(const MIRInstruction *inst, const char *value)
{
    for (size_t u = 0; u < inst->use_count; u++)
        if (inst->uses[u] != NULL && strcmp(inst->uses[u], value) == 0) return true;
    return false;
}

static const MIRInstruction *unique_definition(const MIRRoutine *r,
    uint32_t binding, size_t *birth)
{
    const MIRInstruction *found = NULL;
    if (binding == 0) return NULL;
    for (size_t b = 0; b < r->block_count; b++)
        for (size_t i = 0; i < r->blocks[b].instruction_count; i++) {
            const MIRInstruction *inst = &r->blocks[b].instructions[i];
            if (inst->kind != MIR_INST_DEF || inst->binding_syntax_id != binding) continue;
            if (found != NULL) return NULL;
            found = inst;
            *birth = b;
        }
    return found;
}

/* Only the canonical typed Array.Slice operation creates an alias here.
 * Source spelling is never an origin: the receiver must carry the owner's
 * binding ID, and the result must have the corresponding admitted Slice ABI. */
static bool scalar_slice_origin(const MIRJsonExpressionGraph *g, int root,
    uint32_t binding)
{
    size_t arity = 0;
    while (root >= 0 && (size_t)root < g->count &&
        strcmp(g->nodes[root].kind, "call_argument") == 0) {
        arity++;
        root = g->nodes[root].left;
    }
    if (arity != 2 || root < 0 || (size_t)root >= g->count) return false;
    const MIRJsonExpressionGraphNode *call = &g->nodes[root];
    if (strcmp(call->kind, "call") != 0 ||
        strcmp(call->call_target_kind, "member") != 0 ||
        strcmp(call->call_target_name, "Slice") != 0 ||
        call->call_target_syntax_id != 0 || call->left < 0 ||
        (size_t)call->left >= g->count) return false;
    const MIRJsonExpressionGraphNode *member = &g->nodes[call->left];
    return strcmp(member->kind, "member_access") == 0 && member->left >= 0 &&
        (size_t)member->left < g->count &&
        strcmp(g->nodes[member->left].kind, "leaf") == 0 &&
        g->nodes[member->left].source_binding_syntax_id == binding;
}

static bool scalar_private_uses(const MIRRoutine *r, uint32_t binding,
    const char *value, const char *type, const bool *region)
{
    ScalarView *views = calloc(r->source_local_type_count + 1, sizeof(*views));
    if (views == NULL) return false;
    size_t count = 1;
    views[0] = (ScalarView){binding, value};
    bool valid = true;
    for (size_t l = 0; valid && l < r->source_local_type_count; l++) {
        const MIRSourceLocalType *local = &r->source_local_types[l];
        if (local->type_name == NULL || strncmp(local->type_name, "Slice<", 6) != 0 ||
            strcmp(local->type_name + 5, type + 5) != 0) continue;
        size_t birth = SIZE_MAX;
        const MIRInstruction *def = unique_definition(r, local->binding_syntax_id, &birth);
        if (def == NULL || !uses_value(def, value)) continue;
        MIRJsonExpressionGraph g = {0};
        int root = mir_json_expression_graph_build_for_routine(&g, def->expr0, r);
        valid = region[birth] && def->result_name != NULL &&
            def->abi_type_name != NULL && strcmp(def->abi_type_name, local->type_name) == 0 &&
            scalar_slice_origin(&g, root, binding);
        mir_json_expression_graph_dispose(&g);
        if (valid) views[count++] = (ScalarView){local->binding_syntax_id, def->result_name};
    }
    for (size_t b = 0; valid && b < r->block_count; b++) {
        const MIRBasicBlock *block = &r->blocks[b];
        if (!block->is_reachable) continue;
        for (size_t i = 0; valid && i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (inst->kind == MIR_INST_CLEANUP_EDGE) continue;
            bool relevant = false;
            for (size_t v = 0; v < count; v++) relevant |= uses_value(inst, views[v].value);
            if (!relevant) continue;
            if (!region[b] || mir_instruction_source_is_defer_stmt(inst)) { valid = false; break; }
            ASTNode *roots[2];
            if (!mir_instruction_evaluation_roots(r, inst, roots)) { valid = false; break; }
            bool *seen = calloc(count, sizeof(*seen));
            if (seen == NULL) { valid = false; break; }
            for (size_t e = 0; valid && e < 2; e++) {
                if (roots[e] == NULL) continue;
                MIRJsonExpressionGraph g = {0};
                int root = mir_json_expression_graph_build_for_routine(&g, roots[e], r);
                valid = root >= 0;
                for (size_t n = 0; valid && n < g.count; n++) {
                    if (strcmp(g.nodes[n].kind, "leaf") != 0) continue;
                    for (size_t v = 0; valid && v < count; v++) {
                        if (g.nodes[n].source_binding_syntax_id != views[v].binding) continue;
                        seen[v] = true;
                        size_t consumers = 0;
                        bool indexed = false;
                        for (size_t p = n + 1; p < g.count; p++)
                            if (g.nodes[p].left == (int)n || g.nodes[p].right == (int)n) {
                                consumers++;
                                indexed = strcmp(g.nodes[p].kind, "index") == 0 &&
                                    g.nodes[p].left == (int)n;
                            }
                        bool slice_birth = false;
                        for (size_t a = 1; a < count; a++)
                            slice_birth |= inst->kind == MIR_INST_DEF &&
                                inst->binding_syntax_id == views[a].binding;
                        valid = consumers == 1 && (indexed || (v == 0 && e == 0 &&
                            slice_birth && scalar_slice_origin(&g, root, binding)));
                    }
                }
                mir_json_expression_graph_dispose(&g);
            }
            for (size_t v = 0; v < count; v++)
                if (uses_value(inst, views[v].value) && !seen[v]) valid = false;
            free(seen);
        }
    }
    free(views);
    return valid;
}

const MIRResourceRuntimeRow *mir_collection_retirement_runtime_row(const MIRInstruction *inst)
{
    return inst != NULL && inst->kind == MIR_INST_CLEANUP_EDGE &&
        inst->collection_cleanup_kind == 3 && inst->resource_runtime_fact_present
        ? &inst->resource_runtime_fact : NULL;
}

static bool cleanup_matches(const MIRInstruction *inst, uint32_t binding,
    const char *value, uint32_t kind, size_t target, const MIRResourceRuntimeRow *runtime)
{
    const MIRResourceRuntimeRow *carried = mir_collection_retirement_runtime_row(inst);
    if (runtime != NULL && (carried == NULL ||
        carried->runtime_call_abi_id != mir_abi_resource_runtime_row_id(runtime) ||
        !mir_abi_resource_runtime_row_matches_owner(carried))) return false;
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
    const char *value, uint32_t kind, size_t target, bool issue, size_t *expected,
    const MIRResourceRuntimeRow *runtime)
{
    MIRBasicBlock *block = &r->blocks[b];
    (*expected)++;
    if (!issue) {
        size_t found = 0;
        for (size_t i = 0; i < block->instruction_count; i++)
            found += cleanup_matches(&block->instructions[i], binding, value, kind, target, runtime);
        return found == 1;
    }
    MIRInstruction inst = {0};
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = LOOP_RETIRE;
    inst.binding_syntax_id = binding;
    inst.arg0 = value;
    inst.collection_cleanup_kind = kind;
    inst.collection_cleanup_target = target;
    if (runtime != NULL) {
        inst.resource_runtime_fact_present = true;
        inst.resource_runtime_fact = *runtime;
        inst.resource_runtime_fact.runtime_call_abi_id = mir_abi_resource_runtime_row_id(runtime);
    }
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
        (r->collection_ownership_fact_count > 0 && r->collection_ownership_facts == NULL) ||
        r->source_local_type_count > r->source_local_type_capacity ||
        (r->source_local_type_count > 0 && r->source_local_types == NULL)) goto invalid;
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
        candidates |= r->collection_ownership_facts[f].origin ==
                PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
            || r->collection_ownership_facts[f].origin ==
                PGY_COLLECTION_ORIGIN_CALL_RESULT
            || r->collection_ownership_facts[f].origin ==
                PGY_COLLECTION_ORIGIN_OWNED_LITERAL;
    for (size_t l = 0; l < r->source_local_type_count; l++)
        candidates |= mir_abi_resource_runtime_row_by_type_name(
            r->source_local_types[l].type_name, "ReleaseStorage") != NULL;
    if (!candidates) {
        if (actual != 0) goto invalid;
        return true;
    }
    if (hir == NULL || !hir->has_cfg) {
        if (actual != 0) goto invalid;
        return true;
    }
    if (hir->cfg.block_count > r->block_count || !hir_cfg_execution_current(hir)) goto invalid;
    for (size_t f = 0; f < r->collection_ownership_fact_count + r->source_local_type_count; f++) {
        const MIRCollectionOwnershipFact *fact = f < r->collection_ownership_fact_count
            ? &r->collection_ownership_facts[f] : NULL;
        const MIRSourceLocalType *local = fact == NULL
            ? &r->source_local_types[f - r->collection_ownership_fact_count] : NULL;
        const MIRResourceRuntimeRow *runtime = local != NULL
            ? mir_abi_resource_runtime_row_by_type_name(local->type_name, "ReleaseStorage") : NULL;
        if (fact != NULL && fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
            fact->origin != PGY_COLLECTION_ORIGIN_CALL_RESULT &&
            fact->origin != PGY_COLLECTION_ORIGIN_OWNED_LITERAL)
            continue;
        if (local != NULL && runtime == NULL) continue;
        uint32_t binding = fact != NULL ? fact->binding_syntax_id : local->binding_syntax_id;
        size_t birth = SIZE_MAX;
        const MIRInstruction *definition = unique_definition(r, binding, &birth);
        const char *value = definition != NULL ? definition->result_name : NULL;
        if (value == NULL || birth >= hir->cfg.block_count ||
            !r->blocks[birth].is_reachable) continue;
        if (runtime != NULL) {
            if (definition->abi_type_name == NULL ||
                strcmp(definition->abi_type_name, local->type_name) != 0) continue;
            MIRJsonExpressionGraph graph = {0};
            int root = mir_json_expression_graph_build_for_routine(&graph, definition->expr0, r);
            bool fresh = root >= 0 && (strcmp(graph.nodes[root].kind, "array_literal") == 0 ||
                strcmp(graph.nodes[root].kind, "array_element") == 0);
            mir_json_expression_graph_dispose(&graph);
            if (!fresh) continue;
        }
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
        bool private = runtime != NULL
            ? scalar_private_uses(r, binding, value, local->type_name, region)
            : private_uses(r, binding, value, region,
                fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS, &kind);
        if (runtime != NULL) kind = 3;
        if (private)
            for (size_t q = 0; valid && q < count; q++) {
                size_t b = queue[q];
                MIRBasicBlock *block = &r->blocks[b];
                size_t targets[2] = {block->has_succ_true ? block->succ_true : SIZE_MAX,
                                     block->has_succ_false ? block->succ_false : SIZE_MAX};
                if (!block->has_succ_true && !block->has_succ_false) {
                    /* Abort/unreachable is not a continuing cleanup exit. */
                    for (size_t i = 0; i < block->instruction_count; i++)
                        if (block->instructions[i].kind == MIR_INST_RETURN)
                            valid = issue_or_check(r, b, binding, value,
                                kind, SIZE_MAX, issue, &expected, runtime);
                } else for (size_t s = 0; valid && s < 2; s++)
                    if (targets[s] < r->block_count && !region[targets[s]] &&
                        (s == 0 || targets[s] != targets[0]))
                        valid = issue_or_check(r, b, binding, value,
                            kind, targets[s], issue, &expected, runtime);
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

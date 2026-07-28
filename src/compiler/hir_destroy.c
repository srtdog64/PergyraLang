#include "hir.h"

#include <stdlib.h>

static void
hir_destroy_synthetic_executable_func(ASTNode *func)
{
    ASTNode *body;

    if (func == NULL || func->type != AST_FUNC_DECL)
        return;

    body = ast_func_detach_body(func);
    if (body != NULL && body->type == AST_BLOCK) {
        ASTNode **borrowed_statements = ast_block_detach_statements(body, NULL);
        free(borrowed_statements);
    }
    ast_destroy(body);
    ast_destroy(func);
}

void
hir_destroy(HIRProgram *hir)
{
    if (hir == NULL)
        return;

    free(hir->items);
    free(hir->decls);
    if (hir->routines != NULL) {
        for (size_t i = 0; i < hir->routine_count; i++) {
            if (hir->routines[i].cfg.blocks != NULL) {
                for (size_t j = 0; j < hir->routines[i].cfg.block_count; j++) {
                    free(hir->routines[i].cfg.blocks[j].statements);
                    free(hir->routines[i].cfg.blocks[j].resource_scope_exits);
                    free(hir->routines[i].cfg.blocks[j].predecessors);
                    free(hir->routines[i].cfg.blocks[j].dom_tree_children);
                    free((void *)hir->routines[i].cfg.blocks[j].local_defs);
                    free(hir->routines[i].cfg.blocks[j].dominance_frontier);
                    free((void *)hir->routines[i].cfg.blocks[j].phi_candidates);
                    if (hir->routines[i].cfg.blocks[j].phi_nodes != NULL) {
                        for (size_t k = 0; k < hir->routines[i].cfg.blocks[j].phi_node_count; k++)
                            free(hir->routines[i].cfg.blocks[j].phi_nodes[k].incoming_predecessors);
                    }
                    free(hir->routines[i].cfg.blocks[j].phi_nodes);
                }
            }
            free(hir->routines[i].cfg.blocks);
            free((void *)hir->routines[i].signature_type_refs);
            free((void *)hir->routines[i].direct_calls);
            free(hir->routines[i].direct_call_decl_ids);
            free(hir->routines[i].callee_routine_ids);
            for (size_t k = 0;
                 k < hir->routines[i].resource_flow_symbol_count;
                 k++)
                free((void *)hir->routines[i].resource_flow_symbols[k].name);
            free(hir->routines[i].resource_flow_symbols);
            free(hir->routines[i].function_param_flow_summaries);
            free(hir->routines[i].loop_flow_summaries);
            free(hir->routines[i].loop_flow_states);
            for (size_t k = 0;
                 k < hir->routines[i].iteration_type_fact_count; k++) {
                free(hir->routines[i].iteration_type_facts[k].binding_type_name);
                free(hir->routines[i].iteration_type_facts[k].iterable_type_name);
            }
            free(hir->routines[i].iteration_type_facts);
            for (size_t k = 0;
                 k < hir->routines[i].destructure_type_fact_count; k++) {
                free(hir->routines[i]
                    .destructure_type_facts[k].binding_type_name);
            }
            free(hir->routines[i].destructure_type_facts);
            for (size_t k = 0;
                 k < hir->routines[i].match_binding_type_fact_count; k++) {
                free(hir->routines[i]
                    .match_binding_type_facts[k].binding_type_name);
            }
            free(hir->routines[i].match_binding_type_facts);
            pgy_arena_destroy(&hir->routines[i].scratch);
        }
    }
    free(hir->routines);
    free(hir->externs);
    free(hir->types);
    free(hir->abilities);
    free(hir->roles);
    free(hir->parties);
    free(hir->rosters);
    free(hir->worlds);
    free(hir->relations);
    free(hir->effects);
    free(hir->zones);
    free(hir->subjects);
    free(hir->events);
    free(hir->intents);
    free(hir->functions);
    free(hir->executables);
    free(hir->region_escape_facts);
    pgy_domain_participant_role_facts_destroy(
        hir->domain_participant_role_facts,
        hir->domain_participant_role_fact_count);
    pgy_domain_projection_member_assignment_facts_destroy(
        hir->domain_projection_member_assignment_facts,
        hir->domain_projection_member_assignment_fact_count);
    hir_destroy_synthetic_executable_func(hir->synthetic_executable_func);
    free(hir);
}

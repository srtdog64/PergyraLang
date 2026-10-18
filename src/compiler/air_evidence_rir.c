/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR evidence collection orchestrator.
 */

#include "air_internal.h"
#include "hir_cfg.h"
#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>

/* The original HIR execution census owns the callable set.  Neither an
 * empty RIR scope list nor an empty derived HIR projection can erase it. */
static bool
air_original_callable_set_current(const RIRProgram *rir,
                                  const RIRScopeInventory *inventory,
                                  const HIRProgram *hir,
                                  const DIRProgram *dir,
                                  char **error_message)
{
    bool native = rir->program_root != NULL ||
        (hir != NULL && (hir->source_program_syntax_id != 0 ||
                         hir->execution_census != NULL));
    if (!native)
        return true;
    if (rir->program_root == NULL || hir == NULL || dir == NULL ||
        !dir_source_program_current(dir, rir->program_root) ||
        !hir_cfg_program_execution_current(hir, dir->program_root) ||
        inventory->count != rir->scope_count ||
        (inventory->count != 0 && inventory->scopes == NULL) ||
        (dir->node_count != 0 && dir->nodes == NULL) ||
        (dir->intent_count != 0 && dir->intents == NULL) ||
        (hir->intent_count != 0 && hir->intents == NULL) ||
        dir->intent_count != hir->intent_count) {
        air_set_error(error_message,
                      "AIR source callable census is incomplete or stale");
        return false;
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        size_t matches = 0;
        if (routine->ast == NULL || routine->source_syntax_id == 0 ||
            !(routine->ast->type == AST_FUNC_DECL ||
              routine->ast->type == AST_INTENT_DECL)) {
            air_set_error(error_message,
                          "AIR source callable lacks original HIR identity");
            return false;
        }
        for (size_t j = 0; j < inventory->count; j++) {
            const RIRScope *scope = rir_scope_inventory_get(inventory, j);
            bool projected = false;
            if (scope == NULL ||
                (scope->ast != routine->ast &&
                 scope->source_syntax_id != routine->source_syntax_id))
                continue;
            if (scope->ast != routine->ast ||
                scope->source_syntax_id != routine->source_syntax_id ||
                (rir->program_root != NULL &&
                 scope->program_root != rir->program_root) ||
                !air_rir_io_owner_projected(routine->ast,
                                             routine->source_syntax_id,
                                             scope, hir, dir, &projected) ||
                ++matches > 1) {
                air_set_error(error_message,
                              "AIR original callable has a foreign or duplicate RIR scope");
                return false;
            }
            if (routine->ast->type == AST_INTENT_DECL && !projected) {
                air_set_error(error_message,
                              "AIR original intent has no DIR projection");
                return false;
            }
        }
        if (matches != 1) {
            air_set_error(error_message,
                          "AIR original callable has no exact RIR scope");
            return false;
        }
    }

    for (size_t i = 0; i < inventory->count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(inventory, i);
        size_t matches = 0;
        if (scope == NULL)
            continue;
        bool callable = (scope->ast != NULL &&
                         (scope->ast->type == AST_FUNC_DECL ||
                          scope->ast->type == AST_INTENT_DECL)) ||
                        scope->kind == RIR_SCOPE_FUNCTION ||
                        scope->kind == RIR_SCOPE_METHOD ||
                        scope->kind == RIR_SCOPE_INTENT;
        if (!callable)
            continue;
        if (scope->ast == NULL || scope->source_syntax_id == 0) {
            air_set_error(error_message,
                          "AIR RIR callable scope lacks original source identity");
            return false;
        }
        for (size_t j = 0; j < hir->routine_count; j++) {
            const HIRRoutine *routine = &hir->routines[j];
            if (routine->ast != scope->ast &&
                routine->source_syntax_id != scope->source_syntax_id)
                continue;
            if (routine->ast != scope->ast ||
                routine->source_syntax_id != scope->source_syntax_id ||
                ++matches > 1) {
                air_set_error(error_message,
                              "AIR RIR callable scope is foreign or ambiguous");
                return false;
            }
        }
        if (matches != 1) {
            air_set_error(error_message,
                          "AIR RIR callable scope is absent from the HIR census");
            return false;
        }
    }

    if (dir != NULL) {
        size_t intent_nodes = 0;
        for (size_t i = 0; i < dir->node_count; i++) {
            const DIRNode *node = &dir->nodes[i];
            size_t matches = 0;
            if (node->kind != DIR_NODE_INTENT)
                continue;
            if (node->ast == NULL || node->source_syntax_id == 0 ||
                node->id != i) {
                air_set_error(error_message,
                              "AIR DIR intent lacks original source identity");
                return false;
            }
            intent_nodes++;
            for (size_t j = 0; j < hir->intent_count; j++) {
                const ASTNode *intent = hir->intents[j];
                if (intent != node->ast &&
                    ast_node_stable_id(intent) != node->source_syntax_id)
                    continue;
                if (intent != node->ast ||
                    ast_node_stable_id(intent) != node->source_syntax_id ||
                    ++matches > 1) {
                    air_set_error(error_message,
                                  "AIR DIR intent is foreign or ambiguous");
                    return false;
                }
            }
            if (matches != 1) {
                air_set_error(error_message,
                              "AIR DIR intent has no original HIR projection");
                return false;
            }
        }
        if (intent_nodes != hir->intent_count) {
            air_set_error(error_message,
                          "AIR DIR intent projection count differs from HIR");
            return false;
        }
    }
    return true;
}

/* AIR projects only HIR routine and DIR intent IO. RIR also owns hosted
 * method scopes, whose IO effects have no AIR boundary in this version. */
static bool
air_validate_projected_rir_io(AIRProgram *air,
                              const RIRProgram *rir,
                              const RIRScopeInventory *inventory,
                              const HIRProgram *hir,
                              const DIRProgram *dir,
                              char **error_message)
{
    if (!air_original_callable_set_current(rir, inventory, hir, dir,
                                           error_message))
        return false;
    if (hir != NULL && hir->routine_count != 0 && hir->routines == NULL) {
        air_set_error(error_message,
                      "AIR IO source routine inventory is incomplete");
        return false;
    }
    if (hir != NULL) {
        for (size_t i = 0; i < hir->routine_count; i++) {
            const HIRRoutine *routine = &hir->routines[i];
            bool projected = false;
            if (routine->execution_layout == NULL)
                continue;
            if (!hir_cfg_execution_owner_current(routine) ||
                !air_rir_io_owner_projected(routine->ast,
                                             routine->source_syntax_id,
                                             NULL, hir, dir, &projected)) {
                air_set_error(error_message,
                              "AIR IO source routine lost its original HIR/DIR owner projection");
                return false;
            }
            /* Only an exactly evidenced hidden method is outside AIR's
             * callable projection; the owner predicate proves that case. */
        }
    }
    for (size_t i = 0; i < inventory->count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(inventory, i);
        bool projected = false;
        bool callable = scope != NULL &&
            ((scope->ast != NULL &&
              (scope->ast->type == AST_FUNC_DECL ||
               scope->ast->type == AST_INTENT_DECL)) ||
             (scope->ast == NULL &&
              (scope->kind == RIR_SCOPE_FUNCTION ||
               scope->kind == RIR_SCOPE_METHOD ||
               scope->kind == RIR_SCOPE_INTENT)));
        if (!callable)
            continue;
        if (scope->ast == NULL || scope->source_syntax_id == 0) {
            if (rir->program_root != NULL || scope->program_root != NULL) {
                air_set_error(error_message,
                              "AIR IO source scope has incomplete callable identity");
                return false;
            }
            continue;
        }
        if ((rir->program_root != NULL &&
             scope->program_root != rir->program_root) ||
            !air_rir_io_owner_projected(scope->ast,
                                         scope->source_syntax_id,
                                         scope, hir, dir, &projected)) {
            air_set_error(error_message,
                          "AIR IO source scope lost its original HIR/DIR owner projection");
            return false;
        }
    }
    size_t boundary_count = air_boundary_node_count(air);
    size_t *matches = NULL;
    if (boundary_count > SIZE_MAX / sizeof(*matches)) {
        air_set_error(error_message, "AIR IO occurrence count overflow");
        return false;
    }
    if (boundary_count != 0) {
        matches = (size_t *)calloc(boundary_count, sizeof(*matches));
        if (matches == NULL) {
            air_set_error(error_message, "AIR IO occurrence allocation failed");
            return false;
        }
    }
    for (size_t i = 0; i < boundary_count; i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        bool projected = false;
        if (boundary == NULL || boundary->kind != AIR_BOUNDARY_IO)
            continue;
        if (!air_rir_io_owner_projected(boundary->io_owner_ast,
                                         boundary->io_owner_syntax_id,
                                         NULL,
                                         hir, dir, &projected) || !projected) {
            air_set_error(error_message,
                          "AIR IO boundary lacks its original HIR/DIR owner inventory");
            free(matches);
            return false;
        }
    }
    for (size_t i = 0; i < inventory->count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(inventory, i);
        bool projected = false;
        bool has_io = false;
        if (scope == NULL)
            continue;
        for (size_t j = 0; j < rir_scope_op_count(scope); j++) {
            const RIROp *op = rir_scope_op_at(scope, j);
            if (op != NULL && op->kind == RIR_OP_IO) {
                has_io = true;
                break;
            }
        }
        if (!has_io)
            continue;
        if (!air_rir_io_owner_projected(scope->ast, scope->source_syntax_id,
                                         scope,
                                         hir, dir, &projected)) {
            air_set_error(error_message,
                          "RIR IO operation lacks its original HIR/DIR owner inventory");
            free(matches);
            return false;
        }
        if (!projected)
            continue;
        for (size_t j = 0; j < rir_scope_op_count(scope); j++) {
            const RIROp *op = rir_scope_op_at(scope, j);
            size_t op_matches = 0;
            if (op == NULL || op->kind != RIR_OP_IO)
                continue;
            for (size_t k = 0; k < boundary_count; k++) {
                const AIRBoundaryNode *boundary = air_boundary_node_at(air, k);
                if (!air_rir_io_tuple_matches(scope, op, boundary))
                    continue;
                if (++op_matches > 1 || ++matches[k] > 1) {
                    air_set_error(error_message,
                                  "AIR IO boundary has duplicate RIR occurrences");
                    free(matches);
                    return false;
                }
            }
            if (op_matches != 1) {
                air_set_error(error_message,
                              "RIR IO operation has no exact AIR boundary");
                free(matches);
                return false;
            }
        }
    }
    free(matches);
    /* A zero match retains the existing boundary-evidence drift diagnostic. */
    return true;
}

/* A zone owns authority only through an explicit `authority` declaration,
 * which the RIR builder records as an AUTHORIZED authority-handle fact on the
 * zone scope. Stamp that ownership onto each zone boundary before evidence
 * collection runs, so the intent-alias admission in
 * air_rir_scope_provides_boundary_evidence can require it: a step-local
 * `authorized by:` against a zone that declares no authority proves nothing,
 * and the boundary must keep failing closed as evidence-missing. */
static void
air_stamp_zone_authority_ownership(AIRProgram *air,
                                   const RIRScopeInventory *inventory)
{
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, i);
        if (boundary == NULL || boundary->kind != AIR_BOUNDARY_ZONE)
            continue;
        for (size_t j = 0; j < inventory->count && !boundary->zone_owns_rir_authority; j++) {
            const RIRScope *scope = rir_scope_inventory_get(inventory, j);
            if (scope == NULL || scope->kind != RIR_SCOPE_ZONE
                || !air_name_matches(scope->name, boundary->source_name))
                continue;
            for (size_t k = 0; k < rir_scope_fact_count(scope); k++) {
                const RIRFact *fact = rir_scope_fact_at(scope, k);
                if (fact != NULL
                    && fact->kind == RIR_FACT_AUTHORITY
                    && fact->resource_kind == RIR_RESOURCE_AUTHORITY_HANDLE
                    && fact->state == RIR_STATE_AUTHORIZED) {
                    boundary->zone_owns_rir_authority = true;
                    break;
                }
            }
        }
    }
}

bool
air_collect_rir_evidence(AIRProgram *air,
                         const RIRProgram *rir,
                         const HIRProgram *hir,
                         const DIRProgram *dir,
                         char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;

    air_mark_rir_input(air);
    RIRScopeInventory inventory;
    rir_scope_inventory_from_program(rir, &inventory);

    if (!air_validate_projected_rir_io(air, rir, &inventory, hir, dir,
                                        error_message))
        return false;

    air_stamp_zone_authority_ownership(air, &inventory);

    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);

        if (!air_collect_rir_scope_propagation_evidence(air,
                                                        scope,
                                                        error_message)) {
            return false;
        }
        if (!air_collect_rir_scope_boundary_evidence(air,
                                                     scope,
                                                     error_message)) {
            return false;
        }
    }
    return true;
}

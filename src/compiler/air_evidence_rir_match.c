/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR scope and boundary matching owner.
 */

#include "air_internal.h"
#include "hir_cfg.h"
#include "../parser/ast_api.h"

#include <string.h>

const char *
air_rir_scope_provider_name(const RIRScope *scope)
{
    if (scope == NULL)
        return NULL;
    if (!air_name_is_empty(scope->name))
        return scope->name;
    if (!air_name_is_empty(scope->owner_name))
        return scope->owner_name;
    return NULL;
}

bool
air_require_rir_scope_provider(const RIRScope *scope,
                               char **error_message)
{
    if (air_rir_scope_provider_name(scope) != NULL)
        return true;
    air_set_error(error_message,
                  "AIR RIR evidence requires scope name or owner provenance");
    return false;
}

static bool
air_rir_scope_matches_boundary(const RIRScope *scope,
                               const AIRBoundaryNode *boundary)
{
    if (scope == NULL || boundary == NULL)
        return false;
    if (!(scope->kind == RIR_SCOPE_INTENT
          || scope->kind == RIR_SCOPE_ZONE
          || scope->kind == RIR_SCOPE_WORLD)) {
        return false;
    }
    /* An action-inherited authorization is evidenced by the intent-local
     * Authorize operation.  A zone authority row names the zone slot, which
     * need not equal the intent participant alias (runner vs runner_alias),
     * so it must not become a compatibility authority for this boundary. */
    if (boundary->authority_from_action) {
        if (scope->kind == RIR_SCOPE_INTENT) {
            return air_name_matches(scope->owner_name, boundary->owner_name)
                || air_name_matches(scope->name, boundary->owner_name);
        }
        return scope->kind == RIR_SCOPE_ZONE
            && air_name_matches(scope->name, boundary->source_name);
    }
    if (boundary->kind == AIR_BOUNDARY_PARALLEL
        || boundary->kind == AIR_BOUNDARY_IO
        || boundary->kind == AIR_BOUNDARY_CHANNEL) {
        return air_name_matches(scope->name, boundary->source_name);
    }
    if (boundary->kind == AIR_BOUNDARY_WORLD) {
        return air_name_matches(scope->name, boundary->source_name)
            || air_name_matches(scope->name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->source_name);
    }
    return air_name_matches(scope->name, boundary->source_name)
        || air_name_matches(scope->owner_name, boundary->owner_name)
        || air_name_matches(scope->owner_name, boundary->source_name);
}

static bool
air_rir_op_matches_boundary_ast(const RIROp *op,
                                const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL)
        return false;
    if (boundary->ast == NULL)
        return true;
    return op->ast == boundary->ast
        || air_ast_contains_node(boundary->ast, op->ast);
}

static bool
air_io_name_exact(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
        return left == right;
    return strcmp(left, right) == 0;
}

/* Replay the original RIR builder's scope identity, not the scope's claim
 * about whether its owner is projected by AIR. */
static bool
air_io_scope_issued_tuple_current(const RIRScope *scope,
                                  const ASTNode *owner,
                                  uint32_t owner_id,
                                  RIRScopeKind kind,
                                  const char *name,
                                  const char *owner_name,
                                  size_t parameter_count)
{
    return scope == NULL ||
        (scope->ast == owner && scope->source_syntax_id == owner_id &&
         scope->kind == kind && scope->parameter_count == parameter_count &&
         air_io_name_exact(scope->name, name) &&
         air_io_name_exact(scope->owner_name, owner_name));
}

static bool
air_io_host_decl_kind_current(const HIRDecl *decl)
{
    if (decl == NULL || decl->ast == NULL)
        return false;
    switch (decl->ast->type) {
    case AST_CLASS_DECL:
    case AST_ENUM_DECL:
        return decl->kind == HIR_TOPLEVEL_TYPE;
    case AST_ROLE_DECL:
        return decl->kind == HIR_TOPLEVEL_ROLE;
    case AST_PARTY_DECL:
        return decl->kind == HIR_TOPLEVEL_PARTY;
    case AST_ROSTER_DECL:
        return decl->kind == HIR_TOPLEVEL_SYSTEMIC;
    case AST_WORLD_DECL:
        return decl->kind == HIR_TOPLEVEL_WORLD;
    case AST_RELATION_DECL:
        return decl->kind == HIR_TOPLEVEL_RELATION;
    case AST_EFFECT_DECL:
        return decl->kind == HIR_TOPLEVEL_EFFECT;
    case AST_ZONE_DECL:
        return decl->kind == HIR_TOPLEVEL_ZONE;
    default:
        return false;
    }
}

bool
air_rir_io_owner_projected(const ASTNode *owner,
                           uint32_t owner_id,
                           const RIRScope *scope,
                           const HIRProgram *hir,
                           const DIRProgram *dir,
                           bool *projected)
{
    size_t original_matches = 0;
    size_t projection_matches = 0;
    if (projected == NULL || owner == NULL || owner_id == 0 ||
        ast_node_stable_id(owner) != owner_id)
        return false;
    *projected = false;
    if (owner->type == AST_FUNC_DECL) {
        if (hir == NULL ||
            (hir->decl_count != 0 && hir->decls == NULL) ||
            (hir->function_count != 0 && hir->functions == NULL) ||
            (hir->routine_count != 0 && hir->routines == NULL))
            return false;
        const HIRDecl *own_decl = NULL;
        const HIRRoutine *routine = NULL;
        for (size_t i = 0; i < hir->decl_count; i++) {
            const HIRDecl *decl = &hir->decls[i];
            if (decl->ast != owner && decl->source_syntax_id != owner_id)
                continue;
            if (decl->ast != owner || decl->source_syntax_id != owner_id ||
                !(decl->kind == HIR_TOPLEVEL_FUNCTION ||
                  decl->kind == HIR_TOPLEVEL_EXECUTABLE) ||
                decl->id != i || ++original_matches > 1)
                return false;
            own_decl = decl;
        }
        for (size_t i = 0; i < hir->function_count; i++) {
            const ASTNode *candidate = hir->functions[i];
            uint32_t candidate_id = ast_node_stable_id(candidate);
            if (candidate != owner && candidate_id != owner_id)
                continue;
            if (candidate != owner || candidate_id != owner_id ||
                ++projection_matches > 1)
                return false;
        }
        for (size_t i = 0; i < hir->routine_count; i++) {
            const HIRRoutine *candidate = &hir->routines[i];
            if (candidate->ast != owner &&
                candidate->source_syntax_id != owner_id)
                continue;
            if (candidate->ast != owner ||
                candidate->source_syntax_id != owner_id ||
                candidate->routine_id != i + 1 || routine != NULL)
                return false;
            routine = candidate;
        }
        const char *name = ast_declaration_name(owner);
        if (name == NULL)
            return false;
        if (original_matches == 1 && projection_matches == 1) {
            if (routine != NULL &&
                (routine->decl_id != own_decl->id ||
                 routine->kind != own_decl->kind ||
                 !air_io_name_exact(routine->name, name) ||
                 routine->owner_name != NULL || routine->owner_ast_type != 0 ||
                 routine->is_action_like != ast_func_is_action(owner) ||
                 routine->is_hosted != (strchr(name, '.') != NULL) ||
                 routine->parameter_count != ast_func_param_count(owner)))
                return false;
            if (!air_io_scope_issued_tuple_current(
                    scope, owner, owner_id,
                    ast_func_is_action(owner) ? RIR_SCOPE_METHOD
                                              : RIR_SCOPE_FUNCTION,
                    name, NULL, ast_func_param_count(owner)))
                return false;
            *projected = true;
            return true;
        }
        if (original_matches != 0 || projection_matches != 0 ||
            routine == NULL || !routine->is_hosted ||
            routine->kind != HIR_TOPLEVEL_FUNCTION ||
            routine->is_action_like != ast_func_is_action(owner) ||
            routine->parameter_count != ast_func_param_count(owner) ||
            !air_io_name_exact(routine->name, name) ||
            routine->decl_id >= hir->decl_count)
            return false;
        const HIRDecl *host = &hir->decls[routine->decl_id];
        if (host->id != routine->decl_id ||
            host->source_syntax_id == 0 || host->ast == owner ||
            ast_node_stable_id(host->ast) != host->source_syntax_id ||
            host->ast != routine->declaration_ast ||
            !hir_cfg_execution_owner_current(routine) ||
            !air_io_host_decl_kind_current(host) ||
            routine->owner_ast_type != host->ast->type ||
            !air_io_name_exact(host->name, ast_declaration_name(host->ast)) ||
            !air_io_name_exact(routine->owner_name, host->name) ||
            routine->owner_name == NULL ||
            !air_io_scope_issued_tuple_current(
                scope, owner, owner_id, RIR_SCOPE_METHOD,
                name, routine->owner_name, ast_func_param_count(owner)))
            return false;
        return true;
    } else if (owner->type == AST_INTENT_DECL) {
        if (dir == NULL ||
            (dir->node_count != 0 && dir->nodes == NULL) ||
            (dir->intent_count != 0 && dir->intents == NULL))
            return false;
        size_t node_id = SIZE_MAX;
        for (size_t i = 0; i < dir->node_count; i++) {
            const DIRNode *node = &dir->nodes[i];
            if (node->ast != owner && node->source_syntax_id != owner_id)
                continue;
            if (node->ast != owner || node->source_syntax_id != owner_id ||
                node->kind != DIR_NODE_INTENT || node->id != i ||
                ++original_matches > 1)
                return false;
            node_id = node->id;
        }
        for (size_t i = 0; i < dir->intent_count; i++) {
            const DIRIntentInfo *intent = &dir->intents[i];
            if (intent->ast != owner &&
                ast_node_stable_id(intent->ast) != owner_id &&
                (node_id == SIZE_MAX || intent->node_id != node_id))
                continue;
            if (intent->ast != owner || node_id == SIZE_MAX ||
                intent->node_id != node_id ||
                ++projection_matches > 1)
                return false;
        }
        if (original_matches != 1 || projection_matches != 1 ||
            !air_io_scope_issued_tuple_current(scope, owner, owner_id,
                                               RIR_SCOPE_INTENT,
                                               ast_intent_decl_name(owner),
                                               NULL, 0))
            return false;
        *projected = true;
        return true;
    } else {
        return false;
    }
}

bool
air_rir_io_tuple_matches(const RIRScope *scope,
                         const RIROp *op,
                         const AIRBoundaryNode *boundary)
{
    return scope != NULL && op != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO && op->kind == RIR_OP_IO
        && boundary->io_owner_ast != NULL && boundary->io_owner_syntax_id != 0
        && ast_node_stable_id(boundary->io_owner_ast) ==
            boundary->io_owner_syntax_id
        && scope->ast == boundary->io_owner_ast
        && scope->source_syntax_id == boundary->io_owner_syntax_id
        && boundary->ast != NULL && boundary->ast->type == AST_CALL
        && boundary->source_stable_id != 0
        && ast_node_stable_id(boundary->ast) == boundary->source_stable_id
        && op->ast == boundary->ast
        && air_boundary_kind_from_ast(boundary->ast) == AIR_BOUNDARY_IO
        && air_name_matches(air_boundary_source_from_ast(boundary->ast),
                            boundary->source_name)
        && air_name_matches(op->subject, boundary->source_name)
        && (boundary->io_defer_registration_syntax_id == 0 ||
            (op->has_source_statement_syntax_id &&
             op->source_statement_syntax_id ==
                 boundary->io_defer_registration_syntax_id));
}

static bool
air_rir_channel_op_matches_boundary(const RIROp *op,
                                    const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL || boundary->kind != AIR_BOUNDARY_CHANNEL)
        return false;
    if (air_name_matches(boundary->source_name, "channel-send"))
        return op->kind == RIR_OP_CHANNEL_SEND
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "channel-recv"))
        return op->kind == RIR_OP_CHANNEL_RECV
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "select"))
        return op->kind == RIR_OP_CHANNEL_SELECT
            && air_rir_op_matches_boundary_ast(op, boundary);
    return false;
}

bool
air_rir_parallel_op_matches_boundary(const RIROp *op,
                                     const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL || boundary->kind != AIR_BOUNDARY_PARALLEL)
        return false;
    if (air_name_matches(boundary->source_name, "await"))
        return (op->kind == RIR_OP_AWAIT_LOCAL
                || op->kind == RIR_OP_AWAIT_REMOTE)
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "spawn"))
        return op->kind == RIR_OP_SPAWN
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "async"))
        return op->kind == RIR_OP_ASYNC
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "parallel"))
        return op->kind == RIR_OP_PARALLEL
            && air_rir_op_matches_boundary_ast(op, boundary);
    return false;
}

bool
air_rir_scope_provides_boundary_evidence(const RIRScope *scope,
                                         const AIRBoundaryNode *boundary)
{
    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_PARALLEL) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (air_rir_parallel_op_matches_boundary(op, boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO) {
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (air_rir_io_tuple_matches(scope, op, boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_CHANNEL) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (air_rir_channel_op_matches_boundary(op, boundary))
                return true;
        }
        return false;
    }

    /* A local authorization may use an intent participant alias that differs
     * from the zone subject-slot name. Admit the intent scope only when its
     * exact step owns an Authorize op for a declared boundary participant,
     * and only against a zone that owns authority to grant: a zone with no
     * `authority` declaration has nothing a step-local `authorized by:` could
     * bind to, so such a boundary stays evidence-missing (fail closed).
     * Whether the step also `requires` an ability is not part of the
     * question: a step without one used to fall through to the zone's own
     * authority row, which matched only when the alias happened to spell the
     * slot name. */
    if (scope != NULL && boundary != NULL
        && scope->kind == RIR_SCOPE_INTENT
        && boundary->kind == AIR_BOUNDARY_ZONE
        && boundary->authority_required
        && boundary->zone_owns_rir_authority
        && air_name_matches(scope->name, boundary->owner_name)) {
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (op != NULL
                && op->kind == RIR_OP_AUTHORIZE
                && air_boundary_declares_authority_name(boundary, op->subject)
                && air_rir_op_matches_boundary_ast(op, boundary)) {
                return true;
            }
        }
    }

    if (!air_rir_scope_matches_boundary(scope, boundary))
        return false;
    if (boundary->kind != AIR_BOUNDARY_WORLD)
        return true;

    for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
        const RIROp *op = rir_scope_op_at(scope, i);
        if (op == NULL)
            continue;
        if (op->kind == RIR_OP_CLAIM
            && air_name_matches(op->subject, boundary->source_name)
            && air_rir_op_matches_boundary_ast(op, boundary)) {
            return true;
        }
        if (op->kind == RIR_OP_MOVE
            && air_name_matches(op->arg0, boundary->source_name)
            && air_rir_op_matches_boundary_ast(op, boundary)) {
            return true;
        }
    }
    return false;
}

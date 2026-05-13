#include "module_normalizer_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static const char *module_rename_scope_lookup(const ModuleRenameScope *scope,
                                              const char *name);

bool
module_rename_scope_add(ModuleRenameScope *scope,
                        const char *old_name,
                        const char *new_name)
{
    char *owned_old_name;
    char *owned_new_name;

    if (scope == NULL || old_name == NULL || new_name == NULL)
        return false;

    if (scope->count == scope->capacity) {
        size_t next = 8;
        if (scope->capacity != 0) {
            if (scope->capacity > SIZE_MAX / 2)
                return false;
            next = scope->capacity * 2;
        }
        if (next > SIZE_MAX / sizeof(ModuleRenameEntry)) {
            return false;
        }
        ModuleRenameEntry *grown =
            realloc(scope->entries, next * sizeof(ModuleRenameEntry));
        if (grown == NULL)
            return false;
        scope->entries = grown;
        scope->capacity = next;
    }
    owned_old_name = pergyra_strdup(old_name);
    owned_new_name = pergyra_strdup(new_name);
    if (owned_old_name == NULL || owned_new_name == NULL) {
        free(owned_old_name);
        free(owned_new_name);
        return false;
    }
    scope->entries[scope->count].old_name = owned_old_name;
    scope->entries[scope->count].new_name = owned_new_name;
    scope->count++;
    return true;
}

static const char *
module_rename_scope_lookup(const ModuleRenameScope *scope, const char *name)
{
    for (const ModuleRenameScope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->count; i++) {
            if (strcmp(s->entries[i].old_name, name) == 0)
                return s->entries[i].new_name;
        }
    }
    return NULL;
}

void
module_rename_scope_destroy(ModuleRenameScope *scope)
{
    if (scope == NULL)
        return;
    for (size_t i = 0; i < scope->count; i++) {
        free(scope->entries[i].old_name);
        free(scope->entries[i].new_name);
    }
    free(scope->entries);
    scope->entries = NULL;
    scope->count = 0;
    scope->capacity = 0;
}

static void
normalize_generic_params(GenericParams *params,
                         ModuleRenameScope *scope,
                         ModuleShadowNames *shadow)
{
    if (params == NULL)
        return;
    size_t saved = shadow->count;
    for (size_t i = 0; i < params->count; i++) {
        if (params->params[i] != NULL && params->params[i]->name != NULL)
            module_shadow_push(shadow, params->params[i]->name);
    }
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (param == NULL)
            continue;
        module_normalizer_normalize_node_refs(param->constraint, scope, shadow);
        module_normalizer_normalize_node_refs(param->default_type, scope, shadow);
    }
    module_shadow_pop_to(shadow, saved);
}

static void
normalize_type_node(ASTNode *node,
                    ModuleRenameScope *scope,
                    ModuleShadowNames *shadow)
{
    if (node == NULL || node->type != AST_TYPE)
        return;

    if (node->data.type.name != NULL
        && !module_shadow_contains(shadow, node->data.type.name)) {
        const char *replacement =
            module_rename_scope_lookup(scope, node->data.type.name);
        if (replacement != NULL && strcmp(node->data.type.name, replacement) != 0) {
            free(node->data.type.name);
            node->data.type.name = pergyra_strdup(replacement);
        }
    }

    if (node->data.type.generic_args != NULL) {
        for (size_t i = 0; i < node->data.type.generic_args->count; i++) {
            GenericParam *arg = node->data.type.generic_args->params[i];
            if (arg == NULL)
                continue;
            module_normalizer_normalize_node_refs(arg->constraint, scope, shadow);
            module_normalizer_normalize_node_refs(arg->default_type, scope, shadow);
        }
    }
}

static void
normalize_call_args(ASTNode **args,
                    size_t count,
                    ModuleRenameScope *scope,
                    ModuleShadowNames *shadow)
{
    for (size_t i = 0; i < count; i++)
        module_normalizer_normalize_node_refs(args[i], scope, shadow);
}

void
module_normalizer_normalize_node_refs(ASTNode *node,
                                      ModuleRenameScope *scope,
                                      ModuleShadowNames *shadow)
{
    if (node == NULL)
        return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_NAMESPACE_DECL:
        case AST_IMPORT_DECL:
        case AST_ENUM_DECL:
            return;

        case AST_IDENTIFIER:
            if (node->data.identifier.name != NULL
                && !module_shadow_contains(shadow, node->data.identifier.name)) {
                const char *replacement =
                    module_rename_scope_lookup(scope, node->data.identifier.name);
                if (replacement != NULL
                    && strcmp(node->data.identifier.name, replacement) != 0) {
                    free(node->data.identifier.name);
                    node->data.identifier.name = pergyra_strdup(replacement);
                }
            }
            return;

        case AST_TYPE:
            normalize_type_node(node, scope, shadow);
            return;

        case AST_LET_DECL:
            module_normalizer_normalize_node_refs(node->data.let_decl.type, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.let_decl.initializer, scope, shadow);
            return;

        case AST_FUNC_DECL: {
            normalize_generic_params(node->data.func_decl.generic_params, scope, shadow);
            size_t saved = shadow->count;
            if (node->data.func_decl.generic_params != NULL) {
                for (size_t i = 0; i < node->data.func_decl.generic_params->count; i++) {
                    GenericParam *gp = node->data.func_decl.generic_params->params[i];
                    if (gp != NULL && gp->name != NULL)
                        module_shadow_push(shadow, gp->name);
                }
            }
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                FuncParam *param = node->data.func_decl.params[i];
                if (param == NULL)
                    continue;
                module_normalizer_normalize_node_refs(param->type, scope, shadow);
                module_normalizer_normalize_node_refs(param->default_value, scope, shadow);
                module_shadow_push(shadow, param->name);
            }
            module_normalizer_normalize_node_refs(node->data.func_decl.return_type, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.func_decl.body, scope, shadow);
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_CLASS_DECL:
            normalize_generic_params(node->data.class_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                if (node->data.class_decl.fields[i] != NULL) {
                    module_normalizer_normalize_node_refs(
                        node->data.class_decl.fields[i]->type, scope, shadow);
                }
            }
            for (size_t i = 0; i < node->data.class_decl.method_count; i++)
                module_normalizer_normalize_node_refs(node->data.class_decl.methods[i], scope, shadow);
            return;

        case AST_ABILITY_DECL:
            normalize_generic_params(node->data.ability_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++)
                module_normalizer_normalize_node_refs(node->data.ability_decl.require_fields[i], scope, shadow);
            for (size_t i = 0; i < ast_ability_method_count(node); i++)
                module_normalizer_normalize_node_refs(ast_ability_method(node, i), scope, shadow);
            return;

        case AST_ROLE_DECL:
            module_normalizer_normalize_node_refs(ast_role_for_type(node), scope, shadow);
            normalize_generic_params(node->data.role_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < ast_role_include_count(node); i++)
                module_normalizer_normalize_node_refs(ast_role_include(node, i), scope, shadow);
            for (size_t i = 0; i < ast_role_impl_count(node); i++)
                module_normalizer_normalize_node_refs(ast_role_impl(node, i), scope, shadow);
            module_normalizer_normalize_node_refs(node->data.role_decl.parallel_block, scope, shadow);
            return;

        case AST_PARTY_DECL:
            module_normalizer_normalize_node_refs(node->data.party_decl.extends, scope, shadow);
            normalize_generic_params(node->data.party_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < ast_party_role_count(node); i++)
                module_normalizer_normalize_node_refs(ast_party_role(node, i), scope, shadow);
            for (size_t i = 0; i < ast_party_shared_count(node); i++)
                module_normalizer_normalize_node_refs(ast_party_shared(node, i), scope, shadow);
            for (size_t i = 0; i < ast_party_method_count(node); i++)
                module_normalizer_normalize_node_refs(ast_party_method(node, i), scope, shadow);
            return;

        case AST_ROSTER_DECL:
            normalize_generic_params(node->data.roster_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < ast_roster_party_count(node); i++)
                module_normalizer_normalize_node_refs(ast_roster_party(node, i), scope, shadow);
            for (size_t i = 0; i < ast_roster_shared_count(node); i++)
                module_normalizer_normalize_node_refs(ast_roster_shared(node, i), scope, shadow);
            for (size_t i = 0; i < ast_roster_method_count(node); i++)
                module_normalizer_normalize_node_refs(ast_roster_method(node, i), scope, shadow);
            return;

        case AST_WORLD_DECL:
            for (size_t i = 0; i < node->data.world_decl.roster_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.rosters[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.zone_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.zones[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.activate_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.activations[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.deactivations[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.maintained_zones[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.state_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.states[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.method_count; i++)
                module_normalizer_normalize_node_refs(node->data.world_decl.methods[i], scope, shadow);
            return;

        case AST_RELATION_DECL:
            for (size_t i = 0; i < node->data.relation_decl.slot_count; i++)
                module_normalizer_normalize_node_refs(node->data.relation_decl.slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.relation_decl.shared_count; i++)
                module_normalizer_normalize_node_refs(node->data.relation_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.relation_decl.method_count; i++)
                module_normalizer_normalize_node_refs(node->data.relation_decl.methods[i], scope, shadow);
            return;

        case AST_EFFECT_DECL:
            for (size_t i = 0; i < node->data.effect_decl.slot_count; i++)
                module_normalizer_normalize_node_refs(node->data.effect_decl.slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.effect_decl.shared_count; i++)
                module_normalizer_normalize_node_refs(node->data.effect_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.effect_decl.method_count; i++)
                module_normalizer_normalize_node_refs(node->data.effect_decl.methods[i], scope, shadow);
            return;

        case AST_ZONE_DECL:
            for (size_t i = 0; i < node->data.zone_decl.slot_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.layer_slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.apply_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.applies[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.link_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.links[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.detach_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.detaches[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.unlinks[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.refreshes[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.maintained_effects[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.maintained_relations[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.maintained_states[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.authority_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.authorities[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.state_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.states[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.shared_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.method_count; i++)
                module_normalizer_normalize_node_refs(node->data.zone_decl.methods[i], scope, shadow);
            return;

        case AST_DOMAIN_SLOT:
            module_normalizer_normalize_node_refs(node->data.domain_slot.type, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.domain_slot.initializer, scope, shadow);
            return;

        case AST_WORLD_ZONE:
        case AST_WORLD_ACTIVATE:
        case AST_WORLD_DEACTIVATE:
        case AST_WORLD_MAINTAIN:
        case AST_WORLD_STATE:
        case AST_ZONE_LAYER_SLOT:
        case AST_ZONE_APPLY:
        case AST_ZONE_LINK:
        case AST_ZONE_DETACH:
        case AST_ZONE_UNLINK:
        case AST_ZONE_REFRESH:
        case AST_ZONE_MAINTAIN_EFFECT:
        case AST_ZONE_MAINTAIN_RELATION:
        case AST_ZONE_MAINTAIN_STATE:
        case AST_ZONE_AUTHORITY:
        case AST_ZONE_STATE:
            return;

        case AST_EVENT_DECL:
            for (size_t i = 0; i < node->data.event_decl.param_count; i++)
                module_normalizer_normalize_node_refs(node->data.event_decl.params[i], scope, shadow);
            module_normalizer_normalize_node_refs(node->data.event_decl.return_type, scope, shadow);
            return;

        case AST_REQUIRE_FIELD:
            module_normalizer_normalize_node_refs(node->data.require_field.type, scope, shadow);
            return;

        case AST_PARTY_SHARED:
            module_normalizer_normalize_node_refs(node->data.party_shared.type, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.party_shared.initializer, scope, shadow);
            return;

        case AST_SYSTEMIC_SLOT:
            if (node->data.roster_slot.party_type != NULL) {
                const char *replacement =
                    module_rename_scope_lookup(scope, node->data.roster_slot.party_type);
                if (replacement != NULL
                    && strcmp(node->data.roster_slot.party_type, replacement) != 0) {
                    free(node->data.roster_slot.party_type);
                    node->data.roster_slot.party_type = pergyra_strdup(replacement);
                }
            }
            return;

        case AST_WORLD_SYSTEMIC:
            if (node->data.world_roster.roster_type != NULL) {
                const char *replacement =
                    module_rename_scope_lookup(scope, node->data.world_roster.roster_type);
                if (replacement != NULL
                    && strcmp(node->data.world_roster.roster_type, replacement) != 0) {
                    free(node->data.world_roster.roster_type);
                    node->data.world_roster.roster_type = pergyra_strdup(replacement);
                }
            }
            module_normalizer_normalize_node_refs(node->data.world_roster.initializer, scope, shadow);
            return;

        case AST_BLOCK:
        case AST_ASYNC_BLOCK: {
            size_t saved = shadow->count;
            ASTNode **stmts = node->type == AST_BLOCK
                ? node->data.block.statements
                : node->data.async_block.statements;
            size_t count = node->type == AST_BLOCK
                ? node->data.block.count
                : node->data.async_block.statement_count;
            for (size_t i = 0; i < count; i++) {
                ASTNode *stmt = stmts[i];
                module_normalizer_normalize_node_refs(stmt, scope, shadow);
                if (stmt != NULL && stmt->type == AST_LET_DECL)
                    module_shadow_push(shadow, stmt->data.let_decl.name);
                if (stmt != NULL && stmt->type == AST_FOR_LOOP
                    && stmt->data.for_loop.variable != NULL)
                    module_shadow_push(shadow, stmt->data.for_loop.variable);
            }
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WITH_STMT: {
            module_normalizer_normalize_node_refs(node->data.with_stmt.slot_type, scope, shadow);
            size_t saved = shadow->count;
            if (node->data.with_stmt.alias != NULL)
                module_shadow_push(shadow, node->data.with_stmt.alias);
            module_normalizer_normalize_node_refs(node->data.with_stmt.body, scope, shadow);
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_FOR_LOOP: {
            module_normalizer_normalize_node_refs(node->data.for_loop.range_start, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.for_loop.range_end, scope, shadow);
            size_t saved = shadow->count;
            module_shadow_push(shadow, node->data.for_loop.variable);
            module_normalizer_normalize_node_refs(node->data.for_loop.body, scope, shadow);
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WHILE_LOOP:
            module_normalizer_normalize_node_refs(node->data.while_loop.condition, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.while_loop.body, scope, shadow);
            return;

        case AST_IF_STMT:
            module_normalizer_normalize_node_refs(node->data.if_stmt.condition, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.if_stmt.then_branch, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.if_stmt.else_branch, scope, shadow);
            return;

        case AST_RETURN:
            module_normalizer_normalize_node_refs(node->data.return_stmt.value, scope, shadow);
            return;

        case AST_BINARY:
            module_normalizer_normalize_node_refs(node->data.binary.left, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.binary.right, scope, shadow);
            return;

        case AST_UNARY:
            module_normalizer_normalize_node_refs(node->data.unary.operand, scope, shadow);
            return;

        case AST_CALL:
            module_normalizer_normalize_node_refs(node->data.call.callee, scope, shadow);
            normalize_call_args(node->data.call.arguments, node->data.call.arg_count, scope, shadow);
            return;

        case AST_MEMBER_ACCESS:
            module_normalizer_normalize_node_refs(node->data.member.object, scope, shadow);
            return;

        case AST_ARRAY_ACCESS:
            module_normalizer_normalize_node_refs(node->data.array_access.array, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.array_access.index, scope, shadow);
            return;

        case AST_ARRAY_LITERAL:
            normalize_call_args(node->data.array_literal.elements, node->data.array_literal.count, scope, shadow);
            return;

        case AST_ASSIGNMENT:
            module_normalizer_normalize_node_refs(node->data.assignment.target, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.assignment.value, scope, shadow);
            return;

        case AST_AWAIT_EXPR:
            module_normalizer_normalize_node_refs(node->data.await_expr.expression, scope, shadow);
            return;

        case AST_CHANNEL_SEND:
            module_normalizer_normalize_node_refs(node->data.channel_send.channel, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.channel_send.value, scope, shadow);
            return;

        case AST_CHANNEL_RECV:
            module_normalizer_normalize_node_refs(node->data.channel_recv.channel, scope, shadow);
            return;

        case AST_SELECT_STMT:
            normalize_call_args(node->data.select_stmt.cases, node->data.select_stmt.case_count, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.select_stmt.default_case, scope, shadow);
            return;

        case AST_MATCH_STMT:
            module_normalizer_normalize_node_refs(node->data.match_stmt.subject, scope, shadow);
            normalize_call_args(node->data.match_stmt.cases, node->data.match_stmt.case_count, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.match_stmt.default_body, scope, shadow);
            return;

        case AST_MATCH_CASE:
            module_normalizer_normalize_node_refs(node->data.match_case.pattern, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.match_case.guard, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.match_case.body, scope, shadow);
            return;

        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
            module_normalizer_normalize_node_refs(node->data.event_op.event, scope, shadow);
            module_normalizer_normalize_node_refs(node->data.event_op.handler, scope, shadow);
            return;

        case AST_EVENT_INVOKE:
            module_normalizer_normalize_node_refs(node->data.event_invoke.event, scope, shadow);
            normalize_call_args(node->data.event_invoke.arguments, node->data.event_invoke.arg_count, scope, shadow);
            return;

        case AST_UNSAFE_BLOCK:
            module_normalizer_normalize_node_refs(node->data.unsafe_block.body, scope, shadow);
            return;

        case AST_DEFER_STMT:
            module_normalizer_normalize_node_refs(node->data.defer_stmt.body, scope, shadow);
            return;

        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_BREAK:
        case AST_CONTINUE:
            return;

        default:
            return;
    }
}

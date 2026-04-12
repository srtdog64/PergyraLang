/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "module_normalizer.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

typedef struct
{
    ASTNode **items;
    size_t    count;
    size_t    capacity;
} ASTVec;

typedef struct
{
    char *old_name;
    char *new_name;
} RenameEntry;

typedef struct RenameScope
{
    struct RenameScope *parent;
    RenameEntry        *entries;
    size_t              count;
    size_t              capacity;
} RenameScope;

typedef struct
{
    char  **names;
    size_t  count;
    size_t  capacity;
} ShadowNames;

static bool
astvec_push(ASTVec *vec, ASTNode *node)
{
    if (vec->count == vec->capacity) {
        size_t next = vec->capacity == 0 ? 8 : vec->capacity * 2;
        ASTNode **grown = realloc(vec->items, next * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        vec->items = grown;
        vec->capacity = next;
    }
    vec->items[vec->count++] = node;
    return true;
}

static char *
join_names(const char *a, const char *b)
{
    size_t alen = a != NULL ? strlen(a) : 0;
    size_t blen = b != NULL ? strlen(b) : 0;
    char *result = malloc(alen + blen + 1);
    if (result == NULL)
        return NULL;
    if (alen > 0)
        memcpy(result, a, alen);
    if (blen > 0)
        memcpy(result + alen, b, blen);
    result[alen + blen] = '\0';
    return result;
}

static char *
namespace_prefix_join(const char *prefix, const char *name)
{
    size_t plen = prefix != NULL ? strlen(prefix) : 0;
    size_t nlen = strlen(name);
    char *result = malloc(plen + nlen + 2);
    if (result == NULL)
        return NULL;
    if (plen > 0)
        memcpy(result, prefix, plen);
    memcpy(result + plen, name, nlen);
    result[plen + nlen] = '_';
    result[plen + nlen + 1] = '\0';
    return result;
}

static bool
rename_scope_add(RenameScope *scope, const char *old_name, const char *new_name)
{
    if (scope->count == scope->capacity) {
        size_t next = scope->capacity == 0 ? 8 : scope->capacity * 2;
        RenameEntry *grown = realloc(scope->entries, next * sizeof(RenameEntry));
        if (grown == NULL)
            return false;
        scope->entries = grown;
        scope->capacity = next;
    }
    scope->entries[scope->count].old_name = pergyra_strdup(old_name);
    scope->entries[scope->count].new_name = pergyra_strdup(new_name);
    if (scope->entries[scope->count].old_name == NULL
        || scope->entries[scope->count].new_name == NULL) {
        free(scope->entries[scope->count].old_name);
        free(scope->entries[scope->count].new_name);
        return false;
    }
    scope->count++;
    return true;
}

static const char *
rename_scope_lookup(const RenameScope *scope, const char *name)
{
    for (const RenameScope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->count; i++) {
            if (strcmp(s->entries[i].old_name, name) == 0)
                return s->entries[i].new_name;
        }
    }
    return NULL;
}

static void
rename_scope_destroy(RenameScope *scope)
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

static bool
shadow_push(ShadowNames *shadow, const char *name)
{
    if (name == NULL)
        return true;
    if (shadow->count == shadow->capacity) {
        size_t next = shadow->capacity == 0 ? 8 : shadow->capacity * 2;
        char **grown = realloc(shadow->names, next * sizeof(char *));
        if (grown == NULL)
            return false;
        shadow->names = grown;
        shadow->capacity = next;
    }
    shadow->names[shadow->count++] = pergyra_strdup(name);
    return shadow->names[shadow->count - 1] != NULL;
}

static bool
shadow_contains(const ShadowNames *shadow, const char *name)
{
    for (size_t i = shadow->count; i > 0; i--) {
        if (strcmp(shadow->names[i - 1], name) == 0)
            return true;
    }
    return false;
}

static void
shadow_pop_to(ShadowNames *shadow, size_t saved_count)
{
    while (shadow->count > saved_count) {
        free(shadow->names[shadow->count - 1]);
        shadow->count--;
    }
}

static void
shadow_destroy(ShadowNames *shadow)
{
    shadow_pop_to(shadow, 0);
    free(shadow->names);
    shadow->names = NULL;
    shadow->capacity = 0;
}

static char **
node_name_slot(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_FUNC_DECL: return &node->data.func_decl.name;
        case AST_CLASS_DECL: return &node->data.class_decl.name;
        case AST_LET_DECL: return &node->data.let_decl.name;
        case AST_ABILITY_DECL: return &node->data.ability_decl.name;
        case AST_ROLE_DECL: return &node->data.role_decl.name;
        case AST_PARTY_DECL: return &node->data.party_decl.name;
        case AST_ROSTER_DECL: return &node->data.roster_decl.name;
        case AST_WORLD_DECL: return &node->data.world_decl.name;
        case AST_RELATION_DECL: return &node->data.relation_decl.name;
        case AST_EFFECT_DECL: return &node->data.effect_decl.name;
        case AST_ZONE_DECL: return &node->data.zone_decl.name;
        case AST_EVENT_DECL: return &node->data.event_decl.name;
        case AST_ENUM_DECL: return &node->data.enum_decl.name;
        default: return NULL;
    }
}

static bool
module_has_explicit_exports_in_stmt(ASTNode *node)
{
    if (node == NULL)
        return false;
    if (node->has_explicit_export)
        return true;
    if (node->type == AST_NAMESPACE_DECL) {
        for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
            if (module_has_explicit_exports_in_stmt(node->data.namespace_decl.statements[i]))
                return true;
        }
    }
    return false;
}

static bool
module_has_explicit_exports(ASTNode *program)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;
    for (size_t i = 0; i < program->data.program.count; i++) {
        if (module_has_explicit_exports_in_stmt(program->data.program.statements[i]))
            return true;
    }
    return false;
}

static void normalize_node_refs(ASTNode *node, RenameScope *scope, ShadowNames *shadow);

static void
normalize_generic_params(GenericParams *params, RenameScope *scope, ShadowNames *shadow)
{
    if (params == NULL)
        return;
    size_t saved = shadow->count;
    for (size_t i = 0; i < params->count; i++) {
        if (params->params[i] != NULL && params->params[i]->name != NULL)
            shadow_push(shadow, params->params[i]->name);
    }
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (param == NULL)
            continue;
        normalize_node_refs(param->constraint, scope, shadow);
        normalize_node_refs(param->default_type, scope, shadow);
    }
    shadow_pop_to(shadow, saved);
}

static void
normalize_type_node(ASTNode *node, RenameScope *scope, ShadowNames *shadow)
{
    if (node == NULL || node->type != AST_TYPE)
        return;

    if (node->data.type.name != NULL && !shadow_contains(shadow, node->data.type.name)) {
        const char *replacement = rename_scope_lookup(scope, node->data.type.name);
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
            normalize_node_refs(arg->constraint, scope, shadow);
            normalize_node_refs(arg->default_type, scope, shadow);
        }
    }
}

static void
normalize_call_args(ASTNode **args, size_t count, RenameScope *scope, ShadowNames *shadow)
{
    for (size_t i = 0; i < count; i++)
        normalize_node_refs(args[i], scope, shadow);
}

static void
normalize_node_refs(ASTNode *node, RenameScope *scope, ShadowNames *shadow)
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
                && !shadow_contains(shadow, node->data.identifier.name)) {
                const char *replacement = rename_scope_lookup(scope, node->data.identifier.name);
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
            normalize_node_refs(node->data.let_decl.type, scope, shadow);
            normalize_node_refs(node->data.let_decl.initializer, scope, shadow);
            return;

        case AST_FUNC_DECL: {
            normalize_generic_params(node->data.func_decl.generic_params, scope, shadow);
            size_t saved = shadow->count;
            if (node->data.func_decl.generic_params != NULL) {
                for (size_t i = 0; i < node->data.func_decl.generic_params->count; i++) {
                    GenericParam *gp = node->data.func_decl.generic_params->params[i];
                    if (gp != NULL && gp->name != NULL)
                        shadow_push(shadow, gp->name);
                }
            }
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                FuncParam *param = node->data.func_decl.params[i];
                if (param == NULL)
                    continue;
                normalize_node_refs(param->type, scope, shadow);
                normalize_node_refs(param->default_value, scope, shadow);
                shadow_push(shadow, param->name);
            }
            normalize_node_refs(node->data.func_decl.return_type, scope, shadow);
            normalize_node_refs(node->data.func_decl.body, scope, shadow);
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_CLASS_DECL:
            normalize_generic_params(node->data.class_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                if (node->data.class_decl.fields[i] != NULL)
                    normalize_node_refs(node->data.class_decl.fields[i]->type, scope, shadow);
            }
            for (size_t i = 0; i < node->data.class_decl.method_count; i++)
                normalize_node_refs(node->data.class_decl.methods[i], scope, shadow);
            return;

        case AST_ABILITY_DECL:
            normalize_generic_params(node->data.ability_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++)
                normalize_node_refs(node->data.ability_decl.require_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.ability_decl.method_count; i++)
                normalize_node_refs(node->data.ability_decl.methods[i], scope, shadow);
            return;

        case AST_ROLE_DECL:
            normalize_node_refs(node->data.role_decl.for_type, scope, shadow);
            normalize_generic_params(node->data.role_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.role_decl.include_count; i++)
                normalize_node_refs(node->data.role_decl.includes[i], scope, shadow);
            for (size_t i = 0; i < node->data.role_decl.impl_count; i++)
                normalize_node_refs(node->data.role_decl.impl_abilities[i], scope, shadow);
            normalize_node_refs(node->data.role_decl.parallel_block, scope, shadow);
            return;

        case AST_PARTY_DECL:
            normalize_node_refs(node->data.party_decl.extends, scope, shadow);
            normalize_generic_params(node->data.party_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.party_decl.role_count; i++)
                normalize_node_refs(node->data.party_decl.role_slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.party_decl.shared_count; i++)
                normalize_node_refs(node->data.party_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.party_decl.method_count; i++)
                normalize_node_refs(node->data.party_decl.methods[i], scope, shadow);
            return;

        case AST_ROSTER_DECL:
            normalize_generic_params(node->data.roster_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.roster_decl.party_count; i++)
                normalize_node_refs(node->data.roster_decl.party_slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.roster_decl.shared_count; i++)
                normalize_node_refs(node->data.roster_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.roster_decl.method_count; i++)
                normalize_node_refs(node->data.roster_decl.methods[i], scope, shadow);
            return;

        case AST_WORLD_DECL:
            for (size_t i = 0; i < node->data.world_decl.roster_count; i++)
                normalize_node_refs(node->data.world_decl.rosters[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.zone_count; i++)
                normalize_node_refs(node->data.world_decl.zones[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.activate_count; i++)
                normalize_node_refs(node->data.world_decl.activations[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++)
                normalize_node_refs(node->data.world_decl.deactivations[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++)
                normalize_node_refs(node->data.world_decl.maintained_zones[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.state_count; i++)
                normalize_node_refs(node->data.world_decl.states[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++)
                normalize_node_refs(node->data.world_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.method_count; i++)
                normalize_node_refs(node->data.world_decl.methods[i], scope, shadow);
            return;

        case AST_RELATION_DECL:
            for (size_t i = 0; i < node->data.relation_decl.slot_count; i++)
                normalize_node_refs(node->data.relation_decl.slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.relation_decl.shared_count; i++)
                normalize_node_refs(node->data.relation_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.relation_decl.method_count; i++)
                normalize_node_refs(node->data.relation_decl.methods[i], scope, shadow);
            return;

        case AST_EFFECT_DECL:
            for (size_t i = 0; i < node->data.effect_decl.slot_count; i++)
                normalize_node_refs(node->data.effect_decl.slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.effect_decl.shared_count; i++)
                normalize_node_refs(node->data.effect_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.effect_decl.method_count; i++)
                normalize_node_refs(node->data.effect_decl.methods[i], scope, shadow);
            return;

        case AST_ZONE_DECL:
            for (size_t i = 0; i < node->data.zone_decl.slot_count; i++)
                normalize_node_refs(node->data.zone_decl.slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++)
                normalize_node_refs(node->data.zone_decl.layer_slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.apply_count; i++)
                normalize_node_refs(node->data.zone_decl.applies[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.link_count; i++)
                normalize_node_refs(node->data.zone_decl.links[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.detach_count; i++)
                normalize_node_refs(node->data.zone_decl.detaches[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++)
                normalize_node_refs(node->data.zone_decl.unlinks[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++)
                normalize_node_refs(node->data.zone_decl.refreshes[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++)
                normalize_node_refs(node->data.zone_decl.maintained_effects[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++)
                normalize_node_refs(node->data.zone_decl.maintained_relations[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++)
                normalize_node_refs(node->data.zone_decl.maintained_states[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.authority_count; i++)
                normalize_node_refs(node->data.zone_decl.authorities[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.state_count; i++)
                normalize_node_refs(node->data.zone_decl.states[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.shared_count; i++)
                normalize_node_refs(node->data.zone_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.zone_decl.method_count; i++)
                normalize_node_refs(node->data.zone_decl.methods[i], scope, shadow);
            return;

        case AST_DOMAIN_SLOT:
            normalize_node_refs(node->data.domain_slot.type, scope, shadow);
            normalize_node_refs(node->data.domain_slot.initializer, scope, shadow);
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
                normalize_node_refs(node->data.event_decl.params[i], scope, shadow);
            normalize_node_refs(node->data.event_decl.return_type, scope, shadow);
            return;

        case AST_REQUIRE_FIELD:
            normalize_node_refs(node->data.require_field.type, scope, shadow);
            return;

        case AST_PARTY_SHARED:
            normalize_node_refs(node->data.party_shared.type, scope, shadow);
            normalize_node_refs(node->data.party_shared.initializer, scope, shadow);
            return;

        case AST_SYSTEMIC_SLOT:
            if (node->data.roster_slot.party_type != NULL) {
                const char *replacement = rename_scope_lookup(scope, node->data.roster_slot.party_type);
                if (replacement != NULL
                    && strcmp(node->data.roster_slot.party_type, replacement) != 0) {
                    free(node->data.roster_slot.party_type);
                    node->data.roster_slot.party_type = pergyra_strdup(replacement);
                }
            }
            return;

        case AST_WORLD_SYSTEMIC:
            if (node->data.world_roster.roster_type != NULL) {
                const char *replacement = rename_scope_lookup(scope, node->data.world_roster.roster_type);
                if (replacement != NULL
                    && strcmp(node->data.world_roster.roster_type, replacement) != 0) {
                    free(node->data.world_roster.roster_type);
                    node->data.world_roster.roster_type = pergyra_strdup(replacement);
                }
            }
            normalize_node_refs(node->data.world_roster.initializer, scope, shadow);
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
                normalize_node_refs(stmt, scope, shadow);
                if (stmt != NULL && stmt->type == AST_LET_DECL)
                    shadow_push(shadow, stmt->data.let_decl.name);
                if (stmt != NULL && stmt->type == AST_FOR_LOOP
                    && stmt->data.for_loop.variable != NULL)
                    shadow_push(shadow, stmt->data.for_loop.variable);
            }
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WITH_STMT: {
            normalize_node_refs(node->data.with_stmt.slot_type, scope, shadow);
            size_t saved = shadow->count;
            if (node->data.with_stmt.alias != NULL)
                shadow_push(shadow, node->data.with_stmt.alias);
            normalize_node_refs(node->data.with_stmt.body, scope, shadow);
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_FOR_LOOP: {
            normalize_node_refs(node->data.for_loop.range_start, scope, shadow);
            normalize_node_refs(node->data.for_loop.range_end, scope, shadow);
            size_t saved = shadow->count;
            shadow_push(shadow, node->data.for_loop.variable);
            normalize_node_refs(node->data.for_loop.body, scope, shadow);
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WHILE_LOOP:
            normalize_node_refs(node->data.while_loop.condition, scope, shadow);
            normalize_node_refs(node->data.while_loop.body, scope, shadow);
            return;

        case AST_IF_STMT:
            normalize_node_refs(node->data.if_stmt.condition, scope, shadow);
            normalize_node_refs(node->data.if_stmt.then_branch, scope, shadow);
            normalize_node_refs(node->data.if_stmt.else_branch, scope, shadow);
            return;

        case AST_RETURN:
            normalize_node_refs(node->data.return_stmt.value, scope, shadow);
            return;

        case AST_BINARY:
            normalize_node_refs(node->data.binary.left, scope, shadow);
            normalize_node_refs(node->data.binary.right, scope, shadow);
            return;

        case AST_UNARY:
            normalize_node_refs(node->data.unary.operand, scope, shadow);
            return;

        case AST_CALL:
            normalize_node_refs(node->data.call.callee, scope, shadow);
            normalize_call_args(node->data.call.arguments, node->data.call.arg_count, scope, shadow);
            return;

        case AST_MEMBER_ACCESS:
            normalize_node_refs(node->data.member.object, scope, shadow);
            return;

        case AST_ARRAY_ACCESS:
            normalize_node_refs(node->data.array_access.array, scope, shadow);
            normalize_node_refs(node->data.array_access.index, scope, shadow);
            return;

        case AST_ARRAY_LITERAL:
            normalize_call_args(node->data.array_literal.elements, node->data.array_literal.count, scope, shadow);
            return;

        case AST_ASSIGNMENT:
            normalize_node_refs(node->data.assignment.target, scope, shadow);
            normalize_node_refs(node->data.assignment.value, scope, shadow);
            return;

        case AST_AWAIT_EXPR:
            normalize_node_refs(node->data.await_expr.expression, scope, shadow);
            return;

        case AST_CHANNEL_SEND:
            normalize_node_refs(node->data.channel_send.channel, scope, shadow);
            normalize_node_refs(node->data.channel_send.value, scope, shadow);
            return;

        case AST_CHANNEL_RECV:
            normalize_node_refs(node->data.channel_recv.channel, scope, shadow);
            return;

        case AST_SELECT_STMT:
            normalize_call_args(node->data.select_stmt.cases, node->data.select_stmt.case_count, scope, shadow);
            normalize_node_refs(node->data.select_stmt.default_case, scope, shadow);
            return;

        case AST_MATCH_STMT:
            normalize_node_refs(node->data.match_stmt.subject, scope, shadow);
            normalize_call_args(node->data.match_stmt.cases, node->data.match_stmt.case_count, scope, shadow);
            normalize_node_refs(node->data.match_stmt.default_body, scope, shadow);
            return;

        case AST_MATCH_CASE:
            normalize_node_refs(node->data.match_case.pattern, scope, shadow);
            normalize_node_refs(node->data.match_case.guard, scope, shadow);
            normalize_node_refs(node->data.match_case.body, scope, shadow);
            return;

        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
            normalize_node_refs(node->data.event_op.event, scope, shadow);
            normalize_node_refs(node->data.event_op.handler, scope, shadow);
            return;

        case AST_EVENT_INVOKE:
            normalize_node_refs(node->data.event_invoke.event, scope, shadow);
            normalize_call_args(node->data.event_invoke.arguments, node->data.event_invoke.arg_count, scope, shadow);
            return;

        case AST_UNSAFE_BLOCK:
            normalize_node_refs(node->data.unsafe_block.body, scope, shadow);
            return;

        case AST_DEFER_STMT:
            normalize_node_refs(node->data.defer_stmt.body, scope, shadow);
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

static void
free_namespace_shell(ASTNode *node)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return;
    free(node->data.namespace_decl.name);
    free(node->data.namespace_decl.statements);
    node->data.namespace_decl.name = NULL;
    node->data.namespace_decl.statements = NULL;
    node->data.namespace_decl.count = 0;
    free(node);
}

static bool
normalize_statement_list(ASTNode **statements, size_t count,
                         const char *public_prefix,
                         const char *private_prefix,
                         bool imported,
                         bool has_explicit_exports,
                         bool inherited_export,
                         RenameScope *parent_scope,
                         ASTVec *flat)
{
    RenameScope scope = { .parent = parent_scope };
    ShadowNames shadow = {0};

    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        char **name_slot;
        if (stmt == NULL || stmt->type == AST_NAMESPACE_DECL)
            continue;
        name_slot = node_name_slot(stmt);
        if (name_slot == NULL || *name_slot == NULL)
            continue;

        char *public_name = join_names(public_prefix, *name_slot);
        bool visible = !imported || !has_explicit_exports
            || inherited_export || stmt->is_exported;
        bool explicit_private =
            stmt->has_explicit_access
            && (stmt->access == ACCESS_PRIVATE || stmt->access == ACCESS_PROTECTED);
        char *final_name = visible
            ? pergyra_strdup(public_name)
            : join_names(private_prefix, public_name);
        if (public_name == NULL || final_name == NULL) {
            free(public_name);
            free(final_name);
            rename_scope_destroy(&scope);
            shadow_destroy(&shadow);
            return false;
        }

        if (strcmp(*name_slot, final_name) != 0) {
            if (!rename_scope_add(&scope, *name_slot, final_name)) {
                free(public_name);
                free(final_name);
                rename_scope_destroy(&scope);
                shadow_destroy(&shadow);
                return false;
            }
            free(*name_slot);
            *name_slot = pergyra_strdup(final_name);
        }

        free(public_name);
        free(final_name);

        if (imported) {
            stmt->is_exported = visible && !explicit_private;
        }
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_NAMESPACE_DECL) {
            char *child_prefix = namespace_prefix_join(public_prefix, stmt->data.namespace_decl.name);
            bool child_export = inherited_export || stmt->is_exported;
            if (child_prefix == NULL
                || !normalize_statement_list(stmt->data.namespace_decl.statements,
                                             stmt->data.namespace_decl.count,
                                             child_prefix,
                                             private_prefix,
                                             imported,
                                             has_explicit_exports,
                                             child_export,
                                             &scope,
                                             flat)) {
                free(child_prefix);
                rename_scope_destroy(&scope);
                shadow_destroy(&shadow);
                return false;
            }
            free(child_prefix);
            free_namespace_shell(stmt);
            continue;
        }

        normalize_node_refs(stmt, &scope, &shadow);
        if (!astvec_push(flat, stmt)) {
            rename_scope_destroy(&scope);
            shadow_destroy(&shadow);
            return false;
        }
    }

    rename_scope_destroy(&scope);
    shadow_destroy(&shadow);
    return true;
}

bool
module_normalize_ast(ASTNode *program, bool imported, const char *private_prefix)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ASTVec flat = {0};
    bool has_explicit = imported && module_has_explicit_exports(program);
    bool ok = normalize_statement_list(program->data.program.statements,
                                       program->data.program.count,
                                       "",
                                       private_prefix != NULL ? private_prefix : "",
                                       imported,
                                       has_explicit,
                                       false,
                                       NULL,
                                       &flat);
    if (!ok) {
        free(flat.items);
        return false;
    }

    free(program->data.program.statements);
    program->data.program.statements = flat.items;
    program->data.program.count = flat.count;
    return true;
}

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"

static char *
resolution_participant_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void
semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                    SemanticContext *ctx)
{
    const char *party_name;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t shared_count;
    size_t role_count;
    size_t method_count;

    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL || ctx == NULL)
        return;
    party_name = ast_party_name(party_decl);
    shared_fields = ast_party_shared_fields(party_decl, &shared_count);
    role_count = ast_party_role_count(party_decl);
    methods = ast_party_methods(party_decl, &method_count);

    semantic_type_resolution_collect_generic_contract_inventory(
        ast_party_generic_params(party_decl),
        NULL,
        ctx,
        party_decl,
        "party",
        party_name);

    semantic_type_resolution_collect_type_refs(
        ast_party_extends(party_decl),
        ctx,
        party_decl,
        party_name != NULL ? party_name : "<party>",
        "party extends lookup");

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            ast_party_shared_type(field),
            ctx,
            field,
            ast_party_shared_name(field) != NULL
                ? ast_party_shared_name(field) : "<party-shared>",
            "party shared field type lookup");
    }

    for (size_t i = 0; i < role_count; i++) {
        ASTNode *role_slot = ast_party_role(party_decl, i);
        const char *slot_name = ast_role_slot_name(role_slot);
        size_t ability_count = ast_role_slot_required_ability_count(role_slot);
        ASTNode **abilities =
            ast_role_slot_required_abilities(role_slot, NULL);
        char *consumer_name;

        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;

        consumer_name = resolution_participant_strdup_fmt(
            "party %s.%s",
            party_name != NULL ? party_name : "<party>",
            slot_name != NULL ? slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            abilities,
            ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            party_name);
    }
}

void
semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                     SemanticContext *ctx)
{
    const char *roster_name;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t shared_count;
    size_t party_count;
    size_t method_count;

    if (roster_decl == NULL || roster_decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;
    roster_name = ast_roster_name(roster_decl);
    shared_fields = ast_roster_shared_fields(roster_decl, &shared_count);
    party_count = ast_roster_party_count(roster_decl);
    methods = ast_roster_methods(roster_decl, &method_count);

    semantic_type_resolution_collect_generic_contract_inventory(
        ast_roster_generic_params(roster_decl),
        NULL,
        ctx,
        roster_decl,
        "roster",
        roster_name);

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            ast_party_shared_type(field),
            ctx,
            field,
            ast_party_shared_name(field) != NULL
                ? ast_party_shared_name(field) : "<roster-shared>",
            "roster shared field type lookup");
    }

    for (size_t i = 0; i < party_count; i++) {
        ASTNode *slot = ast_roster_party(roster_decl, i);
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            ast_roster_slot_name(slot) != NULL
                ? ast_roster_slot_name(slot) : "<roster-slot>",
            ast_roster_slot_party_type(slot),
            "roster party lookup");
    }

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            roster_name);
    }
}

void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx)
{
    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ast_role_generic_params(role_decl),
        ast_role_where_clause(role_decl),
        ctx,
        role_decl,
        "role",
        ast_role_name(role_decl));

    semantic_type_resolution_collect_type_refs(
        semantic_role_for_type_node(role_decl),
        ctx,
        role_decl,
        ast_role_name(role_decl) != NULL
            ? ast_role_name(role_decl) : "<role>",
        "role host-type lookup");

    for (size_t i = 0; i < ast_role_include_count(role_decl); i++) {
        ASTNode *inc = ast_role_include(role_decl, i);
        const char *role_name = ast_include_role_name(inc);
        GenericParams *type_args = ast_include_type_args(inc);
        char *consumer_name;

        if (role_name == NULL)
            continue;

        consumer_name = resolution_participant_strdup_fmt(
            "role %s.include",
            ast_role_name(role_decl) != NULL
                ? ast_role_name(role_decl) : "<role>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_record_string_dependency(
            ctx,
            inc,
            consumer_name,
            role_name,
            "role include lookup");

        if (type_args != NULL) {
            size_t type_arg_count = ast_generic_param_count(type_args);
            for (size_t j = 0; j < type_arg_count; j++) {
                GenericParam *arg = ast_generic_param_at(type_args, j);
                ASTNode *constraint = ast_generic_param_constraint(arg);
                if (constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        constraint,
                        ctx,
                        inc,
                        consumer_name,
                        "role include type-argument lookup");
                }
            }
        }
        free(consumer_name);
    }

    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        semantic_type_resolution_collect_type_refs(
            ast_impl_ability_ref(impl),
            ctx,
            impl,
            ast_role_name(role_decl) != NULL
                ? ast_role_name(role_decl) : "<role>",
            "role impl ability lookup");
    }
}

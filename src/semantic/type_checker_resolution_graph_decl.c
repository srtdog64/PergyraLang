#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"

static char *
resolution_decl_strdup_fmt(const char *fmt, ...)
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

        consumer_name = resolution_decl_strdup_fmt(
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

        consumer_name = resolution_decl_strdup_fmt(
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
            for (size_t j = 0; j < type_args->count; j++) {
                GenericParam *arg = type_args->params[j];
                if (arg != NULL && arg->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
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

void
semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                    SemanticContext *ctx)
{
    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ast_class_generic_params(class_decl),
        ast_class_where_clause(class_decl),
        ctx,
        class_decl,
        "class",
        ast_class_name(class_decl));

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(class_decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        char *consumer_name;

        if (field == NULL)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "class %s.%s",
            ast_class_name(class_decl) != NULL
                ? ast_class_name(class_decl) : "<class>",
            field->name != NULL ? field->name : "<field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                field->type,
                ctx,
                class_decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
    }

    size_t method_count = 0;
    ASTNode **methods = ast_class_methods(class_decl, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        semantic_type_resolution_precollect_action_contract(
            method,
            ctx,
            ast_class_name(class_decl));
    }
}

void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name)
{
    const char *consumer_name;

    if (method == NULL || method->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = ast_declaration_name(method) != NULL
        ? ast_declaration_name(method)
        : (fallback_name != NULL ? fallback_name : "<action>");

    semantic_type_resolution_collect_generic_contract_inventory(
        ast_func_generic_params(method),
        ast_func_where_clause(method),
        ctx,
        method,
        "func",
        consumer_name);

    for (size_t i = 0; i < ast_func_param_count(method); i++) {
        FuncParam *param = ast_func_param(method, i);
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = resolution_decl_strdup_fmt(
            "func %s.%s",
            consumer_name,
            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                param->type,
                ctx,
                method,
                param_consumer_name,
                "function parameter type lookup");
            free(param_consumer_name);
        }
    }

    semantic_type_resolution_collect_type_refs(
        ast_func_return_type(method),
        ctx,
        method,
        consumer_name,
        "function return type lookup");

    semantic_type_resolution_precollect_required_abilities(
        method->data.func_decl.required_abilities,
        method->data.func_decl.required_ability_count,
        ctx,
        method,
        consumer_name,
        "action ability consumer lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.within_zone,
        "action within-zone lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.causes_effect,
        "action causes-effect lookup");

    semantic_type_resolution_precollect_body_type_refs(
        ast_func_body(method),
        ctx,
        method,
        consumer_name);
}

void
semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                      SemanticContext *ctx)
{
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ast_ability_generic_params(ability_decl),
        ast_ability_where_clause(ability_decl),
        ctx,
        ability_decl,
        "ability",
        ast_ability_name(ability_decl));

    for (size_t i = 0; i < ast_ability_require_field_count(ability_decl); i++) {
        ASTNode *req = ast_ability_require_field(ability_decl, i);
        const char *req_name = ast_require_field_name(req);
        ASTNode *req_type = ast_require_field_type(req);
        char *consumer_name;

        if (req_name == NULL || req_type == NULL)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "ability %s.%s",
            ast_ability_name(ability_decl) != NULL
                ? ast_ability_name(ability_decl) : "<ability>",
            req_name);
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                req_type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < ast_ability_method_count(ability_decl); i++) {
        semantic_type_resolution_precollect_action_contract(
            ast_ability_method(ability_decl, i),
            ctx,
            ast_ability_name(ability_decl));
    }
}

void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx)
{
    if (enum_decl == NULL || enum_decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    size_t variant_count = 0;
    char **variants = ast_enum_variants(enum_decl, &variant_count);
    for (size_t i = 0; i < variant_count; i++) {
        size_t param_count = ast_enum_variant_param_count(enum_decl, i);
        const char *variant_name = variants != NULL ? variants[i] : NULL;
        char *consumer_name;

        if (param_count == 0)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "enum %s.%s",
            ast_enum_name(enum_decl) != NULL
                ? ast_enum_name(enum_decl) : "<enum>",
            variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            semantic_type_resolution_collect_type_refs(
                ast_enum_variant_param(enum_decl, i, j),
                ctx,
                enum_decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }

    size_t method_count = 0;
    ASTNode **methods = ast_enum_methods(enum_decl, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods != NULL ? methods[i] : NULL,
            ctx,
            ast_enum_name(enum_decl));
    }
}

void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                    SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < ast_event_param_count(event_decl); i++) {
        ASTNode *param = ast_event_param(event_decl, i);
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "event %s.%s",
            ast_event_name(event_decl) != NULL
                ? ast_event_name(event_decl) : "<event>",
            param->data.let_decl.name != NULL
                ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    semantic_type_resolution_collect_type_refs(
        ast_event_return_type(event_decl),
        ctx,
        event_decl,
        ast_event_name(event_decl) != NULL
            ? ast_event_name(event_decl) : "<event>",
        "event return type lookup");
}

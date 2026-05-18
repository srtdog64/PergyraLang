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
                                                    const char *owner_name_hint)
{
    const char *consumer_name;

    if (method == NULL || method->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = ast_declaration_name(method) != NULL
        ? ast_declaration_name(method)
        : (owner_name_hint != NULL ? owner_name_hint : "<action>");

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
        ast_func_required_abilities(method, NULL),
        ast_func_required_ability_count(method),
        ctx,
        method,
        consumer_name,
        "action ability consumer lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        ast_func_within_zone(method),
        "action within-zone lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        ast_func_causes_effect(method),
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
            ast_let_name(param) != NULL
                ? ast_let_name(param) : "<param>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            ast_let_type(param),
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
stage_nominal_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
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
semantic_stage_class_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        decl->data.class_decl.generic_params,
        decl->data.class_decl.where_clause,
        ctx,
        decl,
        "class",
        decl->data.class_decl.name);
    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        char *consumer_name;
        if (field == NULL)
            continue;
        consumer_name = stage_nominal_strdup_fmt(
            "class %s.%s",
            decl->data.class_decl.name != NULL
                ? decl->data.class_decl.name : "<class>",
            field->name != NULL ? field->name : "<field>");
        if (consumer_name == NULL)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            field->type,
            ctx,
            decl,
            consumer_name,
            "class field type lookup");
        free(consumer_name);
    }
    semantic_stage_method_array(
        decl->data.class_decl.methods,
        decl->data.class_decl.method_count,
        ctx,
        decl->data.class_decl.name);
}

void
semantic_stage_enum_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < decl->data.enum_decl.variant_count; i++) {
        ASTNode **params = decl->data.enum_decl.variant_params != NULL
            ? decl->data.enum_decl.variant_params[i] : NULL;
        size_t param_count = decl->data.enum_decl.variant_param_counts != NULL
            ? decl->data.enum_decl.variant_param_counts[i] : 0;
        const char *variant_name = decl->data.enum_decl.variants != NULL
            ? decl->data.enum_decl.variants[i] : NULL;
        char *consumer_name;

        if (params == NULL || param_count == 0)
            continue;

        consumer_name = stage_nominal_strdup_fmt(
            "enum %s.%s",
            decl->data.enum_decl.name != NULL
                ? decl->data.enum_decl.name : "<enum>",
            variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            (void)semantic_stage_resolve_type_quiet(
                params[j],
                ctx,
                decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }
    semantic_stage_method_array(
        decl->data.enum_decl.methods,
        decl->data.enum_decl.method_count,
        ctx,
        decl->data.enum_decl.name);
}

void
semantic_stage_ability_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        decl->data.ability_decl.generic_params,
        decl->data.ability_decl.where_clause,
        ctx,
        decl,
        "ability",
        decl->data.ability_decl.name);
    for (size_t i = 0; i < decl->data.ability_decl.require_count; i++) {
        ASTNode *req = decl->data.ability_decl.require_fields[i];
        char *consumer_name;
        if (req == NULL || req->type != AST_REQUIRE_FIELD)
            continue;
        consumer_name = stage_nominal_strdup_fmt(
            "ability %s.%s",
            decl->data.ability_decl.name != NULL
                ? decl->data.ability_decl.name : "<ability>",
            req->data.require_field.name != NULL
                ? req->data.require_field.name : "<require-field>");
        if (consumer_name == NULL)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            req->data.require_field.type,
            ctx,
            req,
            consumer_name,
            "ability require-field type lookup");
        free(consumer_name);
    }
    semantic_stage_method_array(
        decl->data.ability_decl.methods,
        decl->data.ability_decl.method_count,
        ctx,
        decl->data.ability_decl.name);
}

void
semantic_stage_role_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        decl->data.role_decl.generic_params,
        decl->data.role_decl.where_clause,
        ctx,
        decl,
        "role",
        decl->data.role_decl.name);
    (void)semantic_stage_resolve_type_quiet(
        decl->data.role_decl.for_type,
        ctx,
        decl,
        decl->data.role_decl.name,
        "role host-type lookup");
    for (size_t i = 0; i < decl->data.role_decl.include_count; i++) {
        ASTNode *inc = decl->data.role_decl.includes[i];
        ASTNode *included_role_decl;
        ASTNode **effective = NULL;
        size_t effective_count = 0;

        if (inc == NULL || inc->type != AST_INCLUDE_STMT)
            continue;

        included_role_decl = semantic_stage_named_decl_quiet(
            ctx,
            AST_ROLE_DECL,
            inc->data.include_stmt.role_name);
        effective = collect_effective_generic_arg_nodes(
            (included_role_decl != NULL && included_role_decl->type == AST_ROLE_DECL)
                ? included_role_decl->data.role_decl.generic_params
                : NULL,
            inc->data.include_stmt.type_args,
            inc,
            ctx,
            "role include",
            inc->data.include_stmt.role_name,
            &effective_count);
        free(effective);
        (void)effective_count;
    }
    for (size_t i = 0; i < decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        if (impl->data.impl_ability.ability_ref != NULL
            && impl->data.impl_ability.ability_ref->type == AST_TYPE
            && impl->data.impl_ability.ability_ref->data.type.name != NULL) {
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                impl->data.impl_ability.ability_ref->data.type.name);
        }
        (void)semantic_stage_resolve_type_quiet(
            impl->data.impl_ability.ability_ref,
            ctx,
            impl,
            decl->data.role_decl.name,
            "role impl ability lookup");
    }
}

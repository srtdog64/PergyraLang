#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"

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

static bool
stage_nominal_enter_generic_scope(GenericParams *gp, SemanticContext *ctx)
{
    bool entered = false;

    size_t generic_count = ast_generic_param_count(gp);
    if (generic_count == 0 || ctx == NULL)
        return false;

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    entered = true;
    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_generic_param_at(gp, i);
        const char *param_name = ast_generic_param_name(param);
        Type *type;
        Symbol *sym;

        if (param_name == NULL)
            continue;

        type = type_create_generic(param_name);
        sym = symbol_create_variable(param_name,
                                     type != NULL ? type : TYPE_UNKNOWN,
                                     0,
                                     0);
        if (sym == NULL)
            continue;
        sym->kind = SYMBOL_TYPE_PARAM;
        scope_declare(ctx->scope, sym);
    }

    return entered;
}

void
semantic_stage_class_decl(ASTNode *decl, SemanticContext *ctx)
{
    bool generic_scope_entered;

    if (decl == NULL || decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        ast_class_generic_params(decl),
        ast_class_where_clause(decl),
        ctx,
        decl,
        "class",
        ast_class_name(decl));

    generic_scope_entered = stage_nominal_enter_generic_scope(
        ast_class_generic_params(decl), ctx);
    size_t field_count = 0;
    ClassField **fields = ast_class_fields(decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        char *consumer_name;
        if (field == NULL)
            continue;
        consumer_name = stage_nominal_strdup_fmt(
            "class %s.%s",
            ast_class_name(decl) != NULL ? ast_class_name(decl) : "<class>",
            field->name != NULL ? field->name : "<field>");
        if (consumer_name == NULL)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx, field->type)
            == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                field->type,
                ctx,
                decl,
                consumer_name,
                "class field type lookup");
        }
        free(consumer_name);
    }
    size_t method_count = 0;
    ASTNode **methods = ast_class_methods(decl, &method_count);
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        ast_class_name(decl));
    if (generic_scope_entered)
        scope_exit(&ctx->scope);
}

void
semantic_stage_enum_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    size_t variant_count = 0;
    char **variants = ast_enum_variants(decl, &variant_count);
    for (size_t i = 0; i < variant_count; i++) {
        size_t param_count = ast_enum_variant_param_count(decl, i);
        const char *variant_name = variants != NULL ? variants[i] : NULL;
        char *consumer_name;

        if (param_count == 0)
            continue;

        consumer_name = stage_nominal_strdup_fmt(
            "enum %s.%s",
            ast_enum_name(decl) != NULL ? ast_enum_name(decl) : "<enum>",
            variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            (void)semantic_stage_resolve_type_quiet(
                ast_enum_variant_param(decl, i, j),
                ctx,
                decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }
    size_t method_count = 0;
    ASTNode **methods = ast_enum_methods(decl, &method_count);
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        ast_enum_name(decl));
}

void
semantic_stage_ability_decl(ASTNode *decl, SemanticContext *ctx)
{
    bool generic_scope_entered;

    if (decl == NULL || decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        ast_ability_generic_params(decl),
        ast_ability_where_clause(decl),
        ctx,
        decl,
        "ability",
        ast_ability_name(decl));
    generic_scope_entered = stage_nominal_enter_generic_scope(
        ast_ability_generic_params(decl), ctx);
    for (size_t i = 0; i < ast_ability_require_field_count(decl); i++) {
        ASTNode *req = ast_ability_require_field(decl, i);
        const char *req_name = ast_require_field_name(req);
        ASTNode *req_type = ast_require_field_type(req);
        char *consumer_name;
        if (req_name == NULL || req_type == NULL)
            continue;
        consumer_name = stage_nominal_strdup_fmt(
            "ability %s.%s",
            ast_ability_name(decl) != NULL
                ? ast_ability_name(decl) : "<ability>",
            req_name);
        if (consumer_name == NULL)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            req_type,
            ctx,
            req,
            consumer_name,
            "ability require-field type lookup");
        free(consumer_name);
    }
    for (size_t i = 0; i < ast_ability_method_count(decl); i++) {
        ASTNode *method = ast_ability_method(decl, i);
        semantic_stage_method_array(
            &method,
            1,
            ctx,
            ast_ability_name(decl));
    }
    if (generic_scope_entered)
        scope_exit(&ctx->scope);
}

void
semantic_stage_role_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        ast_role_generic_params(decl),
        ast_role_where_clause(decl),
        ctx,
        decl,
        "role",
        ast_role_name(decl));
    (void)semantic_stage_resolve_type_quiet(
        semantic_role_for_type_node(decl),
        ctx,
        decl,
        ast_role_name(decl),
        "role host-type lookup");
    for (size_t i = 0; i < ast_role_include_count(decl); i++) {
        ASTNode *inc = ast_role_include(decl, i);
        const char *role_name = ast_include_role_name(inc);
        ASTNode *included_role_decl;
        ASTNode **effective = NULL;
        size_t effective_count = 0;

        if (role_name == NULL)
            continue;

        included_role_decl = semantic_stage_named_decl_quiet(
            ctx,
            AST_ROLE_DECL,
            role_name);
        effective = collect_effective_generic_arg_nodes(
            (included_role_decl != NULL && included_role_decl->type == AST_ROLE_DECL)
                ? ast_role_generic_params(included_role_decl)
                : NULL,
            ast_include_type_args(inc),
            inc,
            ctx,
            "role include",
            role_name,
            &effective_count);
        free(effective);
        (void)effective_count;
    }
    for (size_t i = 0; i < ast_role_impl_count(decl); i++) {
        ASTNode *impl = ast_role_impl(decl, i);
        ASTNode *ability_ref;
        const char *ability_name;
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        ability_ref = ast_impl_ability_ref(impl);
        ability_name = ast_impl_ability_name(impl);
        if (ability_name != NULL) {
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                ability_name);
        }
        (void)semantic_stage_resolve_type_quiet(
            ability_ref,
            ctx,
            impl,
            ast_role_name(decl),
            "role impl ability lookup");
    }
}

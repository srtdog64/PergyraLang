#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"

const char *semantic_role_for_type_name(ASTNode *role_decl);
ASTNode *semantic_find_next_role_decl_for_type_name(SemanticContext *ctx,
                                                   const char *type_name,
                                                   const ASTNode *after);

static ASTNode *
role_lookup_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

static ASTNode *
role_lookup_find_decl_by_name(ASTNode *program, const char *role_name)
{
    if (program == NULL || program->type != AST_PROGRAM || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *stmt_role_name = ast_role_name(stmt);
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt_role_name != NULL
            && strcmp(stmt_role_name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

ASTNode *
semantic_find_role_decl_by_name(SemanticContext *ctx, const char *role_name)
{
    ASTNode *decl;

    if (ctx == NULL || role_name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ROLE_DECL,
                                                 role_name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return role_lookup_find_decl_by_name(role_lookup_program(ctx), role_name);
}

ASTNode *
semantic_find_role_decl_for_type_name(SemanticContext *ctx,
                                      const char *type_name)
{
    return semantic_find_next_role_decl_for_type_name(ctx, type_name, NULL);
}

ASTNode *
semantic_find_next_role_decl_for_type_name(SemanticContext *ctx,
                                           const char *type_name,
                                           const ASTNode *after)
{
    ASTNode *program = role_lookup_program(ctx);
    ASTNode *stmt;
    bool past_after = after == NULL;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    if (ctx->host_decl_index.count > 0) {
        stmt = (ASTNode *)after;
        while ((stmt = semantic_host_index_find_next_decl_of_type(
                    ctx,
                    AST_ROLE_DECL,
                    stmt)) != NULL) {
            const char *role_type = semantic_role_for_type_name(stmt);
            if (role_type != NULL && strcmp(role_type, type_name) == 0)
                return stmt;
        }
        return NULL;
    }

    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        stmt = ast_program_statement(program, i);
        const char *role_type = semantic_role_for_type_name(stmt);
        if (!past_after) {
            if (stmt == after)
                past_after = true;
            continue;
        }
        if (role_type != NULL && strcmp(role_type, type_name) == 0)
            return stmt;
    }

    return NULL;
}

ASTNode *
semantic_role_for_type_node(ASTNode *role_decl)
{
    return ast_role_for_type(role_decl);
}

const char *
semantic_role_for_type_name(ASTNode *role_decl)
{
    ASTNode *for_type = semantic_role_for_type_node(role_decl);

    return ast_type_name(for_type);
}

static ASTNode *
semantic_find_next_subject_role_decl(SemanticContext *ctx,
                                     const ASTNode *after)
{
    ASTNode *program = role_lookup_program(ctx);
    ASTNode *stmt;
    bool past_after = after == NULL;

    if (ctx == NULL)
        return NULL;

    if (ctx->host_decl_index.count > 0) {
        stmt = (ASTNode *)after;
        while ((stmt = semantic_host_index_find_next_decl_of_type(
                    ctx,
                    AST_ROLE_DECL,
                    stmt)) != NULL) {
            ASTNode *role_type = semantic_role_for_type_node(stmt);
            Type *resolved_type = semantic_host_resolve_type_ref(role_type, ctx);
            ASTNode *type_decl;

            if (resolved_type == NULL || resolved_type == TYPE_UNKNOWN)
                continue;

            type_decl = semantic_host_decl_for_type(ctx, resolved_type);
            if (decl_is_subject_host(type_decl))
                return stmt;
        }
        return NULL;
    }

    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        stmt = ast_program_statement(program, i);
        ASTNode *role_type = semantic_role_for_type_node(stmt);
        Type *resolved_type = semantic_host_resolve_type_ref(role_type, ctx);
        ASTNode *type_decl;

        if (!past_after) {
            if (stmt == after)
                past_after = true;
            continue;
        }

        if (resolved_type == NULL || resolved_type == TYPE_UNKNOWN)
            continue;

        type_decl = semantic_host_decl_for_type(ctx, resolved_type);
        if (decl_is_subject_host(type_decl))
            return stmt;
    }

    return NULL;
}

bool
any_subject_role_has_ability(SemanticContext *ctx, ASTNode *ability_ref)
{
    ASTNode *stmt = NULL;

    if (ctx == NULL || ability_ref == NULL)
        return false;

    while ((stmt = semantic_find_next_subject_role_decl(ctx, stmt)) != NULL) {
        if (semantic_role_decl_has_ability(ctx, stmt, ability_ref))
            return true;
    }

    return false;
}

ASTNode *
any_subject_role_find_base_ability_impl(SemanticContext *ctx,
                                        const char *ability_name)
{
    ASTNode *stmt = NULL;

    if (ctx == NULL || ability_name == NULL)
        return NULL;

    while ((stmt = semantic_find_next_subject_role_decl(ctx, stmt)) != NULL) {
        for (size_t j = 0; j < ast_role_impl_count(stmt); j++) {
            ASTNode *impl = ast_role_impl(stmt, j);
            ASTNode *ability_ref;

            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            ability_ref = ast_impl_ability_ref(impl);
            if (ast_impl_ability_name(impl) == NULL) {
                continue;
            }
            if (strcmp(ast_impl_ability_name(impl), ability_name) == 0)
                return ability_ref;
        }
    }

    return NULL;
}

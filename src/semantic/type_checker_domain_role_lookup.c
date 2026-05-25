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

ASTNode *
semantic_find_role_decl(ASTNode *program, const char *role_name)
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
    if (ctx == NULL || role_name == NULL)
        return NULL;
    return semantic_find_role_decl(role_lookup_program(ctx), role_name);
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
    bool past_after = after == NULL;

    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
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

bool
any_subject_role_has_ability(SemanticContext *ctx, ASTNode *ability_ref)
{
    ASTNode *program = role_lookup_program(ctx);

    if (program == NULL || program->type != AST_PROGRAM
        || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        ASTNode *role_type = semantic_role_for_type_node(stmt);
        Type *resolved_type = semantic_host_resolve_type_ref(role_type, ctx);
        ASTNode *type_decl;

        if (resolved_type == NULL || resolved_type == TYPE_UNKNOWN)
            continue;

        type_decl = semantic_host_decl_for_type(ctx, resolved_type);
        if (!decl_is_subject_host(type_decl))
            continue;

        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

ASTNode *
any_subject_role_find_base_ability_impl(SemanticContext *ctx,
                                        const char *ability_name)
{
    ASTNode *program = role_lookup_program(ctx);

    if (program == NULL || program->type != AST_PROGRAM
        || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        ASTNode *role_type = semantic_role_for_type_node(stmt);
        Type *resolved_type = semantic_host_resolve_type_ref(role_type, ctx);
        ASTNode *type_decl;

        if (resolved_type == NULL || resolved_type == TYPE_UNKNOWN)
            continue;

        type_decl = semantic_host_decl_for_type(ctx, resolved_type);
        if (!decl_is_subject_host(type_decl))
            continue;

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

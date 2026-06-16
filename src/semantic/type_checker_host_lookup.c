#include <string.h>

#include "type_checker_internal.h"

static ASTNode *
host_lookup_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

static ASTNode *
host_find_type_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt == NULL || stmt->type != AST_CLASS_DECL)
            continue;
        if (ast_class_name(stmt) != NULL
            && strcmp(ast_class_name(stmt), type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
host_find_ability_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *ability_name = ast_ability_name(stmt);
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL
            || ability_name == NULL)
            continue;
        if (strcmp(ability_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
host_find_callable_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *stmt_name;

        if (stmt == NULL)
            continue;
        stmt_name = stmt->is_async_decl
            ? ast_async_func_name(stmt)
            : ast_declaration_name(stmt);
        if (stmt->type == AST_FUNC_DECL
            && stmt_name != NULL
            && strcmp(stmt_name, name) == 0)
            return stmt;
        if (stmt->type == AST_EVENT_DECL
            && ast_event_name(stmt) != NULL
            && strcmp(ast_event_name(stmt), name) == 0)
            return stmt;
        if (stmt->type == AST_INTENT_DECL
            && ast_intent_decl_name(stmt) != NULL
            && strcmp(ast_intent_decl_name(stmt), name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
host_find_enum_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && ast_enum_name(stmt) != NULL
            && strcmp(ast_enum_name(stmt), name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
host_find_function_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *stmt_name;

        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;
        stmt_name = stmt->is_async_decl
            ? ast_async_func_name(stmt)
            : ast_declaration_name(stmt);
        if (stmt_name != NULL && strcmp(stmt_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static const char *
host_domain_decl_name(ASTNode *decl, ASTNodeType decl_type)
{
    if (decl == NULL || decl->type != decl_type)
        return NULL;

    switch (decl_type) {
    case AST_RELATION_DECL:
        return ast_relation_name(decl);
    case AST_EFFECT_DECL:
        return ast_effect_name(decl);
    case AST_ZONE_DECL:
        return ast_zone_name(decl);
    case AST_WORLD_DECL:
        return ast_world_name(decl);
    case AST_PARTY_DECL:
        return ast_party_name(decl);
    case AST_ROSTER_DECL:
        return ast_roster_name(decl);
    default:
        return NULL;
    }
}

static ASTNode *
host_find_domain_decl_by_name(ASTNode *program,
                              ASTNodeType decl_type,
                              const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *decl_name = host_domain_decl_name(stmt, decl_type);
        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind,
                                 const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return host_find_type_decl_by_name(program, name);
    case SYMBOL_PARTY:
        return host_find_domain_decl_by_name(program, AST_PARTY_DECL, name);
    case SYMBOL_ROSTER:
        return host_find_domain_decl_by_name(program, AST_ROSTER_DECL, name);
    case SYMBOL_WORLD:
        return host_find_domain_decl_by_name(program, AST_WORLD_DECL, name);
    case SYMBOL_ZONE:
        return host_find_domain_decl_by_name(program, AST_ZONE_DECL, name);
    case SYMBOL_RELATION:
        return host_find_domain_decl_by_name(program, AST_RELATION_DECL, name);
    case SYMBOL_EFFECT:
        return host_find_domain_decl_by_name(program, AST_EFFECT_DECL, name);
    default:
        return NULL;
    }
}

ASTNode *
semantic_find_type_alias_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;
    ASTNode *program;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_TYPE_ALIAS, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;

    program = host_lookup_program(ctx);
    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *alias_name = ast_type_alias_name(stmt);
        if (stmt != NULL && stmt->type == AST_TYPE_ALIAS
            && alias_name != NULL && strcmp(alias_name, name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

ASTNode *
semantic_find_zone_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ZONE_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_ZONE_DECL, name);
}

ASTNode *
semantic_find_relation_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_RELATION_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_RELATION_DECL, name);
}

ASTNode *
semantic_find_effect_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_EFFECT_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_EFFECT_DECL, name);
}

ASTNode *
semantic_find_world_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_WORLD_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_WORLD_DECL, name);
}

ASTNode *
semantic_find_party_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_PARTY_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_PARTY_DECL, name);
}

ASTNode *
semantic_find_roster_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ROSTER_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_ROSTER_DECL, name);
}

ASTNode *
semantic_find_class_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_CLASS_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_type_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_intent_decl_by_name(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return semantic_host_index_find_decl_by_name(ctx, AST_INTENT_DECL, name);
}

ASTNode *
semantic_find_ability_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ABILITY_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_ability_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_enum_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ENUM_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_enum_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_function_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_FUNC_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_function_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_callable_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_FUNC_DECL, name);
    if (decl != NULL)
        return decl;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_EVENT_DECL, name);
    if (decl != NULL)
        return decl;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_INTENT_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_callable_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_constructor_decl_for_symbol_kind(SemanticContext *ctx,
                                          SymbolKind kind,
                                          const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return semantic_find_class_decl_by_name(ctx, name);
    case SYMBOL_PARTY:
        return semantic_find_party_decl_by_name(ctx, name);
    case SYMBOL_ROSTER:
        return semantic_find_roster_decl_by_name(ctx, name);
    case SYMBOL_WORLD:
        return semantic_find_world_decl_by_name(ctx, name);
    case SYMBOL_ZONE:
        return semantic_find_zone_decl_by_name(ctx, name);
    case SYMBOL_RELATION:
        return semantic_find_relation_decl_by_name(ctx, name);
    case SYMBOL_EFFECT:
        return semantic_find_effect_decl_by_name(ctx, name);
    default:
        return constructor_decl_for_symbol_kind(host_lookup_program(ctx),
                                                kind, name);
    }
}

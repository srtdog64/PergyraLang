#include "type_checker_internal.h"

#include <string.h>

static Type *
func_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                       type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

Type *
type_check_signature_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    if (type_ref == NULL)
        return TYPE_UNKNOWN;
    return func_resolve_type_ref(type_ref, ctx);
}

Type *
type_check_func_resolve_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return func_resolve_type_ref(param->type, ctx);
}

Type *
type_check_func_resolve_return_type(ASTNode *func_decl, SemanticContext *ctx)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL
        || ast_func_return_type(func_decl) == NULL) {
        return TYPE_VOID;
    }
    return func_resolve_type_ref(ast_func_return_type(func_decl), ctx);
}

const char *
type_check_func_current_implicit_self_host_name(SemanticContext *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_nominal_decl != NULL) {
        if (ctx->current_nominal_decl->type == AST_CLASS_DECL)
            return ast_class_name(ctx->current_nominal_decl);
        if (ctx->current_nominal_decl->type == AST_ENUM_DECL)
            return ast_enum_name(ctx->current_nominal_decl);
    }
    if (ctx->current_relation != NULL)
        return ast_relation_name(ctx->current_relation);
    if (ctx->current_effect != NULL)
        return ast_effect_name(ctx->current_effect);
    if (ctx->current_party != NULL)
        return ast_party_name(ctx->current_party);
    if (ctx->current_roster != NULL)
        return ast_roster_name(ctx->current_roster);
    if (ctx->current_zone != NULL)
        return ast_zone_name(ctx->current_zone);
    if (ctx->current_world != NULL)
        return ast_world_name(ctx->current_world);
    return NULL;
}

bool
type_check_func_symbol_is_self_host(Symbol *sym)
{
    if (sym == NULL || sym->type == NULL)
        return false;
    return sym->kind == SYMBOL_CLASS
        || sym->kind == SYMBOL_ZONE
        || sym->kind == SYMBOL_WORLD
        || sym->kind == SYMBOL_RELATION
        || sym->kind == SYMBOL_EFFECT
        || sym->kind == SYMBOL_ROSTER
        || sym->kind == SYMBOL_PARTY;
}

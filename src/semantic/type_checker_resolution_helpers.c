#include <stdio.h>
#include <string.h>

#include "type_checker_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

static ASTNode *
resolution_helper_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

ASTNode *
find_type_alias_decl(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *alias_name = ast_type_alias_name(stmt);
        if (stmt == NULL || stmt->type != AST_TYPE_ALIAS
            || alias_name == NULL) {
            continue;
        }
        if (strcmp(alias_name, name) == 0)
            return stmt;
    }

    return NULL;
}

ASTNode *
semantic_find_type_alias_decl_by_name(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return find_type_alias_decl(resolution_helper_program(ctx), name);
}

bool
name_looks_qualified(const char *name)
{
    return name != NULL && strchr(name, '.') != NULL;
}

const char *
semantic_symbol_kind_label(SymbolKind kind)
{
    switch (kind) {
    case SYMBOL_VARIABLE: return "variable";
    case SYMBOL_FUNCTION: return "function";
    case SYMBOL_CLASS: return "class";
    case SYMBOL_TYPE_PARAM: return "type parameter";
    case SYMBOL_SLOT: return "slot";
    case SYMBOL_TOKEN: return "token";
    case SYMBOL_ABILITY: return "ability";
    case SYMBOL_ROLE: return "role";
    case SYMBOL_PARTY: return "party";
    case SYMBOL_ROSTER: return "roster";
    case SYMBOL_WORLD: return "world";
    case SYMBOL_INTENT: return "intent";
    case SYMBOL_RELATION: return "relation";
    case SYMBOL_EFFECT: return "effect";
    case SYMBOL_ZONE: return "zone";
    default: return "symbol";
    }
}

const char *
type_name_or_unknown(const Type *type)
{
    if (type == NULL)
        return "<unknown>";
    if (type->name != NULL && type->name[0] != '\0')
        return type->name;
    return "<unknown>";
}

void
semantic_format_function_signature(const Type *type, char *out, size_t out_cap)
{
    size_t pos = 0;
    if (out == NULL || out_cap == 0)
        return;

    out[0] = '\0';
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;

    pos = pergyra_str_append(out, out_cap, "fn(");
    for (size_t i = 0; i < type_function_param_count(type); i++) {
        const char *param_name =
            type_name_or_unknown(type_function_param_type(type, i));
        if (i > 0)
            pos = pergyra_str_append(out, out_cap, ", ");
        pos = pergyra_str_append(out, out_cap, param_name);
        if (pos >= out_cap)
            break;
    }
    if (pos < out_cap)
        (void)pergyra_str_appendf(out, out_cap, ")->%s",
                                  type_name_or_unknown(type_function_return_type(type)));
}

static const char *
root_identifier_name(ASTNode *expr)
{
    if (expr == NULL)
        return NULL;

    switch (expr->type) {
    case AST_IDENTIFIER:
        return ast_identifier_name(expr);
    case AST_MEMBER_ACCESS:
        return root_identifier_name(ast_member_object(expr));
    case AST_ARRAY_ACCESS:
        return root_identifier_name(ast_array_access_array(expr));
    default:
        return NULL;
    }
}

static size_t
embedded_world_zone_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++) {
        if (ctx->embedded_world_zone_names[i] != NULL
            && strcmp(ctx->embedded_world_zone_names[i], name) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

static bool
has_embedded_world_zone_name(SemanticContext *ctx, const char *name)
{
    return embedded_world_zone_index(ctx, name) != (size_t)-1;
}

static const char *
embedded_world_zone_world_name(SemanticContext *ctx, const char *name)
{
    size_t index = embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_world_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_world_names[index];
}

static const char *
embedded_world_zone_slot_name(SemanticContext *ctx, const char *name)
{
    size_t index = embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_slot_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_slot_names[index];
}

void
reject_if_embedded_world_zone_mutation(SemanticContext *ctx, ASTNode *site,
                                       ASTNode *target, const char *op_name)
{
    if (ctx == NULL || site == NULL || target == NULL || op_name == NULL)
        return;
    if (ctx->current_world != NULL)
        return;

    const char *root_name = root_identifier_name(target);
    if (root_name == NULL)
        return;

    Symbol *root_sym = scope_lookup(ctx->scope, root_name);
    if ((root_sym == NULL || !root_sym->embedded_in_world)
        && !has_embedded_world_zone_name(ctx, root_name)) {
        return;
    }

    const char *owner_world = embedded_world_zone_world_name(ctx, root_name);
    const char *owner_slot = embedded_world_zone_slot_name(ctx, root_name);

    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
        PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
        "Zone '%s' cannot be mutated via %s after it was embedded into world '%s' slot '%s'.\n"
        "Reason:\n"
        "- origin binding is '%s'\n"
        "Contract source:\n"
        "- world '%s' zone slot '%s'\n"
        "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
        "- derived embedding provenance points to world '%s' slot '%s'\n"
        "- ownership/authority now flows through the world-owned slot rather than the old local binding\n"
        "- owned embedding hands authority-bearing visibility to the world-owned slot\n"
        "- mutating the old binding would diverge from the world-owned handoff destination\n"
        "Fix:\n"
        "- finish configuring '%s' before constructing world '%s'\n"
        "- or mutate it through world '%s' slot '%s' after embedding",
        root_name, op_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>");
}

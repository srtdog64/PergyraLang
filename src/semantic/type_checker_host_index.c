#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"

static const char *
host_decl_index_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_TYPE_ALIAS:
        return ast_type_alias_name(decl);
    case AST_CLASS_DECL:
        return ast_class_name(decl);
    case AST_ABILITY_DECL:
        return ast_ability_name(decl);
    case AST_ROLE_DECL:
        return ast_role_name(decl);
    case AST_FUNC_DECL:
        return decl->is_async_decl
            ? ast_async_func_name(decl)
            : ast_declaration_name(decl);
    case AST_EVENT_DECL:
        return ast_event_name(decl);
    case AST_ENUM_DECL:
        return ast_enum_name(decl);
    case AST_INTENT_DECL:
        return ast_intent_decl_name(decl);
    case AST_PARTY_DECL:
        return ast_party_name(decl);
    case AST_ROSTER_DECL:
        return ast_roster_name(decl);
    case AST_WORLD_DECL:
        return ast_world_name(decl);
    case AST_RELATION_DECL:
        return ast_relation_name(decl);
    case AST_EFFECT_DECL:
        return ast_effect_name(decl);
    case AST_ZONE_DECL:
        return ast_zone_name(decl);
    default:
        return NULL;
    }
}

static bool
host_decl_index_reserve(SemanticContext *ctx, size_t next_count)
{
    size_t new_cap;
    ASTNode **new_decls;
    const char **new_names;
    ASTNodeType *new_types;

    if (ctx == NULL)
        return false;
    if (next_count <= ctx->host_decl_index.capacity)
        return true;
    if (ctx->host_decl_index.capacity > (size_t)-1 / 2)
        return false;

    new_cap = ctx->host_decl_index.capacity == 0
        ? 32
        : ctx->host_decl_index.capacity * 2;
    while (new_cap < next_count) {
        if (new_cap > (size_t)-1 / 2)
            return false;
        new_cap *= 2;
    }
    if (new_cap > (size_t)-1 / sizeof(ASTNode *))
        return false;

    new_decls = calloc(new_cap, sizeof(ASTNode *));
    new_names = calloc(new_cap, sizeof(const char *));
    new_types = calloc(new_cap, sizeof(ASTNodeType));
    if (new_decls == NULL || new_names == NULL || new_types == NULL) {
        free(new_decls);
        free(new_names);
        free(new_types);
        return false;
    }

    if (ctx->host_decl_index.count > 0) {
        memcpy(new_decls, ctx->host_decl_index.decls,
               ctx->host_decl_index.count * sizeof(ASTNode *));
        memcpy(new_names, ctx->host_decl_index.names,
               ctx->host_decl_index.count * sizeof(const char *));
        memcpy(new_types, ctx->host_decl_index.types,
               ctx->host_decl_index.count * sizeof(ASTNodeType));
    }

    free(ctx->host_decl_index.decls);
    free(ctx->host_decl_index.names);
    free(ctx->host_decl_index.types);
    ctx->host_decl_index.decls = new_decls;
    ctx->host_decl_index.names = new_names;
    ctx->host_decl_index.types = new_types;
    ctx->host_decl_index.capacity = new_cap;
    return true;
}

static bool
host_decl_index_append(SemanticContext *ctx, ASTNode *decl)
{
    const char *name = host_decl_index_name(decl);
    size_t index;

    if (ctx == NULL || decl == NULL || name == NULL)
        return true;
    if (!host_decl_index_reserve(ctx, ctx->host_decl_index.count + 1))
        return false;

    index = ctx->host_decl_index.count++;
    ctx->host_decl_index.decls[index] = decl;
    ctx->host_decl_index.names[index] = name;
    ctx->host_decl_index.types[index] = decl->type;
    return true;
}

/* FNV-1a over the name, mixed with the decl type. Must be byte-identical
 * between build and lookup. */
static size_t
host_decl_key_hash(ASTNodeType type, const char *name)
{
    size_t h = 1469598103934665603ULL;
    const unsigned char *p;

    for (p = (const unsigned char *)name; *p != '\0'; p++) {
        h ^= (size_t)*p;
        h *= 1099511628211ULL;
    }
    h ^= (size_t)type * 2654435761ULL;
    return h;
}

static void
host_decl_index_free_hash(SemanticContext *ctx)
{
    free(ctx->host_decl_index.hash);
    ctx->host_decl_index.hash = NULL;
    ctx->host_decl_index.hash_capacity = 0;
}

/* Best-effort: on OOM the hash stays NULL and lookups fall back to the linear
 * scan, so failure here is slow, not wrong. Keeps the FIRST index for a given
 * (type, name) to match the linear scan's first-match semantics. */
static void
host_decl_index_build_hash(SemanticContext *ctx)
{
    size_t count = ctx->host_decl_index.count;
    size_t cap = 16;
    size_t *hash;
    size_t mask;

    host_decl_index_free_hash(ctx);
    if (count == 0)
        return;
    if (count > ((size_t)-1) / 4)
        return;  /* pathological; keep linear */
    while (cap < count * 2)
        cap *= 2;
    hash = calloc(cap, sizeof(size_t));
    if (hash == NULL)
        return;
    mask = cap - 1;

    for (size_t i = 0; i < count; i++) {
        const char *nm = ctx->host_decl_index.names[i];
        size_t slot;

        if (nm == NULL)
            continue;
        slot = host_decl_key_hash(ctx->host_decl_index.types[i], nm) & mask;
        for (size_t probe = 0; probe < cap; probe++) {
            size_t entry = hash[slot];
            size_t j;

            if (entry == 0) {
                hash[slot] = i + 1;
                break;
            }
            j = entry - 1;
            if (ctx->host_decl_index.types[j] == ctx->host_decl_index.types[i]
                && ctx->host_decl_index.names[j] != NULL
                && strcmp(ctx->host_decl_index.names[j], nm) == 0) {
                break;  /* duplicate (type, name): keep the first index */
            }
            slot = (slot + 1) & mask;
        }
    }
    ctx->host_decl_index.hash = hash;
    ctx->host_decl_index.hash_capacity = cap;
}

bool
semantic_build_host_decl_index(SemanticContext *ctx, ASTNode *program)
{
    if (ctx == NULL || program == NULL || program->type != AST_PROGRAM)
        return false;

    ctx->host_decl_index.count = 0;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        if (!host_decl_index_append(ctx, ast_program_statement(program, i)))
            return false;
    }
    host_decl_index_build_hash(ctx);
    return true;
}

ASTNode *
semantic_host_index_find_decl_by_name(SemanticContext *ctx,
                                      ASTNodeType decl_type,
                                      const char *name)
{
    if (ctx == NULL || name == NULL || ctx->host_decl_index.count == 0)
        return NULL;

    if (ctx->host_decl_index.hash != NULL
        && ctx->host_decl_index.hash_capacity > 0) {
        size_t mask = ctx->host_decl_index.hash_capacity - 1;
        size_t slot = host_decl_key_hash(decl_type, name) & mask;

        for (size_t probe = 0; probe < ctx->host_decl_index.hash_capacity;
             probe++) {
            size_t entry = ctx->host_decl_index.hash[slot];
            size_t idx;

            if (entry == 0)
                return NULL;  /* empty slot: key absent */
            idx = entry - 1;
            if (ctx->host_decl_index.types[idx] == decl_type
                && ctx->host_decl_index.names[idx] != NULL
                && strcmp(ctx->host_decl_index.names[idx], name) == 0) {
                return ctx->host_decl_index.decls[idx];
            }
            slot = (slot + 1) & mask;
        }
        return NULL;
    }

    /* Hash not built (OOM or empty): linear fallback. */
    for (size_t i = 0; i < ctx->host_decl_index.count; i++) {
        if (ctx->host_decl_index.types[i] == decl_type
            && ctx->host_decl_index.names[i] != NULL
            && strcmp(ctx->host_decl_index.names[i], name) == 0) {
            return ctx->host_decl_index.decls[i];
        }
    }
    return NULL;
}

ASTNode *
semantic_host_index_find_top_level_decl_by_label(SemanticContext *ctx,
                                                 const char *label,
                                                 TypeResolutionNodeKind kind)
{
    if (ctx == NULL || label == NULL || ctx->host_decl_index.count == 0)
        return NULL;

    for (size_t i = 0; i < ctx->host_decl_index.count; i++) {
        TypeResolutionNodeKind entry_kind =
            ctx->host_decl_index.types[i] == AST_TYPE_ALIAS
                ? TYPE_RES_NODE_ALIAS
                : TYPE_RES_NODE_DECL;
        if (entry_kind == kind
            && ctx->host_decl_index.names[i] != NULL
            && strcmp(ctx->host_decl_index.names[i], label) == 0) {
            return ctx->host_decl_index.decls[i];
        }
    }
    return NULL;
}

ASTNode *
semantic_host_index_find_next_decl_of_type(SemanticContext *ctx,
                                           ASTNodeType decl_type,
                                           const ASTNode *after)
{
    bool past_after = after == NULL;

    if (ctx == NULL || ctx->host_decl_index.count == 0)
        return NULL;

    for (size_t i = 0; i < ctx->host_decl_index.count; i++) {
        ASTNode *decl = ctx->host_decl_index.decls[i];
        if (ctx->host_decl_index.types[i] != decl_type)
            continue;
        if (!past_after) {
            if (decl == after)
                past_after = true;
            continue;
        }
        return decl;
    }
    return NULL;
}

/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "module_normalizer.h"
#include "module_normalizer_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

typedef struct
{
    ASTNode **items;
    size_t    count;
    size_t    capacity;
} ASTVec;

static bool
astvec_push(ASTVec *vec, ASTNode *node)
{
    if (vec->count == vec->capacity) {
        size_t next = 8;
        if (vec->capacity != 0) {
            if (vec->capacity > SIZE_MAX / 2)
                return false;
            next = vec->capacity * 2;
        }
        if (next > SIZE_MAX / sizeof(ASTNode *)) {
            return false;
        }
        ASTNode **grown = realloc(vec->items, next * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        vec->items = grown;
        vec->capacity = next;
    }
    vec->items[vec->count++] = node;
    return true;
}

static char *
join_names(const char *a, const char *b)
{
    size_t alen = a != NULL ? strlen(a) : 0;
    size_t blen = b != NULL ? strlen(b) : 0;
    if (alen > SIZE_MAX - blen - 1)
        return NULL;
    char *result = malloc(alen + blen + 1);
    if (result == NULL)
        return NULL;
    if (alen > 0)
        memcpy(result, a, alen);
    if (blen > 0)
        memcpy(result + alen, b, blen);
    result[alen + blen] = '\0';
    return result;
}

static char *
namespace_prefix_join(const char *prefix, const char *name)
{
    size_t plen = prefix != NULL ? strlen(prefix) : 0;
    size_t nlen = strlen(name);
    if (plen > SIZE_MAX - nlen - 2)
        return NULL;
    char *result = malloc(plen + nlen + 2);
    if (result == NULL)
        return NULL;
    if (plen > 0)
        memcpy(result, prefix, plen);
    memcpy(result + plen, name, nlen);
    result[plen + nlen] = '_';
    result[plen + nlen + 1] = '\0';
    return result;
}

static char **
node_name_slot(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_FUNC_DECL: return &node->data.func_decl.name;
        case AST_CLASS_DECL: return &node->data.class_decl.name;
        case AST_LET_DECL: return &node->data.let_decl.name;
        case AST_ABILITY_DECL: return &node->data.ability_decl.name;
        case AST_ROLE_DECL: return &node->data.role_decl.name;
        case AST_PARTY_DECL: return &node->data.party_decl.name;
        case AST_ROSTER_DECL: return &node->data.roster_decl.name;
        case AST_WORLD_DECL: return &node->data.world_decl.name;
        case AST_RELATION_DECL: return &node->data.relation_decl.name;
        case AST_EFFECT_DECL: return &node->data.effect_decl.name;
        case AST_ZONE_DECL: return &node->data.zone_decl.name;
        case AST_EVENT_DECL: return &node->data.event_decl.name;
        case AST_ENUM_DECL: return &node->data.enum_decl.name;
        default: return NULL;
    }
}

static bool
module_has_explicit_exports_in_stmt(ASTNode *node)
{
    if (node == NULL)
        return false;
    if (node->has_explicit_export)
        return true;
    if (node->type == AST_NAMESPACE_DECL) {
        for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
            if (module_has_explicit_exports_in_stmt(
                    node->data.namespace_decl.statements[i])) {
                return true;
            }
        }
    }
    return false;
}

static bool
module_has_explicit_exports(ASTNode *program)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;
    for (size_t i = 0; i < program->data.program.count; i++) {
        if (module_has_explicit_exports_in_stmt(program->data.program.statements[i]))
            return true;
    }
    return false;
}

static void
free_namespace_shell(ASTNode *node)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return;
    free(node->data.namespace_decl.name);
    free(node->data.namespace_decl.statements);
    node->data.namespace_decl.name = NULL;
    node->data.namespace_decl.statements = NULL;
    node->data.namespace_decl.count = 0;
    free(node);
}

static bool
normalize_statement_list(ASTNode **statements,
                         size_t count,
                         const char *public_prefix,
                         const char *private_prefix,
                         bool imported,
                         bool has_explicit_exports,
                         bool inherited_export,
                         ModuleRenameScope *parent_scope,
                         ASTVec *flat)
{
    ModuleRenameScope scope = { .parent = parent_scope };
    ModuleShadowNames shadow = {0};

    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        char **name_slot;
        if (stmt == NULL || stmt->type == AST_NAMESPACE_DECL)
            continue;
        name_slot = node_name_slot(stmt);
        if (name_slot == NULL || *name_slot == NULL)
            continue;

        char *public_name = join_names(public_prefix, *name_slot);
        bool visible = !imported || !has_explicit_exports
            || inherited_export || stmt->is_exported;
        bool explicit_private =
            stmt->has_explicit_access
            && (stmt->access == ACCESS_PRIVATE || stmt->access == ACCESS_PROTECTED);
        char *final_name = visible
            ? pergyra_strdup(public_name)
            : join_names(private_prefix, public_name);
        if (public_name == NULL || final_name == NULL) {
            free(public_name);
            free(final_name);
            module_rename_scope_destroy(&scope);
            module_shadow_destroy(&shadow);
            return false;
        }

        if (strcmp(*name_slot, final_name) != 0) {
            if (!module_rename_scope_add(&scope, *name_slot, final_name)) {
                free(public_name);
                free(final_name);
                module_rename_scope_destroy(&scope);
                module_shadow_destroy(&shadow);
                return false;
            }
            free(*name_slot);
            *name_slot = pergyra_strdup(final_name);
        }

        free(public_name);
        free(final_name);

        if (imported)
            stmt->is_exported = visible && !explicit_private;
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_NAMESPACE_DECL) {
            char *child_prefix =
                namespace_prefix_join(public_prefix, stmt->data.namespace_decl.name);
            bool child_export = inherited_export || stmt->is_exported;
            if (child_prefix == NULL
                || !normalize_statement_list(stmt->data.namespace_decl.statements,
                                             stmt->data.namespace_decl.count,
                                             child_prefix,
                                             private_prefix,
                                             imported,
                                             has_explicit_exports,
                                             child_export,
                                             &scope,
                                             flat)) {
                free(child_prefix);
                module_rename_scope_destroy(&scope);
                module_shadow_destroy(&shadow);
                return false;
            }
            free(child_prefix);
            free_namespace_shell(stmt);
            continue;
        }

        module_normalizer_normalize_node_refs(stmt, &scope, &shadow);
        if (!astvec_push(flat, stmt)) {
            module_rename_scope_destroy(&scope);
            module_shadow_destroy(&shadow);
            return false;
        }
    }

    module_rename_scope_destroy(&scope);
    module_shadow_destroy(&shadow);
    return true;
}

bool
module_normalize_ast(ASTNode *program, bool imported, const char *private_prefix)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ASTVec flat = {0};
    bool has_explicit = imported && module_has_explicit_exports(program);
    bool ok = normalize_statement_list(program->data.program.statements,
                                       program->data.program.count,
                                       "",
                                       private_prefix != NULL ? private_prefix : "",
                                       imported,
                                       has_explicit,
                                       false,
                                       NULL,
                                       &flat);
    if (!ok) {
        free(flat.items);
        return false;
    }

    free(program->data.program.statements);
    program->data.program.statements = flat.items;
    program->data.program.count = flat.count;
    return true;
}

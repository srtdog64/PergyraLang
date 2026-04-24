/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic validation for ability `fields` requirements.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_ability_fields_internal.h"
#include "type_checker_visibility.h"

static Type *
ability_fields_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;
    if (type_ref == NULL)
        return NULL;
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_ref);
    if (resolved != NULL)
        return resolved;
    return resolve_type_node(type_ref, ctx);
}

void
validate_ability_require_fields(ASTNode *node, SemanticContext *ctx)
{
    const char *name;

    if (node == NULL || node->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    name = node->data.ability_decl.name;

    for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
        ASTNode *req = node->data.ability_decl.require_fields[i];
        if (req == NULL || req->data.require_field.name == NULL
            || req->data.require_field.type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
                PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
                node,
                "Ability '%s' has an invalid fields declaration",
                name != NULL ? name : "<ability>");
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            ASTNode *prev = node->data.ability_decl.require_fields[j];
            if (prev != NULL && prev->data.require_field.name != NULL
                && strcmp(prev->data.require_field.name,
                          req->data.require_field.name) == 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
                    PGY_CAUSE_ABILITY_CONTRACT,
                    PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
                    req,
                    "Ability '%s' declares duplicate field '%s' in fields",
                    name != NULL ? name : "<ability>",
                    req->data.require_field.name);
                break;
            }
        }
        ability_fields_resolve_type_ref(req->data.require_field.type, ctx);
        if (req->data.require_field.type != NULL
            && req->data.require_field.type->type == AST_TYPE
            && req->data.require_field.type->data.type.name != NULL) {
            ASTNode *type_decl = find_type_decl_by_name(
                ctx->program_root, req->data.require_field.type->data.type.name);
            if (type_decl != NULL
                && !explicit_type_reference_allowed(type_decl, node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
                    PGY_CAUSE_ABILITY_CONTRACT,
                    PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
                    req,
                    "Ability '%s' cannot declare field '%s' in fields with non-exported type '%s' from another module",
                    name != NULL ? name : "<ability>",
                    req->data.require_field.name,
                    req->data.require_field.type->data.type.name);
            }
        }
    }
}

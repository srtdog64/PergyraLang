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

void
validate_ability_require_fields(ASTNode *node, SemanticContext *ctx)
{
    const char *name;

    if (node == NULL || node->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    name = ast_ability_name(node);

    for (size_t i = 0; i < ast_ability_require_field_count(node); i++) {
        ASTNode *req = ast_ability_require_field(node, i);
        const char *req_name = ast_require_field_name(req);
        ASTNode *req_type = ast_require_field_type(req);
        if (req_name == NULL || req_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
                PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
                node,
                "Ability '%s' has an invalid fields declaration",
                name != NULL ? name : "<ability>");
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            ASTNode *prev = ast_ability_require_field(node, j);
            const char *prev_name = ast_require_field_name(prev);
            if (prev_name != NULL && strcmp(prev_name, req_name) == 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
                    PGY_CAUSE_ABILITY_CONTRACT,
                    PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
                    req,
                    "Ability '%s' declares duplicate field '%s' in fields",
                    name != NULL ? name : "<ability>",
                    req_name);
                break;
            }
        }
        Type *resolved_type = ability_resolve_type_ref(req_type, ctx);
        if (req_type != NULL && req_type->type == AST_TYPE
            && ast_type_name(req_type) != NULL) {
            ASTNode *type_decl = semantic_host_decl_for_type(ctx, resolved_type);
            if (type_decl != NULL
                && !explicit_type_reference_allowed(type_decl, node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
                    PGY_CAUSE_ABILITY_CONTRACT,
                    PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
                    req,
                    "Ability '%s' cannot declare field '%s' in fields with non-exported type '%s' from another module",
                    name != NULL ? name : "<ability>",
                    req_name,
                    ast_type_name(req_type));
            }
        }
    }
}

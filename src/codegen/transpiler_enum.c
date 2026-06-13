/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend enum lowering helpers.
 */

#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_inventory_view.h"

bool
lookup_enum_variant_qualified_name_copy(TranspilerCtx *ctx,
                                        const char *variant_name,
                                        char *out,
                                        size_t out_size)
{
    ASTNode **types = NULL;
    size_t type_count = 0;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (ctx == NULL || variant_name == NULL)
        return false;

    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL)
        return false;

    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        const MIRDeclHeader *enum_header = NULL;
        bool use_mir_variants = false;
        const char *enum_name = NULL;
        size_t variant_count = 0;
        char **variants = NULL;
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        enum_name = transpiler_decl_name_local(stmt);
        if (enum_name == NULL)
            return false;
        enum_header = transpiler_active_decl_header_of_type(
            ctx, AST_ENUM_DECL, enum_name);
        if (transpiler_active_has_mir(ctx) && enum_header == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing enum variant metadata for '%s'",
                enum_name);
            return false;
        }
        use_mir_variants = enum_header != NULL;
        if (use_mir_variants) {
            variant_count = mir_decl_header_enum_variant_count(enum_header);
        } else {
            variants = ast_enum_variants(stmt, &variant_count);
        }
        bool enum_has_data = false;
        for (size_t k = 0; k < variant_count; k++) {
            const MIRDeclEnumVariant *variant_meta = use_mir_variants
                ? mir_decl_header_enum_variant(enum_header, k) : NULL;
            size_t param_count = use_mir_variants
                ? mir_decl_enum_variant_param_count(variant_meta)
                : ast_enum_variant_param_count(stmt, k);
            if (param_count > 0) {
                enum_has_data = true;
                break;
            }
        }
        for (size_t j = 0; j < variant_count; j++) {
            const MIRDeclEnumVariant *variant_meta = use_mir_variants
                ? mir_decl_header_enum_variant(enum_header, j) : NULL;
            const char *candidate = use_mir_variants
                ? mir_decl_enum_variant_name(variant_meta)
                : (variants != NULL ? variants[j] : NULL);
            if (candidate != NULL && strcmp(candidate, variant_name) == 0) {
                int written;
                (void)enum_has_data;
                /* Payload-less tagged variant is a value-style macro constant,
                 * so every variant -- plain, payload, or payload-less tagged --
                 * is referenced by its bare `Enum_Variant` name. */
                written = snprintf(out, out_size, "%s_%s",
                    enum_name, candidate);
                if (written < 0 || (size_t)written >= out_size) {
                    out[0] = '\0';
                    return false;
                }
                return true;
            }
        }
    }

    return false;
}

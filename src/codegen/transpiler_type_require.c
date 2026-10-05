/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type requirement helpers.
 */

#include "transpiler_type_require.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "codegen_channel_runtime_abi.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_specialization_registry.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"

static const char *
transpiler_bound_type_name(TranspilerCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return type_name;

    for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
        if (strcmp(ctx->generic_bindings[i].name, type_name) == 0)
            return ctx->generic_bindings[i].concrete_type;
    }
    return type_name;
}

/* Token-wise substitution of active generic-parameter names inside a type
 * name string, so `Array<T>` becomes `Array<Int>`, `Map<K, V>` becomes
 * `Map<Int, String>`, recursing naturally over nested `<...>`. */
static bool
transpiler_subst_generics_in_type_name(TranspilerCtx *ctx, const char *in,
                                       char *out, size_t out_size)
{
    size_t oi = 0;
    size_t i = 0;

    if (ctx == NULL || in == NULL || out == NULL || out_size == 0)
        return false;
    while (in[i] != '\0') {
        char c = in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            size_t start = i;
            char tok[128];
            size_t len;
            const char *bound;
            const char *rep;
            while (in[i] != '\0'
                && ((in[i] >= 'A' && in[i] <= 'Z')
                    || (in[i] >= 'a' && in[i] <= 'z')
                    || (in[i] >= '0' && in[i] <= '9') || in[i] == '_'))
                i++;
            len = i - start;
            if (len >= sizeof(tok)) {
                for (size_t k = 0; k < len; k++) {
                    if (oi + 1 >= out_size) return false;
                    out[oi++] = in[start + k];
                }
                continue;
            }
            memcpy(tok, in + start, len);
            tok[len] = '\0';
            bound = transpiler_bound_type_name(ctx, tok);
            rep = (bound != NULL && bound != tok) ? bound : tok;
            for (size_t k = 0; rep[k] != '\0'; k++) {
                if (oi + 1 >= out_size) return false;
                out[oi++] = rep[k];
            }
        } else {
            if (oi + 1 >= out_size) return false;
            out[oi++] = c;
            i++;
        }
    }
    out[oi] = '\0';
    return true;
}

/* Public entry for the token-wise substitution above: returns `buf` with
 * active generic bindings applied when that changes the text, or the
 * original pointer otherwise. Lets non-require consumers (expression type
 * inference feeding Some_/IsSome_ suffix derivation) share the one
 * substitution owner instead of growing private copies. */
const char *
transpiler_type_name_apply_generic_bindings(TranspilerCtx *ctx,
                                            const char *type_name,
                                            char *buf,
                                            size_t buf_size)
{
    char subst_buf[256];
    const char *bound = type_name;

    if (ctx == NULL || type_name == NULL || buf == NULL || buf_size == 0)
        return type_name;
    if (ctx->generic_binding_count > 0
        && transpiler_subst_generics_in_type_name(ctx, type_name, subst_buf,
            sizeof(subst_buf))
        && strcmp(subst_buf, type_name) != 0)
        bound = subst_buf;
    /* Constructor and predicate suffixes (Some_/Ok_/IsSome_) name the same
     * specialization the declarations use, so aliases resolve here too. */
    bound = transpiler_type_name_resolve_aliases(ctx, bound, buf, buf_size);
    if (bound == subst_buf && !pergyra_str_copy(buf, buf_size, subst_buf))
        return type_name;
    return bound == subst_buf ? buf : bound;
}

/* One token-wise pass replacing each type-alias name by its target. */
static bool
transpiler_subst_aliases_in_type_name_once(TranspilerCtx *ctx, const char *in,
                                           char *out, size_t out_size,
                                           bool *changed)
{
    size_t oi = 0;
    size_t i = 0;

    *changed = false;
    while (in[i] != '\0') {
        char c = in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            size_t start = i;
            char tok[128];
            size_t len;
            const char *target;
            while (in[i] != '\0'
                && ((in[i] >= 'A' && in[i] <= 'Z')
                    || (in[i] >= 'a' && in[i] <= 'z')
                    || (in[i] >= '0' && in[i] <= '9') || in[i] == '_'))
                i++;
            len = i - start;
            target = NULL;
            if (len < sizeof(tok)) {
                memcpy(tok, in + start, len);
                tok[len] = '\0';
                target = transpiler_type_alias_target_type_name_from_headers(
                    ctx, tok);
            }
            if (target != NULL)
                *changed = true;
            const char *rep = target != NULL ? target : in + start;
            size_t rep_len = target != NULL ? strlen(target) : len;
            if (oi + rep_len >= out_size)
                return false;
            memcpy(out + oi, rep, rep_len);
            oi += rep_len;
        } else {
            if (oi + 1 >= out_size)
                return false;
            out[oi++] = c;
            i++;
        }
    }
    out[oi] = '\0';
    return true;
}

/* `type Gold = Int` is transparent, so Array<Gold> and Result<Gold, E> are
 * Array<Int> and Result<Int, E>: the same C specializations, not distinct
 * structs. Returns `buf` holding the type with every alias (also inside an
 * alias target) replaced, or the original pointer when none occurs. */
const char *
transpiler_type_name_resolve_aliases(TranspilerCtx *ctx,
                                     const char *type_name,
                                     char *buf,
                                     size_t buf_size)
{
    char scratch[2][256];
    const char *current = type_name;
    bool any_change = false;

    if (ctx == NULL || type_name == NULL || buf == NULL || buf_size == 0)
        return type_name;
    if (ctx->type_alias_presence == 0) {
        MIRDeclHeaderInventory inventory;
        transpiler_active_decl_header_inventory(ctx, &inventory);
        ctx->type_alias_presence = 1;
        for (size_t i = 0; i < inventory.count; i++) {
            if (inventory.headers[i].ast_type == AST_TYPE_ALIAS) {
                ctx->type_alias_presence = 2;
                break;
            }
        }
    }
    if (ctx->type_alias_presence == 1)
        return type_name;
    for (size_t depth = 0; depth < 8; depth++) {
        bool changed = false;
        char *next = scratch[depth % 2];
        if (!transpiler_subst_aliases_in_type_name_once(ctx, current, next,
                sizeof(scratch[0]), &changed))
            return type_name;
        if (!changed)
            break;
        any_change = true;
        current = next;
    }
    if (!any_change || !pergyra_str_copy(buf, buf_size, current))
        return type_name;
    return buf;
}

bool
transpiler_require_ast_c_type_copy(TranspilerCtx *ctx,
                                   ASTNode *type_ast,
                                   const char *surface_desc,
                                   char *out,
                                   size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_ast == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot emit %s in C backend: missing explicit type",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }

    if (!pergyra_ast_type_to_c_copy_in_ctx(ctx, type_ast, out, out_size)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot lower %s to a bounded C type buffer",
            surface_desc != NULL ? surface_desc : "declaration");
        out[0] = '\0';
        return false;
    }
    if (out[0] == '\0' || strcmp(out, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot lower %s to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration");
        out[0] = '\0';
        return false;
    }
    return true;
}

bool
transpiler_require_type_name_c_type_copy(TranspilerCtx *ctx,
                                         const char *type_name,
                                         const char *surface_desc,
                                         char *out,
                                         size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_name == NULL || type_name[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot emit %s in C backend: missing concrete type name",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }

    char subst_buf[256];
    char alias_buf[256];
    const char *eff_type_name = type_name;
    if (ctx != NULL && ctx->generic_binding_count > 0
        && transpiler_subst_generics_in_type_name(ctx, type_name,
               subst_buf, sizeof(subst_buf)))
        eff_type_name = subst_buf;
    eff_type_name = transpiler_type_name_resolve_aliases(ctx, eff_type_name,
        alias_buf, sizeof(alias_buf));

    const char *resolved_type_name =
        transpiler_bound_type_name(ctx, eff_type_name);
    const char *generic_class_type_name =
        transpiler_ensure_generic_class_specialization_from_type_name(
            ctx, ctx != NULL ? ctx->decls : NULL, resolved_type_name);
    if (generic_class_type_name != NULL)
        resolved_type_name = generic_class_type_name;

    if (resolved_type_name[0] >= 'A' && resolved_type_name[0] <= 'Z'
        && resolved_type_name[1] == '\0') {
        if (!pergyra_str_copy(out, out_size, resolved_type_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "cannot lower %s type '%s' to a concrete C type",
                surface_desc != NULL ? surface_desc : "declaration",
                resolved_type_name);
            out[0] = '\0';
            return false;
        }
        return true;
    }

    if (!pergyra_type_to_c_copy(resolved_type_name, out, out_size)
        || out[0] == '\0'
        || strcmp(out, "Unknown") == 0) {
        if (transpiler_type_name_is_channel(resolved_type_name)) {
            char inner[128];

            if (!slot_inner_type_name_copy(resolved_type_name,
                    inner, sizeof(inner)))
                pergyra_str_copy(inner, sizeof(inner), "<unknown>");
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: %s type '%s' has no runtime Channel<%s> ABI; beta runtime supports %s",
                surface_desc != NULL ? surface_desc : "declaration",
                resolved_type_name,
                inner,
                pgy_channel_runtime_payload_supported_list());
            out[0] = '\0';
            return false;
        }
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot lower %s type '%s' to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration",
            resolved_type_name);
        out[0] = '\0';
        return false;
    }

    return true;
}

bool
transpiler_copy_c_type_or_user_type_name(const char *type_name,
                                         char *out,
                                         size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_name == NULL)
        return false;

    if (type_name[0] >= 'A' && type_name[0] <= 'Z' && type_name[1] == '\0')
        return pergyra_str_copy(out, out_size, type_name);

    if (pergyra_type_to_c_copy(type_name, out, out_size))
        return true;

    return pergyra_str_copy(out, out_size, type_name);
}

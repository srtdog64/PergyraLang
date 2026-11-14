/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Clone owner: what a Clone copies into new storage, by type.
 */

#include "transpiler_clone_value_emit.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/string_compat.h"
#include "../../parser/ast_api.h"
#include "../../semantic/diag_codes.h"
#include "../codegen_hashmap_key_policy.h"
#include "../codegen_type_mapping.h"
#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_stdlib_builtin.h"
#include "transpiler_format.h"
#include "transpiler_option_context.h"
#include "transpiler_slot_runtime_row.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_type_require.h"

/* No program struct nests deeper; a deeper chain is refused, not cut. */
#define TRANSPILER_CLONE_MAX_DEPTH 16

static char *transpiler_clone_value_at(TranspilerCtx *ctx, const char *type_name,
                                       const char *value, int depth);

static char *
transpiler_clone_array(TranspilerCtx *ctx, const char *type_name,
                       const char *value, int depth)
{
    const char *inner = NULL;
    char inner_buf[128];

    if (!transpiler_resolve_unary_constructed_inner(ctx, type_name, "Array",
            inner_buf, sizeof(inner_buf), &inner)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Clone requires concrete Array<T> metadata");
        return NULL;
    }
    if (transpiler_type_name_is_array(inner)
        && transpiler_array_copy_leaf_suffix(ctx, inner) == NULL) {
        /* A field of such an Array is copied by value; the checker keeps
         * that Clone dependent on its argument. */
        if (depth > 0)
            return strdup(value);
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: Clone has no concrete nested-array copy operation");
        return NULL;
    }
    transpiler_array_inner_sanitize_suffix(inner_buf, sizeof(inner_buf), &inner);
    int tmp_id = ++ctx->tmp_counter;
    return strdup_fmt(
        "({ PgyArray_%s _pgy_array_clone_%d = %s; "
        "pgy_array_clone_%s(&_pgy_array_clone_%d); })",
        inner, tmp_id, value, inner, tmp_id);
}

static char *
transpiler_clone_slot(TranspilerCtx *ctx, const char *type_name, const char *value)
{
    char inner_buf[128];
    const char *inner = NULL;

    if (slot_inner_type_name_copy(type_name, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;
    if (inner == NULL || inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Clone requires concrete Slot<T> metadata");
        return NULL;
    }
    const char *claim_fn = transpiler_slot_runtime_fn(ctx, false, inner, "Claim");
    const char *read_fn = transpiler_slot_runtime_fn(ctx, false, inner, "Read");
    const char *write_fn = transpiler_slot_runtime_fn(ctx, false, inner, "Write");
    if (claim_fn == NULL || read_fn == NULL || write_fn == NULL)
        return NULL;
    return strdup_fmt("({ PgySlot_%s _c = %s(); %s(&_c, %s(&%s)); _c; })",
        inner, claim_fn, write_fn, read_fn, value);
}

/* An Option is copied with its payload; None stays None. */
static char *
transpiler_clone_option(TranspilerCtx *ctx, const char *type_name,
                        const char *value, int depth)
{
    char inner[128];
    char suffix[128];

    if (!slot_inner_type_name_copy(type_name, inner, sizeof(inner)) || inner[0] == '\0'
        || !transpiler_option_suffix_from_type_name(type_name, suffix, sizeof(suffix))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Clone requires concrete Option<T> metadata");
        return NULL;
    }
    int tmp_id = ++ctx->tmp_counter;
    char *payload_ref = strdup_fmt("_pgy_option_clone_%d.value", tmp_id);
    char *payload = payload_ref == NULL ? NULL
        : transpiler_clone_value_at(ctx, inner, payload_ref, depth + 1);
    char *result = NULL;
    if (payload != NULL && strcmp(payload, payload_ref) == 0)
        result = strdup(value);
    else if (payload != NULL)
        result = strdup_fmt(
            "({ PgyOption_%s _pgy_option_clone_%d = %s; "
            "if (_pgy_option_clone_%d.tag == PgyOptionSome) %s = %s; "
            "_pgy_option_clone_%d; })",
            suffix, tmp_id, value, tmp_id, payload_ref, payload, tmp_id);
    free(payload_ref);
    free(payload);
    return result;
}

/* A HashMap with a String, Int, Long or Bool key and a String or scalar
 * value is copied slot by slot into new storage (pgy_map_clone_<value>);
 * any other map is copied by value, and the checker keeps that Clone
 * dependent on its argument. */
static char *
transpiler_clone_map(TranspilerCtx *ctx, const char *type_name, const char *value)
{
    char key[128], element[128], suffix[128], map_type[256];
    copy_constructed_arg_name_at(type_name, 0, key, sizeof(key));
    copy_constructed_arg_name_at(type_name, 1, element, sizeof(element));
    if (pgy_hashmap_key_kind_from_name(key) == PGY_HASHMAP_KEY_UNKNOWN ||
        !(strcmp(element, "String") == 0 || pgy_hashmap_value_copies_by_bytes(element)))
        return strdup(value);
    if (!transpiler_require_type_name_c_type_copy(ctx, type_name, "HashMap Clone", map_type, sizeof(map_type)) ||
        !collection_runtime_suffix_copy(element, suffix, sizeof(suffix)))
        return NULL;
    ensure_collection_specialization(ctx, "Map", element);
    int tmp_id = ++ctx->tmp_counter;
    return strdup_fmt("({ %s _pgy_map_clone_%d = %s; pgy_map_clone_%s(&_pgy_map_clone_%d); })",
        map_type, tmp_id, value, suffix, tmp_id);
}

/* A struct or class declared under this name is copied field by field (a
 * generic instance is not, and is copied by value). A subject field
 * keeps its identity and is copied by value. */
static char *
transpiler_clone_struct(TranspilerCtx *ctx, const char *type_name, ASTNode *decl,
                        const char *value, int depth)
{
    TranspilerHostedFieldView view =
        transpiler_hosted_class_field_view_from_decl(ctx, type_name, decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(&view)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C backend: Clone of '%s' has no declaration field metadata", type_name);
        return NULL;
    }
    int tmp_id = ++ctx->tmp_counter;
    char *body = strdup_fmt("({ %s _pgy_struct_clone_%d = %s; ", type_name, tmp_id, value);
    bool copied = false;
    for (size_t i = 0; body != NULL && i < view.count; i++) {
        const char *field = transpiler_hosted_field_view_name(&view, i);
        const char *field_type = transpiler_hosted_field_view_type_name(&view, i);
        if (field == NULL || field_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend: Clone of '%s' has no type name for field %zu", type_name, i);
            free(body);
            return NULL;
        }
        if (transpiler_hosted_field_view_is_subject_like(&view, i))
            continue;
        char *field_ref = strdup_fmt("_pgy_struct_clone_%d.%s", tmp_id, field);
        char *field_copy = field_ref == NULL ? NULL
            : transpiler_clone_value_at(ctx, field_type, field_ref, depth + 1);
        char *grown = NULL;
        if (field_copy != NULL && strcmp(field_copy, field_ref) != 0) {
            grown = strdup_fmt("%s%s = %s; ", body, field_ref, field_copy);
            copied = true;
        } else if (field_copy != NULL) {
            grown = strdup(body);
        }
        free(field_ref);
        free(field_copy);
        free(body);
        body = grown;
    }
    if (body == NULL)
        return NULL;
    char *result = copied ? strdup_fmt("%s_pgy_struct_clone_%d; })", body, tmp_id) : strdup(value);
    free(body);
    return result;
}

static char *
transpiler_clone_value_at(TranspilerCtx *ctx, const char *type_name,
                          const char *value, int depth)
{
    if (value == NULL)
        return NULL;
    if (depth > TRANSPILER_CLONE_MAX_DEPTH) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: Clone of '%s' nests deeper than %d values",
            type_name != NULL ? type_name : "?", TRANSPILER_CLONE_MAX_DEPTH);
        return NULL;
    }
    if (type_name == NULL)
        return strdup(value);
    if (transpiler_type_name_is_array(type_name))
        return transpiler_clone_array(ctx, type_name, value, depth);
    /* A Slot field keeps its claim: only a Slot itself is claimed anew. */
    if (strncmp(type_name, "Slot<", 5) == 0)
        return depth == 0 ? transpiler_clone_slot(ctx, type_name, value) : strdup(value);
    if (strcmp(type_name, "String") == 0)
        return strdup_fmt("StringConcat(%s, \"\")", value);
    if (strncmp(type_name, "Option<", 7) == 0)
        return transpiler_clone_option(ctx, type_name, value, depth);
    if (transpiler_type_name_is_hashmap(type_name))
        return transpiler_clone_map(ctx, type_name, value);
    /* A subject is copied field by field only as the Clone argument itself. */
    ASTNode *decl = find_class_decl(ctx, type_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL
        && (ast_class_nominal_kind(decl) == NOMINAL_DECL_STRUCT
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_CLASS
            || (ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT && depth == 0)))
        return transpiler_clone_struct(ctx, type_name, decl, value, depth);
    return strdup(value);
}

char *
transpiler_emit_clone_value(TranspilerCtx *ctx, const ASTNode *site, const char *type_name,
                            const char *value)
{
    /* A site receipt describes this result, never its aggregate fields. */
    if (type_name != NULL && strcmp(type_name, "String") == 0 && value != NULL)
        return transpiler_region_concat(ctx, site, value, "\"\"");
    return transpiler_clone_value_at(ctx, type_name, value, 0);
}

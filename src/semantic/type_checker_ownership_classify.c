/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker ownership classification helpers.
 */

#include "type_checker_ownership_internal.h"
#include "type_checker_resolution_internal.h"

enum { OWNERSHIP_ENUM_PAYLOAD_DEPTH_LIMIT = 8 };

/* An enum value carries what its payloads carry. A payload-free enum, or one
 * whose payloads are all copy-only, stays copy-only. Any other payload (a
 * struct, a collection, a subject, a runtime handle) makes the enum a tracked
 * aggregate like a struct, so `own` moves it and a second use is refused
 * instead of releasing the payload twice. A payload type that does not
 * resolve counts as tracked. */
static OwnershipTypeClass
ownership_classify_enum_payloads(const Type *type, SemanticContext *ctx,
                                 unsigned depth)
{
    ASTNode *decl = semantic_find_enum_decl_by_name(ctx, type->name);
    size_t variant_count = 0;

    if (decl == NULL)
        return OWNERSHIP_TYPE_COPY_ONLY; /* builtin enums carry no payload */
    if (depth >= OWNERSHIP_ENUM_PAYLOAD_DEPTH_LIMIT)
        return OWNERSHIP_TYPE_BORROW_TRACKED;
    (void)ast_enum_variants(decl, &variant_count);
    for (size_t v = 0; v < variant_count; v++) {
        for (size_t p = 0; p < ast_enum_variant_param_count(decl, v); p++) {
            Type *payload = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, ast_enum_variant_param(decl, v, p));
            OwnershipTypeClass klass;

            if (payload == NULL || payload == TYPE_UNKNOWN)
                return OWNERSHIP_TYPE_BORROW_TRACKED;
            klass = payload->kind == TYPE_KIND_ENUM
                ? ownership_classify_enum_payloads(payload, ctx, depth + 1)
                : semantic_classify_ownership_type(payload, ctx);
            if (klass != OWNERSHIP_TYPE_COPY_ONLY)
                return OWNERSHIP_TYPE_BORROW_TRACKED;
        }
    }
    return OWNERSHIP_TYPE_COPY_ONLY;
}

OwnershipTypeClass
semantic_classify_ownership_type(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || ctx == NULL)
        return OWNERSHIP_TYPE_COPY_ONLY;
    if (type->kind == TYPE_KIND_GENERIC)
        return OWNERSHIP_TYPE_BORROW_TRACKED;
    if (type_is_anchored_resource_handle(type))
        return OWNERSHIP_TYPE_ANCHORED_HANDLE;
    if (type_is_movable_resource_handle(type))
        return OWNERSHIP_TYPE_MOVE_ONLY;
    if (type_is_subject_type(type, ctx))
        return OWNERSHIP_TYPE_SUBJECT_IDENTITY;
    if (type->kind == TYPE_KIND_ENUM && type->name != NULL)
        return ownership_classify_enum_payloads(type, ctx, 0);
    if (type_requires_boundary_borrow_tracking(type, ctx))
        return OWNERSHIP_TYPE_BORROW_TRACKED;
    return OWNERSHIP_TYPE_COPY_ONLY;
}

const char *
semantic_ownership_value_label(OwnershipTypeClass klass)
{
    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        return "slot handle (movable)";
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        return "subject";
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        return "boundary value";
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        return "slot handle (anchored)";
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        return "value";
    }
}

const char *
semantic_ownership_provenance_label(OwnershipTypeClass klass)
{
    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        return "slot-handle (movable) provenance";
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        return "subject provenance";
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        return "boundary provenance";
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        return "slot-handle (anchored) provenance";
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        return "value provenance";
    }
}

const char *
semantic_ownership_replacement_label(OwnershipTypeClass klass)
{
    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        return "a copied/projection/value result";
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        return "a projection/object/tobject/value result";
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        return "a copied/value/projection result";
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        return "a projection/value result";
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        return "a value result";
    }
}

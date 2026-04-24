/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker ownership classification helpers.
 */

#include "type_checker_ownership_internal.h"

OwnershipTypeClass
semantic_classify_ownership_type(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || ctx == NULL)
        return OWNERSHIP_TYPE_COPY_ONLY;
    if (type_is_anchored_resource_handle(type))
        return OWNERSHIP_TYPE_ANCHORED_HANDLE;
    if (type_is_movable_resource_handle(type))
        return OWNERSHIP_TYPE_MOVE_ONLY;
    if (type_is_subject_type(type, ctx))
        return OWNERSHIP_TYPE_SUBJECT_IDENTITY;
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

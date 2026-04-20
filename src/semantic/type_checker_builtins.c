/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker built-in dispatch and stdlib helpers
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../common/string_compat.h"
#include "type_checker_internal.h"

typedef enum OwnershipTypeClass {
    OWNERSHIP_TYPE_COPY_ONLY = 0,
    OWNERSHIP_TYPE_MOVE_ONLY,
    OWNERSHIP_TYPE_BORROW_TRACKED,
    OWNERSHIP_TYPE_SUBJECT_IDENTITY,
    OWNERSHIP_TYPE_ANCHORED_HANDLE
} OwnershipTypeClass;

typedef enum OwnershipConsumerKind {
    OWNERSHIP_CONSUMER_NEW_BINDING = 0,
    OWNERSHIP_CONSUMER_ASSIGNMENT_REBIND,
    OWNERSHIP_CONSUMER_CONTAINER_STORE,
    OWNERSHIP_CONSUMER_RETURN,
    OWNERSHIP_CONSUMER_CHANNEL_SEND,
    OWNERSHIP_CONSUMER_HELPER_CALL,
    OWNERSHIP_CONSUMER_CONSTRUCTOR_FIELD_STORE
} OwnershipConsumerKind;

static char *
builtin_expr_source_path(ASTNode *value_expr);

bool
semantic_validate_channel_transport_ownership(ASTNode *value_expr,
                                              Type *value_type,
                                              SemanticContext *ctx,
                                              const char *transport_name,
                                              OwnershipTypeClass expected_class,
                                              OwnershipTypeClass element_ownership,
                                              OwnershipTypeClass value_ownership,
                                              const char *contract_label,
                                              const char *expected_name,
                                              const char *actual_name,
                                              const char *value_label,
                                              const char *named_binding_fix);

void
semantic_report_channel_transport_policy(ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *transport_name,
                                         const char *why_text,
                                         const char *fix_text);

static bool
builtin_type_is_subject_type(const Type *type, SemanticContext *ctx)
{
    (void)ctx;
    return type != NULL
        && type->kind == TYPE_KIND_CLASS
        && type->nominal_flavor == TYPE_NOMINAL_SUBJECT;
}

static OwnershipTypeClass
semantic_classify_ownership_type(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || ctx == NULL)
        return OWNERSHIP_TYPE_COPY_ONLY;
    if (type_is_anchored_resource_handle(type))
        return OWNERSHIP_TYPE_ANCHORED_HANDLE;
    if (type_is_movable_resource_handle(type))
        return OWNERSHIP_TYPE_MOVE_ONLY;
    if (builtin_type_is_subject_type(type, ctx))
        return OWNERSHIP_TYPE_SUBJECT_IDENTITY;
    if (type_requires_boundary_borrow_tracking(type, ctx))
        return OWNERSHIP_TYPE_BORROW_TRACKED;
    return OWNERSHIP_TYPE_COPY_ONLY;
}

static bool
semantic_validate_borrowed_escape(ASTNode *site,
                                  ASTNode *source_expr,
                                  SemanticContext *ctx,
                                  const Type *value_type,
                                  const char *borrowed_name_override,
                                  OwnershipConsumerKind consumer_kind,
                                  ASTNode *target_expr,
                                  const char *dest_name,
                                  const char *secondary_name,
                                  bool transitive_call,
                                  const char *mode_label,
                                  const char *local_fix_label)
{
    const char *borrowed_name = borrowed_name_override;
    OwnershipTypeClass klass;
    const char *value_label = "value";
    const char *provenance_label = "value provenance";
    const char *dest_label = dest_name != NULL ? dest_name : "<destination>";
    const char *consumer_label = dest_label;
    char *source_path = NULL;
    char *source_clause = NULL;
    char *reason_text = NULL;

    (void)target_expr;
    (void)transitive_call;
    (void)mode_label;
    (void)local_fix_label;

    if (ctx == NULL || site == NULL || value_type == NULL)
        return false;
    if (borrowed_name == NULL)
        return false;

    klass = semantic_classify_ownership_type(value_type, ctx);
    if (klass == OWNERSHIP_TYPE_COPY_ONLY)
        return false;

    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        value_label = "movable resource";
        provenance_label = "movable-resource provenance";
        break;
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        value_label = "boundary value";
        provenance_label = "boundary provenance";
        break;
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        value_label = "subject";
        provenance_label = "subject provenance";
        break;
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        value_label = "anchored handle";
        provenance_label = "anchored-handle provenance";
        break;
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        value_label = "value";
        provenance_label = "value provenance";
        break;
    }

    source_path = builtin_expr_source_path(source_expr != NULL ? source_expr : site);
    if (source_path != NULL) {
        size_t clause_needed = snprintf(NULL, 0, " from '%s'", source_path);
        source_clause = malloc(clause_needed + 1);
        if (source_clause != NULL) {
            snprintf(source_clause, clause_needed + 1, " from '%s'", source_path);
        }
    }
    if (source_path != NULL) {
        size_t needed = snprintf(NULL, 0,
            "- '%s' is derived from that borrowed %s\n",
            source_path, provenance_label);
        reason_text = malloc(needed + 1);
        if (reason_text != NULL) {
            snprintf(reason_text, needed + 1,
                "- '%s' is derived from that borrowed %s\n",
                source_path, provenance_label);
        }
    }

    switch (consumer_kind) {
    case OWNERSHIP_CONSUMER_CONTAINER_STORE:
        if (dest_name != NULL) {
            if (strcmp(dest_name, "array") == 0)
                consumer_label = "array store";
            else if (strcmp(dest_name, "list") == 0)
                consumer_label = "list store";
            else if (strcmp(dest_name, "set") == 0)
                consumer_label = "set store";
            else if (strcmp(dest_name, "queue") == 0)
                consumer_label = "queue store";
            else if (strcmp(dest_name, "map") == 0)
                consumer_label = "map store";
        }
        semantic_error_with_hints(
            ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:ownership:borrow_escape",
            "store-a-copied-value-instead", site,
            "Borrowed ref %s '%s' cannot escape through %s%s.\n"
            "Reason:\n"
            "%s"
            "- storing it in %s '%s' would let it outlive '%s'\n"
            "Fix:\n"
            "- store a copied/projection/value result instead\n"
            "- or keep '%s' local to the current scope",
            value_label, borrowed_name,
            consumer_label,
            source_clause != NULL ? source_clause : "",
            reason_text != NULL ? reason_text : "",
            consumer_label,
            secondary_name != NULL ? secondary_name : "<container>",
            borrowed_name,
            borrowed_name);
        free(reason_text);
        free(source_clause);
        free(source_path);
        return true;
    case OWNERSHIP_CONSUMER_CHANNEL_SEND:
        semantic_error_with_hints(
            ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "send-a-copied-value-instead", site,
            "Borrowed ref %s '%s' cannot escape through channel send%s.\n"
            "Reason:\n"
            "%s"
            "- channel transport would let it outlive '%s'\n"
            "Fix:\n"
            "- send a copied/projection/value result instead\n"
            "- or keep '%s' local to the current scope",
            value_label, borrowed_name,
            source_clause != NULL ? source_clause : "",
            reason_text != NULL ? reason_text : "",
            borrowed_name,
            borrowed_name);
        free(reason_text);
        free(source_clause);
        free(source_path);
        return true;
    default:
        semantic_error_with_hints(
            ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:ownership:borrow_escape",
            "keep-borrowed-value-local", site,
            "Cannot move borrowed %s derived from '%s' into '%s'.\n"
            "Reason:\n"
            "- ownership transfer would let the borrowed value escape its source binding\n"
            "Fix:\n"
            "- keep '%s' local\n"
            "- or materialize a copied/projection/value result first",
            value_label, borrowed_name, dest_label, borrowed_name);
        free(reason_text);
        free(source_clause);
        free(source_path);
        return true;
    }
}

static bool
type_is_future_like(const Type *type)
{
    return type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

#include "type_checker_builtins_query_domain.inc"

#include "type_checker_builtins_query.inc"

#include "type_checker_builtins_stdlib.inc"

#include "type_checker_builtins_nominal.inc"

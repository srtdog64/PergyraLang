#ifndef PERGYRA_TYPE_CHECKER_OWNERSHIP_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_OWNERSHIP_INTERNAL_H

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

OwnershipTypeClass
semantic_classify_ownership_type(const Type *type, SemanticContext *ctx);

const char *
semantic_ownership_value_label(OwnershipTypeClass klass);

const char *
semantic_ownership_provenance_label(OwnershipTypeClass klass);

const char *
semantic_ownership_replacement_label(OwnershipTypeClass klass);

bool
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
                                  const char *local_fix_label);

#endif

#ifndef PERGYRA_PARALLEL_CAPTURE_REACH_INTERNAL_H
#define PERGYRA_PARALLEL_CAPTURE_REACH_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "type_checker_internal.h"

/* Shared by the two parallel capture reach owners:
 * parallel_capture_storage_reach.c owns the host field model below, and
 * parallel_capture_write_reach.c walks task bodies with it. */

enum { PARALLEL_REACH_DEPTH_LIMIT = 8 };

/* One field a host declaration carries. The families are the ones
 * expr_current_host_field_type types: class fields (declaration field
 * model), world roster and zone slots and zone layer slots (typed by name),
 * and host overlay fields (zone, relation and effect slots, shared fields).
 * The lookups here do not diagnose; `type` is NULL when the declared type
 * does not resolve, and `name` is NULL for an entry no family types. */
typedef struct {
    const char *name;
    Type *type;
} ParallelReachField;

typedef struct {
    ParallelReachField *items;
    size_t count;
    bool complete; /* false when allocation failed: fail closed */
} ParallelReachFields;

/* A user nominal other than an enum, or NULL. */
ASTNode *parallel_reach_nominal_decl(SemanticContext *ctx, const Type *type);

/* Every field `decl` carries; the caller frees `items`. */
ParallelReachFields parallel_reach_host_fields(SemanticContext *ctx,
                                               ASTNode *decl);

/* The declared type of `field_name` on `decl`, or NULL when there is no such
 * field or its type does not resolve. */
Type *parallel_reach_field_type(SemanticContext *ctx, ASTNode *decl,
                                const char *field_name);

#endif /* PERGYRA_PARALLEL_CAPTURE_REACH_INTERNAL_H */

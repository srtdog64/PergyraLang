/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend projection field-path helpers.
 */

#ifndef PERGYRA_TRANSPILER_PROJECTION_FIELD_PATH_H
#define PERGYRA_TRANSPILER_PROJECTION_FIELD_PATH_H

#include "transpiler.h"

const char *assignment_target_root_slot_name(ASTNode *target);
const char *assignment_target_root_subfield_name(ASTNode *target);
bool host_projection_relevant_field_exists(TranspilerCtx *ctx,
                                           const char *host_type_name,
                                           const char *field_name);
const char *host_projection_subject_field_type_name(TranspilerCtx *ctx,
                                                   const char *host_type_name,
                                                   const char *field_name);
const char *method_projection_write_field_name(TranspilerCtx *ctx,
                                               const char *host_type_name,
                                               const char *root_name,
                                               const char *member_name);
const char *method_assignment_projection_field_name(TranspilerCtx *ctx,
                                                   const char *host_type_name,
                                                   ASTNode *target);

#endif /* PERGYRA_TRANSPILER_PROJECTION_FIELD_PATH_H */

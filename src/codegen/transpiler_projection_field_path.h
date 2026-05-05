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
ClassField *find_host_field_by_name_local(ASTNode *host_decl,
                                          const char *field_name);
const char *method_assignment_projection_field_name(TranspilerCtx *ctx,
                                                   const char *host_type_name,
                                                   ASTNode *target);

#endif /* PERGYRA_TRANSPILER_PROJECTION_FIELD_PATH_H */

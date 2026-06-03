/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent context and binding lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_INTENT_CONTEXT_H
#define PERGYRA_TRANSPILER_INTENT_CONTEXT_H

#include "transpiler.h"

ASTNode *find_intent_participant_local(ASTNode *intent, const char *alias);
ASTNode *find_subject_action_decl(TranspilerCtx *ctx,
                                  const char *subject_name,
                                  const char *action_name);
const MIRDeclMethod *find_subject_action_metadata(TranspilerCtx *ctx,
                                                  const char *subject_name,
                                                  const char *action_name);
bool intent_action_metadata_has_only_self(const MIRDeclMethod *method);
ASTNode *find_zone_decl_in_program_view(TranspilerCtx *ctx,
                                        const char *zone_name);
const char *intent_participant_type_name(ASTNode *intent, const char *alias);
const char *intent_step_effective_zone_alias(ASTNode *step);
const char *intent_zone_binding_type_name(ASTNode *intent, const char *alias);
const char *intent_zone_binding_type_name_with_metadata(
    ASTNode *intent,
    const char *alias,
    const char **participant_aliases,
    const char **participant_types,
    size_t participant_count);

#endif /* PERGYRA_TRANSPILER_INTENT_CONTEXT_H */

/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent context and binding lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_INTENT_CONTEXT_H
#define PERGYRA_TRANSPILER_INTENT_CONTEXT_H

#include "intent_binding_metadata_view.h"
#include "transpiler.h"

ASTNode *find_intent_participant_local(ASTNode *intent, const char *alias);
const MIRDeclMethod *find_subject_action_metadata(TranspilerCtx *ctx,
                                                  const char *subject_name,
                                                  const char *action_name);
bool intent_action_metadata_has_only_self(const MIRDeclMethod *method);
ASTNode *find_zone_decl_in_program_view(TranspilerCtx *ctx,
                                        const char *zone_name);
const char *intent_participant_type_name(ASTNode *intent, const char *alias);
const char *intent_step_effective_zone_alias(ASTNode *step);
const char *intent_zone_binding_type_name(ASTNode *intent, const char *alias);
const char *intent_zone_binding_type_name_with_bindings(
    ASTNode *intent,
    const char *alias,
    const IntentBindingMetadataView *bindings);

#endif /* PERGYRA_TRANSPILER_INTENT_CONTEXT_H */

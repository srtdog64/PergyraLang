/* Intent-specific helper functions promoted to external linkage so they can
 * be shared between top-level semantic orchestration and intent declaration
 * owners.
 *
 * Definitions live in focused semantic owner TUs; this header keeps the seam
 * explicit without depending on a legacy include-fragment chain.
 */
#ifndef PERGYRA_TYPE_CHECKER_INTENT_HELPERS_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_INTENT_HELPERS_INTERNAL_H

#include "type_checker_internal.h"

bool intent_clause_invokes_authority_sensitive_call(ASTNode *expr,
                                                    SemanticContext *ctx);

void intent_step_warn_redundant_action_contract(ASTNode *intent_decl,
                                                ASTNode *step,
                                                SemanticContext *ctx);

void intent_step_format_contract_source_summary(const ASTNode *intent_decl,
                                                const ASTNode *step,
                                                SemanticContext *ctx,
                                                char *buffer,
                                                size_t buffer_size);

bool intent_condition_is_bool(ASTNode *expr,
                              SemanticContext *ctx,
                              const char *label);

bool intent_clause_rejects_control_transfer(ASTNode *expr,
                                            SemanticContext *ctx,
                                            const char *step_name,
                                            const char *label);

bool intent_involves_is_subject_host(ASTNode *program, ASTNode *involves);

bool subject_decl_has_action_named(ASTNode *decl, const char *action_name);

void intent_step_derive_who_from_action(ASTNode *intent_decl,
                                        ASTNode *step,
                                        SemanticContext *ctx);

void intent_step_inherit_action_contract(ASTNode *intent_decl,
                                         ASTNode *step,
                                         SemanticContext *ctx);

void intent_step_derive_transfer_context(ASTNode *intent_decl,
                                         ASTNode *step,
                                         SemanticContext *ctx);

void intent_step_derive_zone_binding_context(ASTNode *intent_decl,
                                             ASTNode *step,
                                             SemanticContext *ctx);

void type_check_intent_step_authority_contract(ASTNode *intent_decl,
                                               ASTNode *step,
                                               ASTNode *zone_decl,
                                               bool has_subintent,
                                               bool step_requires_authority_flow,
                                               SemanticContext *ctx);

void type_check_intent_step_participant_contract(ASTNode *intent_decl,
                                                 ASTNode *step,
                                                 ASTNode *zone_decl,
                                                 bool *matched_action,
                                                 SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_INTENT_HELPERS_INTERNAL_H */

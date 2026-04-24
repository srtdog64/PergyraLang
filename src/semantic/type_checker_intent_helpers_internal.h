/* Intent-specific helper functions promoted to external linkage so they can
 * be shared between type_checker.c (which pulls them in via decls_a.inc) and
 * type_checker_intent_decl.c.
 *
 * Definitions still live in type_checker_decls_a.inc for now — this header
 * only exposes them across translation unit boundaries.  When decls_a.inc is
 * further decomposed (3-B slice), these definitions move to their own TU.
 *
 * See docs/101_semantic_split_template.md for the split roadmap.
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

#endif /* PERGYRA_TYPE_CHECKER_INTENT_HELPERS_INTERNAL_H */

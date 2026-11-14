#ifndef PERGYRA_TYPE_CHECKER_OWNERSHIP_DIAG_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_OWNERSHIP_DIAG_INTERNAL_H

#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_support_internal.h"

const char *
semantic_current_consumer_name(SemanticContext *ctx);

void
semantic_report_borrowed_new_binding_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *binding_name,
                                            const char *value_label,
                                            const char *provenance_label);

void
semantic_report_borrowed_return_escape(ASTNode *site,
                                       ASTNode *source_expr,
                                       SemanticContext *ctx,
                                       const char *borrowed_name,
                                       const char *value_label,
                                       const char *provenance_label,
                                       const char *replacement_label,
                                       bool summary_only);

void
semantic_report_borrowed_assignment_rebind_escape(ASTNode *site,
                                                  ASTNode *target_expr,
                                                  ASTNode *source_expr,
                                                  SemanticContext *ctx,
                                                  const char *borrowed_name,
                                                  const char *value_label,
                                                  const char *provenance_label,
                                                  const char *rebind_label);

void
semantic_report_borrowed_container_store_escape(ASTNode *site,
                                                ASTNode *source_expr,
                                                SemanticContext *ctx,
                                                const char *borrowed_name,
                                                const char *value_label,
                                                const char *provenance_label,
                                                const char *container_kind,
                                                const char *container_name,
                                                const char *replacement_label,
                                                const char *transfer_label);

void
semantic_report_borrowed_channel_send_escape(ASTNode *site,
                                             ASTNode *source_expr,
                                             SemanticContext *ctx,
                                             const char *borrowed_name,
                                             const char *value_label,
                                             const char *provenance_label,
                                             const char *replacement_label,
                                             bool summary_only);

void
semantic_report_borrowed_slot_handle_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *escape_kind,
                                            const char *detail_line,
                                            const char *replacement_label,
                                            const char *secondary_fix,
                                            bool summary_only);

void
semantic_report_named_boundary_argument_required(ASTNode *site,
                                                 ASTNode *source_expr,
                                                 SemanticContext *ctx,
                                                 const char *value_label_cap,
                                                 const char *value_label_lower,
                                                 const char *bind_fix);

void
semantic_report_borrowed_helper_call_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *value_label,
                                            const char *provenance_label,
                                            const char *callee_name,
                                            bool transitive_ref_escape,
                                            const char *mode_label,
                                            const char *local_fix_label);

void
semantic_report_borrowed_constructor_field_escape(ASTNode *site,
                                                  ASTNode *source_expr,
                                                  SemanticContext *ctx,
                                                  const char *borrowed_name,
                                                  const char *value_label,
                                                  const char *provenance_label,
                                                  const char *constructor_name,
                                                  const char *constructor_field,
                                                  const char *snapshot_label,
                                                  bool identity_binding);

#endif

#ifndef PERGYRA_TYPE_CHECKER_CHANNEL_TRANSPORT_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_CHANNEL_TRANSPORT_INTERNAL_H

#include "type_checker_ownership_consumers_internal.h"

bool
semantic_check_channel_send_borrowed_transfer(ASTNode *value_expr,
                                              SemanticContext *ctx,
                                              const char *value_label,
                                              const char *provenance_label,
                                              const char *snapshot_label,
                                              const char *named_binding_fix);

bool
semantic_channel_transfer_requires_named_binding(ASTNode *value_expr,
                                                 const char *borrowed_root_name);

void
semantic_report_named_channel_transfer_required(ASTNode *site,
                                                SemanticContext *ctx,
                                                const char *transport_name,
                                                const char *value_label,
                                                const char *source_path,
                                                const char *bind_fix);

void
semantic_report_channel_transport_mismatch(ASTNode *site,
                                           SemanticContext *ctx,
                                           const char *transport_name,
                                           const char *contract_label,
                                           const char *expected_name,
                                           const char *actual_name);

bool
semantic_check_borrowed_channel_transfer(ASTNode *value_expr,
                                         Type *value_type,
                                         SemanticContext *ctx,
                                         const char *value_label,
                                         const char *named_binding_fix);

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

#endif

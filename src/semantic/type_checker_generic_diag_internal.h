#ifndef PERGYRA_TYPE_CHECKER_GENERIC_DIAG_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_GENERIC_DIAG_INTERNAL_H

#include "type_checker_internal.h"

void
semantic_report_ability_generic_bound_failure(SemanticContext *ctx,
                                              const ASTNode *site,
                                              const char *owner_label,
                                              const char *owner_name,
                                              const char *ability_name,
                                              const char *param_name,
                                              const char *bound_name,
                                              const char *bounds_text,
                                              const char *required_text,
                                              const char *concrete_name);

void
semantic_report_function_generic_bound_failure(SemanticContext *ctx,
                                               ASTNode *site,
                                               const char *function_name,
                                               const char *param_name,
                                               const char *bound_name,
                                               const char *bounds_text,
                                               const char *expected_sig,
                                               const char *actual_sig,
                                               const char *concrete_name);

void
semantic_report_class_generic_bound_failure(SemanticContext *ctx,
                                            ASTNode *site,
                                            const char *class_name,
                                            const char *param_name,
                                            const char *bound_name,
                                            const char *bounds_text,
                                            const char *expected_text,
                                            const char *actual_text,
                                            const char *concrete_name,
                                            const char *site_label);

#endif

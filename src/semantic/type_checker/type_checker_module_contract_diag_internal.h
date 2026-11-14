#ifndef PERGYRA_TYPE_CHECKER_MODULE_CONTRACT_DIAG_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_MODULE_CONTRACT_DIAG_INTERNAL_H

#include "type_checker_internal.h"

void report_subject_ability_requirement_mismatch(SemanticContext *ctx,
                                                 const ASTNode *site,
                                                 const char *owner_label,
                                                 const char *owner_name,
                                                 const char *subject_label,
                                                 const char *subject_name,
                                                 const char *required_text,
                                                 const char *actual_text,
                                                 const char *fix_tail);

#endif

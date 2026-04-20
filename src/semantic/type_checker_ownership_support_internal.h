#ifndef PERGYRA_TYPE_CHECKER_OWNERSHIP_SUPPORT_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_OWNERSHIP_SUPPORT_INTERNAL_H

#include "type_checker_internal.h"

char *
semantic_assignment_target_path(ASTNode *expr);

const char *
semantic_borrowed_boundary_root_name(ASTNode *expr, SemanticContext *ctx);

#endif

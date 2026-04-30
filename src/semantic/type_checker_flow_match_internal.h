#ifndef TYPE_CHECKER_FLOW_MATCH_INTERNAL_H
#define TYPE_CHECKER_FLOW_MATCH_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "type_checker_internal.h"

bool match_pattern_is_named_variant(ASTNode *pat,
                                    const char **name_out,
                                    ASTNode ***args_out,
                                    size_t *arg_count_out);

ASTNode *find_enum_decl_for_type(SemanticContext *ctx, const Type *type);

#endif

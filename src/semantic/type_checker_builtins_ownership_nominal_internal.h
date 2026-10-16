#ifndef PGY_TYPE_CHECKER_BUILTINS_OWNERSHIP_NOMINAL_INTERNAL_H
#define PGY_TYPE_CHECKER_BUILTINS_OWNERSHIP_NOMINAL_INTERNAL_H

#include "type_checker.h"

/* Private cross-component declarations; semantic authority is unchanged. */
Type *nominal_builtin_normalize_type(Type *type);
Type *type_check_text_builder_builtin(ASTNode *call, SemanticContext *ctx,
                                      BuiltinKind kind);

#endif

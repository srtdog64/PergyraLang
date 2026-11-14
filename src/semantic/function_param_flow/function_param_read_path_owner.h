#ifndef PERGYRA_SEMANTIC_FUNCTION_PARAM_READ_PATH_OWNER_H
#define PERGYRA_SEMANTIC_FUNCTION_PARAM_READ_PATH_OWNER_H

/* The access paths a routine reads of a formal: every place the formal
 * appears reads its whole access path there, except a path handed to a
 * declared callee, which reads what that callee reads of its own formal. */
#include "../type_checker.h"

#include <stdbool.h>
#include <stddef.h>

/* The callee that takes argument `argument` of `call` reads nothing of it at
 * or below `rest` (".a.b" under the argument). False whenever the paths
 * cannot be named: an unresolved callee, a routine with a deferred,
 * concurrent or closure body, or a chain deeper than the owner follows. */
bool function_param_read_paths_miss(SemanticContext *ctx, ASTNode *call, size_t argument, const char *rest);

#endif

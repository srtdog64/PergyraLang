#ifndef PERGYRA_HIR_ANALYSIS_H
#define PERGYRA_HIR_ANALYSIS_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

bool hir_ast_contains_control_flow(ASTNode *node);
bool hir_collect_func_signature_refs(ASTNode *node,
                                     const char ***names,
                                     size_t *count,
                                     size_t *capacity);
bool hir_collect_intent_signature_refs(ASTNode *node,
                                       const char ***names,
                                       size_t *count,
                                       size_t *capacity);
bool hir_collect_direct_calls(ASTNode *node,
                              const char ***names,
                              size_t *count,
                              size_t *capacity);

#endif

#ifndef PERGYRA_AST_ANALYSIS_H
#define PERGYRA_AST_ANALYSIS_H

#include "ast.h"

#include <stdbool.h>

typedef bool (*ASTIdentifierPredicate)(const char *name, void *userdata);

bool ast_contains_identifier_call(const ASTNode *node,
                                  ASTIdentifierPredicate predicate,
                                  void *userdata);
bool ast_contains_identifier_ref(const ASTNode *node,
                                 ASTIdentifierPredicate predicate,
                                 void *userdata);
bool ast_contains_free_identifier_ref(const ASTNode *node,
                                      const char *name);
bool ast_uses_intent_observability_surface(const ASTNode *node);
bool ast_uses_thread_pool_surface(const ASTNode *node);

#endif /* PERGYRA_AST_ANALYSIS_H */

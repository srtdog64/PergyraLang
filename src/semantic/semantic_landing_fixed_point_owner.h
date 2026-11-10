#ifndef PERGYRA_SEMANTIC_LANDING_FIXED_POINT_OWNER_H
#define PERGYRA_SEMANTIC_LANDING_FIXED_POINT_OWNER_H

#include "semantic.h"

typedef ASTNode *(*SemanticLandingReload)(void *context,
                                          char **error_message);

/* Reanalyzes one unchanged source identity namespace until no new compiler-
 * owned String store or loan-copy landing is selected.  A continuing round
 * destroys the previous AST and replaces *ast through reload. */
SemanticResult *semantic_analyze_landing_fixed_point(
    ASTNode **ast,
    SemanticLandingReload reload,
    void *reload_context,
    char **error_message);

#endif

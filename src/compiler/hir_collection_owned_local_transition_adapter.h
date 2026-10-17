#ifndef PGY_HIR_COLLECTION_OWNED_LOCAL_TRANSITION_ADAPTER_H
#define PGY_HIR_COLLECTION_OWNED_LOCAL_TRANSITION_ADAPTER_H
#include "hir.h"
bool hir_collection_owned_local_transitions_validate(const HIRRoutine *routine,
                                                    char **error_message);
/* Reuses the owned-expression vocabulary, without mutation cardinality rules. */
bool hir_collection_source_expression_current(const PgyCollectionEffectSnapshot *snapshot,
    const HIRRoutine *routine, ASTNode *root);
#endif

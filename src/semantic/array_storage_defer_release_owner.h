#ifndef PERGYRA_ARRAY_STORAGE_DEFER_RELEASE_OWNER_H
#define PERGYRA_ARRAY_STORAGE_DEFER_RELEASE_OWNER_H

#include <stdbool.h>

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;

/* A defer cleanup runs when the routine exits, so the Array it releases must
 * still own its storage at every exit, not only where the defer was written.
 * The defer body records each release (ArrayDrop, ArrayDropOwnedStrings, an
 * own handoff of a named Array) on the routine's function scope. */
bool semantic_array_storage_record_defer_release(ASTNode *receiver,
    bool owned_strings, SemanticContext *ctx);
/* A later binding move would transfer the storage while the registered
 * cleanup still names the source binding. Refuse at that move site. */
bool semantic_array_storage_admit_move_before_defer_release(
    ASTNode *source, SemanticContext *ctx);
/* Each exit (a return, after its value, or a fallthrough) admits every
 * recorded release again against the binding's state there. `returned` is
 * the return's value, or NULL; returning the released binding is refused. */
bool semantic_array_storage_defer_releases_at_exit(ASTNode *site,
    const ASTNode *returned, SemanticContext *ctx);
/* A block that falls through ends its bindings. A release of one is
 * admitted there, against its last state, and settled: the routine's
 * later exits cannot change a binding nothing can name. */
bool semantic_array_storage_defer_releases_at_block_exit(ASTNode *site,
    SemanticContext *ctx);

#endif

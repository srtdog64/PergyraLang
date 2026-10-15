/* Private native projection validation view. Adapters admit actual typed
 * operations; this owner validates their ordered CFG lifetime witnesses. */
#ifndef PGY_COLLECTION_OWNED_LOCAL_TRANSITION_OWNER_H
#define PGY_COLLECTION_OWNED_LOCAL_TRANSITION_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../semantic/collection_ownership_fact.h"

typedef enum {
    PGY_COLLECTION_TRANSITION_BIRTH = 1,
    PGY_COLLECTION_TRANSITION_MOVE,
    PGY_COLLECTION_TRANSITION_DROP
} PgyCollectionTransitionOperationKind;

typedef struct {
    PgyCollectionTransitionOperationKind kind;
    uint32_t binding_syntax_id;
    uint32_t source_binding_syntax_id;
    uint32_t syntax_id;
} PgyCollectionTransitionOperation;

typedef struct {
    bool reachable;
    bool terminal;
    size_t predecessor_start;
    size_t predecessor_count;
    size_t operation_start;
    size_t operation_count;
} PgyCollectionTransitionBlock;

typedef struct {
    uint32_t function_syntax_id;
    const PgyCollectionOwnershipFact *bindings;
    size_t binding_count;
    const PgyCollectionTransitionBlock *blocks;
    size_t block_count;
    const size_t *predecessors;
    size_t predecessor_count;
    const PgyCollectionTransitionOperation *operations;
    size_t operation_count;
    size_t entry_block;
} PgyCollectionTransitionView;

/* The subset is owned local birth/transfer facts, never formal/unknown rows. */
bool pgy_collection_owned_local_fact_tracked(
    const PgyCollectionOwnershipFact *fact);
bool pgy_collection_owned_local_transitions_validate(
    const PgyCollectionTransitionView *view, char **error_message);

#endif

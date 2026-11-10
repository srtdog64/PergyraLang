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
    PGY_COLLECTION_TRANSITION_DROP,
    PGY_COLLECTION_TRANSITION_OWNED_STRING_SET,
    PGY_COLLECTION_TRANSITION_OWNED_STRING_POP,
    PGY_COLLECTION_TRANSITION_OWNED_STRING_PUSH
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

/* One cleanup that executes only while control crosses predecessor ->
 * successor. SIZE_MAX names a terminal return edge. HIR may defer issuance
 * of these rows to MIR, but MIR validation must provide every actual row. */
typedef struct {
    size_t predecessor;
    size_t successor;
    uint32_t binding_syntax_id;
} PgyCollectionTransitionRetirement;

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
    const PgyCollectionTransitionRetirement *retirements;
    size_t retirement_count;
    /* HIR may name exact retired bindings whose cleanup topology is issued
     * only after MIR exists. MIR must pass an empty pending set and provide
     * concrete retirement rows instead. */
    const uint32_t *pending_retirement_bindings;
    size_t pending_retirement_count;
} PgyCollectionTransitionView;

/* The subset is owned local birth/transfer facts, never formal/unknown rows. */
bool pgy_collection_owned_local_fact_tracked(
    const PgyCollectionOwnershipFact *fact);
bool pgy_collection_owned_local_transitions_validate(
    const PgyCollectionTransitionView *view, char **error_message);

#endif

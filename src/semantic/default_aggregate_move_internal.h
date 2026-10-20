#ifndef PGY_DEFAULT_AGGREGATE_MOVE_INTERNAL_H
#define PGY_DEFAULT_AGGREGATE_MOVE_INTERNAL_H

/* State the default-aggregate move owner and its copy owner share while
 * they walk one routine in execution order. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "function_param_flow_summary_internal.h"

typedef struct {
    uint32_t binding;
    char *fields;          /* ".a.b" below the binding; "" is the whole value */
    const char *callee;
    const char *param;
    /* The read-and-return call that lent this path, the copy of a path whose
     * other side changed (with no callee), or NULL for a move. */
    ASTNode *lending_call;
    /* Nonzero for a copy row: the binding that shares this path's storage
     * since `lending_call` copied it. A copy row refuses nothing itself. */
    uint32_t carrier;
} MovedPath;

typedef struct {
    MovedPath *rows;
    size_t count, capacity;
} MoveState;

typedef struct {
    SemanticContext *ctx;
    ASTNode *routine;
    bool failed;
    /* Each refused use is reported once even though loops are walked twice. */
    const ASTNode **reported;
    size_t reported_count, reported_capacity;
    /* Loan verdicts of lending calls, each checked once. */
    struct { const ASTNode *call; bool admitted; FunctionParamFlowLoanFailure failure; } *lent;
    size_t lent_count, lent_capacity;
    /* Calls whose result a Clone copies into storage of its own. */
    const ASTNode **copied;
    size_t copied_count, copied_capacity;
} MoveWalk;

bool move_walk_state_add(MoveWalk *walk, MoveState *state, uint32_t binding, const char *fields,
    const char *callee, const char *param, ASTNode *lending_call, uint32_t carrier);
/* Access path of an identifier, member chain or element access. An element
 * names its whole Array. Returns false for any other expression. */
bool move_walk_access_path(const ASTNode *node, uint32_t *binding, char *buf, size_t size);
/* `prefix` names the same value as `fields` or a value containing it. */
bool move_walk_path_contains(const char *prefix, const char *fields);

/* Records that `value`, or each path a value built from it copies, now
 * shares storage with `carrier`. */
void move_walk_copy_record(MoveWalk *walk, MoveState *state, ASTNode *value, uint32_t carrier);
/* The storage `path` names changes in place: every other side of a copy
 * that shares it, directly or through other copies, is refused at its next
 * use. */
void move_walk_copy_changed(MoveWalk *walk, MoveState *state, const ASTNode *path);
/* `binding` takes a fresh value: its copies and the rows on it end. */
void move_walk_copy_rebound(MoveState *state, uint32_t binding);

#endif

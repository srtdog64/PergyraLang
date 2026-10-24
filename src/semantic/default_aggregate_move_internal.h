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
     * since `lending_call` copied it, at `carrier_fields` below it ("" is
     * the whole value; NULL on other rows). A copy row refuses nothing
     * itself. */
    uint32_t carrier;
    char *carrier_fields;
    /* The argument the moving or lending call took. A copy stands there when
     * a moved path is used again, and when a refused loan has no other place
     * a copy can stand. */
    ASTNode *argument;
} MovedPath;

typedef struct {
    MovedPath *rows;
    size_t count, capacity;
} MoveState;

typedef struct MoveAssignmentBackingLoan {
    uint32_t binding;
    char fields[512];
    const ASTNode *receiver;
    const ASTNode *assignment;
    struct MoveAssignmentBackingLoan *previous;
} MoveAssignmentBackingLoan;

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
    /* The call and argument ordinal of the argument being walked, if any. */
    ASTNode *argument_call;
    size_t argument_ordinal;
    /* Stack-local loans held while an indexed assignment has captured an
     * address into collection backing but has not performed its store. */
    MoveAssignmentBackingLoan *assignment_loan;
} MoveWalk;

bool move_walk_state_add(MoveWalk *walk, MoveState *state, uint32_t binding, const char *fields,
    const char *callee, const char *param, ASTNode *lending_call, uint32_t carrier, const char *carrier_fields,
    ASTNode *argument);
/* Access path of an identifier, member chain or element access. An element
 * names its whole Array. Returns false for any other expression. */
bool move_walk_access_path(const ASTNode *node, uint32_t *binding, char *buf, size_t size);
/* `prefix` names the same value as `fields` or a value containing it. */
bool move_walk_path_contains(const char *prefix, const char *fields);

/* Records that `value`, or each path a value built from it copies, now
 * shares storage with the path `carrier_fields` of `carrier`. */
void move_walk_copy_record(MoveWalk *walk, MoveState *state, ASTNode *value, uint32_t carrier,
    const char *carrier_fields);
/* The storage `path` names changes in place: every other side of a copy
 * that shares it, directly or through other copies, is refused at its next
 * use. */
void move_walk_copy_changed(MoveWalk *walk, MoveState *state, const ASTNode *path);
/* An inout call changes `arg`: a struct in the fields its callee's formal
 * does not preserve, anything else whole. */
void move_walk_copy_inout_changed(MoveWalk *walk, MoveState *state, const ASTNode *arg, ASTNode *callee,
    size_t formal);
/* Two parts of the value `value` builds name one storage: the earlier one
 * is refused and copied where the value takes it. */
void move_walk_copy_parts_distinct(MoveWalk *walk, const MoveState *state, ASTNode *value);
/* True the first time `node` is reported: loops are walked twice. */
bool move_walk_reported_once(MoveWalk *walk, const ASTNode *node);
/* `binding` takes a fresh value: its copies and the rows on it end. */
void move_walk_copy_rebound(MoveState *state, uint32_t binding);
/* At an exit of the routine, an inout formal's path that shares storage
 * with a copy changed since is refused. */
void move_walk_copy_inout_at_exit(MoveWalk *walk, const MoveState *state, ASTNode *exit);

/* Call and indexed-assignment effects are a separate responsibility from
 * control-flow joining, but consume the same per-routine move state. */
bool move_walk_note_copied_argument(MoveWalk *walk, const ASTNode *call);
MoveAssignmentBackingLoan *move_walk_assignment_loan_for_path(
    MoveWalk *walk, const MoveState *state, const ASTNode *path,
    MoveAssignmentBackingLoan *loans);
void move_walk_refuse_assignment_loan_change(MoveWalk *walk,
    const MoveAssignmentBackingLoan *loan, ASTNode *site);
void move_walk_check_assignment_loan_call(MoveWalk *walk,
    const MoveState *state, ASTNode *call);
void move_walk_apply_call_moves(MoveWalk *walk, MoveState *state,
    ASTNode *call);
bool move_walk_reject_overlapping_moving_arguments(MoveWalk *walk,
    ASTNode *call);

#endif

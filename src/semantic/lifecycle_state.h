/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Domain-Lifecycle State Engine (N-state lift of the slot 2-state model).
 *
 * Companion to docs/semantics/12_domain_lifecycle_evidence.md.
 *
 * The slot analyzer tracks a binary live/released state per resource and warns
 * when a branch releases on only one path. This module generalises that to an
 * N-state domain lifecycle (e.g. Payment: Pending -> Authorized -> Captured) and
 * realises the doc/12 risk-scaled rule at a single decision point
 * (lc_apply_op):
 *
 *   - current state KNOWN and violates the op's precondition  -> STATIC reject
 *   - current state AMBIGUOUS (differs across a prior branch)  -> needs a runtime
 *     evidence check (the fail-closed state tag), not a static reject
 *   - precondition satisfied                                   -> ok, advance
 *
 * This is the pure engine: no AST, no surface. It is unit-testable on a
 * hand-built machine and is the foundation the (future) state-declaration
 * surface + AST walk plug into.
 */

#ifndef PERGYRA_LIFECYCLE_STATE_H
#define PERGYRA_LIFECYCLE_STATE_H

#include <stdbool.h>
#include <stddef.h>

/* A tracked value's lifecycle state during analysis. Non-negative values index
 * into LcMachine.state_names; the two negatives are analysis lattice points. */
typedef int LcState;
#define LC_UNINIT    (-1)  /* not yet constructed / no state established */
#define LC_AMBIGUOUS (-2)  /* join of branches that ended in different states */

/* One allowed transition: applying `op` in state `from` moves to `to`. */
typedef struct {
    int from;
    int op;
    int to;
} LcTransition;

/* A declared domain state machine. The transition table is the single source of
 * truth: an op is permitted in exactly the `from` states for which a transition
 * with that op exists (so a multi-state op like Cancel:{Pending,Authorized} is
 * just two transitions). A transition with from == LC_UNINIT is a constructor
 * (establishes the first state from "before construction"). */
typedef struct {
    const char *const *state_names;   /* state_count entries */
    int                state_count;
    const char *const *op_names;       /* op_count entries (diagnostics only) */
    int                op_count;
    const LcTransition *transitions;
    int                transition_count;
} LcMachine;

typedef enum {
    LC_OK,                 /* precondition held; state advanced */
    LC_NEEDS_RUNTIME_CHECK,/* state ambiguous; emit fail-closed runtime guard */
    LC_ERR_PRECONDITION,   /* known state violates the op's required state */
    LC_ERR_NO_TRANSITION   /* no declared transition for (state, op) */
} LcResult;

/*
 * Apply `op` to a value currently in `cur`.
 *   *next is the resulting state (LC_AMBIGUOUS stays ambiguous).
 *   Returns the verdict per the risk-scaled rule above.
 * A NULL machine or out-of-range op returns LC_ERR_NO_TRANSITION.
 */
LcResult lc_apply_op(const LcMachine *m, LcState cur, int op, LcState *next);

/*
 * Merge two branch-exit states into the state at the control-flow join.
 * Equal states merge to themselves; LC_UNINIT is absorbing-from-below
 * (uninit on either side stays the known side is NOT assumed - see impl);
 * any genuine disagreement yields LC_AMBIGUOUS. This mirrors the slot
 * analyzer's "released on only one branch" divergence, generalised.
 */
LcState lc_merge(LcState a, LcState b);

/* Human-readable state name for diagnostics ("<uninit>" / "<ambiguous>"). */
const char *lc_state_name(const LcMachine *m, LcState s);

#endif /* PERGYRA_LIFECYCLE_STATE_H */

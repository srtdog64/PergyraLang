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
#include <stdint.h>

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

/* Bitmask of the real (>= 0) states from which `op` has a transition, i.e. the
 * states in which `op` is permitted. Lowered into the runtime guard's
 * `valid_mask`. Constructor transitions (from == LC_UNINIT) are excluded since a
 * live runtime state is always a real state index. */
uint32_t lc_op_valid_from_mask(const LcMachine *m, int op);

/* ----------------------------------------------------------------
 * Lifecycle declaration registry.
 *
 * Populated by the semantic lifecycle pass from AST_LIFECYCLE_DECL nodes.
 * Neutral storage: no AST dependency in the engine itself. Fixed-capacity
 * (no malloc); reset at the start of each lifecycle analysis run.
 * ---------------------------------------------------------------- */

#define LC_MAX_SPECS   64
#define LC_NAME_LEN    64
#define LC_MAX_STATES  32
#define LC_MAX_OPS     32
#define LC_MAX_TRANS   64

typedef struct {
    char         subject[LC_NAME_LEN];
    char         state_buf[LC_MAX_STATES][LC_NAME_LEN];
    const char  *state_names[LC_MAX_STATES];
    int          state_count;
    char         op_buf[LC_MAX_OPS][LC_NAME_LEN];
    const char  *op_names[LC_MAX_OPS];
    int          op_count;
    LcTransition transitions[LC_MAX_TRANS];
    int          transition_count;
} LcSpec;

void           lc_registry_reset(void);
/* Begin a spec for `subject`; returns spec id, or -1 on overflow/duplicate. */
int            lc_registry_begin(const char *subject);
/* Add `op: from -> to` to spec `sid`, resolving/creating state+op indices.
 * Returns false on overflow. */
bool           lc_registry_add_transition(int sid, const char *op,
                                          const char *from, const char *to);
int            lc_registry_count(void);
const LcSpec  *lc_registry_at(int i);
const LcSpec  *lc_registry_find(const char *subject);

/* ----------------------------------------------------------------
 * Lifecycle runtime-guard side-table.
 *
 * The semantic pass annotates AST nodes (a governed `let`, or a `v.Op()` call)
 * that need runtime state-tag lowering. Keyed by the node pointer so both
 * backends -- which both consume the same AST nodes at their member-call /
 * let emit -- read the same annotation and emit the same runtime call, keeping
 * the C and LLVM lowerings uniform by construction. Populated during semantic
 * analysis, read during codegen; reset at the start of each analysis run.
 * ---------------------------------------------------------------- */

#define LC_MAX_GUARDS 1024

typedef enum {
    LC_GUARD_SET,   /* record state (construction / proven transition) */
    LC_GUARD_CHECK  /* fail-closed check against valid_mask, then advance */
} LcGuardKind;

typedef struct {
    const void *node;        /* AST node pointer (key) */
    LcGuardKind kind;
    uint32_t    valid_mask;  /* permitted-from set (LC_GUARD_CHECK) */
    int         to_state;    /* state to record after the op (or init state) */
    char        op[LC_NAME_LEN];
    char        subject[LC_NAME_LEN];
} LcGuardSite;

void               lc_guard_reset(void);
/* Register a guard site for `node`. A second add for the same node overwrites
 * (idempotent re-annotation). Returns false only on capacity overflow. */
bool               lc_guard_add(const void *node, LcGuardKind kind,
                                uint32_t valid_mask, int to_state,
                                const char *op, const char *subject);
const LcGuardSite *lc_guard_find(const void *node);
int                lc_guard_count(void);

/* View an LcSpec as an LcMachine (points into the spec's arrays). */
LcMachine      lc_spec_machine(const LcSpec *s);
/* Resolve a state/op name to its index within a spec, or -1. */
int            lc_spec_state_index(const LcSpec *s, const char *name);
int            lc_spec_op_index(const LcSpec *s, const char *name);

#endif /* PERGYRA_LIFECYCLE_STATE_H */

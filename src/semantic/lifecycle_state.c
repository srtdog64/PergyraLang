/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Domain-lifecycle state engine - see lifecycle_state.h and
 * docs/semantics/12_domain_lifecycle_evidence.md.
 */

#include "lifecycle_state.h"

static const LcTransition *
lc_find_transition(const LcMachine *m, int from, int op)
{
    if (m == NULL)
        return NULL;
    for (int i = 0; i < m->transition_count; i++) {
        if (m->transitions[i].from == from && m->transitions[i].op == op)
            return &m->transitions[i];
    }
    return NULL;
}

/* True iff `op` has a transition out of EVERY declared state - i.e. it is
 * unconditionally valid, so an ambiguous current state cannot violate it. */
static bool
lc_op_valid_from_all_states(const LcMachine *m, int op)
{
    if (m == NULL || m->state_count <= 0)
        return false;
    for (int s = 0; s < m->state_count; s++) {
        if (lc_find_transition(m, s, op) == NULL)
            return false;
    }
    return true;
}

/* The common target if every transition for `op` lands in the same state;
 * otherwise LC_AMBIGUOUS (the op's result depends on which state it ran from). */
static LcState
lc_op_unique_target(const LcMachine *m, int op)
{
    LcState target = LC_UNINIT;
    bool seen = false;

    if (m == NULL)
        return LC_AMBIGUOUS;
    for (int i = 0; i < m->transition_count; i++) {
        if (m->transitions[i].op != op)
            continue;
        if (!seen) {
            target = m->transitions[i].to;
            seen = true;
        } else if (m->transitions[i].to != target) {
            return LC_AMBIGUOUS;
        }
    }
    return seen ? target : LC_AMBIGUOUS;
}

LcResult
lc_apply_op(const LcMachine *m, LcState cur, int op, LcState *next)
{
    LcState scratch;
    if (next == NULL)
        next = &scratch;
    *next = cur;

    if (m == NULL || op < 0 || op >= m->op_count)
        return LC_ERR_NO_TRANSITION;

    /* Known state (including LC_UNINIT, which is a real "before construction"
     * point a constructor transitions out of): the verdict is exact. */
    if (cur >= 0 || cur == LC_UNINIT) {
        const LcTransition *t = lc_find_transition(m, cur, op);
        if (t != NULL) {
            *next = t->to;
            return LC_OK;
        }
        /* No transition for (state, op): the op's precondition is not met in
         * this statically-known state -> fail closed at compile time. */
        return LC_ERR_PRECONDITION;
    }

    /* Ambiguous current state (join of branches that ended differently). We
     * cannot prove the precondition statically. */
    if (lc_op_valid_from_all_states(m, op)) {
        /* Valid from every state, so ambiguity cannot violate it. */
        *next = lc_op_unique_target(m, op);
        return LC_OK;
    }
    /* Might be invalid in some of the possible states: emit the fail-closed
     * runtime state-tag check (doc/12 section 2.3). After it narrows to a permitted
     * state the result is the op's target where deterministic. */
    *next = lc_op_unique_target(m, op);
    return LC_NEEDS_RUNTIME_CHECK;
}

LcState
lc_merge(LcState a, LcState b)
{
    if (a == b)
        return a;
    /* Any genuine disagreement -- including constructed-on-one-branch
     * (LC_UNINIT vs a state) -- is ambiguous, mirroring the slot analyzer's
     * "released on only one branch" divergence. */
    return LC_AMBIGUOUS;
}

const char *
lc_state_name(const LcMachine *m, LcState s)
{
    if (s == LC_UNINIT)
        return "<uninit>";
    if (s == LC_AMBIGUOUS)
        return "<ambiguous>";
    if (m == NULL || s < 0 || s >= m->state_count)
        return "<invalid>";
    return m->state_names[s];
}

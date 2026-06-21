/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Domain-lifecycle state engine - see lifecycle_state.h and
 * docs/semantics/12_domain_lifecycle_evidence.md.
 */

#include "lifecycle_state.h"

#include <string.h>

/*
 * Sole owner of the lifecycle spec registry storage. Holding the array + count
 * behind one accessor (a function-local static) makes this an explicit
 * single-owner source of truth, not an ambient file-global registry that any
 * in-file site could mutate independently. (build-source-inventory SoT gate.)
 */
typedef struct {
    LcSpec specs[LC_MAX_SPECS];
    int    count;
} LcSpecRegistry;

static LcSpecRegistry *
lc_spec_registry_owner(void)
{
    static LcSpecRegistry reg;
    return &reg;
}

static bool
lc_copy_name(char dest[LC_NAME_LEN], const char *src)
{
    size_t len;

    if (dest == NULL || src == NULL || src[0] == '\0')
        return false;
    len = strlen(src);
    if (len >= LC_NAME_LEN)
        return false;
    memcpy(dest, src, len + 1);
    return true;
}

static int
lc_spec_find_state_index(const LcSpec *s, const char *name)
{
    if (s == NULL || name == NULL)
        return -1;
    for (int i = 0; i < s->state_count; i++) {
        if (s->state_names[i] != NULL && strcmp(s->state_names[i], name) == 0)
            return i;
    }
    return -1;
}

static int
lc_spec_find_op_index(const LcSpec *s, const char *name)
{
    if (s == NULL || name == NULL)
        return -1;
    for (int i = 0; i < s->op_count; i++) {
        if (s->op_names[i] != NULL && strcmp(s->op_names[i], name) == 0)
            return i;
    }
    return -1;
}

static int
lc_spec_intern_state(LcSpec *s, const char *name)
{
    int existing;

    if (s == NULL || name == NULL)
        return -1;
    existing = lc_spec_find_state_index(s, name);
    if (existing >= 0)
        return existing;
    if (s->state_count >= LC_MAX_STATES)
        return -1;
    if (!lc_copy_name(s->state_buf[s->state_count], name))
        return -1;
    s->state_names[s->state_count] = s->state_buf[s->state_count];
    return s->state_count++;
}

static int
lc_spec_intern_op(LcSpec *s, const char *name)
{
    int existing;

    if (s == NULL || name == NULL)
        return -1;
    existing = lc_spec_find_op_index(s, name);
    if (existing >= 0)
        return existing;
    if (s->op_count >= LC_MAX_OPS)
        return -1;
    if (!lc_copy_name(s->op_buf[s->op_count], name))
        return -1;
    s->op_names[s->op_count] = s->op_buf[s->op_count];
    return s->op_count++;
}

static bool
lc_transition_from_name(LcSpec *s, const char *name, int *out)
{
    if (out == NULL)
        return false;
    if (name != NULL
        && (strcmp(name, "<uninit>") == 0 || strcmp(name, "UNINIT") == 0)) {
        *out = LC_UNINIT;
        return true;
    }
    *out = lc_spec_intern_state(s, name);
    return *out >= 0;
}

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

uint32_t
lc_op_valid_from_mask(const LcMachine *m, int op)
{
    uint32_t mask = 0;

    if (m == NULL)
        return 0;
    for (int i = 0; i < m->transition_count; i++) {
        if (m->transitions[i].op != op)
            continue;
        if (m->transitions[i].from >= 0 && m->transitions[i].from < 32)
            mask |= (uint32_t)1u << m->transitions[i].from;
    }
    return mask;
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

void
lc_registry_reset(void)
{
    LcSpecRegistry *R = lc_spec_registry_owner();
    memset(R->specs, 0, sizeof(R->specs));
    R->count = 0;
}

int
lc_registry_begin(const char *subject)
{
    LcSpecRegistry *R = lc_spec_registry_owner();
    LcSpec *spec;

    if (subject == NULL || subject[0] == '\0')
        return -1;
    if (lc_registry_find(subject) != NULL)
        return -1;
    if (R->count >= LC_MAX_SPECS)
        return -1;
    spec = &R->specs[R->count];
    memset(spec, 0, sizeof(*spec));
    if (!lc_copy_name(spec->subject, subject))
        return -1;
    return R->count++;
}

bool
lc_registry_add_transition(int sid, const char *op, const char *from,
                           const char *to)
{
    LcSpecRegistry *R = lc_spec_registry_owner();
    LcSpec *spec;
    int from_index;
    int op_index;
    int to_index;

    if (sid < 0 || sid >= R->count)
        return false;
    spec = &R->specs[sid];
    if (!lc_transition_from_name(spec, from, &from_index))
        return false;
    op_index = lc_spec_intern_op(spec, op);
    to_index = lc_spec_intern_state(spec, to);
    if (op_index < 0 || to_index < 0)
        return false;

    for (int i = 0; i < spec->transition_count; i++) {
        LcTransition *existing = &spec->transitions[i];
        if (existing->from == from_index && existing->op == op_index)
            return existing->to == to_index;
    }

    if (spec->transition_count >= LC_MAX_TRANS)
        return false;
    spec->transitions[spec->transition_count++] =
        (LcTransition){ from_index, op_index, to_index };
    return true;
}

int
lc_registry_count(void)
{
    return lc_spec_registry_owner()->count;
}

const LcSpec *
lc_registry_at(int i)
{
    LcSpecRegistry *R = lc_spec_registry_owner();
    if (i < 0 || i >= R->count)
        return NULL;
    return &R->specs[i];
}

const LcSpec *
lc_registry_find(const char *subject)
{
    LcSpecRegistry *R = lc_spec_registry_owner();
    if (subject == NULL)
        return NULL;
    for (int i = 0; i < R->count; i++) {
        if (strcmp(R->specs[i].subject, subject) == 0)
            return &R->specs[i];
    }
    return NULL;
}

LcMachine
lc_spec_machine(const LcSpec *s)
{
    LcMachine m = { 0 };
    if (s == NULL)
        return m;
    m.state_names = s->state_names;
    m.state_count = s->state_count;
    m.op_names = s->op_names;
    m.op_count = s->op_count;
    m.transitions = s->transitions;
    m.transition_count = s->transition_count;
    return m;
}

/* ---- runtime-guard side-table ---- */

/* Sole owner of the runtime-guard side-table (same single-owner rationale as
 * the spec registry above). */
typedef struct {
    LcGuardSite guards[LC_MAX_GUARDS];
    int         count;
} LcGuardRegistry;

static LcGuardRegistry *
lc_guard_registry_owner(void)
{
    static LcGuardRegistry reg;
    return &reg;
}

void
lc_guard_reset(void)
{
    lc_guard_registry_owner()->count = 0;
}

bool
lc_guard_add(const void *node, LcGuardKind kind, uint32_t valid_mask,
             int to_state, const char *op, const char *subject)
{
    LcGuardRegistry *R = lc_guard_registry_owner();
    LcGuardSite *site = NULL;

    if (node == NULL)
        return false;
    for (int i = 0; i < R->count; i++) {
        if (R->guards[i].node == node) {
            site = &R->guards[i];
            break;
        }
    }
    if (site == NULL) {
        if (R->count >= LC_MAX_GUARDS)
            return false;
        site = &R->guards[R->count++];
    }
    site->node = node;
    site->kind = kind;
    site->valid_mask = valid_mask;
    site->to_state = to_state;
    site->op[0] = '\0';
    site->subject[0] = '\0';
    if (op != NULL)
        lc_copy_name(site->op, op);
    if (subject != NULL)
        lc_copy_name(site->subject, subject);
    return true;
}

const LcGuardSite *
lc_guard_find(const void *node)
{
    LcGuardRegistry *R = lc_guard_registry_owner();
    if (node == NULL)
        return NULL;
    for (int i = 0; i < R->count; i++) {
        if (R->guards[i].node == node)
            return &R->guards[i];
    }
    return NULL;
}

int
lc_guard_count(void)
{
    return lc_guard_registry_owner()->count;
}

int
lc_spec_state_index(const LcSpec *s, const char *name)
{
    return lc_spec_find_state_index(s, name);
}

int
lc_spec_op_index(const LcSpec *s, const char *name)
{
    return lc_spec_find_op_index(s, name);
}

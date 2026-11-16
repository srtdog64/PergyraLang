#ifndef PERGYRA_CALL_HANDOFF_LEDGER_H
#define PERGYRA_CALL_HANDOFF_LEDGER_H

/* What the native call hand-off owner (collection_handoff_owner.c) decided,
 * as rows MIR JSON publishes beside the collection ownership rows (slice S2
 * of the hand-off rule migration, study
 * docs/audits/unified_handoff_rule_study_2026-11-16.md section 3). The values
 * are the call_handoff_rule.def vocabulary. In S2 nothing reads the rows for a
 * verdict: the self-host MIR reader validates them and the G2 measurement
 * compares them with the self-host's. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The effect class of one default-mode Array<String> formal of a routine
 * with a body. `divergence` is a PgyCallHandoffDivergenceId, or -1. */
typedef struct {
    uint32_t function_syntax_id;
    uint32_t parameter_index;
    unsigned members;
    int divergence;
} PgyCallHandoffClassRow;

/* One argument site of a tracked local Array<String> row: a default-mode
 * formal of a routine with a body, or an Array<String> field of a struct
 * constructor (class Keep). The lookup inputs are the ones the owner used;
 * `action` is the table's answer. `handed_off` says the binding's row was
 * marked handed-off at this site. `compiler_clone` says a compiler Clone of
 * the binding stands at the argument (origin Fresh). */
typedef struct {
    uint32_t function_syntax_id;
    uint32_t call_site;
    uint32_t binding_syntax_id;
    uint32_t parameter_index;
    char *callee;
    bool compiler_clone;
    unsigned members;
    unsigned origin, later, context, elements, action;
    int divergence;
    bool handed_off;
} PgyCallHandoffActionRow;

/* One compiler-placed Clone (the copy plan). `site` is the copied
 * expression's identity; `binding_syntax_id` is its binding when it is a
 * name, else 0; `callee` and `argument_index` name the call that takes the
 * copy, or are NULL and -1. */
typedef struct {
    uint32_t function_syntax_id;
    uint32_t site;
    uint32_t binding_syntax_id;
    char *callee;
    long argument_index;
} PgyCallHandoffCloneRow;

/* A routine whose body the hand-off walk could not enumerate publishes no
 * action or copy rows; a routine with a Clone site whose callee class has
 * no settled summary (reading one would start an analysis) leaves that site
 * out and keeps its other rows. Both are listed, so the gap is visible. */
typedef struct PgyCallHandoffLedger {
    PgyCallHandoffClassRow *classes;
    size_t class_count, class_capacity;
    PgyCallHandoffActionRow *actions;
    size_t action_count, action_capacity;
    PgyCallHandoffCloneRow *clones;
    size_t clone_count, clone_capacity;
    uint32_t *incomplete_function_syntax_ids;
    size_t incomplete_count, incomplete_capacity;
} PgyCallHandoffLedger;

void pgy_call_handoff_ledger_clear(PgyCallHandoffLedger *ledger);
/* Whether the walk of `function_syntax_id` was incomplete. */
bool pgy_call_handoff_ledger_incomplete(const PgyCallHandoffLedger *ledger,
    uint32_t function_syntax_id);

#endif

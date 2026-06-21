/*
 * Standalone unit test for the domain-lifecycle state engine.
 * Build: gcc -std=c11 -Wall -Wextra src/semantic/lifecycle_state.c \
 *            src/semantic/test_lifecycle_state.c -o /tmp/test_lc && /tmp/test_lc
 * (Not yet wired into the Makefile; this proves the engine ahead of the
 *  state-declaration surface + AST walk that will consume it.)
 */

#include "lifecycle_state.h"
#include <stdio.h>
#include <string.h>

/* Payment: Pending -> Authorized -> Captured, Cancel from Pending|Authorized. */
enum { S_Pending, S_Authorized, S_Captured, S_Cancelled };
enum { OP_Create, OP_Authorize, OP_Capture, OP_Cancel };

static const char *const STATES[] = {
    "Pending", "Authorized", "Captured", "Cancelled"
};
static const char *const OPS[] = {
    "Create", "Authorize", "Capture", "Cancel"
};
static const LcTransition TRANS[] = {
    { LC_UNINIT,    OP_Create,    S_Pending    },
    { S_Pending,    OP_Authorize, S_Authorized },
    { S_Authorized, OP_Capture,   S_Captured   },
    { S_Pending,    OP_Cancel,    S_Cancelled  },
    { S_Authorized, OP_Cancel,    S_Cancelled  },
};
static const LcMachine PAYMENT = {
    .state_names = STATES, .state_count = 4,
    .op_names = OPS, .op_count = 4,
    .transitions = TRANS, .transition_count = 5,
};

static int failures = 0;

static void
check(const char *label, LcResult got, LcResult want,
      LcState got_next, LcState want_next)
{
    bool ok = (got == want) && (got_next == want_next);
    printf("  [%s] %s -> result=%d next=%s\n",
           ok ? "PASS" : "FAIL", label, (int)got,
           lc_state_name(&PAYMENT, got_next));
    if (!ok) {
        printf("        expected result=%d next=%s\n",
               (int)want, lc_state_name(&PAYMENT, want_next));
        failures++;
    }
}

static void
check_bool(const char *label, bool ok)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
        failures++;
}

int
main(void)
{
    LcState s;
    printf("domain-lifecycle engine: Payment machine\n");

    /* 1. Valid sequence: Create -> Authorize -> Capture. */
    LcResult r = lc_apply_op(&PAYMENT, LC_UNINIT, OP_Create, &s);
    check("Create on <uninit>", r, LC_OK, s, S_Pending);
    r = lc_apply_op(&PAYMENT, s, OP_Authorize, &s);
    check("Authorize on Pending", r, LC_OK, s, S_Authorized);
    r = lc_apply_op(&PAYMENT, s, OP_Capture, &s);
    check("Capture on Authorized", r, LC_OK, s, S_Captured);

    /* 2. Precondition violation (capture before authorize): static reject. */
    r = lc_apply_op(&PAYMENT, S_Pending, OP_Capture, &s);
    check("Capture on Pending (too early)", r, LC_ERR_PRECONDITION, s, S_Pending);

    /* 3. Use before construction. */
    r = lc_apply_op(&PAYMENT, LC_UNINIT, OP_Authorize, &s);
    check("Authorize on <uninit>", r, LC_ERR_PRECONDITION, s, LC_UNINIT);

    /* 4. Branch ambiguity -> runtime check. merge(Pending,Authorized). */
    LcState joined = lc_merge(S_Pending, S_Authorized);
    check("merge(Pending,Authorized)", LC_OK, LC_OK, joined, LC_AMBIGUOUS);
    /* Cancel is valid from Pending AND Authorized but not all states ->
     * cannot prove statically -> fail-closed runtime tag; target deterministic
     * (both -> Cancelled). */
    r = lc_apply_op(&PAYMENT, joined, OP_Cancel, &s);
    check("Cancel on <ambiguous>", r, LC_NEEDS_RUNTIME_CHECK, s, S_Cancelled);
    /* Capture on ambiguous: NOT valid from Pending -> also needs runtime check. */
    r = lc_apply_op(&PAYMENT, joined, OP_Capture, &s);
    check("Capture on <ambiguous>", r, LC_NEEDS_RUNTIME_CHECK, s, S_Captured);

    /* 5. merge identity + uninit divergence. */
    check("merge(Authorized,Authorized)", LC_OK, LC_OK,
          lc_merge(S_Authorized, S_Authorized), S_Authorized);
    check("merge(<uninit>,Pending)", LC_OK, LC_OK,
          lc_merge(LC_UNINIT, S_Pending), LC_AMBIGUOUS);

    /* 6. Registry path: declaration storage is the SoT for future parser input. */
    lc_registry_reset();
    int sid = lc_registry_begin("Payment");
    check_bool("registry begin Payment", sid >= 0);
    check_bool("registry duplicate subject rejected",
               lc_registry_begin("Payment") < 0);
    check_bool("registry Create transition",
               lc_registry_add_transition(sid, "Create", "<uninit>", "Pending"));
    check_bool("registry Authorize transition",
               lc_registry_add_transition(sid, "Authorize", "Pending", "Authorized"));
    check_bool("registry Capture transition",
               lc_registry_add_transition(sid, "Capture", "Authorized", "Captured"));
    check_bool("registry rejects conflicting transition",
               !lc_registry_add_transition(sid, "Capture", "Authorized", "Pending"));
    check_bool("registry count is one", lc_registry_count() == 1);

    const LcSpec *spec = lc_registry_find("Payment");
    check_bool("registry find Payment", spec != NULL);
    check_bool("registry state index", lc_spec_state_index(spec, "Authorized") >= 0);
    check_bool("registry op index", lc_spec_op_index(spec, "Capture") >= 0);
    LcMachine registered = lc_spec_machine(spec);
    int pending = lc_spec_state_index(spec, "Pending");
    int authorize = lc_spec_op_index(spec, "Authorize");
    int authorized = lc_spec_state_index(spec, "Authorized");
    r = lc_apply_op(&registered, pending, authorize, &s);
    check("registered Authorize on Pending", r, LC_OK, s, authorized);

    printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
           failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

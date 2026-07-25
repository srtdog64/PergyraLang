#!/usr/bin/env bash
# Negative self-test for coq_kernel_check.sh: prove the axiom-budget gate
# actually BITES.
#
# The parent gate fail-closes structurally (a missing coqchk section, an empty
# parse, or a drifted budget all exit non-zero), but nothing demonstrated
# end-to-end that a planted `Admitted`/`Axiom` is actually caught. A gate whose
# reject path has never fired is indistinguishable from a no-op until the day it
# has to bite -- exactly the "negative gate" one of the four SoT CLOSED
# conditions demands.
#
# This is a controlled experiment. Both runs invoke the REAL gate (zero logic
# duplicated here) against a temp corpus via its PGY_COQ_PROOFS_DIR /
# PGY_COQ_EXPECTED_AXIOMS seams. The ONLY difference between the control and the
# treatment is one planted `Admitted`, so a green control plus a red treatment
# isolates the cause to the proof hole and nothing else.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GATE="$ROOT_DIR/tests/coq_kernel_check.sh"

# Same prover detection as the parent. This self-test is wired into the same
# rocq9 CI job where the prover exists, so a missing prover is a fail-closed
# error, not a skip: a self-test that quietly skips proves nothing about
# whether the gate bites.
if ! command -v rocq >/dev/null 2>&1 && ! command -v coqc >/dev/null 2>&1; then
    echo "coq-kernel-selftest: FAIL -- no prover found (looked for rocq, coqc);" \
         "the self-test cannot demonstrate the gate bites without one." >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# A self-contained CLEAN proof: closed with Qed, assumes nothing. This is the
# control corpus -- it must pass with an empty axiom budget.
cat > "$work/SelfTestClean.v" <<'PROOF'
Theorem selftest_clean : forall n : nat, n + 0 = n.
Proof.
  induction n as [| n IH]; simpl.
  - reflexivity.
  - rewrite IH. reflexivity.
Qed.
PROOF

# --- Control: clean corpus, expect zero axioms -> the gate MUST PASS ---
# If this fails, the self-test harness itself is broken (bad temp corpus, seam
# not wired), not the gate -- surface that distinctly so it is never mistaken
# for a real regression.
if ! PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS="" \
        bash "$GATE" >"$work/control.log" 2>&1; then
    echo "coq-kernel-selftest: FAIL -- control (clean corpus, no axioms) did" \
         "not pass. The self-test setup is broken, not the gate:" >&2
    sed 's/^/  | /' "$work/control.log" >&2
    exit 1
fi

# --- Treatment: same corpus + one planted Admitted -> the gate MUST FAIL ---
# `Admitted` closes the theorem by assumption; coqchk surfaces it as an axiom
# the corpus relies on. Against an empty expected budget this is a drift.
cat > "$work/SelfTestPlanted.v" <<'PROOF'
Theorem selftest_planted : forall n : nat, n + 0 = n.
Admitted.
PROOF

if PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS="" \
        bash "$GATE" >"$work/planted.log" 2>&1; then
    echo "coq-kernel-selftest: FAIL -- the gate PASSED a corpus containing a" \
         "planted Admitted. The axiom budget does not bite; a real proof hole" \
         "would slip through a green gate." >&2
    sed 's/^/  | /' "$work/planted.log" >&2
    exit 1
fi

# The rejection must be the axiom-budget drift specifically, and it must NAME
# the planted hole -- not some unrelated breakage (a missing section, a compile
# error) that would reject the corpus for the wrong reason and leave the actual
# budget logic still unexercised.
if ! grep -qF -- "axiom budget drifted" "$work/planted.log"; then
    echo "coq-kernel-selftest: FAIL -- the gate rejected the planted corpus for" \
         "the WRONG reason (expected 'axiom budget drifted'); the budget check" \
         "itself was not the thing that fired:" >&2
    sed 's/^/  | /' "$work/planted.log" >&2
    exit 1
fi

if ! grep -qF -- "selftest_planted" "$work/planted.log"; then
    echo "coq-kernel-selftest: FAIL -- the drift report did not name the planted" \
         "axiom 'selftest_planted'; the extraction is not reporting what leaked:" >&2
    sed 's/^/  | /' "$work/planted.log" >&2
    exit 1
fi

echo "coq-kernel-selftest: ok (the gate passes a clean corpus and fail-closes on" \
     "a planted Admitted, naming it -- the axiom budget is a live check, not a"  \
     "no-op that happens to be green)"

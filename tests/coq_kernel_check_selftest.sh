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
# Controlled corpora invoke the REAL gate (no budget/type logic duplicated
# here). The original empty-budget pair differs by one planted `Admitted`;
# the approved-API pair checks benign type/domain drift under unchanged names.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GATE="$ROOT_DIR/tests/coq_kernel_check.sh"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"

# Same prover detection as the parent. This self-test is wired into the same
# rocq9 CI job where the prover exists, so a missing prover is a fail-closed
# error, not a skip: a self-test that quietly skips proves nothing about
# whether the gate bites.
pgy_rocq_require

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

# Stale and orphan build products must remain untouched and never be consumed.
printf 'preserved-stale-artifact\n' > "$work/SelfTestClean.vo"
printf 'preserved-orphan-artifact\n' > "$work/Orphan.vo"
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

grep -Fxq 'preserved-stale-artifact' "$work/SelfTestClean.vo"
grep -Fxq 'preserved-orphan-artifact' "$work/Orphan.vo"

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

# Approved abstract APIs must also pass; unchanged names with benign type or
# domain drift must fail at the approval module, not at the name budget.
approved="$work/approved"
mkdir "$approved"
cp "$ROOT_DIR/tests/coq/assumption_budget/SlotCalculus.v" "$approved/"
cp "$ROOT_DIR/docs/semantics/proofs/AssumptionBudget.v" "$approved/"
if ! PGY_COQ_PROOFS_DIR="$approved" bash "$GATE" >"$work/approved.log" 2>&1; then
    echo "coq-kernel-selftest: FAIL -- approved API control rejected" >&2
    sed 's/^/  | /' "$work/approved.log" >&2
    exit 1
fi

for treatment in max-slot-type verifier-result-type access-mode-domain empty-contract; do
    mutant="$work/$treatment"
    mkdir "$mutant"
    cp "$approved"/*.v "$mutant/"
    case "$treatment" in
        max-slot-type)
            sed -i 's/Parameter MaxSlotId : nat\./Parameter MaxSlotId : bool./' "$mutant/SlotCalculus.v" ;;
        verifier-result-type)
            sed -i 's/AccessMode -> bool\./AccessMode -> nat./' "$mutant/SlotCalculus.v" ;;
        access-mode-domain)
            sed -i 's/| ModeClaim\./| ModeClaim | ModeExtended./' "$mutant/SlotCalculus.v" ;;
        empty-contract)
            cp /dev/null "$mutant/AssumptionBudget.v" ;;
    esac
    if PGY_COQ_PROOFS_DIR="$mutant" bash "$GATE" >"$work/$treatment.log" 2>&1; then
        echo "coq-kernel-selftest: FAIL -- same-name $treatment was accepted" >&2
        exit 1
    fi
    expected_reason='approved assumption contract type drift'
    if [ "$treatment" = empty-contract ]; then
        expected_reason='approved assumption contract export/binding drift'
    fi
    if ! grep -qF "$expected_reason" "$work/$treatment.log"; then
        echo "coq-kernel-selftest: FAIL -- $treatment rejected for the wrong reason" >&2
        sed 's/^/  | /' "$work/$treatment.log" >&2
        exit 1
    fi
done

missing="$work/missing-contract"
mkdir "$missing"
cp "$approved/SlotCalculus.v" "$missing/"
if PGY_COQ_PROOFS_DIR="$missing" bash "$GATE" >"$work/missing-contract.log" 2>&1 ||
   ! grep -qF 'approved assumption contract module is missing' "$work/missing-contract.log"; then
    echo "coq-kernel-selftest: FAIL -- missing approval module did not fail closed" >&2
    exit 1
fi

echo "coq-kernel-selftest: ok (clean and approved controls pass; planted admission," \
     "three same-name API contract drifts and empty/missing approval modules are refused)"

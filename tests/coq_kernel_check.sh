#!/usr/bin/env bash
# Kernel-re-check the Coq/Rocq proof corpus and pin its axiom budget.
#
# `coqc` only tells you the elaborator accepted a file. It does not tell you
# what the corpus *assumes*. `coqchk` re-runs the trusted kernel over the
# compiled .vo and reports the assumption base, so the things that would
# quietly hollow out a proof -- an `Axiom`, an `Admitted`, impredicative Set,
# type-in-type, an unsafe fixpoint, an assumed-positive inductive -- cannot
# slip in behind a green gate.
#
# The expected budget is the two deliberate abstract Parameters in
# SlotCalculus (an opaque token verifier and a slot-id bound). They are
# interface abstractions, not proof holes, but they ARE assumptions in the
# kernel's sense, so we name them rather than claim "0 axioms". Anything else
# appearing here is a regression in what the corpus actually establishes.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# PROOFS_DIR and EXPECTED_AXIOMS are overridable ONLY so this gate's own
# negative self-test (coq_kernel_check_selftest.sh) can point it at a planted
# corpus and prove the axiom-budget logic actually bites -- a "negative gate" in
# the SoT sense. Unset, they resolve to the real corpus and its two declared
# abstractions, so a normal run is unchanged. `${VAR-default}` (not `:-`) lets
# the self-test pass an explicitly empty EXPECTED_AXIOMS ("no axioms allowed").
PROOFS_DIR="${PGY_COQ_PROOFS_DIR:-$ROOT_DIR/docs/semantics/proofs}"

EXPECTED_AXIOMS="${PGY_COQ_EXPECTED_AXIOMS-SlotCalculus.MaxSlotId
SlotCalculus.verify_token}"

# Rocq 9 renamed the CLI: `rocq compile` replaces `coqc`, `rocqchk` replaces
# `coqchk`. The Rocq Platform installer still ships the legacy names, so a local
# run never notices -- but the official rocq/rocq-prover image ships ONLY the
# new names, and Ubuntu's apt `coq` (8.x) ships only the legacy ones. Detect
# instead of assuming, or this gate fails on a prover that is sitting right
# there.
if command -v rocq >/dev/null 2>&1; then
    coq_compile="rocq compile"
    coq_version_cmd="rocq --version"
elif command -v coqc >/dev/null 2>&1; then
    coq_compile="coqc"
    coq_version_cmd="coqc --version"
else
    echo "coq-kernel-check: FAIL -- no prover found (looked for rocq, coqc)" >&2
    exit 1
fi

if command -v rocqchk >/dev/null 2>&1; then
    coq_check="rocqchk"
elif command -v coqchk >/dev/null 2>&1; then
    coq_check="coqchk"
else
    echo "coq-kernel-check: FAIL -- no kernel checker found" \
         "(looked for rocqchk, coqchk); coqc alone cannot pin the axiom budget." >&2
    exit 1
fi

echo "coq-kernel-check: $($coq_version_cmd 2>&1 | head -1) [compile='$coq_compile' check='$coq_check']"

# `-Q . ""` binds PROOFS_DIR to the empty logical prefix so a proof can
# `Require Import PergyraCore` (a sibling .vo) rather than only stdlib. The
# corpus used to be 38 independent models with no cross-Require; the shared
# PergyraCore foundation is the first file others build on, so the load path is
# now load-bearing. Existing files Require only `Coq.*`, so this is inert for
# them. Kept identical on the coqchk side so the kernel resolves the same deps.
LOADPATH=(-Q . "")

# Foundation modules that other proofs Require must have their .vo built before
# the requiring file, regardless of the alphabetical glob order (an importer
# whose name sorts before its dependency -- e.g. AIRBinding before PergyraCore
# -- would otherwise fail). Compile these first, then the rest, skipping repeats.
FOUNDATION_FIRST=(PergyraCore.v)

compile_proof() {
    (cd "$PROOFS_DIR" && $coq_compile "${LOADPATH[@]}" "$1")
}

proof_count=0
for base in "${FOUNDATION_FIRST[@]}"; do
    [ -f "$PROOFS_DIR/$base" ] || continue
    compile_proof "$base"
    proof_count=$((proof_count + 1))
done
for proof_abs in "$PROOFS_DIR"/*.v; do
    base="$(basename "$proof_abs")"
    case " ${FOUNDATION_FIRST[*]} " in *" $base "*) continue;; esac
    compile_proof "$base"
    proof_count=$((proof_count + 1))
done

if [ "$proof_count" -eq 0 ]; then
    echo "coq-kernel-check: FAIL -- no .v proofs found under $PROOFS_DIR" >&2
    exit 1
fi
echo "coq-kernel-check: $proof_count proofs compiled"

report=$(cd "$PROOFS_DIR" && $coq_check "${LOADPATH[@]}" -silent -o $(ls *.vo | sed 's/\.vo$//') 2>&1)
echo "$report"

# Each escape hatch must be reported and must be empty. A missing section is
# also a failure -- we do not want a coqchk output change to silently drop a
# check.
check_none() {
    local needle="$1"
    local line
    line=$(printf '%s\n' "$report" | grep -F -- "$needle" || true)
    if [ -z "$line" ]; then
        echo "coq-kernel-check: FAIL -- coqchk report has no '$needle' section;" \
             "the check cannot be assumed to have passed." >&2
        exit 1
    fi
    if ! printf '%s\n' "$line" | grep -qF -- "<none>"; then
        echo "coq-kernel-check: FAIL -- kernel reports reliance on '$needle':" >&2
        echo "  $line" >&2
        exit 1
    fi
}

check_none "type-in-type"
check_none "unsafe (co)fixpoints"
check_none "positivity is assumed"

if ! printf '%s\n' "$report" | grep -qF -- "Set is predicative"; then
    echo "coq-kernel-check: FAIL -- kernel does not report 'Set is predicative';" \
         "an impredicative Set changes what the proofs mean." >&2
    exit 1
fi

actual_axioms=$(printf '%s\n' "$report" \
    | awk '/^\* Axioms:/ {inside=1; next} /^\*/ {inside=0} inside && NF {print $1}' \
    | sort -u)
expected_axioms=$(printf '%s\n' "$EXPECTED_AXIOMS" | sort -u)

if [ "$actual_axioms" != "$expected_axioms" ]; then
    echo "coq-kernel-check: FAIL -- axiom budget drifted." >&2
    echo "  expected (declared abstractions):" >&2
    printf '%s\n' "$expected_axioms" | sed 's/^/    /' >&2
    echo "  actual (what the kernel says the corpus assumes):" >&2
    printf '%s\n' "${actual_axioms:-<none>}" | sed 's/^/    /' >&2
    echo "  An added Axiom/Admitted, or a removed Parameter, must be a" >&2
    echo "  deliberate decision -- update EXPECTED_AXIOMS in this script." >&2
    exit 1
fi

axiom_count=$(printf '%s\n' "$expected_axioms" | wc -l | tr -d '[:space:]')
echo "coq-kernel-check: ok ($proof_count proofs kernel-verified;" \
     "axiom budget = $axiom_count declared abstractions, no admits,"  \
     "no unsafe kernel features)"

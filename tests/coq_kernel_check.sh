#!/usr/bin/env bash
# Kernel-re-check the Coq/Rocq proof corpus and pin assumption names AND types.
#
# `rocq compile` only tells you the elaborator accepted a file. It does not tell you
# what the corpus *assumes*. `rocqchk` re-runs the trusted kernel over the
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
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"

# PROOFS_DIR and EXPECTED_AXIOMS are overridable ONLY so this gate's own
# negative self-test (coq_kernel_check_selftest.sh) can point it at a planted
# corpus and prove the axiom-budget logic actually bites -- a "negative gate" in
# the SoT sense. Unset, they resolve to the real corpus and its two declared
# abstractions, so a normal run is unchanged. `${VAR-default}` (not `:-`) lets
# the self-test pass an explicitly empty EXPECTED_AXIOMS ("no axioms allowed").
PROOFS_DIR="${PGY_COQ_PROOFS_DIR:-$ROOT_DIR/docs/semantics/proofs}"

EXPECTED_AXIOMS="${PGY_COQ_EXPECTED_AXIOMS-SlotCalculus.MaxSlotId
SlotCalculus.verify_token}"
APPROVED_AXIOMS='SlotCalculus.MaxSlotId
SlotCalculus.verify_token'

# Only the approved API profile and the isolated, axiom-free self-test profile
# exist. An environment override is not permission to approve another axiom.
if [ "$EXPECTED_AXIOMS" != "$APPROVED_AXIOMS" ]; then
    if [ -n "$EXPECTED_AXIOMS" ] ||
       [ "$(cd "$PROOFS_DIR" && pwd -P)" = "$(cd "$ROOT_DIR/docs/semantics/proofs" && pwd -P)" ]; then
        echo "coq-kernel-check: FAIL -- unsupported assumption approval profile" >&2
        exit 1
    fi
elif [ ! -f "$PROOFS_DIR/AssumptionBudget.v" ]; then
    echo "coq-kernel-check: FAIL -- approved assumption contract module is missing" >&2
    exit 1
fi

pgy_rocq_require

# Source and generated artifacts never share authority or a directory. An old
# .vo, including an orphan, is neither read nor deleted by this gate.
source_proofs_dir="$PROOFS_DIR"
production_corpus=0
if [ "$(cd "$source_proofs_dir" && pwd -P)" = "$(cd "$ROOT_DIR/docs/semantics/proofs" && pwd -P)" ]; then
    production_corpus=1
fi
PROOFS_DIR="$(mktemp -d)"
trap 'rm -rf "$PROOFS_DIR"' EXIT
cp "$source_proofs_dir"/*.v "$PROOFS_DIR/"
# Permanent independent consumers share the production snapshot and kernel
# pass. Extraction drivers belong to their own executable gates, not here.
if [ "$production_corpus" -eq 1 ]; then
    for consumer in AsyncReuseRedteamRegression LifecycleAuthorityRedteamRegression \
                    ReuseIdentityAudit ProofRedteamMainRegression \
                    OwnershipCleanGCComparisonAudit MemoryBoundaryCompositionAudit \
                    OwnershipTeardownRedteam OwnershipCutoverPreflightAudit; do
        cp "$ROOT_DIR/tests/coq/$consumer.v" "$PROOFS_DIR/"
    done
fi
echo 'coq-kernel-check: fixed fresh source snapshot (sha256)'
(cd "$PROOFS_DIR" && sha256sum ./*.v)

# `-Q . ""` binds PROOFS_DIR to the empty logical prefix so a proof can
# `Require Import PergyraCore` (a sibling .vo) rather than only stdlib. The
# corpus used to be 38 independent models with no cross-Require; the shared
# PergyraCore foundation is the first file others build on, so the load path is
# now load-bearing. The compiler and checker resolve exactly the same deps.
LOADPATH=(-Q . "")

compile_proof() {
    if ! (cd "$PROOFS_DIR" && "${PGY_ROCQ_COMPILE[@]}" -q "${LOADPATH[@]}" "$1"); then
        if [ "$1" = "AssumptionBudget.v" ]; then
            echo "coq-kernel-check: FAIL -- approved assumption contract type drift" >&2
        else
            echo "coq-kernel-check: FAIL -- proof compilation failed: $1" >&2
        fi
        return 1
    fi
}

# Actual imports own dependency order, not a second hand-written foundation list.
dependency_order="$(cd "$PROOFS_DIR" && rocq dep "${LOADPATH[@]}" -sort ./*.v)"
read -r -a ORDERED_PROOFS <<<"$dependency_order"
proof_count=0
COMPILED_MODULES=()
for proof_abs in "${ORDERED_PROOFS[@]}"; do
    base="$(basename "$proof_abs")"
    compile_proof "$base"
    COMPILED_MODULES+=("${base%.v}")
    proof_count=$((proof_count + 1))
done

if [ "$proof_count" -eq 0 ]; then
    echo "coq-kernel-check: FAIL -- no .v proofs found under $PROOFS_DIR" >&2
    exit 1
fi
echo "coq-kernel-check: $proof_count proofs compiled"

# Consume the approval exports and kernel-check their actual-API links. A
# present-but-empty approval file must not restore the old names-only path.
contract_audit_summary=""
if [ "$EXPECTED_AXIOMS" = "$APPROVED_AXIOMS" ]; then
    contract_work="$PROOFS_DIR/approval"
    mkdir "$contract_work"
    cp "$ROOT_DIR/tests/coq/AssumptionBudgetAudit.v" "$contract_work/"
    LOADPATH+=(-Q "$contract_work" "")
    if ! (cd "$PROOFS_DIR" && "${PGY_ROCQ_COMPILE[@]}" -q "${LOADPATH[@]}" "$contract_work/AssumptionBudgetAudit.v"); then
        echo "coq-kernel-check: FAIL -- approved assumption contract export/binding drift" >&2
        exit 1
    fi
    COMPILED_MODULES+=(AssumptionBudgetAudit)
    contract_audit_summary=" plus approval export/binding consumer"
fi

# Check exactly the modules just compiled, not whatever `*.vo` happens to be on
# disk, so the kernel verdict is about this run's corpus and nothing else.
# `set -e` would abort here with the message trapped inside the assignment, so
# capture the status and print the report before judging it.
set +e
report=$(cd "$PROOFS_DIR" && "${PGY_ROCQ_CHECK[@]}" "${LOADPATH[@]}" -silent -o "${COMPILED_MODULES[@]}" 2>&1)
check_status=$?
set -e
if [ "$check_status" -ne 0 ]; then
    echo "coq-kernel-check: FAIL -- rocqchk exited $check_status:" >&2
    printf '%s\n' "$report" | sed 's/^/  | /' >&2
    exit 1
fi
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
if ! printf '%s\n' "$report" | grep -qF -- "Rewrite rules are not allowed"; then
    echo 'coq-kernel-check: FAIL -- rewrite-rule-free kernel theory was not reported' >&2
    exit 1
fi
# Rocq 9.3 reports the default indices-not-mattering dependencies (including
# stdlib equality). Preserve this visible theory profile; it is not an Axiom
# name and cannot honestly be folded into the two-name abstract API budget.
if ! printf '%s\n' "$report" | grep -qF -- 'Inductives relying on indices not mattering:'; then
    echo 'coq-kernel-check: FAIL -- indices theory dependency report is missing' >&2
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
    echo "  deliberate contract decision -- review AssumptionBudget.v and this gate." >&2
    exit 1
fi

if [ -n "${PGY_ROCQ_EXTRACT_DIR:-}" ]; then
    [ -d "$PGY_ROCQ_EXTRACT_DIR" ] || { echo 'extraction destination is missing' >&2; exit 1; }
    # Export only outputs generated by this successful fresh kernel run.
    extraction_count=0
    for extracted in "$PROOFS_DIR"/*.ml "$PROOFS_DIR"/*.mli; do
        [ -f "$extracted" ] || continue
        cp "$extracted" "$PGY_ROCQ_EXTRACT_DIR/"
        extraction_count=$((extraction_count + 1))
    done
    [ "$extraction_count" -gt 0 ] || { echo 'no freshly extracted outputs' >&2; exit 1; }
fi

axiom_count=$(printf '%s\n' "$expected_axioms" | awk 'NF {n++} END {print n+0}')
echo "coq-kernel-check: ok ($proof_count proofs kernel-verified$contract_audit_summary;" \
     "axiom budget = $axiom_count declared abstractions with approved types, no admits,"  \
     "no unsafe kernel features)"

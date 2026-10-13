#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Coq/Rocq machine-check.
coq_proof_count=$(find "$ROOT_DIR/docs/semantics/proofs" -maxdepth 1 -name '*.v' \
    | wc -l | tr -d '[:space:]')
if [ "$coq_proof_count" -eq 0 ]; then
    echo "formal semantics Coq smoke: FAIL -- no .v proofs found under" \
         "docs/semantics/proofs (an empty corpus must not pass silently)" >&2
    exit 1
fi

# The kernel gate owns compilation, dependency order, fresh artifacts and the
# assumption verdict.
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
if ! command -v rocq >/dev/null 2>&1; then
    if [ "${PGY_ALLOW_MISSING_COQ:-0}" = "1" ]; then
        echo "formal semantics Coq smoke: DECLARED SKIP -- no admitted Rocq" \
             "PGY_ALLOW_MISSING_COQ=1; legacy Coq is not proof evidence"
        echo "  ${coq_proof_count} proofs were NOT machine-checked on this runner."
    else
        echo "formal semantics Coq smoke: FAIL -- no stable Rocq;" \
             "${coq_proof_count} proofs would go unchecked." >&2
        echo "  Install the pinned Rocq, or set PGY_ALLOW_MISSING_COQ=1 to declare" \
             "the skip explicitly." >&2
        exit 1
    fi
else
    pgy_rocq_require
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
    echo "formal semantics Coq smoke: ok (${coq_proof_count} proofs freshly kernel-checked)"
fi

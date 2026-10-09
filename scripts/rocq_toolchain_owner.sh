#!/usr/bin/env bash
# One admission contract for the project's proof compiler and kernel checker.
PGY_ROCQ_VERSION=9.3.0
PGY_ROCQ_STDLIB_VERSION=9.2.0
PGY_ROCQ_SWITCH=pgy-rocq-9.3.0
PGY_ROCQ_BOOTSTRAP_OCAML_VERSION=4.14.2
PGY_ROCQ_COMPILE=(rocq compile)
PGY_ROCQ_CHECK=(rocqchk)

pgy_rocq_require() {
    local observed checker_observed stdlib_observed
    command -v rocq >/dev/null 2>&1 || {
        echo '[rocq-toolchain] FAIL -- stable Rocq is required; no Coq fallback' >&2
        return 1
    }
    observed="$(rocq --version)" || return 1
    observed="$(printf '%s\n' "$observed" | sed -n 's/^The Rocq Prover, version \([^ ]*\).*$/\1/p')"
    if [ "$observed" != "$PGY_ROCQ_VERSION" ]; then
        echo "[rocq-toolchain] FAIL -- expected stable $PGY_ROCQ_VERSION, observed ${observed:-unknown}" >&2
        return 1
    fi
    command -v rocqchk >/dev/null 2>&1 || {
        echo '[rocq-toolchain] FAIL -- rocqchk is required; compilation alone is not kernel evidence' >&2
        return 1
    }
    checker_observed="$(rocqchk --version)" || return 1
    checker_observed="$(printf '%s\n' "$checker_observed" | sed -n 's/^The Rocq Proof Checker, version \([^ ]*\).*$/\1/p')"
    if [ "$checker_observed" != "$PGY_ROCQ_VERSION" ]; then
        echo "[rocq-toolchain] FAIL -- expected checker $PGY_ROCQ_VERSION, observed ${checker_observed:-unknown}" >&2
        return 1
    fi
    command -v opam >/dev/null 2>&1 || {
        echo '[rocq-toolchain] FAIL -- opam package provenance is required' >&2
        return 1
    }
    stdlib_observed="$(opam var rocq-stdlib:version 2>/dev/null)" || {
        echo '[rocq-toolchain] FAIL -- Stdlib package provenance is missing' >&2
        return 1
    }
    if [ "$stdlib_observed" != "$PGY_ROCQ_STDLIB_VERSION" ]; then
        echo "[rocq-toolchain] FAIL -- expected Stdlib $PGY_ROCQ_STDLIB_VERSION, observed ${stdlib_observed:-unknown}" >&2
        return 1
    fi
    echo "[rocq-toolchain] admitted stable Rocq $observed / Stdlib $stdlib_observed"
}

pgy_rocq_check_isolated() {
    local proof="$1" proof_work status
    pgy_rocq_require || return 1
    proof_work="$(mktemp -d)" || return 1
    cp "$proof" "$proof_work/" || { rmdir "$proof_work"; return 1; }
    if PGY_COQ_PROOFS_DIR="$proof_work" PGY_COQ_EXPECTED_AXIOMS='' \
        bash "$(dirname "${BASH_SOURCE[0]}")/../tests/coq_kernel_check.sh"; then
        status=0
    else
        status=$?
    fi
    rm -rf "$proof_work"
    return "$status"
}

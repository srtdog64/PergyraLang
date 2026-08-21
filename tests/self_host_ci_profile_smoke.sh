#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKEFILE="$ROOT_DIR/Makefile"
LINUX_STEPS="$ROOT_DIR/scripts/ci_linux_steps.sh"
MACOS_STEPS="$ROOT_DIR/scripts/ci_macos_steps.sh"
WINDOWS_STEPS="$ROOT_DIR/scripts/ci_windows_steps.sh"
WORKFLOW="$ROOT_DIR/.github/workflows/ci.yml"
DRIVER_BOOTSTRAP="$ROOT_DIR/tests/self_hosted/parity/driver_bootstrap.sh"

for file in \
    "$MAKEFILE" \
    "$LINUX_STEPS" \
    "$MACOS_STEPS" \
    "$WINDOWS_STEPS" \
    "$WORKFLOW" \
    "$DRIVER_BOOTSTRAP"; do
    if [[ ! -f "$file" ]]; then
        echo "[self-host-ci-profile] missing input: $file" >&2
        exit 1
    fi
done

for required in \
    'all: $(PGY) $(PGY_LSP) self-host-compiler' \
    'release: $(PGY) self-host-compiler' \
    'self-host-preparation-platform-test-smoke:' \
    'self-host-preparation-platform-parity-test-smoke:' \
    'self-host-preparation-exhaustive-parity-test-smoke:' \
    'self-host-preparation-parity-test-smoke: self-host-preparation-exhaustive-parity-test-smoke self-host-codegen-bootstrap-test-smoke self-host-driver-bootstrap-test-smoke self-host-hard-driver-rung2-parity-test-smoke' \
    'SELFHOST_SCALAR_GRAPH_PLAN_GATE ?= $(if $(filter 0,$(LLVM_ENABLED)),,self-host-direct-mir-scalar-graph-plan-test-smoke)' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-readonly-logical-record-single-value-result-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-owned-string-parameter-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-direct-scalar-callable-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-value-parameter-rebind-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-int-comparison-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-array-index-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-array-mutation-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-array-int-value-result-indexed-assignment-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-array-string-dynamic-indexed-assignment-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-nested-logical-record-array-string-indexed-assignment-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-value-result-logical-record-array-int-indexed-assignment-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-collection-phi-value-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-program-control-transfer-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-option-int-try-let-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-bool-sub-equals-short-circuit-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-option-bool-equality-short-circuit-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-process-args-direct-call-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-dir-walk-direct-call-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-file-exists-direct-call-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-populated-array-bool-literal-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-read-file-direct-call-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-array-string-nested-expression-literal-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-int-multiply-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-int-divide-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-remainder-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-addition-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-multiplication-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-subtraction-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-int-to-long-cast-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-division-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-phi-value-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-long-greater-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-program-routine-admission-diagnostic-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-set-string-value-parameter-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-array-string-readonly-ref-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-entrypoint-early-return-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-namespace-internal-call-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-one-mir-string-indexof-projection-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-one-mir-string-collection-builtin-projection-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke: self-host-direct-mir-scalar-multi-routine-test-smoke self-host-direct-mir-scalar-option-int-test-smoke self-host-direct-mir-scalar-option-string-test-smoke self-host-direct-mir-scalar-option-bool-test-smoke self-host-direct-mir-scalar-two-int-nominal-test-smoke self-host-direct-mir-scalar-logical-record-test-smoke self-host-direct-mir-scalar-logical-record-option-return-test-smoke self-host-direct-mir-scalar-recursive-logical-record-phi-test-smoke self-host-direct-mir-scalar-zero-parameter-callable-test-smoke self-host-direct-mir-scalar-array-int-value-result-test-smoke self-host-direct-mir-scalar-array-int-value-parameter-test-smoke self-host-direct-mir-scalar-array-string-value-parameter-test-smoke self-host-direct-mir-scalar-array-string-value-result-void-test-smoke self-host-direct-mir-scalar-owned-array-string-return-test-smoke self-host-direct-mir-scalar-logical-record-collection-fields-test-smoke self-host-direct-mir-scalar-nested-logical-record-array-bool-return-test-smoke self-host-direct-mir-scalar-array-int-return-test-smoke self-host-direct-mir-scalar-long-literal-return-test-smoke self-host-direct-mir-scalar-bool-array-string-value-result-test-smoke self-host-direct-mir-scalar-logical-record-value-result-test-smoke self-host-direct-mir-scalar-void-process-exit-test-smoke self-host-direct-mir-scalar-populated-array-int-literal-test-smoke self-host-direct-mir-scalar-logical-record-array-value-result-test-smoke self-host-direct-mir-scalar-logical-record-array-record-input-test-smoke self-host-direct-mir-scalar-logical-record-array-element-input-test-smoke self-host-direct-mir-scalar-logical-record-mixed-collection-value-result-test-smoke self-host-direct-mir-scalar-bool-mixed-collection-value-result-test-smoke self-host-direct-mir-scalar-void-logical-record-array-int-value-result-test-smoke self-host-direct-mir-scalar-logical-record-inputs-value-result-test-smoke self-host-direct-mir-scalar-readonly-logical-record-two-array-string-value-result-test-smoke self-host-direct-mir-scalar-int-two-array-string-value-result-test-smoke self-host-direct-mir-scalar-bool-two-array-string-two-array-int-value-result-test-smoke self-host-direct-mir-scalar-readonly-logical-record-array-bool-return-test-smoke self-host-direct-mir-scalar-owned-logical-record-return-test-smoke self-host-direct-mir-scalar-readonly-logical-record-string-array-string-value-result-test-smoke self-host-direct-mir-scalar-readonly-logical-record-two-logical-record-value-result-test-smoke self-host-direct-mir-scalar-logical-record-return-array-string-value-result-test-smoke self-host-direct-mir-composable-logical-record-return-test-smoke self-host-direct-mir-scalar-void-logical-record-array-string-value-result-test-smoke self-host-direct-mir-scalar-void-logical-record-three-string-array-string-value-result-test-smoke self-host-direct-mir-scalar-void-logical-record-four-string-array-string-value-result-test-smoke self-host-direct-mir-scalar-logical-record-array-value-parameter-test-smoke self-host-direct-mir-scalar-payload-free-enum-parameter-test-smoke' \
    'tests/self_hosted/parity/parser_parity.sh' \
    'tests/self_hosted/parity/semantic_parity.sh' \
    'tests/self_hosted/parity/codegen_parity.sh' \
    'tests/self_hosted/parity/driver_rung2_body_parity.sh'; do
    if ! grep -Fq "$required" "$MAKEFILE"; then
        echo "[self-host-ci-profile] platform profile missing: $required" >&2
        exit 1
    fi
done

array_param_recipe="$(
    sed -n \
        '/^self-host-one-mir-array-param-projection-test-smoke:/,/^self-host-one-mir-bool-logic-projection-test-smoke:/p' \
        "$MAKEFILE"
)"
if ! grep -Fq 'PGY_SELF_DRIVER_BIN="$(abspath $(SELF_HOST_DRIVER))"' \
        <<<"$array_param_recipe" ||
    grep -Fq 'PGY_BIN=' <<<"$array_param_recipe"; then
    echo "[self-host-ci-profile] Array parameter gate must consume the built self-host driver explicitly" >&2
    exit 1
fi

require_job_timeout() {
    local job="$1"
    local expected="$2"
    local actual
    actual="$(awk -v job="$job" '
        $0 == "  " job ":" { in_job = 1; next }
        in_job && /^  [A-Za-z0-9_-]+:/ { exit }
        in_job && /timeout-minutes:/ {
            sub(/^[[:space:]]*timeout-minutes:[[:space:]]*/, "")
            sub(/[[:space:]]*#.*/, "")
            print
            exit
        }
    ' "$WORKFLOW")"
    if [[ "$actual" != "$expected" ]]; then
        echo "[self-host-ci-profile] $job timeout drifted: expected $expected, got ${actual:-missing}" >&2
        exit 1
    fi
}

require_job_timeout "self-host-parity-linux" 90
require_job_timeout "self-host-bootstrap-linux" 60
require_job_timeout "self-host-codegen-bootstrap-linux" 30
require_job_timeout "build-windows" 90

self_host_parity_job="$(
    sed -n \
        '/^  self-host-parity-linux:/,/^  self-host-bootstrap-linux:/p' \
        "$WORKFLOW"
)"
if [[ "$(grep -Ec '^[[:space:]]+make ' <<<"$self_host_parity_job")" != "1" ]] ||
    ! grep -Fq 'make release' <<<"$self_host_parity_job"; then
    echo "[self-host-ci-profile] parity job must use one make invocation with the release pair first" >&2
    exit 1
fi

for steps in "$LINUX_STEPS" "$MACOS_STEPS" "$WINDOWS_STEPS"; do
    if ! grep -Fq 'self-host-preparation-platform-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] platform profile missing from $steps" >&2
        exit 1
    fi
    if grep -Fq ' self-host-preparation-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] full self-host proof leaked into $steps" >&2
        exit 1
    fi
done

for required in \
    'self-host-parity-linux:' \
    'self-host-bootstrap-linux:' \
    'self-host-codegen-bootstrap-linux:' \
    'timeout-minutes: 90' \
    'timeout-minutes: 60' \
    'timeout-minutes: 30' \
    'make release' \
    'self-host-hard-contract-test-smoke' \
    'self-host-intent-observability-runtime-test-smoke' \
    'self-host-preparation-exhaustive-parity-test-smoke' \
    'self-host-codegen-type-env-preseal-epoch-test-smoke' \
    'self-host-compiler-internal-caller-provenance-test-smoke' \
    'self-host-routine-build-storage-lifetime-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke' \
    'self-host-public-mir-json-replacement-test-smoke' \
    'run: make self-host-codegen-bootstrap-test-smoke' \
    'make self-host-driver-bootstrap-full-test-smoke' \
    'bash tests/selfhost_bootstrap_policy_corpus_smoke.sh' \
    'cancel-in-progress: true'; do
    if ! grep -Fq "$required" "$WORKFLOW"; then
        echo "[self-host-ci-profile] dedicated Linux proof job missing: $required" >&2
        exit 1
    fi
done

bootstrap_job="$(
    sed -n \
        '/^  self-host-bootstrap-linux:/,/^  self-host-codegen-bootstrap-linux:/p' \
        "$WORKFLOW"
)"
if grep -Fq 'make self-host-driver-bootstrap-test-smoke' <<<"$bootstrap_job"; then
    echo "[self-host-ci-profile] bounded-only bootstrap command reopened in the full fixed-point job" >&2
    exit 1
fi
if grep -Fq 'self-host-fixpoint-linux:' "$WORKFLOW"; then
    echo "[self-host-ci-profile] duplicate full fixed-point job reintroduced" >&2
    exit 1
fi

exhaustive_recipe="$(
    sed -n \
        '/^self-host-preparation-exhaustive-parity-test-smoke:/,/^self-host-runtime-boundary-parity-test-smoke:/p' \
        "$MAKEFILE"
)"
for forbidden in \
    'codegen_bootstrap.sh' \
    'driver_bootstrap.sh'; do
    if grep -Fq "$forbidden" <<<"$exhaustive_recipe"; then
        echo "[self-host-ci-profile] bootstrap leaked into exhaustive parity: $forbidden" >&2
        exit 1
    fi
done

for required in \
    'while sleep 60' \
    '[self-host-driver-bootstrap] still running' \
    'trap driver_bootstrap_stop_heartbeat EXIT' \
    '"$CODEGEN_BIN" --source "$driver_rel"' \
    '>"$DRIVER_SEED_C_RAW"' \
    'tr -d '\''\r'\'' <"$DRIVER_SEED_C_RAW" >"$DRIVER_SEED_C"'; do
    if ! grep -Fq "$required" "$DRIVER_BOOTSTRAP"; then
        echo "[self-host-ci-profile] driver bootstrap heartbeat missing: $required" >&2
        exit 1
    fi
done
if grep -Fq '| tr -d '\''\r'\''' "$DRIVER_BOOTSTRAP"; then
    echo "[self-host-ci-profile] driver bootstrap overlaps codegen and normalization lifetimes" >&2
    exit 1
fi

platform_recipe="$(
    sed -n \
        '/^self-host-preparation-platform-parity-test-smoke:/,/^self-host-preparation-parity-test-smoke:/p' \
        "$MAKEFILE"
)"
for forbidden in \
    'selfcheck_sources.sh' \
    'completeness_ledger.sh' \
    'codegen_bootstrap.sh' \
    'driver_bootstrap.sh'; do
    if grep -Fq "$forbidden" <<<"$platform_recipe"; then
        echo "[self-host-ci-profile] exhaustive proof leaked into platform profile: $forbidden" >&2
        exit 1
    fi
done

echo "[self-host-ci-profile] exhaustive parity, bootstrap, and native platform jobs are isolated"

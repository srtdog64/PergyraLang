#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKEFILE="$ROOT_DIR/Makefile"
LINUX_STEPS="$ROOT_DIR/scripts/ci_linux_steps.sh"
MACOS_STEPS="$ROOT_DIR/scripts/ci_macos_steps.sh"
WINDOWS_STEPS="$ROOT_DIR/scripts/ci_windows_steps.sh"
PUSH_LINUX_STEPS="$ROOT_DIR/scripts/ci_push_linux_steps.sh"
PUSH_MACOS_STEPS="$ROOT_DIR/scripts/ci_push_macos_steps.sh"
PUSH_WINDOWS_STEPS="$ROOT_DIR/scripts/ci_push_windows_steps.sh"
WORKFLOW="$ROOT_DIR/.github/workflows/ci.yml"
PLATFORM_WORKFLOW="$ROOT_DIR/.github/workflows/platform_full.yml"
PARITY_WORKFLOW="$ROOT_DIR/.github/workflows/self_host_parity.yml"
DRIVER_BOOTSTRAP="$ROOT_DIR/tests/self_hosted/parity/driver_bootstrap.sh"

for file in \
    "$MAKEFILE" \
    "$LINUX_STEPS" \
    "$MACOS_STEPS" \
    "$WINDOWS_STEPS" \
    "$PUSH_LINUX_STEPS" \
    "$PUSH_MACOS_STEPS" \
    "$PUSH_WINDOWS_STEPS" \
    "$WORKFLOW" \
    "$PLATFORM_WORKFLOW" \
    "$PARITY_WORKFLOW" \
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
    'self-host-preparation-platform-parser-parity-test-smoke:' \
    'self-host-preparation-platform-semantic-parity-test-smoke:' \
    'self-host-preparation-platform-codegen-parity-test-smoke:' \
    'self-host-preparation-platform-driver-parity-test-smoke:' \
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
    local workflow="${3:-$WORKFLOW}"
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
    ' "$workflow")"
    if [[ "$actual" != "$expected" ]]; then
        echo "[self-host-ci-profile] $job timeout drifted: expected $expected, got ${actual:-missing}" >&2
        exit 1
    fi
}

require_job_timeout "self-host-parity-linux" 180 "$PARITY_WORKFLOW"
require_job_timeout "self-host-bootstrap-linux" 60
require_job_timeout "self-host-codegen-bootstrap-linux" 30
require_job_timeout "build-linux" 25
require_job_timeout "build-macos-c-only" 20
require_job_timeout "build-windows" 35
require_job_timeout "platform-full-linux" 60 "$PLATFORM_WORKFLOW"
require_job_timeout "platform-full-macos-c-only" 45 "$PLATFORM_WORKFLOW"
require_job_timeout "platform-full-windows" 90 "$PLATFORM_WORKFLOW"

build_linux_job="$(
    sed -n \
        '/^  build-linux:/,/^  sanitizers-linux:/p' \
        "$WORKFLOW"
)"
if ! grep -Eq \
        'sudo apt-get install -y .*([[:space:]])libomp-dev([[:space:]]|$)' \
        <<<"$build_linux_job"; then
    echo "[self-host-ci-profile] build-linux must install the LLVM OpenMP link dependency" >&2
    exit 1
fi

self_host_parity_job="$(
    sed -n \
        '/^  self-host-parity-linux:/,$p' \
        "$PARITY_WORKFLOW"
)"
if [[ "$(grep -Ec '^[[:space:]]+make ' <<<"$self_host_parity_job")" != "1" ]] ||
    ! grep -Fq 'make release' <<<"$self_host_parity_job"; then
    echo "[self-host-ci-profile] parity job must use one make invocation with the release pair first" >&2
    exit 1
fi

for steps in "$LINUX_STEPS" "$MACOS_STEPS" "$WINDOWS_STEPS"; do
    if ! grep -Fq 'make -j5' "$steps" ||
        ! grep -Fq 'self-host-preparation-platform-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] platform profile missing from $steps" >&2
        exit 1
    fi
    if grep -Fq ' self-host-preparation-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] full self-host proof leaked into $steps" >&2
        exit 1
    fi
done

for steps in "$PUSH_LINUX_STEPS" "$PUSH_MACOS_STEPS" "$PUSH_WINDOWS_STEPS"; do
    if grep -Fq 'self-host-preparation-platform-test-smoke' "$steps" ||
        grep -Fq 'self-host-preparation-platform-parity-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] full platform parity leaked onto push: $steps" >&2
        exit 1
    fi
    if ! grep -Fq 'test-all' "$steps"; then
        echo "[self-host-ci-profile] fast push lost its core executable battery: $steps" >&2
        exit 1
    fi
done
if ! grep -Fq 'self-host-preparation-contract-test-smoke' "$PUSH_LINUX_STEPS"; then
    echo "[self-host-ci-profile] Linux push must own the platform-independent contract once" >&2
    exit 1
fi
if ! grep -Fq 'self-host-compiler' "$PUSH_LINUX_STEPS"; then
    echo "[self-host-ci-profile] Linux push lost the platform-independent self-host build" >&2
    exit 1
fi
if grep -Fq 'self-host-preparation-contract-test-smoke' "$PUSH_MACOS_STEPS" ||
    grep -Fq 'self-host-preparation-contract-test-smoke' "$PUSH_WINDOWS_STEPS" ||
    grep -Fq 'self-host-compiler' "$PUSH_MACOS_STEPS" ||
    grep -Fq 'self-host-compiler' "$PUSH_WINDOWS_STEPS"; then
    echo "[self-host-ci-profile] platform-independent self-host evidence duplicated across push platforms" >&2
    exit 1
fi
for steps in "$PUSH_MACOS_STEPS" "$PUSH_WINDOWS_STEPS"; do
    if ! grep -Fq 'PGY_NATIVE_PIPELINE=1' "$steps"; then
        echo "[self-host-ci-profile] native push profile lost its explicit pipeline boundary: $steps" >&2
        exit 1
    fi
done

if grep -Fq 'self-host-parity-linux:' "$WORKFLOW"; then
    echo "[self-host-ci-profile] exhaustive parity leaked back onto every main push" >&2
    exit 1
fi

for forbidden in 'pull_request:' 'branches:'; do
    if grep -Fq "$forbidden" "$PARITY_WORKFLOW"; then
        echo "[self-host-ci-profile] exhaustive parity regained a per-branch trigger: $forbidden" >&2
        exit 1
    fi
done

for forbidden in 'pull_request:' 'branches:'; do
    if grep -Fq "$forbidden" "$PLATFORM_WORKFLOW"; then
        echo "[self-host-ci-profile] full platform ladder regained a per-branch trigger: $forbidden" >&2
        exit 1
    fi
done

for required in \
    'workflow_dispatch:' \
    'schedule:' \
    "cron: '0 15 * * 0'" \
    'tags:' \
    "- 'v*'" \
    'platform-full-linux:' \
    'platform-full-windows:' \
    'platform-full-macos-c-only:' \
    'make PGY_BACKEND_COMPARE_JOBS=1 ci-linux' \
    'CC=gcc make ci-windows' \
    'CC=cc make ci-macos' \
    'windows-driver-rung2-evidence' \
    'cancel-in-progress: true'; do
    if ! grep -Fq -- "$required" "$PLATFORM_WORKFLOW"; then
        echo "[self-host-ci-profile] scheduled/manual/release platform proof missing: $required" >&2
        exit 1
    fi
done
if grep -Fq 'continue-on-error:' "$PLATFORM_WORKFLOW"; then
    echo "[self-host-ci-profile] full platform proof became advisory" >&2
    exit 1
fi
if grep -Fq 'windows-driver-rung2-evidence' "$WORKFLOW"; then
    echo "[self-host-ci-profile] full-only driver evidence leaked onto fast Windows feedback" >&2
    exit 1
fi

for required in \
    'self-host-parity-linux:' \
    'workflow_dispatch:' \
    'schedule:' \
    "cron: '0 18 * * 0'" \
    'tags:' \
    "- 'v*'" \
    'timeout-minutes: 180' \
    'make release' \
    'self-host-hard-contract-test-smoke' \
    'self-host-intent-observability-runtime-test-smoke' \
    'self-host-preparation-exhaustive-parity-test-smoke' \
    'self-host-codegen-type-env-preseal-epoch-test-smoke' \
    'self-host-compiler-internal-caller-provenance-test-smoke' \
    'self-host-routine-build-storage-lifetime-test-smoke' \
    'self-host-direct-mir-scalar-graph-plan-test-smoke' \
    'self-host-public-mir-json-replacement-test-smoke' \
    'cancel-in-progress: true'; do
    if ! grep -Fq -- "$required" "$PARITY_WORKFLOW"; then
        echo "[self-host-ci-profile] scheduled/manual/release parity proof missing: $required" >&2
        exit 1
    fi
done

for required in \
    'self-host-bootstrap-linux:' \
    'self-host-codegen-bootstrap-linux:' \
    'run: make ci-push-linux' \
    'run: CC=gcc make ci-push-windows' \
    'CC=cc make ci-push-macos' \
    'timeout-minutes: 35' \
    'timeout-minutes: 25' \
    'timeout-minutes: 20' \
    'timeout-minutes: 60' \
    'timeout-minutes: 30' \
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
assignment_probe_line="$(
    grep -nF 'tests/self_hosted/parity/assignment_projection_probe_parity.sh' \
        <<<"$exhaustive_recipe" | cut -d: -f1 || true
)"
first_pgy_gate_line="$(
    grep -nF 'PGY_BIN=' <<<"$exhaustive_recipe" | head -n 1 | cut -d: -f1 || true
)"
completeness_ledger_line="$(
    grep -nF 'tests/self_hosted/parity/completeness_ledger.sh' \
        <<<"$exhaustive_recipe" | cut -d: -f1 || true
)"
codegen_parity_line="$(
    grep -nF 'tests/self_hosted/parity/codegen_parity.sh' \
        <<<"$exhaustive_recipe" | tail -n 1 | cut -d: -f1 || true
)"
if [[ "$(grep -Fc 'tests/self_hosted/parity/assignment_projection_probe_parity.sh' \
        <<<"$exhaustive_recipe")" != "1" ]] ||
    grep -Fq '$(MAKE) clean-scratch' <<<"$exhaustive_recipe" ||
    [[ -z "$assignment_probe_line" || -z "$first_pgy_gate_line" ||
       -z "$completeness_ledger_line" || -z "$codegen_parity_line" ]] ||
    (( assignment_probe_line != first_pgy_gate_line ||
       assignment_probe_line >= completeness_ledger_line ||
       assignment_probe_line >= codegen_parity_line )); then
    echo "[self-host-ci-profile] exhaustive parity must run assignment projection as its first falsifier" >&2
    exit 1
fi
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
        '/^self-host-preparation-platform-parser-parity-test-smoke:/,/^self-host-preparation-parity-test-smoke:/p' \
        "$MAKEFILE"
)"
for required in \
    'self-host-preparation-platform-parser-parity-test-smoke' \
    'self-host-preparation-platform-semantic-parity-test-smoke' \
    'self-host-preparation-platform-codegen-parity-test-smoke' \
    'self-host-preparation-platform-driver-parity-test-smoke' \
    'tests/self_hosted/parity/parser_parity.sh' \
    'tests/self_hosted/parity/semantic_parity.sh' \
    'tests/self_hosted/parity/codegen_parity.sh' \
    'tests/self_hosted/parity/driver_rung2_body_parity.sh'; do
    if ! grep -Fq "$required" <<<"$platform_recipe"; then
        echo "[self-host-ci-profile] parallel platform owner missing: $required" >&2
        exit 1
    fi
done
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

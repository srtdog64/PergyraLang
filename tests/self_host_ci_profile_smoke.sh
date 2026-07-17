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
    'self-host-preparation-platform-test-smoke:' \
    'self-host-preparation-platform-parity-test-smoke:' \
    'self-host-preparation-exhaustive-parity-test-smoke:' \
    'self-host-preparation-parity-test-smoke: self-host-preparation-exhaustive-parity-test-smoke self-host-codegen-bootstrap-test-smoke self-host-driver-bootstrap-test-smoke' \
    'tests/self_hosted/parity/parser_parity.sh' \
    'tests/self_hosted/parity/semantic_parity.sh' \
    'tests/self_hosted/parity/codegen_parity.sh' \
    'tests/self_hosted/parity/driver_rung2_body_parity.sh'; do
    if ! grep -Fq "$required" "$MAKEFILE"; then
        echo "[self-host-ci-profile] platform profile missing: $required" >&2
        exit 1
    fi
done

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
    'timeout-minutes: 40' \
    'timeout-minutes: 30' \
    'run: make self-host-preparation-exhaustive-parity-test-smoke' \
    'run: make self-host-codegen-bootstrap-test-smoke' \
    'run: make self-host-driver-bootstrap-test-smoke' \
    'cancel-in-progress: true'; do
    if ! grep -Fq "$required" "$WORKFLOW"; then
        echo "[self-host-ci-profile] dedicated Linux proof job missing: $required" >&2
        exit 1
    fi
done

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
    'trap driver_bootstrap_stop_heartbeat EXIT'; do
    if ! grep -Fq "$required" "$DRIVER_BOOTSTRAP"; then
        echo "[self-host-ci-profile] driver bootstrap heartbeat missing: $required" >&2
        exit 1
    fi
done

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

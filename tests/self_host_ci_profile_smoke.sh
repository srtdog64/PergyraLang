#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKEFILE="$ROOT_DIR/Makefile"
LINUX_STEPS="$ROOT_DIR/scripts/ci_linux_steps.sh"
MACOS_STEPS="$ROOT_DIR/scripts/ci_macos_steps.sh"
WINDOWS_STEPS="$ROOT_DIR/scripts/ci_windows_steps.sh"

for file in "$MAKEFILE" "$LINUX_STEPS" "$MACOS_STEPS" "$WINDOWS_STEPS"; do
    if [[ ! -f "$file" ]]; then
        echo "[self-host-ci-profile] missing input: $file" >&2
        exit 1
    fi
done

for required in \
    'self-host-preparation-platform-test-smoke:' \
    'self-host-preparation-platform-parity-test-smoke:' \
    'tests/self_hosted/parity/parser_parity.sh' \
    'tests/self_hosted/parity/semantic_parity.sh' \
    'tests/self_hosted/parity/codegen_parity.sh' \
    'tests/self_hosted/parity/driver_rung2_body_parity.sh'; do
    if ! grep -Fq "$required" "$MAKEFILE"; then
        echo "[self-host-ci-profile] platform profile missing: $required" >&2
        exit 1
    fi
done

if ! grep -Fq 'self-host-preparation-test-smoke' "$LINUX_STEPS"; then
    echo "[self-host-ci-profile] Linux must own the full self-host proof" >&2
    exit 1
fi

for steps in "$MACOS_STEPS" "$WINDOWS_STEPS"; do
    if ! grep -Fq 'self-host-preparation-platform-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] platform profile missing from $steps" >&2
        exit 1
    fi
    if grep -Fq ' self-host-preparation-test-smoke' "$steps"; then
        echo "[self-host-ci-profile] full self-host proof leaked into $steps" >&2
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

echo "[self-host-ci-profile] Linux full proof and native platform parity are isolated"

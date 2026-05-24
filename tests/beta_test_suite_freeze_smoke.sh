#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FREEZE_DOC="$ROOT_DIR/docs/111_beta_test_suite_freeze.md"
MAKEFILE="$ROOT_DIR/Makefile"

if [[ ! -f "$FREEZE_DOC" ]]; then
    echo "[beta-test-suite] missing freeze doc: $FREEZE_DOC" >&2
    exit 1
fi
if [[ ! -f "$MAKEFILE" ]]; then
    echo "[beta-test-suite] missing Makefile" >&2
    exit 1
fi

required_doc_terms=(
    "Beta Test Suite Freeze"
    "beta-freeze-source-of-truth"
    "Mandatory Pre-Beta Gates"
    "Platform Gates"
    "Explicitly Not Claimed Yet"
    "Fuzz testing is out-of-beta"
    "Property-based testing is out-of-beta"
    "Coverage percentage is not yet a beta acceptance metric"
    "make beta-test-suite-freeze-test-smoke"
)

for required in "${required_doc_terms[@]}"; do
    if ! grep -Fq "$required" "$FREEZE_DOC"; then
        echo "[beta-test-suite] freeze doc missing: $required" >&2
        exit 1
    fi
done

mandatory_targets=(
    "test-all"
    "test-semantic"
    "test-transpile"
    "test-abi"
    "llvm-test-smoke"
    "llvm-test-abi-same-process"
    "llvm-test-backend-compare"
    "llvm-campaign-projection-test-smoke"
    "llvm-dnd-campaign-test-smoke"
    "cfg-body-dataflow-test-smoke"
    "type-resolution-dag-test-smoke"
    "runtime-frontier-contract-test-smoke"
    "runtime-panic-contract-test-smoke"
    "runtime-panic-abi-test-smoke"
    "runtime-panic-codegen-test-smoke"
    "projection-diagnostic-contract-test-smoke"
    "abi-ownership-shape-test-smoke"
    "diagnostics-json-test-smoke"
    "parser-lexer-diagnostic-test-smoke"
    "diagnostic-registry-test-smoke"
    "formal-semantics-test-smoke"
    "air-drift-test-smoke"
    "air-backend-nonimpact-full-test-smoke"
    "air-strict-backend-compare-test-smoke"
    "codegen-determinism-test-smoke"
    "runtime-none-contract-test-smoke"
    "raw-escape-contract-test-smoke"
    "mir-declaration-inventory-test-smoke"
    "transpile-strict-source-test-smoke"
    "stdlib-test-smoke"
    "package-module-resolver-test-smoke"
    "unicode-policy-test-smoke"
    "observability-schema-test-smoke"
    "async-model-positioning-test-smoke"
    "memory-concurrency-model-test-smoke"
    "documentation-quality-test-smoke"
    "self-host-preparation-test-smoke"
    "tooling-conformance-test-smoke"
    "perf-contract-test-smoke"
    "beta-readiness-checklist-test-smoke"
    "beta-test-suite-freeze-test-smoke"
    "ci-linux"
    "ci-windows"
    "ci-macos"
)

for target in "${mandatory_targets[@]}"; do
    if ! grep -Fq "make $target" "$FREEZE_DOC"; then
        echo "[beta-test-suite] freeze doc missing target: make $target" >&2
        exit 1
    fi
    if ! grep -Eq "^${target}:" "$MAKEFILE"; then
        echo "[beta-test-suite] Makefile missing target: $target" >&2
        exit 1
    fi
done

echo "[beta-test-suite] freeze contract ok"

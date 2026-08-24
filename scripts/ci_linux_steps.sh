# CI Linux step list. Sourced by scripts/ci_step_runner.sh.
#
# Each `run` line is labeled by the runner. The runner defaults to release
# collection mode: it records failures, keeps running independent steps, and
# prints a final summary. Set PGY_CI_FAIL_FAST=1 for first-failure reporting.
# Required env vars: CI_LINUX_CC, CI_LINUX_BUILD_DIR, CI_LINUX_BIN_DIR,
# CI_BACKEND_COMPARE_SHARD_TOTAL, CI_BACKEND_COMPARE_SHARD_INDEX. The Makefile
# `ci-linux` recipe exports these before invoking the runner.

run 'make check-build-tools CC="$CI_LINUX_CC"'
run 'make check-linux-toolchain'

# The shipped compiler delegates every ordinary compile to the installed
# self-host driver, so pgy alone is no longer a usable toolchain: without
# bin/pgy-self-driver every gate that compiles Pergyra source fails with
# "self-host driver is unavailable". bin/ is gitignored, so a fresh checkout
# never has one -- build it once here, before the first step that compiles.
#
# PGY_SELF_DRIVER_BIN is exported because later steps build pgy into
# CI_LINUX_BIN_DIR, and the driver is otherwise resolved next to the launcher.
# Pinning it keeps those steps pointed at the installed driver, which the
# BIN_DIR-scoped `clean` below does not touch. The full workflow may provide the
# exact same-run installed toolchain through an artifact; absence must fail
# closed instead of silently rebuilding a different proof input.
case "${PGY_CI_SELF_HOST_MODE:-build}" in
    build)
        run 'make CC="$CI_LINUX_CC" self-host-compiler'
        ;;
    prebuilt)
        if [[ ! -x "$PWD/bin/pgy" || ! -x "$PWD/bin/pgy-self-driver" ||
              ! -s "$PWD/bin/pgy-self-driver.machine-layer-manifest.json" ]]; then
            echo "ci-linux: prebuilt self-host toolchain artifact is incomplete" >&2
            exit 1
        fi
        ;;
    *)
        echo "ci-linux: invalid PGY_CI_SELF_HOST_MODE=${PGY_CI_SELF_HOST_MODE}" >&2
        exit 2
        ;;
esac
export PGY_SELF_DRIVER_BIN="$PWD/bin/pgy-self-driver"

run 'make build-source-inventory-test-smoke'
run 'make ci-step-runner-test-smoke'
run 'make grammar-cheatsheet-contract-test-smoke'
run 'make grammar-examples-compile-test-smoke'
run 'make source-utf8-test-smoke'
run 'make backend-fail-closed-test-smoke'
run 'make worker-boundary-ub-test-smoke'
run 'make execution-lane-policy-test-smoke'
run 'make sea-execution-lane-golden-test-smoke'
run 'make lane-scheduler-test-smoke'
run 'make self-host-execution-lane-parity-test-smoke'
run 'make memory-safety-failclosed-test-smoke'
run 'make checkedarith-failclosed-test-smoke'
run 'make site-generator-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" clean'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" test-all'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" llvm-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" fmt-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" tooling-conformance-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" stdlib-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" stage4-determinism-test-smoke'
# Emit-surface reproducibility (WO-A4): the same source must emit byte-identical
# C and LLVM artifacts across runs (path spelling aside — pointer values are
# deliberately NOT masked). Linux is the leg with llvm-dev installed, matching
# the target's forced LLVM_ENABLED=1 build (same pattern as llvm-test-smoke);
# macOS/Windows wiring waits on a Makefile LLVM_ENABLED passthrough.
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" codegen-determinism-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" filesystem-directory-walk-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" module-test-smoke'
run 'make module-taxonomy-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" package-module-resolver-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" unicode-policy-test-smoke'
run 'make beta-test-suite-freeze-test-smoke'
run 'make language-contract-golden-test-smoke'
run 'make boundary-migration-test-smoke'
run 'PGY_STABLE_IDENTITY_BACKENDS="c llvm" make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" stable-identity-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" semantic-declaration-identity-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" hir-routine-identity-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" verified-projection-plan-test-smoke'
# Content-sandbox gate family (external red-team R6): capability + resource
# budget, the qualitative + quantitative sandbox axes. Linux runs pgy natively
# with LLVM enabled, so the dynamic gate's C/LLVM runtime fail-close parity is
# load-bearing here (it self-skips on platforms that cannot launch pgy).
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" test-sandbox-gates'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" observability-schema-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" memory-concurrency-model-test-smoke'
run 'make documentation-quality-test-smoke'
case "${PGY_CI_PLATFORM_PARITY_MODE:-full}" in
    full)
        run 'make self-host-preparation-platform-test-smoke'
        ;;
    contract-only)
        run 'make self-host-preparation-contract-test-smoke'
        ;;
    *)
        echo "ci-linux: invalid PGY_CI_PLATFORM_PARITY_MODE=${PGY_CI_PLATFORM_PARITY_MODE}" >&2
        exit 2
        ;;
esac
run 'make debug-hygiene-test-smoke'
run 'make memory-string-safety-test-smoke'
run 'make security-portability-contract-test-smoke'
# Sandbox symlink escape (finding 2026-07-05-001): POSIX-only, so Linux is its
# CI home; the O_NOFOLLOW open-time refusal is exercised here for real.
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" sandbox-symlink-nofollow-test-smoke'
run 'make beta-readiness-checklist-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" dogfood-webgl-test-smoke'
# wasm-backend parity (web / Android-browser coverage). zig is a single pip
# package; the install is best-effort so the gate stays load-bearing where it
# can and SKIPs cleanly where it cannot. wasm output is platform-independent,
# so Linux-only coverage is sufficient.
run 'pip install ziglang --quiet 2>/dev/null || python3 -m pip install ziglang --quiet 2>/dev/null || true'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" wasm-backend-parity-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" runtime-none-contract-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" raw-escape-contract-test-smoke'
run 'make gate-subject-declaration-test-smoke'
run 'make formal-semantics-test-smoke'
run 'make abstraction-loss-contract-test-smoke'
run 'make air-drift-test-smoke'
# Evidence-lifetime meta-gate (WO-A3): registry <-> AIREvidenceKind two-way
# correspondence, grep-based (no binary needed).
run 'make evidence-lifetime-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" air-json-schema-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" air-backend-nonimpact-full-test-smoke'
run 'make semantic-inc-size-test-smoke'
run 'make semantic-tu-size-test-smoke'
run 'make production-header-size-test-smoke'
run 'make backend-inc-size-test-smoke'
run 'make test-inc-size-test-smoke'
run 'make CC="$CI_LINUX_CC" source-test-harness-compile-test-smoke'
run 'make transpile-strict-source-test-smoke'
run 'make inc-sentinel-test-smoke'
run 'make semantic-core-shape-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" type-resolution-dag-test-smoke'
run 'make type-resolution-resolver-inventory-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" semantic-fixture-isolation-test-smoke'
run 'make diagnostic-registry-test-smoke'
run 'make layered-diagnostics-contract-test-smoke'
run 'make intent-compression-contract-test-smoke'
run 'make runtime-authority-contract-test-smoke'
run 'make runtime-panic-contract-test-smoke'
run 'make CC="$CI_LINUX_CC" runtime-panic-abi-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" runtime-panic-codegen-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" slot-contract-test-smoke'
run 'make projection-diagnostic-contract-test-smoke'
run 'make runtime-abi-lifetime-test-smoke'
run 'make abi-ownership-shape-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" runtime-frontier-contract-test-smoke'
run 'make CC="$CI_LINUX_CC" runtime-frontier-policy-test-smoke'
run 'make runtime-intent-observability-contract-test-smoke'
run 'make parallel-core-contract-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" PGY_BACKPRESSURE_STRESS_ITERATIONS=32 parallel-backpressure-stress-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" parallel-production-contract-test-smoke'
# docs/189 C-compiler-dev red-team repair gates: bitcode-twin contract pins,
# surface boundary hygiene (C-reserved words + channel copy edges),
# adversarial-input termination contract, emitted-C warning cleanliness,
# and the runtime-bitcode-ON backend-compare shard (the bc-ON leg CI never
# exercised). Linux runs both backend voices natively, has gcc for the
# -Werror leg, and clang for the .bc build.
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" redteam-repair-contract-test-smoke'
# Sanitizer gate (docs/189 C13): rebuild the compiler under ASan+UBSan in
# its own build tree (LLVM_ENABLED=0, separate BUILD_DIR so the main build
# is untouched) and run the sanitized fixtures + unit batteries. Verified
# green on Ubuntu gcc 13.3 + libasan; MinGW ships no libasan so this only
# runs on the Linux job.
run 'make test-asan'
# Differential fuzzing over the dual backend (docs/189 C14): three fixed
# seeds for reproducibility plus one campaign seed rotated by the CI run
# number so the oracle is not pinned to a fixed corpus.
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" fuzz-backend-parity-matrix-test-smoke'
run 'PGY_FUZZ_SEED="${GITHUB_RUN_NUMBER:-2026}" make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" fuzz-backend-parity-campaign-test-smoke'
# Parallel boundary evidence gates (docs/178): disjoint-split admission,
# reader-snapshot admission, and ability coherence. Linux runs both backend
# voices natively (the LLVM voice is load-bearing for ability-coherence:
# the hole this gate closed was an LLVM silent-accept).
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" parallel-disjoint-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" parallel-snapshot-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" ability-coherence-test-smoke'
# Declared-but-unexecutable parallel surfaces (docs/181) stay fail-closed.
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" parallel-vision-surface-test-smoke'
# Join-form rung 0 (docs/181 SS1): both backend voices on Linux.
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" parallel-join-test-smoke'
run 'make perf-contract-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" perf-c-baseline-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" evidence-guard-amortization-test-smoke'
run 'make parser-lexer-diagnostic-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" diagnostics-json-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" ir-pipeline-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" cfg-body-dataflow-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" loop-flow-summary-test-smoke'
run 'make slot-analyzer-host-index-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" function-param-flow-summary-test-smoke'
run 'make ast-dispatch-test-smoke'
run 'make mir-declaration-inventory-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" example-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" llvm-test-abi-same-process'
run 'PGY_BACKEND_COMPARE_SHARD_TOTAL="$CI_BACKEND_COMPARE_SHARD_TOTAL" PGY_BACKEND_COMPARE_SHARD_INDEX="$CI_BACKEND_COMPARE_SHARD_INDEX" PGY_BACKEND_COMPARE_PRECHECK=0 make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" llvm-test-backend-compare'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" llvm-campaign-projection-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" llvm-dnd-campaign-test-smoke'
run 'PGY_BACKEND_COMPARE_SHARD_TOTAL="$CI_BACKEND_COMPARE_SHARD_TOTAL" PGY_BACKEND_COMPARE_SHARD_INDEX="$CI_BACKEND_COMPARE_SHARD_INDEX" PGY_BACKEND_COMPARE_PRECHECK=0 make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" air-strict-backend-compare-test-smoke'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" clean'
run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" test-all'

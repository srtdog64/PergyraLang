# CI Windows step list. Sourced by scripts/ci_step_runner.sh.
# Required env vars: CI_WINDOWS_CC, CI_WINDOWS_BUILD_DIR, CI_WINDOWS_BIN_DIR,
# CI_WINDOWS_RUNNABLE (0/1), CI_WINDOWS_CC_MACHINE, WINDOWS_LLVM_READY (0/1),
# CI_BACKEND_COMPARE_SHARD_TOTAL, CI_BACKEND_COMPARE_SHARD_INDEX.

run 'make check-build-tools CC="$CI_WINDOWS_CC" LLVM_ENABLED=0'
run 'make check-windows-toolchain'
run 'make build-source-inventory-test-smoke'
run 'make source-utf8-test-smoke'
run 'make backend-fail-closed-test-smoke'
run 'make worker-boundary-ub-test-smoke'
run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" clean'

if [[ "$CI_WINDOWS_RUNNABLE" == "1" ]]; then
    echo "ci-windows: native MSYS2 runtime detected; running core executable tests"
    run 'MAKEFLAGS= make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" test-all'
elif [[ "$CI_WINDOWS_CC_MACHINE" == *mingw* ]]; then
    echo "ci-windows: cross MinGW toolchain detected; running build-only smoke"
    run 'MAKEFLAGS= make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" windows-build-smoke'
else
    echo "ci-windows: unable to run or cross-build Windows artifacts with CC=$CI_WINDOWS_CC"
    exit 1
fi

run 'make beta-test-suite-freeze-test-smoke'
run 'make documentation-quality-test-smoke'
run 'make abstraction-loss-contract-test-smoke'
run 'make debug-hygiene-test-smoke'
run 'make memory-string-safety-test-smoke'
run 'make security-portability-contract-test-smoke'
run 'make beta-readiness-checklist-test-smoke'
run 'make layered-diagnostics-contract-test-smoke'
run 'make intent-compression-contract-test-smoke'
run 'make transpile-strict-source-test-smoke'
run 'make mir-declaration-inventory-test-smoke'
run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" source-test-harness-compile-test-smoke'

if [[ "$CI_WINDOWS_RUNNABLE" == "1" ]]; then
    echo "ci-windows: native MSYS2 runtime detected; running executable contract smokes"
    run 'PGY_STDLIB_BACKENDS=c make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" fmt-test-smoke'
    run 'PGY_STDLIB_BACKENDS=c make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" stdlib-test-smoke'
    run 'PGY_STAGE4_DETERMINISM_BACKENDS=c make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" stage4-determinism-test-smoke'
    run 'PGY_FILESYSTEM_WALK_BACKENDS=c make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" filesystem-directory-walk-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" dogfood-webgl-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" runtime-none-contract-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" slot-contract-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" raw-escape-contract-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" air-drift-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" air-json-schema-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" cfg-body-dataflow-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" semantic-fixture-isolation-test-smoke'
    run 'PGY_EXAMPLE_BACKENDS=c make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" example-test-smoke'
    run 'make test-inc-size-test-smoke'
    if [[ "$WINDOWS_LLVM_READY" == "1" ]]; then
        echo "ci-windows: LLVM toolchain detected; running LLVM smoke and backend compare"
        run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=1 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" llvm-test-smoke'
        run 'PGY_BACKEND_COMPARE_SHARD_TOTAL="$CI_BACKEND_COMPARE_SHARD_TOTAL" PGY_BACKEND_COMPARE_SHARD_INDEX="$CI_BACKEND_COMPARE_SHARD_INDEX" make CC="$CI_WINDOWS_CC" LLVM_ENABLED=1 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" llvm-test-backend-compare'
        run 'PGY_BACKEND_COMPARE_SHARD_TOTAL="$CI_BACKEND_COMPARE_SHARD_TOTAL" PGY_BACKEND_COMPARE_SHARD_INDEX="$CI_BACKEND_COMPARE_SHARD_INDEX" make CC="$CI_WINDOWS_CC" LLVM_ENABLED=1 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" air-strict-backend-compare-test-smoke'
    else
        echo "ci-windows: LLVM toolchain not detected; skipping Windows LLVM smoke/backend compare"
    fi
fi

run 'PGY_AIR_GRAPH_JSON_SKIP_DRIFT=1 make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" self-host-preparation-test-smoke'

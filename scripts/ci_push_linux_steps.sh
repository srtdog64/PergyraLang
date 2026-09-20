# Fast Linux push gate. Full platform coverage remains in ci_linux_steps.sh.

run 'make check-build-tools CC="$CI_LINUX_CC"'
run 'make check-linux-toolchain'
case "${PGY_CI_SELF_HOST_MODE:-build}" in
    build)
        run 'make CC="$CI_LINUX_CC" self-host-compiler'
        ;;
    prebuilt)
        if [[ ! -x "$PWD/bin/pgy" || ! -x "$PWD/bin/pgy-self-driver" ||
              ! -s "$PWD/bin/pgy-self-driver.machine-layer-manifest.json" ]]; then
            echo "ci-push-linux: prebuilt self-host toolchain artifact is incomplete" >&2
            exit 1
        fi
        ;;
    *)
        echo "ci-push-linux: invalid PGY_CI_SELF_HOST_MODE=${PGY_CI_SELF_HOST_MODE}" >&2
        exit 2
        ;;
esac
export PGY_SELF_DRIVER_BIN="$PWD/bin/pgy-self-driver"

PGY_CI_PUSH_LINUX_SHARD="${PGY_CI_PUSH_LINUX_SHARD:-all}"
PGY_CI_PUSH_LINUX_RUN_CORE=0
PGY_CI_PUSH_LINUX_RUN_SELF_HOST=0
case "$PGY_CI_PUSH_LINUX_SHARD" in
    all)
        PGY_CI_PUSH_LINUX_RUN_CORE=1
        PGY_CI_PUSH_LINUX_RUN_SELF_HOST=1
        ;;
    core)
        PGY_CI_PUSH_LINUX_RUN_CORE=1
        ;;
    self-host)
        PGY_CI_PUSH_LINUX_RUN_SELF_HOST=1
        ;;
    *)
        echo "ci-push-linux: invalid PGY_CI_PUSH_LINUX_SHARD=$PGY_CI_PUSH_LINUX_SHARD; expected all, core, or self-host" >&2
        exit 2
        ;;
esac

if [[ "$PGY_CI_PUSH_LINUX_RUN_CORE" == "1" ]]; then
    run 'make self-host-llvm-option-member-assignment-context-test-smoke'
    run 'make self-host-llvm-intent-value-argument-abi-test-smoke'
    run 'make self-host-replacement-frontier-installed-test-smoke'

    run 'make build-source-inventory-test-smoke'
    run 'make gate-subject-declaration-test-smoke'
    run 'make gate-script-reachability-test-smoke'
    run 'bash tests/protocol_registry_smoke.sh'
    run 'make self-host-driver-fixed-point-receipt-test-smoke'
    run 'make self-host-codegen-seed-receipt-test-smoke'
    run 'make self-host-domain-runtime-zone-sync-test-smoke'
    run 'make ci-step-runner-test-smoke'
    run 'make llvm-large-aggregate-return-stack-test-smoke'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" release-primary-debug-section-hygiene-test-smoke'
    run 'make grammar-cheatsheet-contract-test-smoke'
    run 'make grammar-examples-compile-test-smoke'
    run 'make source-utf8-test-smoke'
    run 'make backend-fail-closed-test-smoke'
    run 'make worker-boundary-ub-test-smoke'
    run 'make CC="$CI_LINUX_CC" runtime-spawn-context-propagation-test-smoke'
    run 'make CC="$CI_LINUX_CC" structured-spawn-lifecycle-test-smoke'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" clean'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" test-all'
fi

if [[ "$PGY_CI_PUSH_LINUX_RUN_SELF_HOST" == "1" ]]; then
    run 'python3 tests/concept_semantics/identity_cell_receiver_execution.py "$PWD/bin/pgy" "$PWD/bin/pgy-self-driver"'
    run 'make self-host-collection-ownership-semantic-test-smoke'
    run 'make self-host-task-is-cancelled-builtin-test-smoke'
    run 'make self-host-slice-copy-semantic-bridge-test-smoke'
    run 'make self-host-zone-spawn-transport-admission-test-smoke'
    run 'make self-host-future-aggregate-storage-admission-test-smoke'
    run 'make self-host-preparation-contract-test-smoke'
fi

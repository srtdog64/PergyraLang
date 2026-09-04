# Fast Linux push gate. Full platform coverage remains in ci_linux_steps.sh.

run 'make check-build-tools CC="$CI_LINUX_CC"'
run 'make check-linux-toolchain'
run 'make CC="$CI_LINUX_CC" self-host-compiler'
export PGY_SELF_DRIVER_BIN="$PWD/bin/pgy-self-driver"

run 'make self-host-llvm-option-member-assignment-context-test-smoke'
run 'make self-host-llvm-intent-value-argument-abi-test-smoke'
run 'make self-host-region-user-callee-replacement-test-smoke'
run 'make self-host-lowercase-entrypoint-replacement-test-smoke'

run 'make build-source-inventory-test-smoke'
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
run 'make self-host-preparation-contract-test-smoke'

# Fast Windows push gate. Full platform coverage remains in ci_windows_steps.sh.

run 'make check-build-tools CC="$CI_WINDOWS_CC" LLVM_ENABLED=0'
run 'make check-windows-toolchain'

if [[ "$CI_WINDOWS_RUNNABLE" == "1" ]]; then
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 runtime-spawn-context-propagation-test-smoke'
fi
run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" clean'
if [[ "$CI_WINDOWS_RUNNABLE" == "1" ]]; then
    run 'PGY_NATIVE_PIPELINE=1 MAKEFLAGS= make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" test-all'
    run 'PGY_FILESYSTEM_WALK_BACKENDS=c make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" filesystem-directory-walk-test-smoke'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" air-erasure-gate'
    run 'make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" semantic-fixture-isolation-test-smoke'
elif [[ "$CI_WINDOWS_CC_MACHINE" == *mingw* ]]; then
    run 'PGY_NATIVE_PIPELINE=1 MAKEFLAGS= make CC="$CI_WINDOWS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" windows-build-smoke'
else
    echo "ci-push-windows: unable to run or cross-build Windows artifacts with CC=$CI_WINDOWS_CC" >&2
    exit 1
fi

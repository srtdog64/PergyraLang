# Fast macOS push gate. Full platform coverage remains in ci_macos_steps.sh.

run 'make check-build-tools CC="$CI_MACOS_CC" LLVM_ENABLED=0'
run 'make check-macos-toolchain'

run 'make CC="$CI_MACOS_CC" LLVM_ENABLED=0 runtime-spawn-context-propagation-test-smoke'
run 'make CC="$CI_MACOS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_MACOS_BUILD_DIR" BIN_DIR="$CI_MACOS_BIN_DIR" clean'
run 'PGY_NATIVE_PIPELINE=1 make CC="$CI_MACOS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_MACOS_BUILD_DIR" BIN_DIR="$CI_MACOS_BIN_DIR" test-all'
run 'PGY_FILESYSTEM_WALK_BACKENDS=c make CC="$CI_MACOS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_MACOS_BUILD_DIR" BIN_DIR="$CI_MACOS_BIN_DIR" filesystem-directory-walk-test-smoke'
run 'make CC="$CI_MACOS_CC" LLVM_ENABLED=0 BUILD_DIR="$CI_MACOS_BUILD_DIR" BIN_DIR="$CI_MACOS_BIN_DIR" semantic-fixture-isolation-test-smoke'

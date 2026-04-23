#!/usr/bin/env bash
#
# verify_arena_closure.sh
#
# Arena / lifetime discipline 축의 회귀 검증 스크립트.
# 목적: 누적된 arena 전환이 기존 semantic/transpile/abi/backend-compare 회귀를
# 깨지 않는다는 것을 한 번에 확인.
#
# 실행 방법 (프로젝트 루트에서):
#   bash tests/verify_arena_closure.sh
#
# 종료 코드:
#   0 — 전부 녹색
#   1 — 하나 이상 실패
#
# 주의:
#   - 이 스크립트는 명시적으로 clean rebuild 를 수행한다 (CONFIG_STAMP 오염 회피).
#   - LLVM 관련 테스트는 PGY_LLVM_ENABLED 로 빌드되어 있을 때만 실행.
#   - compare_backends.sh 는 양쪽 백엔드가 모두 동작해야 함.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

fail=0
log_dir="$ROOT/build/verify_arena"
mkdir -p "$log_dir"

report() {
    local label="$1"
    local status="$2"
    local log="$3"
    if [ "$status" -eq 0 ]; then
        printf "[PASS] %s  (log: %s)\n" "$label" "$log"
    else
        printf "[FAIL] %s  (exit %d, log: %s)\n" "$label" "$status" "$log"
        tail -20 "$log" 2>/dev/null | sed 's/^/    /'
        fail=1
    fi
}

run() {
    local label="$1"
    local log="$log_dir/$2.log"
    shift 2
    "$@" > "$log" 2>&1
    local status=$?
    report "$label" "$status" "$log"
    return $status
}

echo "============================================================"
echo "Arena closure verification"
echo "Root: $ROOT"
echo "Log dir: $log_dir"
echo "============================================================"

# 1. Clean + rebuild. CONFIG_STAMP 오염 가능성을 차단.
run "make rebuild"         rebuild        mingw32-make rebuild || exit 1

# 2. Core unit tests (가장 넓은 커버리지).
run "test-semantic"        test-semantic  mingw32-make test-semantic
run "test-transpile"       test-transpile mingw32-make test-transpile
run "test-abi"             test-abi       mingw32-make test-abi

# 3. HIR/MIR/RIR structural tests.
run "test-hir"             test-hir       mingw32-make test-hir
run "test-mir"             test-mir       mingw32-make test-mir
run "test-rir"             test-rir       mingw32-make test-rir

# 4. LLVM pipeline (arena 전환이 가장 많이 일어난 영역).
if [ -n "${PGY_LLVM_ENABLED:-}" ] || grep -q "PGY_LLVM_ENABLED" Makefile; then
    run "llvm-test-smoke"  llvm-smoke     mingw32-make llvm-test-smoke
    run "llvm-test-backend-compare" llvm-compare \
        mingw32-make llvm-test-backend-compare
fi

# 5. Shell-level smoke (백엔드 파리티 + JSON 진단 경계).
run "compare_backends.sh"       compare  bash tests/compare_backends.sh
run "diagnostics_json_smoke.sh" json     bash tests/diagnostics_json_smoke.sh
run "abi_pipeline_smoke.sh"     abi      bash tests/abi_pipeline_smoke.sh

# 6. (Optional) Memory sanity — 누수 감지에 도움 (arena destroy 누락 케이스).
if command -v valgrind >/dev/null 2>&1; then
    run "valgrind bin/test_semantic.exe" vg-sem \
        valgrind --error-exitcode=1 --leak-check=no bin/test_semantic.exe
fi

echo "============================================================"
if [ "$fail" -eq 0 ]; then
    echo "RESULT: PASS — arena closure is regression-clean."
else
    echo "RESULT: FAIL — see failing log(s) above."
fi
echo "============================================================"

exit $fail

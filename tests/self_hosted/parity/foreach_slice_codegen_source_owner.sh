#!/usr/bin/env bash
# Executable Slice iteration and missing-ABI checks over the real Pergyra owners.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap_compile_leg.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CODEGEN_BUILD="${PGY_SELFHOST_CODEGEN_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap}"
CODEGEN="${PGY_CODEGEN_BIN:-$CODEGEN_BUILD/gen2.exe}"
pgy_require_runnable_binary_here slice-foreach "$PGY"
pgy_require_runnable_binary_here slice-foreach "$CODEGEN"
CC="${PGY_SELFHOST_CC:-gcc}"
BACKENDS="${PGY_TEST_BACKENDS:-c llvm}"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/slice-foreach.XXXXXX")"
FIXTURES=tests/self_hosted/parity/fixture
PUBLIC="$FIXTURES/foreach_slice_values.pgy"
FACTS="$FIXTURES/foreach_slice_runtime_fact.pgy"
UNSUPPORTED="$FIXTURES/foreach_slice_unsupported.pgy"
fail() { echo "[slice-foreach] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
sha256sum "$PGY" "$CODEGEN" "$PUBLIC" "$FACTS" "$UNSUPPORTED" >"$B/input.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z |
    xargs -0 sha256sum >"$B/import.sha256"
printf '2\n10\n0\n' >"$B/values.expected"
printf '%s\n' true pgy_self_slice_String 'const char *' \
    'pgy_slice_len_String(&view)' 'pgy_self_slice_get_String(&view, index)' \
    true pgy_self_slice_Int int32_t false false >"$B/facts.expected"

run_and_compare() {
    local binary="$1" expected="$2" label="$3"
    timeout 15s "$binary" >"$B/$label.raw" 2>"$B/$label.err" ||
        fail "$label execution failed"
    [[ ! -s "$B/$label.err" ]] || fail "$label emitted stderr"
    tr -d '\r' <"$B/$label.raw" >"$B/$label.out"
    cmp -s "$expected" "$B/$label.out" || fail "$label output drift"
}

for backend in $BACKENDS; do
    [[ "$backend" == c || "$backend" == llvm ]] || fail "unknown backend: $backend"
    for fixture in values facts; do
        source_path="$PUBLIC"
        [[ "$fixture" != facts ]] || source_path="$FACTS"
        output="$B/$fixture.$backend.exe"
        if ! timeout 90s "$PGY" --native-pipeline "--backend=$backend" \
            "$source_path" -o "$(pgy_path_for_compiler "$PGY" "$output")" \
            >"$B/$fixture.$backend.build.log" 2>&1; then
            cat "$B/$fixture.$backend.build.log" >&2
            fail "$fixture/$backend did not compile"
        fi
        run_and_compare "$output" "$B/$fixture.expected" "$fixture.$backend"
    done
done

if ! timeout 60s "$CODEGEN" --source "$PUBLIC" >"$B/values.self.c" \
    2>"$B/values.self.err"; then
    cat "$B/values.self.c" "$B/values.self.err" >&2
    fail "source emitter refused Slice iteration"
fi
[[ ! -s "$B/values.self.err" ]] || fail "source emitter wrote stderr"
export PGY_SELFHOST_CC_PROFILE=test
compile_c_artifact_with_bounded_log values.self "$B/values.self.c" \
    "$B/values.self.exe" || fail "Slice-emitted C did not compile"
run_and_compare "$B/values.self.exe" "$B/values.expected" values.self

if timeout 60s "$CODEGEN" --source "$UNSUPPORTED" >"$B/unsupported.out" \
    2>"$B/unsupported.err"; then
    fail "unsupported Slice<Float> ABI was accepted"
else
    status="$?"
    [[ "$status" == 1 ]] || fail "Slice<Float> refusal status=$status"
fi
grep -Eq '^CODEGEN ERROR: unsupported C ABI value type.*: Slice<Float>\r?$' \
    "$B/unsupported.out" || fail "Slice<Float> failed for an unrelated reason"
if grep -Eq '^#include|int main\(' "$B/unsupported.out"; then
    fail "unsupported Slice ABI published C"
fi
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[slice-foreach] PASS ($BACKENDS): actual Slice facts, Int/String/empty loops and pre-emission Float ABI refusal; evidence=$B"

#!/usr/bin/env bash
# Exact borrowed-Slice length lowering, not installed-driver evidence.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap_compile_leg.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-clang}"
LABEL=slice-length-codegen-source
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/slice-length-codegen.XXXXXX")"
cd "$ROOT_DIR"
FIXTURE=tests/self_hosted/parity/fixture/slice_length_formal.pgy
MIXED=tests/self_hosted/parity/fixture/slice_runtime_header_coexistence.pgy
PROBE=tests/self_hosted/fixtures/slice_length_runtime_fact_probe.pgy
UNPROVED=tests/self_hosted/parity/fixture/slice_length_compound_unproved_negative.pgy
sha256sum "$PGY" "$FIXTURE" "$MIXED" "$PROBE" "$UNPROVED" >"$B/input.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z |
    xargs -0 sha256sum >"$B/import.sha256"
printf '2\n2\n2\n2\n0\n0\n0\n0\n' >"$B/expected"
printf '2\n2\n5\nblue\n7\nred\n1\n' >"$B/mixed-expected"
for backend in c llvm; do
    for fixture in formal mixed; do
    source="$FIXTURE"; expected="$B/expected"
    if [[ "$fixture" == mixed ]]; then source="$MIXED"; expected="$B/mixed-expected"; fi
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$source" -o "$B/native-$backend-$fixture.exe" >"$B/native-$backend-$fixture.compile" 2>&1 || fail "$backend $fixture did not build"
    timeout 30 "$B/native-$backend-$fixture.exe" >"$B/native-$backend-$fixture.raw" 2>"$B/native-$backend-$fixture.err" || fail "$backend $fixture failed"
    test ! -s "$B/native-$backend-$fixture.err" || fail "$backend $fixture wrote stderr"
    tr -d '\r' <"$B/native-$backend-$fixture.raw" >"$B/native-$backend-$fixture.run"
    cmp "$expected" "$B/native-$backend-$fixture.run" || fail "$backend $fixture oracle drift"
    done
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$PROBE" -o "$B/facts-$backend.exe" >"$B/facts-$backend.compile" 2>&1 || fail "$backend fact probe did not build"
    timeout 30 "$B/facts-$backend.exe" facts >"$B/facts-$backend.run" 2>&1 || fail "$backend facts refused"
    tr -d '\r' <"$B/facts-$backend.run" | grep -Fxq 'SLICE LENGTH FACTS PASS' || fail "$backend fact oracle drift"
    for mode in missing missing-runtime-length unsupported-symbol; do
        if timeout 30 "$B/facts-$backend.exe" "$mode" >"$B/$backend-$mode.run" 2>&1; then fail "$backend accepted $mode"; else status=$?; fi
        [[ "$status" == 1 ]] || fail "$backend refusal=$mode status=$status"
        grep -Fq 'CODEGEN ERROR:' "$B/$backend-$mode.run" || fail "$backend refusal lost its diagnostic"
    done
done
# Build the changed emitter once from the recorded current import graph.
PGY_CC="$CC" timeout 240 "$PGY" --native-pipeline --opt=dev --backend=c \
    src/self_hosted/codegen/main.pgy -o "$B/codegen.exe" >"$B/codegen.compile" 2>&1 || fail "current emitter did not build"
for fixture in formal mixed; do
source="$FIXTURE"; expected="$B/expected"
if [[ "$fixture" == mixed ]]; then source="$MIXED"; expected="$B/mixed-expected"; fi
timeout 30 "$B/codegen.exe" --source "$source" >"$B/selfhost.c" 2>"$B/selfhost.err" || fail "self-host refused $fixture"
test ! -s "$B/selfhost.err" || fail "self-host $fixture emission wrote stderr"
export PGY_SELFHOST_CC_PROFILE=test
compile_c_artifact_with_bounded_log selfhost "$B/selfhost.c" "$B/selfhost.exe" || fail "emitted C did not compile"
timeout 30 "$B/selfhost.exe" >"$B/selfhost.raw" 2>"$B/selfhost.run.err" || fail "emitted C failed"
test ! -s "$B/selfhost.run.err" || fail "emitted C wrote stderr"
tr -d '\r' <"$B/selfhost.raw" >"$B/selfhost.run"
cmp "$expected" "$B/selfhost.run" || fail "self-host $fixture oracle drift"
if [[ "$fixture" == mixed ]]; then
    grep -Fxq '#include "pgy_runtime.h"' "$B/selfhost.c" || fail "mixed fixture did not reach the public runtime header"
    if grep -Eq '^typedef.* PgySlice_(Int|String);|^static.* pgy_(array_slice|slice_get|slice_copy)_(Int|String)\(' "$B/selfhost.c"; then fail "private Slice emission reopened native ABI symbols"; fi
fi
mv "$B/selfhost.c" "$B/selfhost-$fixture.c"
mv "$B/selfhost.run" "$B/selfhost-$fixture.run"
mv "$B/selfhost.exe" "$B/selfhost-$fixture.exe"
done
if timeout 30 "$B/codegen.exe" --source "$UNPROVED" >"$B/unproved.out" 2>"$B/unproved.err"; then fail "missing compound operand type was accepted"; else status=$?; fi
[[ "$status" == 1 ]] || fail "compound type refusal status=$status"
grep -Fxq 'CODEGEN ERROR: ArrayLength argument type fact is missing' "$B/unproved.out" || fail "compound type refusal diagnostic drift"
if grep -Eq '^#include|int main\(' "$B/unproved.out"; then fail "unproved operand published C"; fi
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] native C/LLVM + current self-host C lengths, header coexistence, get/copy and missing-fact refusals PASS; evidence=$B"

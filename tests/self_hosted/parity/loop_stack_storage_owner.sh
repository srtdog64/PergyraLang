#!/usr/bin/env bash
# Stack storage used inside a loop body is allocated once per call.
# The default LLVM route wrote each temporary's alloca next to its first use,
# so a temporary in a loop body took a new stack slot every round and a
# two-million-round HashMap loop died with SIGSEGV on that route only
# (red-team audit R5). The native LLVM backend did the same for StringSplit's
# result slot and for a parallel join's run-time-sized task storage.
# tests/compare_backends.sh runs short native programs only and saw neither.
# - loop_stack_storage.pgy runs on all four legs and prints the same values.
# - Its LLVM IR on both LLVM legs keeps every alloca in the entry block.
# - loop_stack_storage_parallel_join.pgy runs on the native legs; a default
#   leg that accepts it must print the same values. Its native LLVM IR keeps
#   only run-time-sized allocas outside the entry block, and those sit between
#   llvm.stacksave and llvm.stackrestore.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="loop-stack-storage"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/loop_stack_storage"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        default-llvm) echo "--backend=llvm" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# compile NAME FIXTURE LEG [extra flags]: build into WORK_REL/NAME-LEG.
compile() {
    local name="$1" fixture="$2" leg="$3"
    shift 3
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" $(leg_flags "$leg") "$@") \
        >"$WORK_DIR/$name-$leg.log" 2>&1
}

# run_expect NAME LEG: the built binary prints NAME.expected.
run_expect() {
    local name="$1" leg="$2"
    "$WORK_DIR/$name-$leg.exe" </dev/null | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
        fail "$leg $name binary failed (a stack overflow shows up here)"
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
        { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" >&2 || true
          fail "$leg printed other values for $name"; }
}

# non_entry_allocas IR: every alloca outside its function's entry block, with
# a trailing tag " [restored]" when its function calls llvm.stackrestore.
non_entry_allocas() {
    awk '
        /^define / { in_fn = 1; entry = 1; saw_line = 0; n = 0; restored = 0; next }
        /^}/ {
            for (i = 1; i <= n; i++) print rows[i] (restored ? " [restored]" : "")
            in_fn = 0; next
        }
        in_fn && /^[A-Za-z0-9_.$"-]+:/ { if (saw_line) entry = 0; saw_line = 1; next }
        in_fn && /^[ \t]+[^ \t]/ { saw_line = 1 }
        in_fn && /@llvm\.stackrestore/ { restored = 1 }
        in_fn && / = alloca / && !entry { rows[++n] = $0 }
    ' "$1"
}

emit_ir() {
    local name="$1" fixture="$2" leg="$3"
    compile "$name-ir" "$fixture" "$leg" --emit-llvm -o "$WORK_REL/$name-$leg.ll" ||
        { cat "$WORK_DIR/$name-ir-$leg.log" >&2; fail "$leg did not emit IR for $name"; }
}

# --- all four legs: HashMap temporaries and StringSplit's result slot ---
printf '0\n2000000\n995\n6000000\n' >"$WORK_DIR/loop.expected"
for leg in native-c native-llvm default-c default-llvm; do
    compile loop "$FIXTURES/loop_stack_storage.pgy" "$leg" -o "$WORK_REL/loop-$leg.exe" ||
        { cat "$WORK_DIR/loop-$leg.log" >&2; fail "$leg did not compile the loop"; }
    run_expect loop "$leg"
done
for leg in native-llvm default-llvm; do
    emit_ir loop "$FIXTURES/loop_stack_storage.pgy" "$leg"
    non_entry_allocas "$WORK_DIR/loop-$leg.ll" >"$WORK_DIR/loop-$leg.non-entry"
    if [[ -s "$WORK_DIR/loop-$leg.non-entry" ]]; then
        cat "$WORK_DIR/loop-$leg.non-entry" >&2
        fail "$leg IR keeps an alloca outside the entry block"
    fi
done
# The storage this gate is about is present, so the checks above saw it.
value_slots="$(grep -c '\.value = alloca ' "$WORK_DIR/loop-default-llvm.ll" || true)"
map_slots="$(grep -c '\.map = alloca ' "$WORK_DIR/loop-default-llvm.ll" || true)"
[[ "$value_slots" -ge 3 && "$map_slots" -ge 2 ]] ||
    fail "default LLVM IR lost the loop temporaries this gate checks (value=$value_slots map=$map_slots)"
grep -Eq '= alloca %PgyArray_String' "$WORK_DIR/loop-native-llvm.ll" ||
    fail "native LLVM IR lost the StringSplit result slot this gate checks"

# --- parallel join: run-time-sized storage inside a loop ---
printf '400\n400\n' >"$WORK_DIR/join.expected"
for leg in native-c native-llvm; do
    compile join "$FIXTURES/loop_stack_storage_parallel_join.pgy" "$leg" \
        -o "$WORK_REL/join-$leg.exe" ||
        { cat "$WORK_DIR/join-$leg.log" >&2; fail "$leg did not compile the parallel join loop"; }
    run_expect join "$leg"
done
for leg in default-c default-llvm; do
    if compile join "$FIXTURES/loop_stack_storage_parallel_join.pgy" "$leg" \
            -o "$WORK_REL/join-$leg.exe"; then
        run_expect join "$leg"
    fi
done
emit_ir join "$FIXTURES/loop_stack_storage_parallel_join.pgy" native-llvm
non_entry_allocas "$WORK_DIR/join-native-llvm.ll" >"$WORK_DIR/join-native-llvm.non-entry"
[[ -s "$WORK_DIR/join-native-llvm.non-entry" ]] ||
    fail "native LLVM IR lost the run-time-sized join storage this gate checks"
while IFS= read -r row; do
    # A run-time-sized alloca names its element count: `alloca T, i64 N`.
    [[ "$row" =~ \ =\ alloca\ [^,]+,\ i(32|64)\  && "$row" == *" [restored]" ]] ||
        fail "native LLVM keeps a fixed-size or unrestored alloca outside the entry block: $row"
done <"$WORK_DIR/join-native-llvm.non-entry"

echo "[$LABEL] loop temporaries take one slot per call on four legs, and a parallel join in a loop returns its stack: PASS"

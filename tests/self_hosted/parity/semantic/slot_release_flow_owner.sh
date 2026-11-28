#!/usr/bin/env bash
# Release and move state of anchored resource handles (Slot, SecureSlot,
# DeviceSlot) through branches, loops, breaks, match arms and short circuits.
# Native semantic tracks slot_info.state and is_consumed per path
# (type_checker_builtins_slotops.c and its peers); the self-host semantic owns
# the same rule in ast_slot_release_admission_owner.pgy. The default routes
# used to accept these programs and abort at run time on the runtime's
# double-release guard, or read a released slot. This gate checks that:
# - native and both default routes refuse each program with the same first
#   public code, leave no binary, and the default routes name the owned code;
# - the release paths native admits still compile on every route and print
#   the same values.
set -euo pipefail

unset PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="slot-release-flow"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/slot_release_flow"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local fixture="$1" leg="$2" out_rel="$3"
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $leg" ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "${flags[@]}" --error-format=json -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

first_code() {
    grep -o '"code": *"PGY_[A-Z_]*"' "$1" | head -1 | sed 's/.*"\(PGY_[A-Z_]*\)"/\1/'
}

# expect_refused FIXTURE PUBLIC_CODE OWNED_CODE
expect_refused() {
    local fixture="$1" public="$2" owned="$3"
    local name leg out_rel actual
    name="$(echo "$fixture" | tr '/.' '__')"
    for leg in native-c default-c default-llvm; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$fixture" "$leg" "$out_rel"; then
            [[ -e "$ROOT_DIR/$out_rel" ]] && fail "$leg accepted $fixture"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for $fixture"
        actual="$(first_code "$ROOT_DIR/$out_rel.log")"
        [[ "$actual" == "$public" ]] ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2
              fail "$leg refused $fixture with '${actual:-no code}', expected $public"; }
        case "$leg" in
            default-*)
                grep -Fq "Code: $owned" "$ROOT_DIR/$out_rel.log" ||
                    { tail -20 "$ROOT_DIR/$out_rel.log" >&2
                      fail "$leg refused $fixture without the owned code $owned"; } ;;
        esac
    done
}

F=tests/self_hosted/fixtures
R=tests/cases/slot_contract/reject
expect_refused "$R/double_release_slot/main.pgy" PGY_SEM_SLOT_DOUBLE_RELEASE slot_double_release
expect_refused "$R/released_slot_read/main.pgy" PGY_SEM_SLOT_RELEASED slot_read_after_release
expect_refused "$R/released_slot_write/main.pgy" PGY_SEM_SLOT_RELEASED slot_write_after_release
expect_refused "$R/device_double_release/main.pgy" PGY_SEM_BUILTIN_ARGS_INVALID device_slot_double_release
expect_refused "$R/device_released_read/main.pgy" PGY_SEM_SLOT_RELEASED device_slot_use_after_release
expect_refused "$F/slot_release_branch_read.pgy" PGY_SEM_SLOT_RELEASED slot_read_after_release
expect_refused "$F/slot_release_loop_twice.pgy" PGY_SEM_SLOT_DOUBLE_RELEASE slot_double_release
expect_refused "$F/slot_release_range_twice.pgy" PGY_SEM_SLOT_DOUBLE_RELEASE slot_double_release
expect_refused "$F/slot_release_break_read.pgy" PGY_SEM_SLOT_RELEASED slot_read_after_release
expect_refused "$F/slot_release_match_read.pgy" PGY_SEM_SLOT_RELEASED slot_read_after_release
expect_refused "$F/slot_release_short_circuit_read.pgy" PGY_SEM_SLOT_RELEASED slot_read_after_release
expect_refused "$F/slot_release_device_loop.pgy" PGY_SEM_BUILTIN_ARGS_INVALID device_slot_double_release
expect_refused "$F/slot_release_ref_param_release.pgy" PGY_SEM_BUILTIN_ARGS_INVALID slot_release_not_owner
expect_refused "$F/slot_release_borrow_released.pgy" PGY_SEM_SLOT_RELEASED slot_borrow_released
expect_refused "$F/slot_release_device_moved_read.pgy" PGY_SEM_MOVE_FROM_RELEASED slot_move_from_released

# expect_runs NAME OUTPUT: every route admits a release path native admits
# and prints OUTPUT (newlines as spaces). The default LLVM route has no Slot
# value plan yet (the machine-layer projection and Slot<Int> definitions are
# refused after semantic admission, the same before this rule); there the
# gate requires only that no semantic diagnostic refused the program.
LLVM_CODEGEN_GAPS=0
expect_runs() {
    local name="$1" want="$2" leg out_rel actual
    for leg in native-c native-llvm default-c default-llvm; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if ! compile "$F/$name.pgy" "$leg" "$out_rel" && [[ "$leg" == default-llvm ]] &&
            ! grep -Fq 'pgy.selfhost.semantic' "$ROOT_DIR/$out_rel.log"; then
            LLVM_CODEGEN_GAPS=$((LLVM_CODEGEN_GAPS + 1))
            continue
        fi
        [[ -x "$ROOT_DIR/$out_rel" ]] ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2; fail "$leg refused $name"; }
        actual="$("$ROOT_DIR/$out_rel" < /dev/null | tr -d '\r' | tr '\n' ' ')" ||
            fail "$leg $name exited non-zero"
        [[ "$actual" == "$want" ]] || fail "$leg $name printed '$actual', expected '$want'"
    done
}

expect_runs slot_release_paths_ok "1 2 0 3 4 5 8 17 "
expect_runs slot_release_loop_claim_ok "3 "

echo "[$LABEL] PASS: 15 programs refused on native and both default routes with one public code; 2 release-path programs run equal on native C/LLVM and default C, semantically admitted on default LLVM ($LLVM_CODEGEN_GAPS refused later by its Slot code generation)"

#!/usr/bin/env bash
# Rc, Weak, Box, Allocator and TextBuilder values are single-owner runtime
# handles: a plain copy (let, assignment, return of a parameter, generic
# argument) would give one runtime storage two names, and the first release
# would leave the other dangling. Native semantic owns the rule in
# type_checker_builtin_owner_let_contract.c; the self-host semantic owns it in
# ast_single_owner_handle_verdict_owner.pgy. This gate checks that both front
# ends refuse each copy shape with the same first public code and publish no
# binary, that the admitted idioms still compile and run, and that a second
# RcDrop or WeakDrop through one binding panics on both native backends.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="single-owner-handle"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/single_owner_handle"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/cases/single_owner_handle"

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

# expect_refused NAME CODE LEG...   every leg refuses with CODE first
expect_refused() {
    local name="$1" code="$2"
    shift 2
    local leg out_rel actual
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$FIXTURES/$name.pgy" "$leg" "$out_rel"; then
            fail "$leg accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for $name"
        actual="$(first_code "$ROOT_DIR/$out_rel.log")"
        [[ "$actual" == "$code" ]] ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2
              fail "$leg refused $name with '${actual:-no code}', expected $code"; }
    done
}

# expect_unsupported NAME LEG...   the leg has no surface for the handle
expect_unsupported() {
    local name="$1"
    shift
    local leg out_rel
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$FIXTURES/$name.pgy" "$leg" "$out_rel"; then
            fail "$leg accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for $name"
    done
}

# expect_output NAME FIXTURE EXPECTED LEG...   (Print adds no final newline)
expect_output() {
    local name="$1" fixture="$2" expected="$3"
    shift 3
    local leg out_rel actual
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$fixture" "$leg" "$out_rel" ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        actual="$(tr -d '\r' <"$WORK_DIR/$name-$leg.out")"
        [[ "$actual" == "$expected" ]] ||
            fail "$leg printed '$actual' for $name, expected '$expected'"
    done
}

# expect_panic NAME REASON LEG...   the program compiles and the second drop panics
expect_panic() {
    local name="$1" reason="$2"
    shift 2
    local leg out_rel rc
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$FIXTURES/$name.pgy" "$leg" "$out_rel" ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        # The subshell reports the abort into the log instead of the console.
        set +e
        ( "$ROOT_DIR/$out_rel"; exit $? ) >"$WORK_DIR/$name-$leg.out" 2>&1
        rc=$?
        set -e
        [[ $rc -ne 0 ]] || fail "$leg $name returned from a second drop"
        grep -Fq "$reason" "$WORK_DIR/$name-$leg.out" ||
            { cat "$WORK_DIR/$name-$leg.out" >&2; fail "$leg $name lost the '$reason' panic"; }
        ! grep -Fq "second drop returned" "$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name continued after a second drop"
    done
}

COPY=PGY_SEM_ANCHORED_HANDLE_COPY
OWNER=PGY_SEM_BUILTIN_ARGS_INVALID

# Rc, Weak and Box<class> have no builtin surface on the self-host front end;
# native owns their refusal and the default routes refuse the program earlier.
for name in rc_let_copy rc_assign_copy rc_return_parameter rc_generic_identity \
    weak_let_copy box_let_copy; do
    expect_refused "$name" "$COPY" native-c
    expect_unsupported "$name" default-c
done
expect_refused rc_parameter_drop "$OWNER" native-c

for name in box_array_let_copy allocator_let_copy allocator_assign_copy \
    allocator_return_parameter allocator_generic_identity text_builder_rebind; do
    expect_refused "$name" "$COPY" native-c default-c default-llvm
done
expect_refused allocator_parameter_destroy "$OWNER" native-c default-c default-llvm
expect_refused text_builder_released PGY_SEM_MOVE_FROM_RELEASED native-c default-c default-llvm
expect_refused text_builder_return_live PGY_SEM_OWNER_NOT_CONSUMED native-c default-c default-llvm

expect_output rc-shared-clone "$FIXTURES/rc_shared_clone_positive.pgy" \
    $'5\n5\n5' native-c native-llvm
expect_output box-pass-return "$FIXTURES/box_pass_return_positive.pgy" \
    $'1\n2' native-c
expect_output allocator-borrowed-parameter \
    "$FIXTURES/allocator_borrowed_parameter_positive.pgy" \
    '<ok>' native-c native-llvm default-c default-llvm
expect_output text-builder-lifecycle \
    tests/cases/backend_compare/text_builder_lifecycle/main.pgy \
    'PergyraLang' native-c native-llvm default-c default-llvm

expect_panic rc_double_drop_runtime "RcDrop on invalid Rc" native-c native-llvm
expect_panic weak_double_drop_runtime "WeakDrop on invalid Weak" native-c native-llvm

echo "[$LABEL] handle copies are refused with one code by both front ends, admitted idioms run, and a second Rc/Weak drop panics on both native backends: PASS"

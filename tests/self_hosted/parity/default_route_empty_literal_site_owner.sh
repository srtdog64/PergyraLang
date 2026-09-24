#!/usr/bin/env bash
# An empty `[]` or `{}` takes the type its storage site already fixes, on the
# default route as on native C and LLVM: a constructor field, a function or
# method parameter, a declared result type, an assigned binding or field. Both
# front ends refused `Doc(true, [], [])` with Array<Unknown> and `values = []`
# as a type mismatch; native C also refused `Fill([])` and `acc.Sum([])`.
# tests/compare_backends.sh runs the native pipeline only, so the default
# route reached no gate.
# - Each literal is pushed into afterwards, so the element type it lowered
#   with is read back at run time.
# - An empty literal with no typed site is still refused by both front ends,
#   before any binary is published.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-route-empty-literal-site"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/default_route_empty_literal_site"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CASE="tests/cases/backend_compare/empty_literal_typed_site"

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
        "$PGY" "$fixture" "${flags[@]}" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

# expect_output NAME FIXTURE EXPECTED LEG...
expect_output() {
    local name="$1" fixture="$2" expected="$3"
    shift 3
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    local leg out_rel
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$fixture" "$leg" "$out_rel" ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg read another value back from $name"; }
    done
}

# The default LLVM route refuses programs with more than one routine before
# emission, so the whole case runs on the three legs that compile it and the
# single-routine assignment runs on all four. A List or Set returned by a user
# call cannot be bound or passed on the default C route at all ("List<T>
# initializer must be ListNew"), so the List and Set results live in
# empty_literal_collection_result, which tests/compare_backends.sh runs.
expect_output typed-site "$CASE/main.pgy" \
    "$(printf '%s\n' 0 0 42 n 56 x 107 3 11 9 z 0 3 6 1 0 0 12)" \
    native-c native-llvm default-c
expect_output assign-binding "$CASE/assign_binding.pgy" \
    "$(printf '%s\n' 0 3)" \
    native-c native-llvm default-c default-llvm

# `let values = [];` has no typed site. Both front ends refuse it and publish
# nothing: native as PGY_SEM_INFER_COLLECTION, the default route as
# initializer_type_unresolved (the pairing tests/concept_semantics/
# hashmap_admission.sh pins for an unresolved collection constructor).
for leg in native-c default-c; do
    out_rel="$WORK_REL/no-site-$leg.exe"
    if compile "$CASE/bad_no_site.pgy" "$leg" "$out_rel"; then
        fail "$leg accepted an empty array literal with no typed site"
    fi
    [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
        fail "$leg left a binary for an empty array literal with no typed site"
done
grep -Fq "Cannot infer Array<T> from an empty array literal" \
    "$ROOT_DIR/$WORK_REL/no-site-native-c.exe.log" ||
    fail "native refusal lost its inference diagnostic"
grep -Fq "Code: initializer_type_unresolved" \
    "$ROOT_DIR/$WORK_REL/no-site-default-c.exe.log" ||
    fail "default route refusal lost its initializer diagnostic"
grep -Fq "binding: values" "$ROOT_DIR/$WORK_REL/no-site-default-c.exe.log" ||
    fail "default route refusal lost the unresolved binding"

echo "[$LABEL] empty literals take their constructor field, parameter, result and assignment types and push back their element type on native C, native LLVM and the default C route, an assigned one also on default LLVM, and an empty literal with no typed site is refused by both front ends: PASS"

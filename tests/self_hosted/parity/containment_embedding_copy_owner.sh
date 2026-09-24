#!/usr/bin/env bash
# A zone constructor that takes a live subject binding, or a world constructor
# that takes a live zone binding, forks that identity into the constructed
# owner's slot without a declaration (docs/157). Native semantic refuses both
# in type_checker_world_embedding.c and type_checker_call_constructor.c; the
# self-host semantic owns the same rule in
# ast_containment_boundary_verdict_owner.pgy. The default C route used to
# accept both and run them. This gate checks that:
# - every leg refuses each undeclared embedding with PGY_SEM_ANCHORED_HANDLE_COPY
#   as its first public code and leaves no binary;
# - the default routes name the owned self-host code for each shape;
# - Clone(binding) and an inline constructor argument still compile and print
#   the same values on every leg that compiles them.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="containment-embedding-copy"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/containment_embedding_copy"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

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

# expect_refused NAME OWNED_CODE LEG...   first public code is the anchored
# handle copy; the default legs also carry the owned self-host code.
expect_refused() {
    local name="$1" owned="$2"
    shift 2
    local leg out_rel actual
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$FIXTURES/containment_embedding_$name.pgy" "$leg" "$out_rel"; then
            fail "$leg accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for $name"
        actual="$(first_code "$ROOT_DIR/$out_rel.log")"
        [[ "$actual" == PGY_SEM_ANCHORED_HANDLE_COPY ]] ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2
              fail "$leg refused $name with '${actual:-no code}', expected PGY_SEM_ANCHORED_HANDLE_COPY"; }
        case "$leg" in
            default-*)
                grep -Fq "Code: $owned" "$ROOT_DIR/$out_rel.log" ||
                    { tail -20 "$ROOT_DIR/$out_rel.log" >&2
                      fail "$leg refused $name without the owned code $owned"; } ;;
        esac
    done
}

# expect_values NAME EXPECTED LEG...
expect_values() {
    local name="$1" expected="$2"
    shift 2
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    local leg out_rel
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$FIXTURES/containment_embedding_$name.pgy" "$leg" "$out_rel" ||
            { tail -20 "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" </dev/null | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg printed other values for $name"; }
    done
}

ALL_LEGS=(native-c native-llvm default-c default-llvm)
expect_refused zone_copy_negative zone_subject_embedding_copy "${ALL_LEGS[@]}"
expect_refused zone_parameter_copy_negative zone_subject_embedding_copy "${ALL_LEGS[@]}"
expect_refused world_nested_copy_negative zone_subject_embedding_copy "${ALL_LEGS[@]}"
expect_refused world_copy_negative world_zone_embedding_copy "${ALL_LEGS[@]}"

DECLARED="$FIXTURES/containment_embedding_declared_forms.pgy"
DECLARED_VALUES=$'false\ntrue\nfalse\ntrue\nfalse'
expect_values declared_forms "$DECLARED_VALUES" native-c native-llvm default-c
# The default LLVM route refuses zone programs in direct-MIR codegen, after
# semantic. A refusal there must not be a semantic one, and the self-host front
# end must publish MIR for the declared forms; if the leg compiles the program,
# it must print the same values.
if compile "$DECLARED" default-llvm "$WORK_REL/declared_forms-default-llvm.exe"; then
    expect_values declared_forms "$DECLARED_VALUES" default-llvm
elif grep -Eq 'Code: [a-z_]+|"code": *"PGY_SEM_' "$WORK_DIR/declared_forms-default-llvm.exe.log"; then
    tail -20 "$WORK_DIR/declared_forms-default-llvm.exe.log" >&2
    fail "default-llvm refused the declared forms in semantic"
fi
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
    "$PGY" --self-driver --emit-mir-json-verified "$DECLARED") \
    >"$WORK_DIR/declared_forms.mir.json" 2>"$WORK_DIR/declared_forms.mir.err" ||
    { cat "$WORK_DIR/declared_forms.mir.err" >&2; fail "self-host semantic refused the declared forms"; }
grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/declared_forms.mir.json" ||
    fail "self-host front end published no MIR for the declared forms"

echo "[$LABEL] undeclared subject-into-zone and zone-into-world embeddings are refused on all four legs with one code, and the Clone and inline forms stay admitted: PASS"

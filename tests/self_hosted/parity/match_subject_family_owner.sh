#!/usr/bin/env bash
# The match subject's type family, not a variant spelling, decides whether a
# case is an Option/Result wrapper (docs/205 section 2.3). A user enum may
# declare variants named Some, None, Ok or Err.
#
# - native C and LLVM run user enums with such variants and print the
#   expected values;
# - an Option control and a Result<Int, String> control still run on native
#   C, native LLVM and the default C route (native C once left
#   PgyResult_Int_String undeclared);
# - native MIR JSON carries match_subject_family for every case;
# - mir_lower reconstructs a user-enum `Ok(p)` case as a payload read, never
#   as Unwrap(subject);
# - the default self-host route refuses the user-enum programs with no binary:
#   qualified payload constructors (`Verdict.Ok(7)`) are not implemented there
#   yet (docs/205 L3d). When they land, this becomes an execution check.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-match-subject-family"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/match_subject_family"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"
MIR_LOWER_SOURCE="src/self_hosted/mir_lower/main.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local fixture="$1" out_rel="$2" log="$3"
    shift 3
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "$@" -o "$out_rel") >"$log" 2>&1
}

run_expect() {
    local name="$1" fixture="$2" expected="$3"
    shift 3
    local out_rel="$WORK_REL/$name.exe"
    compile "$fixture" "$out_rel" "$WORK_DIR/$name.log" "$@" || {
        cat "$WORK_DIR/$name.log" >&2
        fail "$name did not compile"
    }
    local actual
    actual="$("$ROOT_DIR/$out_rel" | tr -d '\r' | paste -sd '|' -)"
    [[ "$actual" == "$expected" ]] ||
        fail "$name printed '$actual', expected '$expected'"
}

# name:fixture:expected stdout joined by '|'
USER_ENUM_CASES=(
    "verdict-ok:$FIXTURES/match_subject_family_verdict_ok.pgy:7|-3"
    "maybe-some-none:$FIXTURES/match_subject_family_maybe_some_none.pgy:4|-1"
)
for entry in "${USER_ENUM_CASES[@]}"; do
    IFS=: read -r name fixture expected <<<"$entry"
    run_expect "$name-native-c" "$fixture" "$expected" --native-pipeline --backend=c
    run_expect "$name-native-llvm" "$fixture" "$expected" --native-pipeline --backend=llvm
    for backend in c llvm; do
        out_rel="$WORK_REL/$name-default-$backend.exe"
        if compile "$fixture" "$out_rel" "$WORK_DIR/$name-default-$backend.log" \
            --backend="$backend"; then
            fail "default $backend route accepted $name; turn this into an execution check"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
            fail "default $backend route left a binary for refused $name"
    done
done

OPTION_CONTROL="$FIXTURES/match_subject_family_option_control.pgy"
run_expect option-native-c "$OPTION_CONTROL" "5|0" --native-pipeline --backend=c
run_expect option-native-llvm "$OPTION_CONTROL" "5|0" --native-pipeline --backend=llvm
run_expect option-default-c "$OPTION_CONTROL" "5|0" --backend=c
RESULT_CONTROL="$FIXTURES/match_subject_family_result_control.pgy"
run_expect result-native-c "$RESULT_CONTROL" "7|-3" --native-pipeline --backend=c
run_expect result-native-llvm "$RESULT_CONTROL" "7|-3" --native-pipeline --backend=llvm
run_expect result-default-c "$RESULT_CONTROL" "7|-3" --backend=c

families() {
    local fixture="$1" json="$2"
    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$fixture")") >"$json"
    "${PYTHON_BIN:-python3}" - "$json" <<'PY'
import json, sys
doc = json.load(open(sys.argv[1], encoding="utf-8"))
rows = []
for routine in doc.get("routines", []):
    for block in routine.get("blocks", []):
        for inst in block.get("instructions", []):
            if inst.get("source_type") == "AST_MATCH_CASE":
                if "match_subject_family" not in inst:
                    raise SystemExit("match case without match_subject_family")
                rows.append(f"{inst.get('match_variant')}={inst['match_subject_family']}")
print(" ".join(rows))
PY
}
check_families() {
    local name="$1" fixture="$2" expected="$3"
    local actual
    actual="$(families "$fixture" "$WORK_DIR/$name.mirjson")" ||
        fail "$name MIR JSON is missing a subject family"
    [[ "$actual" == "$expected" ]] ||
        fail "$name families '$actual', expected '$expected'"
}
check_families verdict-ok "$FIXTURES/match_subject_family_verdict_ok.pgy" "Ok=enum Bad=enum"
check_families maybe-some-none "$FIXTURES/match_subject_family_maybe_some_none.pgy" "Some=enum None=enum"
check_families option-control "$OPTION_CONTROL" "Some=option None=option"
check_families result-control "$RESULT_CONTROL" "Ok=result Err=result"

MIR_LOWER="$WORK_DIR/mir_lower.exe"
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$MIR_LOWER_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER")") \
    >"$WORK_DIR/mir_lower.build.log" 2>&1 || {
        cat "$WORK_DIR/mir_lower.build.log" >&2
        fail "mir_lower did not build"
    }
(cd "$ROOT_DIR" && "$MIR_LOWER" "$WORK_REL/verdict-ok.mirjson") \
    >"$WORK_DIR/verdict-ok.reast" 2>"$WORK_DIR/verdict-ok.reast.err" || {
        cat "$WORK_DIR/verdict-ok.reast" "$WORK_DIR/verdict-ok.reast.err" >&2
        fail "mir_lower refused the user-enum MIR"
    }
! grep -Fq 'Unwrap(' "$WORK_DIR/verdict-ok.reast" ||
    fail "mir_lower rendered a user-enum case as a wrapper unwrap"
grep -Fq 'v.Ok._0' "$WORK_DIR/verdict-ok.reast" ||
    fail "mir_lower lost the user-enum payload read"

echo "[$LABEL] family-decided user-enum and Option cases on native C/LLVM, MIR JSON and mir_lower: PASS"

#!/usr/bin/env bash
# A `var` declaration row is named "mutable-def" in MIR (a `let` row is
# "ssa-def"); BindingDeclarationModeFromMirName owns that vocabulary. The
# scalar CFG graph admission, the Array<Int> reverse admission and the scalar
# literal-definition reader compared the name with "ssa-def" alone, so from
# 2026-11-10 every program they reached with a `var` declaration ended in
# CODEGEN ERROR on the default LLVM route and the general emitter's C backend.
# Each positive runs on native C/LLVM, the default C/LLVM routes and the
# general emitter's C and LLVM backends with the same stdout and exit status.
# Since 2026-11-20 the general route takes the one-routine foreach, whose
# Array index calls the bounds-panic runtime export; the general emitter's
# LLVM leg links no runtime library, so that program runs on the other five.
# Each MIR mutation must be refused with no artifact:
# - a declaration row with a name outside the vocabulary;
# - a reassignment row named as a declaration.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="direct-mir-mutable-declaration"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON="${PYTHON_BIN:-python3}"
WORK_REL=".tmp/self_hosted/direct_mir_mutable_declaration"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
LEGS=(native-c native-llvm default-c default-llvm direct-c direct-llvm)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/foreach_sum.pgy" <<'PGY'
func Main() -> Void {
    var xs: Array<Int> = [10, 20, 30, 40];
    var total: Int = 0;
    for v in xs {
        total = total + v;
    }
    Log(ToString(total));
}
PGY

cat >"$WORK_DIR/while_counter.pgy" <<'PGY'
func Main() -> Void {
    var i: Int = 0;
    var sum: Int = 0;
    while i < 4 {
        sum = sum + i;
        i = i + 1;
    }
    Log(ToString(sum));
}
PGY

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        default-llvm) echo "--backend=llvm" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# compile NAME LEG: builds WORK_DIR/NAME-LEG.exe, returns the compiler status.
# The direct legs are the general emitter's backends: the self-host MIR of
# NAME projected with --mir-json-backend and built with the C compiler/clang.
compile() {
    local name="$1" leg="$2" status=0
    case "$leg" in
        direct-c)
            (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
                    -o "$WORK_REL/$name.mir.json" &&
                "$DRIVER" --mir-json-backend=c "$WORK_REL/$name.mir.json" \
                    -o "$WORK_REL/$name-direct.c" &&
                ! grep -Fq 'CODEGEN ERROR' "$WORK_REL/$name-direct.c" &&
                "$CC" -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread \
                    "$WORK_REL/$name-direct.c" -o "$WORK_REL/$name-$leg.exe" -lm) \
                >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
            return "$status" ;;
        direct-llvm)
            (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
                    -o "$WORK_REL/$name.mir.json" &&
                "$DRIVER" --mir-json-backend=llvm "$WORK_REL/$name.mir.json" \
                    -o "$WORK_REL/$name-direct.ll" &&
                ! grep -Fq 'CODEGEN ERROR' "$WORK_REL/$name-direct.ll" &&
                "$CLANG" -x ir "$WORK_REL/$name-direct.ll" -o "$WORK_REL/$name-$leg.exe") \
                >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
            return "$status" ;;
    esac
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/$name.pgy" $(leg_flags "$leg") \
        -o "$WORK_REL/$name-$leg.exe") \
        >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
    return "$status"
}

# expect_run NAME STDOUT: every leg prints STDOUT and exits 0.
expect_run() {
    local name="$1" expected_out="$2" leg status
    printf '%s\n' "$expected_out" >"$WORK_DIR/$name.expected"
    for leg in "${LEGS[@]}"; do
        compile "$name" "$leg" ||
            { cat "$WORK_DIR/$name-$leg.compile.log" >&2; fail "$leg did not compile $name"; }
        status=0
        "$WORK_DIR/$name-$leg.exe" >"$WORK_DIR/$name-$leg.out" \
            2>"$WORK_DIR/$name-$leg.err" </dev/null || status=$?
        [[ "$status" == 0 ]] || fail "$leg $name exited $status"
        tr -d '\r' <"$WORK_DIR/$name-$leg.out" >"$WORK_DIR/$name-$leg.out.lf"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" >&2 || true
              fail "$leg $name printed other output"; }
    done
}

# expect_mutation_refused NAME MUTATION: both general-emitter backends refuse
# the mutated MIR of NAME and leave no artifact.
expect_mutation_refused() {
    local name="$1" mutation="$2" target out
    "$PYTHON" - "$WORK_DIR/$name.mir.json" "$mutation" "$WORK_DIR/$name.$mutation.json" <<'PY' ||
import json, sys
source, mutation, target = sys.argv[1:4]
with open(source, encoding="utf-8") as handle:
    program = json.load(handle)
routines = program["routines"] if isinstance(program, dict) else program
main = [routine for routine in routines if routine.get("name") == "Main"][0]
rows = [row for block in main["blocks"] for row in block["instructions"]]
if mutation == "unknown-declaration-name":
    row = [r for r in rows if r.get("name") == "mutable-def" and r.get("arg0") == "total"][0]
    row["name"] = "frozen-def"
elif mutation == "reassignment-named-declaration":
    row = [r for r in rows if r.get("source_type") == "AST_ASSIGNMENT" and r.get("arg0") == "total"][0]
    if row.get("name") != "ssa-def":
        raise SystemExit("reassignment row is not ssa-def")
    row["name"] = "mutable-def"
else:
    raise SystemExit("unknown mutation " + mutation)
with open(target, "w", encoding="utf-8") as handle:
    json.dump(program, handle)
PY
        fail "$mutation mutation failed"
    for target in c llvm; do
        out="$WORK_DIR/$name.$mutation.$target"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
                "$WORK_REL/$name.$mutation.json" -o "$WORK_REL/$name.$mutation.$target") \
                >"$out.log" 2>&1 && ! grep -Fq 'CODEGEN ERROR' "$out" "$out.log"; then
            fail "$target admitted $mutation"
        fi
        grep -Eq 'CODEGEN ERROR|MIR-LOWER ERROR|direct MIR' "$out.log" "$out" 2>/dev/null ||
            { cat "$out.log" >&2; fail "$target refused $mutation without a diagnostic"; }
        if [[ -s "$out" ]] && ! grep -Fq 'CODEGEN ERROR' "$out"; then
            fail "$target left an artifact for $mutation"
        fi
    done
}

expect_run while_counter '6'
LEGS=(native-c native-llvm default-c default-llvm direct-c)
expect_run foreach_sum '100'

expect_mutation_refused foreach_sum unknown-declaration-name
expect_mutation_refused foreach_sum reassignment-named-declaration

echo "[$LABEL] 2 var-declared loop programs equal (1 on six legs, 1 on five); 2 MIR mutations refused on C/LLVM: PASS"

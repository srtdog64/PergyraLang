#!/usr/bin/env bash
# Int `/` and `%` whose divisor is an Int literal other than 0 and -1, inside
# i32, are plain operations on the installed direct-MIR C and LLVM routes, as
# native emits them (src/codegen/codegen_scalar_arithmetic_policy.c). Every
# other divisor keeps the checked runtime helper: a literal 0, a negated
# literal (`-7` and `-1` are negate nodes, natively too), a dynamic divisor and
# a Long literal. Every program runs equal to native C and LLVM, output, exit
# code and panic class. A MIR literal rewritten to 0 or -1 keeps the helper and
# panics; a plain operation there would trap or divide by zero.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-safe-literal-divisor"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_safe_literal_divisor"
WORK_DIR="$ROOT_DIR/$WORK_REL"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python is unavailable"
rm -rf "$WORK_DIR"; mkdir -p "$WORK_DIR"
cp "$ROOT_DIR/tests/self_hosted/fixtures/direct_mir_safe_literal_divisor.pgy" "$WORK_DIR/positive.pgy"
cat >"$WORK_DIR/div_zero.pgy" <<'PGY'
func Divide(x: Int) -> Int {
    return x / 0;
}

func Main() -> Int {
    Log(Divide(7));
    return 0;
}
PGY
cat >"$WORK_DIR/mod_zero.pgy" <<'PGY'
func Remainder(x: Int) -> Int {
    return x % 0;
}

func Main() -> Int {
    Log(Remainder(7));
    return 0;
}
PGY
cat >"$WORK_DIR/div_minus_one.pgy" <<'PGY'
func Divide(x: Int) -> Int {
    return x / -1;
}

func Main() -> Int {
    Log(Divide(5));
    Log(Divide(-2147483647 - 1));
    return 0;
}
PGY
cat >"$WORK_DIR/mod_minus_one.pgy" <<'PGY'
func Remainder(x: Int) -> Int {
    return x % -1;
}

func Main() -> Int {
    Log(Remainder(5));
    Log(Remainder(-2147483647 - 1));
    return 0;
}
PGY
cat >"$WORK_DIR/dynamic_zero.pgy" <<'PGY'
func Divide(x: Int, d: Int) -> Int {
    return x / d;
}

func Remainder(x: Int, d: Int) -> Int {
    return x % d;
}

func Main() -> Int {
    Log(Divide(9, 4));
    Log(Remainder(9, 4));
    Log(Divide(9, 0));
    return 0;
}
PGY
cat >"$WORK_DIR/long_literal.pgy" <<'PGY'
func Divide(x: Long) -> Long {
    return x / 4L;
}

func Remainder(x: Long) -> Long {
    return x % 17L;
}

func Main() -> Int {
    Log(Divide(-9223372036854775807L));
    Log(Remainder(-9223372036854775807L));
    Log(Divide(100L));
    Log(Remainder(100L));
    return 0;
}
PGY
NEGATIVES="div_zero mod_zero div_minus_one mod_minus_one dynamic_zero long_literal"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >/dev/null 2>"$WORK_DIR/runtime.err" || fail "runtime object did not compile"

# Run BIN; write stdout, exit code and the panic class line to STEM.observed.
observe() {
    local bin="$1" stem="$2" rc=0
    (ulimit -c 0; "$bin" </dev/null >"$WORK_DIR/$stem.stdout" 2>"$WORK_DIR/$stem.stderr") 2>/dev/null || rc=$?
    { tr -d '\r' <"$WORK_DIR/$stem.stdout"; echo "rc=$rc"
      grep -o 'class=[a-z-]*' "$WORK_DIR/$stem.stderr" || true; } >"$WORK_DIR/$stem.observed"
}

# Project MIR (relative to the root) on BACKEND into STEM.BACKEND.{c,ll}, build, observe.
self_leg() {
    local mir_rel="$1" stem="$2" backend="$3" suffix=c
    [[ "$backend" == llvm ]] && suffix=ll
    local artifact_rel="$WORK_REL/$stem.$backend.$suffix" bin="$WORK_DIR/$stem.$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mir_rel" -o "$artifact_rel") \
        >"$WORK_DIR/$stem.$backend.project.log" 2>&1 || fail "$stem $backend projection failed"
    if [[ "$backend" == c ]]; then
        "$CC" -x c -std=c11 -Werror=implicit-function-declaration -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" -pthread "$ROOT_DIR/$artifact_rel" -lm -o "$bin" \
            >/dev/null 2>"$WORK_DIR/$stem.c.compile.err" || fail "$stem C did not compile"
    else
        "$CLANG" -x ir "$ROOT_DIR/$artifact_rel" -x none "$WORK_DIR/runtime.o" -pthread -lm \
            -o "$bin" >/dev/null 2>"$WORK_DIR/$stem.llvm.compile.err" || fail "$stem LLVM did not link"
    fi
    observe "$bin" "$stem.$backend"
}

for stem in positive $NEGATIVES; do
    for backend in c llvm; do
        (cd "$ROOT_DIR" && PGY_NATIVE_PIPELINE=1 "$PGY" "$WORK_REL/$stem.pgy" --backend=$backend \
            -o "$WORK_REL/$stem.native-$backend.exe") >"$WORK_DIR/$stem.native-$backend.log" 2>&1 ||
            fail "$stem native $backend build failed"
        observe "$WORK_DIR/$stem.native-$backend.exe" "$stem.native-$backend"
    done
    cmp -s "$WORK_DIR/$stem.native-c.observed" "$WORK_DIR/$stem.native-llvm.observed" ||
        fail "$stem native C and LLVM disagree"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$stem.pgy" \
        -o "$WORK_REL/$stem.mir.json") >"$WORK_DIR/$stem.mir.log" 2>&1 || fail "$stem MIR production failed"
    for backend in c llvm; do
        self_leg "$WORK_REL/$stem.mir.json" "$stem" "$backend"
        cmp -s "$WORK_DIR/$stem.native-c.observed" "$WORK_DIR/$stem.$backend.observed" ||
            fail "$stem $backend runs unlike native"
    done
done
grep -Fq 'class=divide-by-zero' "$WORK_DIR/div_zero.native-c.observed" || fail "native literal 0 did not panic"
grep -Fq 'class=arithmetic-overflow' "$WORK_DIR/div_minus_one.native-c.observed" || fail "native INT_MIN / -1 did not panic"
grep -Fq 'class=divide-by-zero' "$WORK_DIR/dynamic_zero.native-c.observed" || fail "native dynamic 0 did not panic"

# count FILE TEXT: occurrences of TEXT in FILE.
count() { { grep -oF -- "$2" "$1" || true; } | wc -l | tr -d ' '; }
expect() { [[ "$(count "$1" "$2")" == "$3" ]] || fail "${1##*/}: $(count "$1" "$2") of '$2', expected $3"; }
# positive: DivFour, DivMax and `/= 4` divide plainly, ModSeventeen, ModMax and
# `%= 17` take the remainder plainly; only the negated -7 sites keep a helper.
expect "$WORK_DIR/positive.llvm.ll" '= sdiv i32 ' 3
expect "$WORK_DIR/positive.llvm.ll" '= srem i64 ' 3
expect "$WORK_DIR/positive.llvm.ll" 'call i32 @pgy_checked_div_i32_export(' 1
expect "$WORK_DIR/positive.llvm.ll" 'call i64 @pgy_checked_mod_i64_export(' 1
expect "$WORK_DIR/positive.c.c" ') / (int32_t)(' 3
expect "$WORK_DIR/positive.c.c" ') % (int64_t)(' 3
expect "$WORK_DIR/positive.c.c" 'pgy_checked_div_i32_export(' 1
expect "$WORK_DIR/positive.c.c" 'pgy_checked_mod_i64_export(' 1
for stem in $NEGATIVES; do
    for text in '= sdiv ' '= srem '; do expect "$WORK_DIR/$stem.llvm.ll" "$text" 0; done
    for text in ') / (int32_t)(' ') % (int64_t)('; do expect "$WORK_DIR/$stem.c.c" "$text" 0; done
done
expect "$WORK_DIR/div_zero.llvm.ll" 'call i32 @pgy_checked_div_i32_export(' 1
expect "$WORK_DIR/mod_zero.llvm.ll" 'call i64 @pgy_checked_mod_i64_export(' 1
expect "$WORK_DIR/div_minus_one.llvm.ll" 'call i32 @pgy_checked_div_i32_export(' 1
expect "$WORK_DIR/mod_minus_one.llvm.ll" 'call i64 @pgy_checked_mod_i64_export(' 1
expect "$WORK_DIR/dynamic_zero.llvm.ll" 'call i32 @pgy_checked_div_i32_export(' 1
expect "$WORK_DIR/dynamic_zero.llvm.ll" 'call i64 @pgy_checked_mod_i64_export(' 1
expect "$WORK_DIR/long_literal.llvm.ll" 'call i64 @pgy_checked_div_i64_export(' 1
expect "$WORK_DIR/long_literal.llvm.ll" 'call i64 @pgy_checked_mod_i64_export(' 1

# A MIR integer literal divisor rewritten to 0 or -1 (no source spells a -1
# literal node) is not safe: the helper stays and the program panics.
"$PYTHON_BIN" - "$WORK_DIR/positive.mir.json" "$WORK_DIR" <<'PY'
import json, sys
source, out = sys.argv[1:]
document = json.load(open(source, encoding="utf-8"))
for routine_name, old, new, tag in (("DivFour", "4", "-1", "div_minus_one"), ("DivFour", "4", "0", "div_zero"),
                                    ("ModSeventeen", "17", "0", "mod_zero")):
    mutated = json.loads(json.dumps(document))
    routine = next(row for row in mutated["routines"] if row["name"] == routine_name)
    nodes = routine["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"]
    literal = [node for node in nodes if node["kind"] == "integer_literal" and node["text"] == old]
    if len(literal) != 1:
        raise SystemExit(f"{routine_name}: expected one literal {old}")
    literal[0]["text"] = new
    with open(f"{out}/mutated_{tag}.mir.json", "w", encoding="utf-8", newline="\n") as handle:
        json.dump(mutated, handle, separators=(",", ":"))
PY
for tag in div_minus_one div_zero mod_zero; do
    for backend in c llvm; do
        self_leg "$WORK_REL/mutated_$tag.mir.json" "mutated_$tag" "$backend"
        observed="$WORK_DIR/mutated_$tag.$backend.observed"
        class=divide-by-zero; [[ "$tag" == div_minus_one ]] && class=arithmetic-overflow
        grep -Fq "class=$class" "$observed" || fail "mutated $tag $backend did not panic with $class"
    done
    expect "$WORK_DIR/mutated_$tag.llvm.ll" '= sdiv i32 ' "$([[ $tag == mod_zero ]] && echo 3 || echo 2)"
    expect "$WORK_DIR/mutated_$tag.c.c" ') / (int32_t)(' "$([[ $tag == mod_zero ]] && echo 3 || echo 2)"
done
echo "[$LABEL] Int / and % by literals 4, 17, 2147483647 (and /=, %=) are plain on C and LLVM; 0, -1, -7, a dynamic and a Long divisor and MIR literals 0/-1 keep the checked helper; every leg runs as native: PASS"

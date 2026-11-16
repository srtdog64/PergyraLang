#!/usr/bin/env bash
# Builtin and range rows the general direct-MIR emitter gained for the driver
# and fuzz shards (track DM2):
# - Random and SeedRandom call native Random and SeedRandom from the runtime,
#   so the random capability and the generator are the native ones;
# - SetSize reads a Set<String> (its C program includes pgy_runtime.h, whose
#   pgy_bool_to_string then serves ToString(Bool));
# - a range loop may stop at a literal bound.
# Each positive runs on native C/LLVM, the default C/LLVM routes and the
# general emitter's C backend with the same stdout and exit status; the
# default LLVM route refused all of them before. Each neighbour stays refused
# by the default LLVM route while native admits it:
# - ArrayMap with a declared function (no callable-argument row yet);
# - a range loop that stops at a let value (no typed bound identity yet);
# and one MIR mutation: SeedRandom given a String argument.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="direct-mir-general-emitter-builtin-rows"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON="${PYTHON_BIN:-python3}"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir/direct_mir_general_emitter_builtin_rows_mutations.py"
WORK_REL=".tmp/self_hosted/direct_mir_general_emitter_builtin_rows"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CC="${PGY_SELFHOST_CC:-gcc}"
LEGS=(native-c native-llvm default-c default-llvm direct-c)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/random_draw.pgy" <<'PGY'
func Draw(limit: Int) -> Int {
    return Random(limit) + 1;
}

func Main() -> Int {
    SeedRandom(7);
    let first: Int = Draw(1000);
    let second: Int = Draw(1000);
    SeedRandom(7);
    let again: Int = Draw(1000);
    Log(first == again);
    Log(first >= 1 && first <= 1000 && second >= 1 && second <= 1000);
    Log(Random(0));
    var total: Int = 0;
    var i: Int = 0;
    while i < 20 {
        total = total + Random(9);
        i = i + 1;
    }
    Log(total >= 0 && total <= 160);
    return 0;
}
PGY

cat >"$WORK_DIR/set_size.pgy" <<'PGY'
func Collect(words: Array<String>) -> Int {
    var seen: Set<String> = SetNew();
    var i: Int = 0;
    while i < ArrayLength(words) {
        SetAdd(seen, words[i]);
        i = i + 1;
    }
    let has_red: Bool = SetHas(seen, "red");
    Log(ToString(has_red));
    return SetSize(seen);
}

func Main() -> Int {
    let words: Array<String> = ["red", "blue", "red", "green"];
    Log(Collect(words));
    let empty: Set<String> = SetNew();
    Log(SetSize(empty));
    return 0;
}
PGY

cat >"$WORK_DIR/literal_range.pgy" <<'PGY'
func Mix(x: Int, y: Int) -> Int {
    let v: Int = x * 3 + y;
    if v % 2 == 0 {
        return v / 2;
    }
    return v + 5;
}

func Main() -> Int {
    var acc: Int = 0;
    for i in 0..5 {
        acc = acc + Mix(16 + i, 2);
    }
    Log(acc);
    return 0;
}
PGY

cat >"$WORK_DIR/array_map.pgy" <<'PGY'
func Double(x: Int) -> Int {
    return x * 2;
}

func Main() -> Int {
    var xs: Array<Int> = [];
    ArrayPush(xs, 3);
    let doubled: Array<Int> = ArrayMap(xs, Double);
    Log(doubled[0]);
    return 0;
}
PGY

cat >"$WORK_DIR/let_range.pgy" <<'PGY'
func Square(x: Int) -> Int {
    return x * x;
}

func Main() -> Int {
    let limit: Int = 4;
    var total: Int = 0;
    for i in 0..limit {
        total = total + Square(i);
    }
    Log(total);
    return 0;
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
# The direct-c leg is the general emitter's C backend: the self-host MIR of
# NAME projected with --mir-json-backend=c and built with the C compiler.
compile() {
    local name="$1" leg="$2" status=0
    if [[ "$leg" == direct-c ]]; then
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
                -o "$WORK_REL/$name.direct.mir.json" &&
            "$DRIVER" --mir-json-backend=c "$WORK_REL/$name.direct.mir.json" \
                -o "$WORK_REL/$name-direct.c" &&
            "$CC" -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread \
                "$WORK_REL/$name-direct.c" -o "$WORK_REL/$name-$leg.exe" -lm) \
            >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
        return "$status"
    fi
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/$name.pgy" $(leg_flags "$leg") \
        -o "$WORK_REL/$name-$leg.exe") \
        >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
    return "$status"
}

# expect_run NAME STDOUT STATUS: every leg prints STDOUT and exits STATUS.
expect_run() {
    local name="$1" expected_out="$2" expected_status="$3" leg status
    printf '%s\n' "$expected_out" >"$WORK_DIR/$name.expected"
    for leg in "${LEGS[@]}"; do
        compile "$name" "$leg" ||
            { cat "$WORK_DIR/$name-$leg.compile.log" >&2; fail "$leg did not compile $name"; }
        status=0
        "$WORK_DIR/$name-$leg.exe" >"$WORK_DIR/$name-$leg.out" \
            2>"$WORK_DIR/$name-$leg.err" </dev/null || status=$?
        [[ "$status" == "$expected_status" ]] ||
            fail "$leg $name exited $status instead of $expected_status"
        tr -d '\r' <"$WORK_DIR/$name-$leg.out" >"$WORK_DIR/$name-$leg.out.lf"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" >&2 || true
              fail "$leg $name printed other output"; }
    done
}

# expect_direct_refusal NAME NATIVE_STDOUT: native LLVM runs NAME, and the
# default LLVM route refuses it with a CODEGEN ERROR and writes no program.
expect_direct_refusal() {
    local name="$1" native_out="$2"
    compile "$name" native-llvm ||
        { cat "$WORK_DIR/$name-native-llvm.compile.log" >&2; fail "native LLVM refused $name"; }
    "$WORK_DIR/$name-native-llvm.exe" </dev/null | tr -d '\r' >"$WORK_DIR/$name-native-llvm.out" ||
        fail "native LLVM $name failed"
    printf '%s\n' "$native_out" | cmp -s - "$WORK_DIR/$name-native-llvm.out" ||
        fail "native LLVM $name printed other output"
    if compile "$name" default-llvm; then
        fail "default LLVM admitted $name"
    fi
    [[ ! -e "$WORK_DIR/$name-default-llvm.exe" ]] ||
        fail "default LLVM wrote a program for refused $name"
    grep -Fq "CODEGEN ERROR" "$WORK_DIR/$name-default-llvm.compile.log" ||
        { cat "$WORK_DIR/$name-default-llvm.compile.log" >&2
          fail "default LLVM refused $name without a direct MIR refusal"; }
}

# expect_mutation_refused NAME MUTATION REASON: the direct MIR LLVM route
# admits NAME's MIR as produced and refuses the mutated MIR naming REASON.
expect_mutation_refused() {
    local name="$1" mutation="$2" reason="$3"
    local mir_rel="$WORK_REL/$name.mir.json" mutated_rel="$WORK_REL/$name.$mutation.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy") \
        >"$ROOT_DIR/$mir_rel" 2>"$WORK_DIR/$name.mir.err" || fail "$name MIR production failed"
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm "$mir_rel" \
        -o "$WORK_REL/$name.control.ll") >"$WORK_DIR/$name.control.out" 2>&1 ||
        { cat "$WORK_DIR/$name.control.out" >&2; fail "direct MIR LLVM refused unmutated $name"; }
    "$PYTHON" "$MUTATIONS" "$ROOT_DIR/$mir_rel" "$mutation" "$ROOT_DIR/$mutated_rel" ||
        fail "$mutation mutation failed"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm "$mutated_rel" \
            -o "$WORK_REL/$name.$mutation.ll") >"$WORK_DIR/$name.$mutation.out" 2>&1; then
        fail "direct MIR LLVM admitted $mutation"
    fi
    [[ ! -s "$WORK_DIR/$name.$mutation.ll" ]] || fail "$mutation left an artifact"
    grep -Fq "$reason" "$WORK_DIR/$name.$mutation.out" ||
        { cat "$WORK_DIR/$name.$mutation.out" >&2; fail "$mutation refused without $reason"; }
}

expect_run random_draw $'true\ntrue\n0\ntrue' 0
expect_run set_size $'true\n3\n0' 0
expect_run literal_range '206' 0

expect_direct_refusal array_map '6'
expect_direct_refusal let_range '14'

expect_mutation_refused random_draw seed-random-string-argument "stage=builtin-call"

echo "[$LABEL] 3 programs equal on five legs; 2 neighbours and 1 MIR mutation refused: PASS"

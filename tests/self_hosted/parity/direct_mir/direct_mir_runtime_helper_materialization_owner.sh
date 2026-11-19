#!/usr/bin/env bash
# The general emitter materializes a runtime helper (a String concat, an
# Array<Record> push, the Array<Bool> type) and declares the C library calls
# the helper makes. Before 2026-11-19 the two decisions read different facts,
# so a program reached one without the other and the LLVM artifact failed to
# assemble, and one operation read the wrong table and panicked the driver:
# - a borrowed String return normalized to an owned copy calls the concat
#   helper, which was emitted only for a source-level Concat;
# - an Array<Record> type emitted its push helper (realloc) whenever the type
#   was referenced, while realloc was declared only for a mutation;
# - MapKeys over Bool keys yields an Array<Bool> temporary whose type (LLVM
#   %pgy.array.bool, C pgy_ab) was defined only for an Array<Bool> local or
#   parameter;
# - ArraySet on a local Array<Record> read the parameter type table at -1.
# Each program runs on native C/LLVM, the default C/LLVM routes and the
# general emitter's C and LLVM backends with the same stdout and exit status.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="direct-mir-runtime-helper-materialization"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_runtime_helper_materialization"
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

cat >"$WORK_DIR/borrowed_string_return.pgy" <<'PGY'
func Pick(flag: Bool, name: String) -> String {
    if flag {
        return ToString(42);
    }
    return name;
}

func Main() -> Void {
    let first: String = Pick(true, "unused");
    let second: String = Pick(false, "kept");
    Log(first);
    Log(second);
}
PGY

cat >"$WORK_DIR/record_array_parameter_only.pgy" <<'PGY'
struct Node {
    value: Int,
    next: Int
}

func Count(nodes: Array<Node>, limit: Int) -> Int {
    if limit < 0 { return -1; }
    return limit;
}

func Main() -> Void {
    let first: Node = Node(10, 1);
    Log(first.value);
    Log(first.next);
}
PGY

cat >"$WORK_DIR/bool_map_keys.pgy" <<'PGY'
func Main() -> Void {
    var flags: HashMap<Bool, Int> = MapNew();
    MapSet(flags, true, 11);
    MapSet(flags, false, 22);
    Log(MapGet(flags, false));
    Log(ArrayLength(MapKeys(flags)));
}
PGY

cat >"$WORK_DIR/local_record_array_set.pgy" <<'PGY'
struct Row {
    indent: Int,
    text: String,
    kind: Int
}

func Main() -> Void {
    var rows: Array<Row> = [];
    ArrayPush(rows, Row(0, "old", 1));
    ArraySet(rows, 0, Row(2, "new", 8));
    Log(ToString(ArrayLength(rows)));
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

expect_run borrowed_string_return $'42\nkept'
expect_run record_array_parameter_only $'10\n1'
# The direct LLVM leg links no runtime library, so it cannot resolve the
# HashMap or bounds-panic runtime exports; the default LLVM route links them.
LEGS=(native-c native-llvm default-c default-llvm direct-c)
expect_run local_record_array_set '1'
expect_run bool_map_keys $'22\n2'

echo "[$LABEL] 2 programs equal on six legs, 2 runtime-calling programs on five: PASS"

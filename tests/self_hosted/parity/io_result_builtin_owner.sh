#!/usr/bin/env bash
# TryReadFile, TryWriteFile and CharFromCode mean the same on native C
# (--native-pipeline --backend=c), native LLVM (--native-pipeline
# --backend=llvm) and the default C route (--backend=c). The default LLVM
# route (--backend=llvm) either prints the same or refuses with its direct-MIR
# route diagnostic.
#
# Static rows: the builtin IoError variants are the rows of
# src/runtime/pgy_runtime_io_error.def, in order. The self-host parser's
# projection (io_error_builtin_enum_composition_owner.pgy) and the documented
# list in docs/108_stdlib_beta_freeze.md must equal them, and every runtime
# status that pgy_try_read_file_result or pgy_try_write_file_result can
# return must have a row, so the typed entries never reach their unmapped-
# status panic.
#
# Executed rows, each on every leg: a write and read-back that succeed, a
# missing file (OpenFailed), a `..` path (ResolveFailed), an absolute path
# without PGY_IO_ALLOW_ABSOLUTE=1 (ResolveFailed) and with it (OpenFailed:
# the path resolves and then does not open), a write into a missing
# directory (OpenFailed) and into a directory without write permission
# (OpenFailed). Under uid 0 the binaries run without CAP_DAC_OVERRIDE so the
# permission bits bind. A program that declares its own IoError beside the
# builtin one is refused on native C, native LLVM and the default C route; the
# native legs must name the program's declaration. The default C route refuses
# it today with an unrelated match_pattern_invalid ("duplicate enum identity"),
# so only the refusal is held there, and the default LLVM leg is not run.
set -euo pipefail

# The default legs are the self-hosted front end; an exported
# PGY_NATIVE_PIPELINE would silently turn them into native legs.
unset PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="io-result-builtin"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/io_result_builtin"
WORK_DIR="$ROOT_DIR/$WORK_REL"
DEF="$ROOT_DIR/src/runtime/pgy_runtime_io_error.def"
SELF_OWNER="$ROOT_DIR/src/self_hosted/parser/io_error_builtin_enum_composition_owner.pgy"
DOC="$ROOT_DIR/docs/108_stdlib_beta_freeze.md"
RUNTIME_IO="$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h"
RUNTIME_LIB_IO="$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

# --- static rows ----------------------------------------------------------
sed -nE 's/^PGY_IO_ERROR_VARIANT\(([A-Za-z]+), (PGY_RUNTIME_IO_STATUS_[A-Z_]+)\)$/\1/p' \
    "$DEF" > "$WORK_DIR/def.names"
sed -nE 's/^PGY_IO_ERROR_VARIANT\(([A-Za-z]+), (PGY_RUNTIME_IO_STATUS_[A-Z_]+)\)$/\2/p' \
    "$DEF" | sort > "$WORK_DIR/def.statuses"
[[ "$(wc -l < "$WORK_DIR/def.names")" -ge 2 ]] ||
    fail "found too few PGY_IO_ERROR_VARIANT rows in $DEF"
[[ -z "$(sort "$WORK_DIR/def.names" | uniq -d)" ]] || fail "$DEF repeats a variant"
sed -n '/^func ParserIoErrorVariantNames/,/^}/p' "$SELF_OWNER" |
    sed -nE 's/^[[:space:]]*"([A-Za-z]+)",?$/\1/p' > "$WORK_DIR/self.names"
cmp -s "$WORK_DIR/def.names" "$WORK_DIR/self.names" || {
    diff "$WORK_DIR/def.names" "$WORK_DIR/self.names" >&2 || true
    fail "ParserIoErrorVariantNames differs from $DEF"
}
sed -n '/^<!-- io-error-variants:start -->$/,/^<!-- io-error-variants:end -->$/p' "$DOC" |
    sed -nE 's/^[0-9]+\. `([A-Za-z]+)`.*$/\1/p' > "$WORK_DIR/doc.names"
cmp -s "$WORK_DIR/def.names" "$WORK_DIR/doc.names" || {
    diff "$WORK_DIR/def.names" "$WORK_DIR/doc.names" >&2 || true
    fail "the IoError list in docs/108_stdlib_beta_freeze.md differs from $DEF"
}
for runtime in "$RUNTIME_IO" "$RUNTIME_LIB_IO"; do
    for entry in pgy_try_read_file_result pgy_try_write_file_result; do
        body="$(sed -nE "/^([A-Za-z_]+ +)?$entry\(/,/^}/p" "$runtime")"
        [[ -n "$body" ]] || fail "no $entry definition in $runtime"
        printf '%s\n' "$body" | grep -oE 'PGY_RUNTIME_IO_STATUS_[A-Z_]+' |
            sort -u > "$WORK_DIR/used.statuses"
        unmapped="$(comm -23 "$WORK_DIR/used.statuses" "$WORK_DIR/def.statuses" | tr '\n' ' ')"
        [[ -z "$unmapped" ]] ||
            fail "$entry in ${runtime#"$ROOT_DIR/"} returns statuses with no IoError row: $unmapped"
    done
done

# --- executed rows ----------------------------------------------------------
cat > "$WORK_DIR/io.pgy" <<'PGY'
func DescribeError(e: IoError) -> String {
    match e {
        case ResolveFailed: return "err ResolveFailed";
        case OpenFailed: return "err OpenFailed";
        case SeekFailed: return "err SeekFailed";
        case TellFailed: return "err TellFailed";
        case TooLarge: return "err TooLarge";
        case AllocFailed: return "err AllocFailed";
        case ReadFailed: return "err ReadFailed";
        case WriteFailed: return "err WriteFailed";
    }
}

func DescribeRead(r: Result<String, IoError>) -> String {
    match r {
        case Ok(text): return "ok " + text;
        case Err(e): return DescribeError(e);
    }
}

func DescribeWrite(r: Result<Bool, IoError>) -> String {
    match r {
        case Ok(done):
            if done { return "ok true"; }
            return "ok false";
        case Err(e): return DescribeError(e);
    }
}

func Main() -> Void {
    Log("write: " + DescribeWrite(TryWriteFile("io_probe_out.txt", "hello")));
    Log("read back: " + DescribeRead(TryReadFile("io_probe_out.txt")));
    let missing = TryReadFile("io_probe_missing.txt");
    Log("missing: " + DescribeRead(missing));
    Log("dotdot: " + DescribeRead(TryReadFile("../io_probe_out.txt")));
    Log("absolute: " + DescribeRead(TryReadFile("/nonexistent-pgy-io-probe/x.txt")));
    Log("missing dir: " + DescribeWrite(TryWriteFile("io_probe_no_dir/x.txt", "x")));
    Log("readonly dir: " + DescribeWrite(TryWriteFile("io_probe_ro_dir/x.txt", "x")));
    let r: Result<String, IoError> = TryReadFile("io_probe_missing.txt");
    if IsErr(r) {
        let e: IoError = UnwrapErr(r);
        if e == OpenFailed { Log("unwrap err: OpenFailed"); }
    }
}
PGY
cat > "$WORK_DIR/io.expected" <<'OUT'
write: ok true
read back: ok hello
missing: err OpenFailed
dotdot: err ResolveFailed
absolute: err ResolveFailed
missing dir: err OpenFailed
readonly dir: err OpenFailed
unwrap err: OpenFailed
OUT
sed 's/^absolute: err ResolveFailed$/absolute: err OpenFailed/' \
    "$WORK_DIR/io.expected" > "$WORK_DIR/io.absolute.expected"

cat > "$WORK_DIR/cfc.pgy" <<'PGY'
func Describe(code: Int) -> Void {
    let c: Option<String> = CharFromCode(code);
    if IsSome(c) {
        let s: String = UnwrapOption(c);
        let n: Int = StringLength(s);
        let text: String = ToString(code) + ":" + ToString(n);
        let i: Int = 0;
        while i < n {
            text = text + " " + ToString(CharCode(s, n, i));
            i = i + 1;
        }
        Log(text);
    } else {
        Log(ToString(code) + ":none");
    }
}

func Main() -> Void {
    Describe(65);
    Describe(2047);
    Describe(2048);
    Describe(55295);
    Describe(57344);
    Describe(65536);
    Describe(1114111);
    Describe(0);
    Describe(-1);
    Describe(55296);
    Describe(57343);
    Describe(1114112);
}
PGY
cat > "$WORK_DIR/cfc.expected" <<'OUT'
65:1 65
2047:2 223 191
2048:3 224 160 128
55295:3 237 159 191
57344:3 238 128 128
65536:4 240 144 128 128
1114111:4 244 143 191 191
0:none
-1:none
55296:none
57343:none
1114112:none
OUT

cat > "$WORK_DIR/dup.pgy" <<'PGY'
enum IoError { Mine }

func Main() -> Void {
    let r: Result<String, IoError> = TryReadFile("x.txt");
    if IsErr(r) { Log("err"); }
}
PGY

# Under uid 0 the permission bits of io_probe_ro_dir do not bind unless the
# binary runs without CAP_DAC_OVERRIDE.
RUN_PREFIX=()
if [[ "$(id -u)" == 0 ]]; then
    command -v setpriv >/dev/null 2>&1 ||
        fail "running as uid 0 needs setpriv to make directory permissions bind"
    RUN_PREFIX=(setpriv --bounding-set=-dac_override,-dac_read_search)
fi

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        default-llvm) echo "--backend=llvm" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# compile PROGRAM LEG -> 0 when it compiled; the log is PROGRAM.LEG.log.
compile() {
    local program="$1" leg="$2" rc=0
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/$program.pgy" $(leg_flags "$leg") \
        -o "$WORK_REL/$program.$leg.exe") >"$WORK_DIR/$program.$leg.log" 2>&1 || rc=$?
    return "$rc"
}

# run_io LEG TAG [ENV...] -> WORK_DIR/io.LEG.TAG.out, run in a fresh directory.
run_io() {
    local leg="$1" tag="$2"
    shift 2
    local run_dir="$WORK_DIR/run.$leg.$tag"
    mkdir -p "$run_dir/sub/io_probe_ro_dir"
    chmod 0555 "$run_dir/sub/io_probe_ro_dir"
    local status=0
    (cd "$run_dir/sub" && env -u PGY_IO_ROOT -u PGY_IO_ALLOW_ABSOLUTE "$@" \
        "${RUN_PREFIX[@]}" "$WORK_DIR/io.$leg.exe" </dev/null \
        >"$WORK_DIR/io.$leg.$tag.raw" 2>"$WORK_DIR/io.$leg.$tag.err") || status=$?
    chmod 0755 "$run_dir/sub/io_probe_ro_dir"
    [[ "$status" == 0 ]] || {
        cat "$WORK_DIR/io.$leg.$tag.err" >&2
        fail "$leg: the TryReadFile/TryWriteFile program exited $status ($tag)"
    }
    tr -d '\r' <"$WORK_DIR/io.$leg.$tag.raw" >"$WORK_DIR/io.$leg.$tag.out"
    [[ "$(cat "$run_dir/sub/io_probe_out.txt")" == "hello" ]] ||
        fail "$leg: TryWriteFile reported success but io_probe_out.txt does not hold its data ($tag)"
    [[ ! -e "$run_dir/sub/io_probe_ro_dir/x.txt" && ! -e "$run_dir/sub/io_probe_no_dir" ]] ||
        fail "$leg: a refused TryWriteFile created its target ($tag)"
}

expect_same() {
    cmp -s "$1" "$2" || {
        diff "$1" "$2" >&2 || true
        fail "$3"
    }
}

for leg in native-c native-llvm default-c; do
    compile io "$leg" || {
        cat "$WORK_DIR/io.$leg.log" >&2
        fail "$leg refused the TryReadFile/TryWriteFile program"
    }
    run_io "$leg" plain
    expect_same "$WORK_DIR/io.expected" "$WORK_DIR/io.$leg.plain.out" \
        "$leg: TryReadFile/TryWriteFile results differ from the expected rows"
    run_io "$leg" absolute PGY_IO_ALLOW_ABSOLUTE=1
    expect_same "$WORK_DIR/io.absolute.expected" "$WORK_DIR/io.$leg.absolute.out" \
        "$leg: with PGY_IO_ALLOW_ABSOLUTE=1 the absolute path did not resolve and then fail to open"

    compile cfc "$leg" || {
        cat "$WORK_DIR/cfc.$leg.log" >&2
        fail "$leg refused the CharFromCode program"
    }
    "$WORK_DIR/cfc.$leg.exe" </dev/null | tr -d '\r' >"$WORK_DIR/cfc.$leg.out"
    expect_same "$WORK_DIR/cfc.expected" "$WORK_DIR/cfc.$leg.out" \
        "$leg: CharFromCode results differ from the expected rows"

    if compile dup "$leg"; then
        fail "$leg accepted a program that declares its own IoError beside the builtin one"
    fi
    # Native composition names the user's declaration, not the builtin one.
    if [[ "$leg" == native-* ]] &&
        ! grep -Fq "dup.pgy:1: this program declares IoError, which is the builtin error enum" \
            "$WORK_DIR/dup.$leg.log"; then
        cat "$WORK_DIR/dup.$leg.log" >&2
        fail "$leg refused the duplicate IoError without naming the program's declaration"
    fi
done

# The default LLVM route prints the same or refuses with its route diagnostic.
for program in io cfc; do
    if compile "$program" default-llvm; then
        if [[ "$program" == io ]]; then
            run_io default-llvm plain
            expect_same "$WORK_DIR/io.expected" "$WORK_DIR/io.default-llvm.plain.out" \
                "default-llvm: TryReadFile/TryWriteFile results differ"
        else
            "$WORK_DIR/cfc.default-llvm.exe" </dev/null | tr -d '\r' >"$WORK_DIR/cfc.default-llvm.out"
            expect_same "$WORK_DIR/cfc.expected" "$WORK_DIR/cfc.default-llvm.out" \
                "default-llvm: CharFromCode results differ"
        fi
    elif ! grep -Eq 'CODEGEN ERROR: direct MIR' "$WORK_DIR/$program.default-llvm.log"; then
        cat "$WORK_DIR/$program.default-llvm.log" >&2
        fail "default-llvm refused the $program program without its direct-MIR route diagnostic"
    fi
done

echo "[$LABEL] $(wc -l < "$WORK_DIR/def.names" | tr -d ' ') IoError rows equal across runtime, self-host and docs; TryReadFile/TryWriteFile/CharFromCode equal on native C, native LLVM and default C: PASS"

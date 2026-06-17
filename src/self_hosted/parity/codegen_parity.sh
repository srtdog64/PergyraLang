#!/usr/bin/env bash
# Rung-0 parity for the Pergyra-origin C codegen substitute (2026-06-17).
#
# This is the first *hard compiler-core* substitution gate, opened after the
# 2026-06-17 BDFL decision lifted the hard-migration freeze
# (docs/self_hosted/README.md). The criterion is run-output equivalence, not
# byte-equal C: the oracle emits MIR-lowered C with runtime headers, while the
# Pergyra emitter produces standalone C. They are judged equal only by the
# observable stdout of the compiled program.
#
# For each fixture <name>.pgy:
#   1. live oracle: build the fixture through the C backend -> exe, run it,
#      capture stdout. The committed expected/<name>_stdout.txt MUST equal this
#      (live-drift guard, mirroring parser_parity.sh).
#   2. self-host: `pgy --ast <fixture>` -> ast.txt; run the codegen tool on
#      ast.txt -> out.c; gcc out.c -> exe; run it, capture stdout.
#   3. assert self stdout == committed expected (== oracle), tr -d '\r'.
#
# The codegen tool itself is compiled through BOTH the C and LLVM backends.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:codegen] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:codegen] missing compiler binary: $PGY" >&2
    exit 1
fi

CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-parity:codegen] SKIP missing C compiler on PATH: $CC"
    exit 0
fi

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/codegen/fixture"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/codegen/expected"
# Build artifacts live under a repo-relative dir. The Pergyra codegen tool is a
# native binary that resolves its AST-path argument relative to cwd, so it is
# always invoked from ROOT_DIR with a repo-relative path (mirrors
# parser_parity.sh). gcc/run paths use the absolute form.
REL_BUILD=".tmp/self_hosted/codegen"
ABS_BUILD="$ROOT_DIR/$REL_BUILD"

if [[ ! -f "$TOOL_SOURCE" ]]; then
    echo "[self-host-parity:codegen] missing Pergyra tool: $TOOL_SOURCE" >&2
    exit 1
fi

mkdir -p "$ABS_BUILD"

# Fixture base names; each resolves to fixture/<base>.pgy and
# expected/<base>_stdout.txt.
FIXTURES=(
    hello
    two_logs
    concat
    nested_concat
    int_arith
    int_subdiv
    mixed_int_str
    int_neg
    while_sum
    if_else
    nested_ctrl
    func_call
    func_recursive
    str_greet
    str_reassign
    for_sum
    for_continue
    while_break
)

# Re-derive the oracle stdout and assert the committed expected has not drifted.
check_oracle_drift() {
    local base="$1"
    local src="$FIXTURE_DIR/${base}.pgy"
    local expected_file="$EXPECTED_DIR/${base}_stdout.txt"
    local oracle_exe="$ABS_BUILD/${base}_oracle.exe"

    if [[ ! -f "$src" ]]; then
        echo "[self-host-parity:codegen] missing fixture source: $src" >&2
        exit 1
    fi
    if [[ ! -f "$expected_file" ]]; then
        echo "[self-host-parity:codegen] missing expected stdout: $expected_file" >&2
        exit 1
    fi

    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$src")" \
        --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_exe")" >/dev/null 2>&1)

    local oracle_out
    oracle_out="$("$oracle_exe" 2>/dev/null | tr -d '\r')"
    local expected_norm
    expected_norm="$(tr -d '\r' < "$expected_file")"
    if [[ "$oracle_out" != "$expected_norm" ]]; then
        echo "[self-host-parity:codegen] $base: committed expected drifted from C-backend oracle" >&2
        echo "regenerate: pgy <fixture> --backend=c -o oracle && oracle > $expected_file" >&2
        diff <(printf '%s\n' "$expected_norm") <(printf '%s\n' "$oracle_out") | head -20 >&2
        exit 1
    fi
}

compile_tool_backend() {
    local backend="$1"
    local tool_bin="$2"

    echo "[self-host-parity:codegen] compiling codegen tool backend=$backend..."
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$tool_bin")" >/dev/null)
}

run_tool_backend() {
    local backend="$1"
    local tool_bin="$2"

    for base in "${FIXTURES[@]}"; do
        local src="$FIXTURE_DIR/${base}.pgy"
        local expected_file="$EXPECTED_DIR/${base}_stdout.txt"
        local ast_rel="$REL_BUILD/${base}_${backend}_ast.txt"
        local ast_file="$ROOT_DIR/$ast_rel"
        local c_file="$ABS_BUILD/${base}_${backend}.c"
        local self_exe="$ABS_BUILD/${base}_${backend}_self.exe"

        # 1. AST text from the live compiler (written to a repo-relative path).
        (cd "$ROOT_DIR" && "$PGY" --ast \
            "$(pgy_path_for_compiler "$PGY" "$src")" 2>/dev/null \
            | tr -d '\r' > "$ast_rel")
        if [[ ! -s "$ast_file" ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: empty --ast output" >&2
            exit 1
        fi

        # 2. Pergyra codegen tool: AST text -> C. The tool resolves its argument
        #    relative to cwd, so run it from ROOT_DIR with the repo-relative path.
        #    Capture the tool's own exit (not a pipe's) before stripping CRs.
        local tool_rc
        set +e
        (cd "$ROOT_DIR" && "$tool_bin" "$ast_rel" > "$c_file.raw" 2>/dev/null)
        tool_rc="$?"
        set -e
        tr -d '\r' < "$c_file.raw" > "$c_file"
        if [[ "$tool_rc" -ne 0 ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: codegen tool exit=$tool_rc" >&2
            cat "$c_file" >&2
            exit 1
        fi

        # 3. gcc the emitted C and run it.
        if ! "$CC" "$c_file" -o "$self_exe" 2>"$ABS_BUILD/${base}_${backend}_cc.log"; then
            echo "[self-host-parity:codegen] backend=$backend $base: emitted C failed to compile" >&2
            cat "$ABS_BUILD/${base}_${backend}_cc.log" >&2
            echo "--- emitted C ---" >&2
            cat "$c_file" >&2
            exit 1
        fi
        local self_out
        self_out="$("$self_exe" 2>/dev/null | tr -d '\r')"

        # 4. Compare against committed expected (== oracle, guarded above).
        local expected_norm
        expected_norm="$(tr -d '\r' < "$expected_file")"
        if [[ "$self_out" != "$expected_norm" ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: RUN-STDOUT DRIFT vs $expected_file" >&2
            diff <(printf '%s\n' "$expected_norm") <(printf '%s\n' "$self_out") | head -20 >&2
            exit 1
        fi
    done

    echo "[self-host-parity:codegen] backend=$backend run-stdout equal (${#FIXTURES[@]} fixtures)"
}

for base in "${FIXTURES[@]}"; do
    check_oracle_drift "$base"
done

BACKENDS="${PGY_SELFHOST_CODEGEN_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    tool_bin="$ABS_BUILD/tool_${backend}.exe"
    compile_tool_backend "$backend" "$tool_bin"
    run_tool_backend "$backend" "$tool_bin"
done

echo "[self-host-parity:codegen] rung-0..5 parity ok (${#FIXTURES[@]} fixtures; backends=$BACKENDS)"

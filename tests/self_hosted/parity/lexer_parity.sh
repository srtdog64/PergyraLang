#!/usr/bin/env bash
# Rung 1 parity for the minimal Pergyra-origin lexer (2026-05-27).
# Asserts: clean exit, byte-equal stdout vs committed C-lexer fixtures, and
# live-drift guard vs `pgy --tokens` for each source pair.
# See tests/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:lexer] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:lexer] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/lexer/main.pgy"
COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lexer}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
COMPARATOR_BIN="$PERGYRA_TOOL_BUILD_DIR/backend_output_comparator.exe"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/lexer/fixture"
LEXER_FIXTURE_MANIFEST_FILE="$PERGYRA_TOOL_BUILD_DIR/lexer_fixture_manifest.txt"

if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:lexer] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -f "$COMPARATOR_SOURCE" ]]; then
    echo "[self-host-parity:lexer] missing Pergyra comparator: $COMPARATOR_SOURCE" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lexer/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/"

path_relative_to_root() {
    local path="$1"
    printf '%s\n' "${path#"$ROOT_DIR"/}"
}

normalize_text_artifact() {
    local input="$1"
    local output="$2"

    tr -d '\r' < "$input" \
        | awk 'NR > 1 { printf "\n" } { printf "%s", $0 }' >"$output"
}

compile_backend_output_comparator() {
    local compile_log="$PERGYRA_TOOL_BUILD_DIR/backend_output_comparator.compile.log"

    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$COMPARATOR_SOURCE")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$COMPARATOR_BIN")" \
        >"$compile_log" 2>&1); then
        echo "[self-host-parity:lexer] backend output comparator failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

read_lexer_fixture_manifest() {
    local line

    SOURCE_PAIRS=()
    if ! (cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main.exe" --fixture-manifest \
        >"$LEXER_FIXTURE_MANIFEST_FILE" \
        2>"$PERGYRA_TOOL_BUILD_DIR/lexer_fixture_manifest.err"); then
        echo "[self-host-parity:lexer] fixture manifest emission failed" >&2
        cat "$PERGYRA_TOOL_BUILD_DIR/lexer_fixture_manifest.err" >&2
        exit 1
    fi

    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        SOURCE_PAIRS+=("$line")
    done <"$LEXER_FIXTURE_MANIFEST_FILE"

    if [[ "${#SOURCE_PAIRS[@]}" -ne 7 ]]; then
        echo "[self-host-parity:lexer] fixture manifest count drifted: ${#SOURCE_PAIRS[@]} != 7" >&2
        exit 1
    fi
}

compare_lexer_output_with_owner() {
    local backend="$1"
    local label="$2"
    local expected_file="$3"
    local actual_file="$4"
    local actual_projection="$5"
    local expected_norm="$PERGYRA_TOOL_BUILD_DIR/${label}_${backend}_expected.out"
    local cmp_out="$PERGYRA_TOOL_BUILD_DIR/${label}_${backend}_compare.out"
    local cmp_err="$PERGYRA_TOOL_BUILD_DIR/${label}_${backend}_compare.err"
    local expected_rel
    local actual_rel

    normalize_text_artifact "$expected_file" "$expected_norm"

    expected_rel="$(path_relative_to_root "$expected_norm")"
    actual_rel="$(path_relative_to_root "$actual_file")"

    if ! (cd "$ROOT_DIR" && "$COMPARATOR_BIN" "$expected_rel" "$actual_rel" 0 "$actual_projection" \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[self-host-parity:lexer] $label: backend=$backend token artifact drift vs $expected_file" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

echo "[self-host-parity:lexer] compiling lexer..."
C_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/main.compile.log"
LLVM_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/main_llvm.compile.log"
LLVM_LEX_AVAILABLE=1

if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_BUILD_DIR/main.exe")" \
    >"$C_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:lexer] C-compiled lexer failed to build" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")" \
    --backend=llvm -o "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_BUILD_DIR/main_llvm.exe")" \
    >"$LLVM_COMPILE_LOG" 2>&1); then
    if pgy_selfhost_log_reports_no_llvm "$LLVM_COMPILE_LOG"; then
        LLVM_LEX_AVAILABLE=0
        echo "[self-host-parity:lexer] LLVM backend unavailable; checking C-compiled lexer only"
    else
        echo "[self-host-parity:lexer] LLVM-compiled lexer failed to build" >&2
        cat "$LLVM_COMPILE_LOG" >&2
        exit 1
    fi
fi

compile_backend_output_comparator

# Sources to lex + their committed fixtures are emitted by the compiled lexer
# owner as "<source path relative to repo root>:<fixture filename>" rows.
SOURCE_PAIRS=()

ANY_DRIFT_GUARD_RAN="no"

read_lexer_fixture_manifest

for pair in "${SOURCE_PAIRS[@]}"; do
    src="${pair%%:*}"
    fix="${pair##*:}"
    expected_file="$FIXTURE_DIR/$fix"
    label="${fix%.txt}"

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[self-host-parity:lexer] missing source: $src" >&2
        exit 1
    fi
    if [[ ! -f "$expected_file" ]]; then
        echo "[self-host-parity:lexer] missing fixture: $expected_file" >&2
        exit 1
    fi

    c_out="$PERGYRA_TOOL_BUILD_DIR/${label}_c_tokens.out"
    c_err="$PERGYRA_TOOL_BUILD_DIR/${label}_c_tokens.err"
    set +e
    (cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main.exe" "$src" 2>"$c_err" \
        | tr -d '\r' \
        | sed '/^pgy: compiled /d' \
        | awk 'NR > 1 { printf "\n" } { printf "%s", $0 }' >"$c_out")
    P_RC=$?
    set -e
    if [[ "$P_RC" -ne 0 ]]; then
        echo "[self-host-parity:lexer] $src: clean exit-code FAIL (pergyra=$P_RC)" >&2
        cat "$c_out" "$c_err" >&2
        exit 1
    fi

    compare_lexer_output_with_owner "c" "$label" "$expected_file" "$c_out" 2

    if [[ "$LLVM_LEX_AVAILABLE" -eq 1 ]]; then
        llvm_out="$PERGYRA_TOOL_BUILD_DIR/${label}_llvm_tokens.out"
        llvm_err="$PERGYRA_TOOL_BUILD_DIR/${label}_llvm_tokens.err"
        set +e
        (cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main_llvm.exe" "$src" 2>"$llvm_err" \
            | tr -d '\r' \
            | sed '/^pgy: compiled /d' \
            | awk 'NR > 1 { printf "\n" } { printf "%s", $0 }' >"$llvm_out")
        LLVM_LEX_RC=$?
        set -e
        if [[ "$LLVM_LEX_RC" -ne 0 ]]; then
            echo "[self-host-parity:lexer] $src: LLVM-compiled lexer exit-code FAIL (pergyra=$LLVM_LEX_RC)" >&2
            cat "$llvm_out" "$llvm_err" >&2
            exit 1
        fi
        compare_lexer_output_with_owner "llvm" "$label" "$expected_file" "$llvm_out" 2
    fi

    # Live C-lexer drift guard for this source pair.
    live_out="$PERGYRA_TOOL_BUILD_DIR/${label}_live_tokens.out"
    live_err="$PERGYRA_TOOL_BUILD_DIR/${label}_live_tokens.err"
    set +e
    (cd "$ROOT_DIR" && "$PGY" --tokens "$src" 2>"$live_err" \
        | tr -d '\r' \
        | awk 'NR > 1 { printf "\n" } { printf "%s", $0 }' >"$live_out")
    LIVE_RC=$?
    set -e
    if [[ "$LIVE_RC" -eq 0 && -s "$live_out" ]]; then
        compare_lexer_output_with_owner "live-tokens" "$label" "$expected_file" "$live_out" 0
        ANY_DRIFT_GUARD_RAN="yes"
    fi
done

BACKENDS_LABEL="c"
if [[ "$LLVM_LEX_AVAILABLE" -eq 1 ]]; then
    BACKENDS_LABEL="c llvm"
else
    BACKENDS_LABEL="c; llvm skipped"
fi

echo "[self-host-parity:lexer] rung-1 parity ok (${#SOURCE_PAIRS[@]} sources byte-equal; backends=$BACKENDS_LABEL; live-drift=$ANY_DRIFT_GUARD_RAN)"

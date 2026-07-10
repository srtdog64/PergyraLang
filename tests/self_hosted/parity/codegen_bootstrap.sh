#!/usr/bin/env bash
# Self-hosting fixpoint gate for the Pergyra-origin C codegen (2026-06-17).
#
# This proves the codegen tool *self-hosts*: a Pergyra-built copy of the tool,
# run on the tool's own source, reproduces its own source-compilation exactly.
#
#   gen0 = C oracle-built tool      (pgy --backend=c main.pgy)
#   gen1 = gen0(main.pgy AST) -> C  -> gcc -> gen1.exe
#   gen2 = gen1.exe(main.pgy AST) -> C  -> gcc -> gen2.exe   (a Pergyra-built tool)
#   gen3 = gen2.exe(main.pgy AST) -> C
#   FIXPOINT: gen2 == gen3 byte-identical.
#
# (gen1 vs gen2 may differ by a trailing newline only -- gen0 uses the oracle's
# `Log`, gen1+ use the emitted `printf("%s\n", ...)`. From gen2 on, the lineage
# is fully Pergyra-built and must be a stable fixpoint.)
#
# Also checks that the Pergyra-built tool emits byte-identical C to the oracle-
# built tool for a sample of committed fixtures.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix_from_current_path)"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    if [[ -z "${PGY_BIN:-}" ]]; then
        echo "[self-host-bootstrap] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-bootstrap] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-bootstrap" "$PGY"
CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-bootstrap] SKIP missing C compiler on PATH: $CC"
    exit 0
fi

B="$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap"
HARNESS_PATHS_FILE="$B/codegen_bootstrap_paths.txt"
HARNESS_COMPONENTS_FILE="$B/codegen_bootstrap_components.txt"
HARNESS_TOOLS_FILE="$B/codegen_bootstrap_tools.txt"
HARNESS_SAMPLES_FILE="$B/codegen_bootstrap_samples.txt"
HARNESS_MIR_FIXTURES_FILE="$B/codegen_bootstrap_mir_fixtures.txt"
mkdir -p "$B"
COMPARATOR_BIN=""
PARSER_BIN="$B/parser_ast_producer.exe"
TOOL_SOURCE=""
PARSER_SOURCE=""
COMPARATOR_SOURCE=""
CODEGEN_FIXTURE_DIR=""
MIR_LOWER_SOURCE=""
MIR_FIXTURE_DIR=""
FUZZ_SOURCE=""
SAMPLE_SRC=""
BOOTSTRAP_COMPONENT_ROWS=()
BOOTSTRAP_TOOL_ROWS=()
BOOTSTRAP_SAMPLE_ROWS=()
BOOTSTRAP_MIR_FIXTURES=()

pgy_selfhost_read_test_harness_manifest \
    "self-host-bootstrap" \
    "$B" \
    "codegen-bootstrap-paths" \
    "$HARNESS_PATHS_FILE"
pgy_selfhost_read_test_harness_manifest \
    "self-host-bootstrap" \
    "$B" \
    "codegen-bootstrap-components" \
    "$HARNESS_COMPONENTS_FILE"
pgy_selfhost_read_test_harness_manifest \
    "self-host-bootstrap" \
    "$B" \
    "codegen-bootstrap-tools" \
    "$HARNESS_TOOLS_FILE"
pgy_selfhost_read_test_harness_manifest \
    "self-host-bootstrap" \
    "$B" \
    "codegen-bootstrap-samples" \
    "$HARNESS_SAMPLES_FILE"
pgy_selfhost_read_test_harness_manifest \
    "self-host-bootstrap" \
    "$B" \
    "codegen-bootstrap-mir-fixtures" \
    "$HARNESS_MIR_FIXTURES_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 9 ]]; then
    echo "[self-host-bootstrap] TestHarness manifest expected 9 bootstrap paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"
COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"
CODEGEN_FIXTURE_DIR="$ROOT_DIR/${harness_paths[3]}"
MIR_LOWER_SOURCE="$ROOT_DIR/${harness_paths[4]}"
MIR_FIXTURE_DIR="$ROOT_DIR/${harness_paths[5]}"
FUZZ_SOURCE="$ROOT_DIR/${harness_paths[6]}"
SAMPLE_SRC="${harness_paths[7]}"

while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    BOOTSTRAP_COMPONENT_ROWS+=("$line")
done <"$HARNESS_COMPONENTS_FILE"
if [[ "${#BOOTSTRAP_COMPONENT_ROWS[@]}" -ne 3 ]]; then
    echo "[self-host-bootstrap] TestHarness manifest expected 3 bootstrap component rows, got ${#BOOTSTRAP_COMPONENT_ROWS[@]}" >&2
    exit 1
fi

while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    BOOTSTRAP_TOOL_ROWS+=("$line")
done <"$HARNESS_TOOLS_FILE"
if [[ "${#BOOTSTRAP_TOOL_ROWS[@]}" -ne 14 ]]; then
    echo "[self-host-bootstrap] TestHarness manifest expected 14 bootstrap tool rows, got ${#BOOTSTRAP_TOOL_ROWS[@]}" >&2
    exit 1
fi

while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    BOOTSTRAP_SAMPLE_ROWS+=("$line")
done <"$HARNESS_SAMPLES_FILE"
if [[ "${#BOOTSTRAP_SAMPLE_ROWS[@]}" -ne 8 ]]; then
    echo "[self-host-bootstrap] TestHarness manifest expected 8 bootstrap sample rows, got ${#BOOTSTRAP_SAMPLE_ROWS[@]}" >&2
    exit 1
fi

while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    BOOTSTRAP_MIR_FIXTURES+=("$line")
done <"$HARNESS_MIR_FIXTURES_FILE"
if [[ "${#BOOTSTRAP_MIR_FIXTURES[@]}" -ne 3 ]]; then
    echo "[self-host-bootstrap] TestHarness manifest expected 3 bootstrap MIR fixture rows, got ${#BOOTSTRAP_MIR_FIXTURES[@]}" >&2
    exit 1
fi

for path in "$TOOL_SOURCE" "$PARSER_SOURCE" "$COMPARATOR_SOURCE" "$MIR_LOWER_SOURCE" "$FUZZ_SOURCE" "$ROOT_DIR/$SAMPLE_SRC"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-bootstrap] missing TestHarness input: $path" >&2
        exit 1
    fi
done
for path in "$CODEGEN_FIXTURE_DIR" "$MIR_FIXTURE_DIR"; do
    if [[ ! -d "$path" ]]; then
        echo "[self-host-bootstrap] missing TestHarness fixture dir: $path" >&2
        exit 1
    fi
done

run_native_capture() {
    local cwd="$1"
    local out="$2"
    local err="$3"
    local bin="$4"
    shift 4

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            local cwd_bash
            local bin_bash
            local out_bash
            local err_bash
            cwd_bash="$(pgy_path_for_bash_tool "$cwd")"
            bin_bash="$(pgy_path_for_bash_tool "$bin")"
            out_bash="$(pgy_path_for_bash_tool "$out")"
            err_bash="$(pgy_path_for_bash_tool "$err")"
            local old_pwd="$PWD"
            cd "$cwd_bash"
            "$bin_bash" "$@" >"$out_bash" 2>"$err_bash"
            local rc=$?
            cd "$old_pwd"
            case "$rc" in
                126|127)
                    ;;
                *)
                    return "$rc"
                    ;;
            esac
            ;;
    esac

    if pgy_binary_is_runnable_here "$bin"; then
        (cd "$cwd" && "$bin" "$@" >"$out" 2>"$err")
        local direct_rc=$?
        case "$direct_rc" in
            126|127)
                case "$(uname -s 2>/dev/null || echo unknown)" in
                    MINGW*|MSYS*|CYGWIN*) ;;
                    *) return "$direct_rc" ;;
                esac
                ;;
            *)
                return "$direct_rc"
                ;;
        esac
    fi

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) return 127 ;;
    esac
    command -v powershell.exe >/dev/null 2>&1 || return 127

    local cwd_native
    local bin_native
    local out_native
    local err_native
    local args_native=""
    local arg

    cwd_native="$(pgy_path_for_windows_tool "$cwd")"
    bin_native="$(pgy_path_for_windows_tool "$bin")"
    out_native="$(pgy_path_for_windows_tool "$out")"
    err_native="$(pgy_path_for_windows_tool "$err")"
    for arg in "$@"; do
        local escaped_arg="${arg//\"/\\\"}"
        args_native="${args_native} \"${escaped_arg}\""
    done

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; \$enc = New-Object System.Text.UTF8Encoding \$false; \$psi = New-Object System.Diagnostics.ProcessStartInfo; \$psi.FileName = $(pgy_powershell_quote "$bin_native"); \$psi.WorkingDirectory = $(pgy_powershell_quote "$(pgy_path_for_windows_tool "$cwd")"); \$psi.UseShellExecute = \$false; \$psi.RedirectStandardOutput = \$true; \$psi.RedirectStandardError = \$true; \$psi.Arguments = $(pgy_powershell_quote "$args_native"); \$p = [System.Diagnostics.Process]::Start(\$psi); if (\$p -eq \$null) { exit 127 }; \$t1 = \$p.StandardOutput.ReadToEndAsync(); \$t2 = \$p.StandardError.ReadToEndAsync(); [System.Threading.Tasks.Task]::WaitAll(@(\$t1, \$t2)); \$p.WaitForExit(); \$stdout = \$t1.Result; \$stderr = \$t2.Result; [System.IO.File]::WriteAllText($(pgy_powershell_quote "$out_native"), \$stdout, \$enc); [System.IO.File]::WriteAllText($(pgy_powershell_quote "$err_native"), \$stderr, \$enc); exit \$p.ExitCode"
}

run_native_to_file() {
    local label="$1"
    local bin="$2"
    local out="$3"
    shift 3
    local raw="$B/${label}.raw"
    local err="$B/${label}.err"

    if ! run_native_capture "$ROOT_DIR" "$raw" "$err" "$bin" "$@"; then
        cat "$err" >&2 || true
        return 1
    fi
    tr -d '\r' < "$raw" > "$out"
}

run_native_stdout() {
    local label="$1"
    local bin="$2"
    shift 2
    local out="$B/${label}.out"

    run_native_to_file "$label" "$bin" "$out" "$@"
    tr -d '\r' < "$out"
}

emit() {  # emit <tool-exe> <out.c>
    run_native_to_file "emit_$(basename "$2")" "$1" "$2" "$AST_REL"
}

compile_c_artifact_with_bounded_log() {
    local label="$1"
    local source="$2"
    local output="$3"
    local log="$B/${label}_cc.log"
    local tmp="$log.tmp"
    local limit="${PGY_SELFHOST_CC_LOG_LIMIT_BYTES:-65536}"
    local rc

    set +e
    "$CC" "$source" -o "$output" 2>&1 | awk -v limit="$limit" '
        BEGIN {
            written = 0
            truncated = 0
        }
        {
            line = $0 "\n"
            if (written < limit) {
                remaining = limit - written
                if (length(line) > remaining) {
                    printf "%s", substr(line, 1, remaining)
                    written = limit
                    truncated = 1
                } else {
                    printf "%s", line
                    written += length(line)
                }
            } else {
                truncated = 1
            }
        }
        END {
            if (truncated) {
                printf "\n[self-host-bootstrap] compiler log truncated at %s bytes\n", limit
            }
        }
    ' >"$tmp"
    rc=${PIPESTATUS[0]}
    set -e
    mv "$tmp" "$log"
    return "$rc"
}

compile_artifact_comparator() {
    pgy_selfhost_compile_backend_output_comparator "self-host-bootstrap" "$B" "$COMPARATOR_SOURCE"
    COMPARATOR_BIN="$(pgy_selfhost_backend_output_comparator_bin "$B")"
}

compile_parser_ast_producer() {
    local compile_log="$B/parser_ast_producer.compile.log"

    echo "[self-host-bootstrap] building self parser AST producer..."
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$PARSER_SOURCE")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PARSER_BIN")" \
        >"$compile_log" 2>&1); then
        echo "[self-host-bootstrap] parser AST producer failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

emit_self_parser_ast() {
    local source_abs="$1"
    local out_rel="$2"
    local source_rel
    local out_abs
    local raw
    local err

    source_rel="$(pgy_selfhost_path_relative_to_root "$source_abs")"
    out_abs="$ROOT_DIR/$out_rel"
    raw="${out_abs}.raw"
    err="${out_abs}.err"
    if ! run_native_capture "$ROOT_DIR" "$raw" "$err" "$PARSER_BIN" "$source_rel"; then
        echo "[self-host-bootstrap] self parser AST failed for $source_rel" >&2
        cat "$err" >&2 || true
        exit 1
    fi
    tr -d '\r' < "$raw" > "$out_abs"
}

compare_artifact_with_owner() {
    local label="$1"
    local expected="$2"
    local actual="$3"
    local artifact_kind="$4"
    local cmp_out="$B/${label}.compare.out"
    local cmp_err="$B/${label}.compare.err"
    local expected_rel
    local actual_rel

    if [[ -z "$COMPARATOR_BIN" ]]; then
        echo "[self-host-bootstrap] comparator was not built before $label" >&2
        exit 1
    fi

    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual")"
    if ! run_native_capture "$ROOT_DIR" "$cmp_out" "$cmp_err" "$COMPARATOR_BIN" \
        "$expected_rel" "$actual_rel" 2 2 "$artifact_kind"; then
        echo "[self-host-bootstrap] $label: $artifact_kind artifact drift" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

# gen0: oracle-built tool
echo "[self-host-bootstrap] building oracle tool (gen0)..."
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/gen0.exe")" >/dev/null)
compile_parser_ast_producer

# main.pgy's own AST (repo-relative path so the native tool resolves it from cwd)
AST_REL=".tmp/self_hosted/codegen/bootstrap/main_ast.txt"
emit_self_parser_ast "$TOOL_SOURCE" "$AST_REL"

emit "$B/gen0.exe" "$B/gen1.c"
if grep -q '^CODEGEN ERROR' "$B/gen1.c"; then
    echo "[self-host-bootstrap] tool rejects its own source (out of subset):" >&2
    grep '^CODEGEN ERROR' "$B/gen1.c" | head -3 >&2
    exit 1
fi
compile_c_artifact_with_bounded_log "gen1" "$B/gen1.c" "$B/gen1.exe" || {
    echo "[self-host-bootstrap] gen1 C failed to compile" >&2; cat "$B/gen1_cc.log" >&2; exit 1; }

emit "$B/gen1.exe" "$B/gen2.c"
compile_c_artifact_with_bounded_log "gen2" "$B/gen2.c" "$B/gen2.exe" || {
    echo "[self-host-bootstrap] gen2 C failed to compile" >&2; cat "$B/gen2_cc.log" >&2; exit 1; }

emit "$B/gen2.exe" "$B/gen3.c"

compile_artifact_comparator
compare_artifact_with_owner "fixpoint_gen2_gen3" "$B/gen2.c" "$B/gen3.c" "emitted_c"
echo "[self-host-bootstrap] fixpoint ok: gen2 == gen3 ($(wc -l < "$B/gen2.c") lines)"

# Sample: the Pergyra-built tool must emit identical C to the oracle-built tool.
for base in "${BOOTSTRAP_SAMPLE_ROWS[@]}"; do
    fa="$B/${base}_ast.txt"
    emit_self_parser_ast "$CODEGEN_FIXTURE_DIR/${base}.pgy" "${fa#$ROOT_DIR/}"
    o="$(run_native_stdout "sample_${base}_oracle" "$B/gen0.exe" "${fa#$ROOT_DIR/}")"
    g="$(run_native_stdout "sample_${base}_self" "$B/gen2.exe" "${fa#$ROOT_DIR/}")"
    if [[ "$o" != "$g" ]]; then
        echo "[self-host-bootstrap] $base: Pergyra-built tool emit differs from oracle-built" >&2
        exit 1
    fi
done
SAMPLE_SUMMARY="$(IFS=', '; printf '%s' "${BOOTSTRAP_SAMPLE_ROWS[*]}")"
echo "[self-host-bootstrap] Pergyra-built tool emits identical C to oracle-built on $SAMPLE_SUMMARY"

# Breadth: the Pergyra-built codegen (gen2) also compiles the OTHER self-host
# components. Build each via gen2, gcc it, and check it produces the same output
# as the oracle-built component on a sample source.
for row in "${BOOTSTRAP_COMPONENT_ROWS[@]}"; do
    comp="${row%%|*}"
    rel="${row#*|}"
    if [[ "$comp" == "$row" || -z "$comp" || -z "$rel" ]]; then
        echo "[self-host-bootstrap] malformed component row: $row" >&2
        exit 1
    fi
    csrc="$ROOT_DIR/$rel"
    [[ -f "$csrc" ]] || continue
    crel=".tmp/self_hosted/codegen/bootstrap/${comp}_ast.txt"
    emit_self_parser_ast "$csrc" "$crel"
    run_native_to_file "${comp}_via_codegen" "$B/gen2.exe" "$B/${comp}_via_codegen.c" "$crel"
    if grep -q '^CODEGEN ERROR' "$B/${comp}_via_codegen.c"; then
        echo "[self-host-bootstrap] $comp: out of codegen subset (skip breadth check)"
        continue
    fi
    if ! compile_c_artifact_with_bounded_log "$comp" "$B/${comp}_via_codegen.c" "$B/${comp}_via_codegen.exe"; then
        echo "[self-host-bootstrap] $comp: codegen-emitted C failed to compile" >&2
        cat "$B/${comp}_cc.log" >&2
        exit 1
    fi
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$csrc")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/${comp}_oracle.exe")" >/dev/null 2>&1)
    via="$(run_native_stdout "${comp}_via_run" "$B/${comp}_via_codegen.exe" "$SAMPLE_SRC")"
    orc="$(run_native_stdout "${comp}_oracle_run" "$B/${comp}_oracle.exe" "$SAMPLE_SRC")"
    if [[ "$via" != "$orc" ]]; then
        echo "[self-host-bootstrap] $comp: codegen-built output differs from oracle-built on $SAMPLE_SRC" >&2
        exit 1
    fi
    echo "[self-host-bootstrap] codegen compiles $comp -> matches oracle-built on $SAMPLE_SRC"
done

# Wider breadth: audit tools (including namespace-imported ones) that read fixed
# files and take no args. The gen2-built binary must match the oracle-built.
TOOL_NAMES=()
for row in "${BOOTSTRAP_TOOL_ROWS[@]}"; do
    name="${row%%|*}"
    rel="${row#*|}"
    if [[ "$name" == "$row" || -z "$name" || -z "$rel" ]]; then
        echo "[self-host-bootstrap] malformed tool row: $row" >&2
        exit 1
    fi
    TOOL_NAMES+=("$name")
    tsrc="$ROOT_DIR/$rel"
    [[ -f "$tsrc" ]] || continue
    trel=".tmp/self_hosted/codegen/bootstrap/tool_${name}_ast.txt"
    emit_self_parser_ast "$tsrc" "$trel"
    run_native_to_file "tool_${name}_emit" "$B/gen2.exe" "$B/tool_${name}.c" "$trel"
    if grep -q '^CODEGEN ERROR' "$B/tool_${name}.c"; then
        echo "[self-host-bootstrap] tool $name out of codegen subset (skip)"
        continue
    fi
    if ! compile_c_artifact_with_bounded_log "tool_${name}" "$B/tool_${name}.c" "$B/tool_${name}_self.exe"; then
        echo "[self-host-bootstrap] tool $name: codegen-emitted C failed to compile" >&2
        cat "$B/tool_${name}_cc.log" >&2; exit 1
    fi
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$tsrc")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/tool_${name}_oracle.exe")" >/dev/null 2>&1)
    set +e
    via="$(run_native_stdout "tool_${name}_self_run" "$B/tool_${name}_self.exe")"
    via_rc=$?
    orc="$(run_native_stdout "tool_${name}_oracle_run" "$B/tool_${name}_oracle.exe")"
    orc_rc=$?
    set -e
    if [[ "$via_rc" -ne "$orc_rc" ]]; then
        echo "[self-host-bootstrap] tool $name: codegen-built exit differs from oracle-built (self=$via_rc oracle=$orc_rc)" >&2
        exit 1
    fi
    if [[ "$via" != "$orc" ]]; then
        echo "[self-host-bootstrap] tool $name: codegen-built output differs from oracle-built" >&2
        exit 1
    fi
    echo "[self-host-bootstrap] codegen compiles tool $name -> matches oracle-built"
done

# MIR-lower breadth: gen2 (the Pergyra-built codegen) must build the MIR JSON
# lowering tool itself. Compare several fact surfaces against the C oracle-built
# mir_lower binary so this is a real compiler-stage substitution check, not just
# a successful C compile.
if [[ -f "$MIR_LOWER_SOURCE" ]]; then
    mir_ast_rel=".tmp/self_hosted/codegen/bootstrap/mir_lower_ast.txt"
    emit_self_parser_ast "$MIR_LOWER_SOURCE" "$mir_ast_rel"
    run_native_to_file "mir_lower_emit" "$B/gen2.exe" "$B/mir_lower_via_codegen.c" "$mir_ast_rel"
    if grep -q '^CODEGEN ERROR' "$B/mir_lower_via_codegen.c"; then
        echo "[self-host-bootstrap] mir_lower out of codegen subset" >&2
        grep '^CODEGEN ERROR' "$B/mir_lower_via_codegen.c" | head -3 >&2
        exit 1
    fi
    if ! compile_c_artifact_with_bounded_log "mir_lower" "$B/mir_lower_via_codegen.c" "$B/mir_lower_self.exe"; then
        echo "[self-host-bootstrap] mir_lower: codegen-emitted C failed to compile" >&2
        cat "$B/mir_lower_cc.log" >&2
        exit 1
    fi
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SOURCE")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/mir_lower_oracle.exe")" >/dev/null 2>&1)
    for mir_base in "${BOOTSTRAP_MIR_FIXTURES[@]}"; do
        mir_json_rel=".tmp/self_hosted/codegen/bootstrap/mir_${mir_base}.json"
        (cd "$ROOT_DIR" && "$PGY" --mir-json \
            "$(pgy_path_for_compiler "$PGY" "$MIR_FIXTURE_DIR/${mir_base}.pgy")" \
            2>/dev/null | tr -d '\r' > "$mir_json_rel")
        mir_via="$(run_native_stdout "mir_${mir_base}_self_run" "$B/mir_lower_self.exe" "$mir_json_rel")"
        mir_orc="$(run_native_stdout "mir_${mir_base}_oracle_run" "$B/mir_lower_oracle.exe" "$mir_json_rel")"
        if [[ "$mir_via" != "$mir_orc" ]]; then
            echo "[self-host-bootstrap] mir_lower: codegen-built output differs from oracle-built on $mir_base" >&2
            exit 1
        fi
    done
    MIR_SUMMARY="$(IFS=', '; printf '%s' "${BOOTSTRAP_MIR_FIXTURES[*]}")"
    echo "[self-host-bootstrap] codegen compiles mir_lower -> matches oracle-built on $MIR_SUMMARY"
fi

# Fuzz-generator breadth: this is not an argless audit tool; it writes a
# deterministic corpus from argv. A Pergyra-built codegen (gen2) must compile it
# into a binary that emits the same stdout and generated files as the C oracle.
if [[ -f "$FUZZ_SOURCE" ]]; then
    fuzz_ast_rel=".tmp/self_hosted/codegen/bootstrap/fuzz_generator_ast.txt"
    emit_self_parser_ast "$FUZZ_SOURCE" "$fuzz_ast_rel"
    run_native_to_file "fuzz_generator_emit" "$B/gen2.exe" "$B/fuzz_generator_via_codegen.c" "$fuzz_ast_rel"
    if grep -q '^CODEGEN ERROR' "$B/fuzz_generator_via_codegen.c"; then
        echo "[self-host-bootstrap] fuzz generator out of codegen subset" >&2
        grep '^CODEGEN ERROR' "$B/fuzz_generator_via_codegen.c" | head -3 >&2
        exit 1
    fi
    if ! compile_c_artifact_with_bounded_log "fuzz_generator" "$B/fuzz_generator_via_codegen.c" "$B/fuzz_generator_self.exe"; then
        echo "[self-host-bootstrap] fuzz generator: codegen-emitted C failed to compile" >&2
        cat "$B/fuzz_generator_cc.log" >&2
        exit 1
    fi
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$FUZZ_SOURCE")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/fuzz_generator_oracle.exe")" >/dev/null 2>&1)
    rm -rf "$B/fuzz_codegen_corpus" "$B/fuzz_oracle_corpus"
    mkdir -p "$B/fuzz_codegen_corpus" "$B/fuzz_oracle_corpus"
    fuzz_via="$(run_native_stdout "fuzz_generator_self_run" "$B/fuzz_generator_self.exe" 1001 8 ".tmp/self_hosted/codegen/bootstrap/fuzz_codegen_corpus")"
    fuzz_orc="$(run_native_stdout "fuzz_generator_oracle_run" "$B/fuzz_generator_oracle.exe" 1001 8 ".tmp/self_hosted/codegen/bootstrap/fuzz_oracle_corpus")"
    if [[ "$fuzz_via" != "$fuzz_orc" ]]; then
        echo "[self-host-bootstrap] fuzz generator: codegen-built stdout differs from oracle-built" >&2
        exit 1
    fi
    compare_artifact_with_owner "fuzz_generator_manifest" \
        "$B/fuzz_codegen_corpus/manifest.jsonl" \
        "$B/fuzz_oracle_corpus/manifest.jsonl" \
        "emitted_self_hosted"
    for fuzz_i in 0 1 2 3 4 5 6 7; do
        compare_artifact_with_owner "fuzz_generator_f${fuzz_i}" \
            "$B/fuzz_codegen_corpus/f${fuzz_i}.pgy" \
            "$B/fuzz_oracle_corpus/f${fuzz_i}.pgy" \
            "emitted_self_hosted"
    done
    echo "[self-host-bootstrap] codegen compiles fuzz backend parity generator -> matches oracle-built corpus"
fi

TOOL_SUMMARY="$(IFS=', '; printf '%s' "${TOOL_NAMES[*]}")"
echo "[self-host-bootstrap] SELF-HOSTING OK (codegen self-hosts + builds lexer/parser/semantic/mir_lower + $TOOL_SUMMARY + fuzz backend parity generator)"

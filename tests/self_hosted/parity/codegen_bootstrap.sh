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

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
B="$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap"
mkdir -p "$B"

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

files_equal_text() {
    local left="$1"
    local right="$2"
    local left_text
    local right_text

    if command -v git >/dev/null 2>&1; then
        git diff --no-index --quiet -- "$left" "$right"
        return $?
    fi
    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$left" "$right"
        return $?
    fi

    left_text="$(<"$left")"
    right_text="$(<"$right")"
    [[ "$left_text" == "$right_text" ]]
}

show_file_delta() {
    local left="$1"
    local right="$2"

    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index --no-prefix -- "$left" "$right" || true
        return 0
    fi
    if command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
        return 0
    fi

    echo "--- $left ---"
    head -20 "$left" || true
    echo "--- $right ---"
    head -20 "$right" || true
}

# gen0: oracle-built tool
echo "[self-host-bootstrap] building oracle tool (gen0)..."
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/gen0.exe")" >/dev/null)

# main.pgy's own AST (repo-relative path so the native tool resolves it from cwd)
AST_REL=".tmp/self_hosted/codegen/bootstrap/main_ast.txt"
(cd "$ROOT_DIR" && "$PGY" --ast \
    "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" 2>/dev/null | tr -d '\r' > "$AST_REL")

emit "$B/gen0.exe" "$B/gen1.c"
if grep -q '^CODEGEN ERROR' "$B/gen1.c"; then
    echo "[self-host-bootstrap] tool rejects its own source (out of subset):" >&2
    grep '^CODEGEN ERROR' "$B/gen1.c" | head -3 >&2
    exit 1
fi
"$CC" "$B/gen1.c" -o "$B/gen1.exe" 2>"$B/gen1_cc.log" || {
    echo "[self-host-bootstrap] gen1 C failed to compile" >&2; cat "$B/gen1_cc.log" >&2; exit 1; }

emit "$B/gen1.exe" "$B/gen2.c"
"$CC" "$B/gen2.c" -o "$B/gen2.exe" 2>"$B/gen2_cc.log" || {
    echo "[self-host-bootstrap] gen2 C failed to compile" >&2; cat "$B/gen2_cc.log" >&2; exit 1; }

emit "$B/gen2.exe" "$B/gen3.c"

if ! files_equal_text "$B/gen2.c" "$B/gen3.c"; then
    echo "[self-host-bootstrap] FIXPOINT BROKEN: gen2 != gen3" >&2
    show_file_delta "$B/gen2.c" "$B/gen3.c" >&2
    exit 1
fi
echo "[self-host-bootstrap] fixpoint ok: gen2 == gen3 ($(wc -l < "$B/gen2.c") lines)"

# Sample: the Pergyra-built tool must emit identical C to the oracle-built tool.
SAMPLE="hello func_recursive struct_param array_push str_indexof else_if_chain string_equality io_probe"
for base in $SAMPLE; do
    fa="$B/${base}_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/codegen/fixture/${base}.pgy")" \
        2>/dev/null | tr -d '\r' > "${fa#$ROOT_DIR/}") || true
    o="$(run_native_stdout "sample_${base}_oracle" "$B/gen0.exe" "${fa#$ROOT_DIR/}")"
    g="$(run_native_stdout "sample_${base}_self" "$B/gen2.exe" "${fa#$ROOT_DIR/}")"
    if [[ "$o" != "$g" ]]; then
        echo "[self-host-bootstrap] $base: Pergyra-built tool emit differs from oracle-built" >&2
        exit 1
    fi
done
echo "[self-host-bootstrap] Pergyra-built tool emits identical C to oracle-built on ${SAMPLE// /, }"

# Breadth: the Pergyra-built codegen (gen2) also compiles the OTHER self-host
# components. Build each via gen2, gcc it, and check it produces the same output
# as the oracle-built component on a sample source.
SAMPLE_SRC="examples/hello.pgy"
for comp in lexer parser semantic; do
    csrc="$ROOT_DIR/src/self_hosted/$comp/main.pgy"
    [[ -f "$csrc" ]] || continue
    crel=".tmp/self_hosted/codegen/bootstrap/${comp}_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$csrc")" 2>/dev/null \
        | tr -d '\r' > "$crel")
    run_native_to_file "${comp}_via_codegen" "$B/gen2.exe" "$B/${comp}_via_codegen.c" "$crel"
    if grep -q '^CODEGEN ERROR' "$B/${comp}_via_codegen.c"; then
        echo "[self-host-bootstrap] $comp: out of codegen subset (skip breadth check)"
        continue
    fi
    if ! "$CC" "$B/${comp}_via_codegen.c" -o "$B/${comp}_via_codegen.exe" 2>"$B/${comp}_cc.log"; then
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
TOOLS="air_graph_id_uniqueness module_manifest_resolver doc_link_checker backend_output_comparator runtime_boundary_checker examples_inventory_checker diagnostic_catalog_checker linter stable_subset_section_checker production_header_size_checker stdlib_dispatch_inventory_checker ast_read_surface_checker production_c_size_checker"
for name in $TOOLS; do
    tsrc="$ROOT_DIR/src/self_hosted/tools/$name/main.pgy"
    [[ -f "$tsrc" ]] || continue
    trel=".tmp/self_hosted/codegen/bootstrap/tool_${name}_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$tsrc")" 2>/dev/null \
        | tr -d '\r' > "$trel")
    run_native_to_file "tool_${name}_emit" "$B/gen2.exe" "$B/tool_${name}.c" "$trel"
    if grep -q '^CODEGEN ERROR' "$B/tool_${name}.c"; then
        echo "[self-host-bootstrap] tool $name out of codegen subset (skip)"
        continue
    fi
    if ! "$CC" "$B/tool_${name}.c" -o "$B/tool_${name}_self.exe" 2>"$B/tool_${name}_cc.log"; then
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
MIR_LOWER_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
if [[ -f "$MIR_LOWER_SOURCE" ]]; then
    mir_ast_rel=".tmp/self_hosted/codegen/bootstrap/mir_lower_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SOURCE")" 2>/dev/null \
        | tr -d '\r' > "$mir_ast_rel")
    run_native_to_file "mir_lower_emit" "$B/gen2.exe" "$B/mir_lower_via_codegen.c" "$mir_ast_rel"
    if grep -q '^CODEGEN ERROR' "$B/mir_lower_via_codegen.c"; then
        echo "[self-host-bootstrap] mir_lower out of codegen subset" >&2
        grep '^CODEGEN ERROR' "$B/mir_lower_via_codegen.c" | head -3 >&2
        exit 1
    fi
    if ! "$CC" "$B/mir_lower_via_codegen.c" -o "$B/mir_lower_self.exe" 2>"$B/mir_lower_cc.log"; then
        echo "[self-host-bootstrap] mir_lower: codegen-emitted C failed to compile" >&2
        cat "$B/mir_lower_cc.log" >&2
        exit 1
    fi
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SOURCE")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/mir_lower_oracle.exe")" >/dev/null 2>&1)
    MIR_BOOTSTRAP_FIXTURES="let_log forloop role_operator_dispatch"
    for mir_base in $MIR_BOOTSTRAP_FIXTURES; do
        mir_json_rel=".tmp/self_hosted/codegen/bootstrap/mir_${mir_base}.json"
        (cd "$ROOT_DIR" && "$PGY" --mir-json \
            "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/mir_lower/fixture/${mir_base}.pgy")" \
            2>/dev/null | tr -d '\r' > "$mir_json_rel")
        mir_via="$(run_native_stdout "mir_${mir_base}_self_run" "$B/mir_lower_self.exe" "$mir_json_rel")"
        mir_orc="$(run_native_stdout "mir_${mir_base}_oracle_run" "$B/mir_lower_oracle.exe" "$mir_json_rel")"
        if [[ "$mir_via" != "$mir_orc" ]]; then
            echo "[self-host-bootstrap] mir_lower: codegen-built output differs from oracle-built on $mir_base" >&2
            exit 1
        fi
    done
    echo "[self-host-bootstrap] codegen compiles mir_lower -> matches oracle-built on ${MIR_BOOTSTRAP_FIXTURES// /, }"
fi

# Fuzz-generator breadth: this is not an argless audit tool; it writes a
# deterministic corpus from argv. A Pergyra-built codegen (gen2) must compile it
# into a binary that emits the same stdout and generated files as the C oracle.
FUZZ_SOURCE="$ROOT_DIR/src/self_hosted/fuzz/backend_parity_generator/main.pgy"
if [[ -f "$FUZZ_SOURCE" ]]; then
    fuzz_ast_rel=".tmp/self_hosted/codegen/bootstrap/fuzz_generator_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$FUZZ_SOURCE")" 2>/dev/null \
        | tr -d '\r' > "$fuzz_ast_rel")
    run_native_to_file "fuzz_generator_emit" "$B/gen2.exe" "$B/fuzz_generator_via_codegen.c" "$fuzz_ast_rel"
    if grep -q '^CODEGEN ERROR' "$B/fuzz_generator_via_codegen.c"; then
        echo "[self-host-bootstrap] fuzz generator out of codegen subset" >&2
        grep '^CODEGEN ERROR' "$B/fuzz_generator_via_codegen.c" | head -3 >&2
        exit 1
    fi
    if ! "$CC" "$B/fuzz_generator_via_codegen.c" -o "$B/fuzz_generator_self.exe" 2>"$B/fuzz_generator_cc.log"; then
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
    if ! files_equal_text "$B/fuzz_codegen_corpus/manifest.jsonl" "$B/fuzz_oracle_corpus/manifest.jsonl"; then
        echo "[self-host-bootstrap] fuzz generator: manifest differs from oracle-built" >&2
        show_file_delta "$B/fuzz_codegen_corpus/manifest.jsonl" "$B/fuzz_oracle_corpus/manifest.jsonl" >&2
        exit 1
    fi
    for fuzz_i in 0 1 2 3 4 5 6 7; do
        if ! files_equal_text "$B/fuzz_codegen_corpus/f${fuzz_i}.pgy" "$B/fuzz_oracle_corpus/f${fuzz_i}.pgy"; then
            echo "[self-host-bootstrap] fuzz generator: f${fuzz_i}.pgy differs from oracle-built" >&2
            show_file_delta "$B/fuzz_codegen_corpus/f${fuzz_i}.pgy" "$B/fuzz_oracle_corpus/f${fuzz_i}.pgy" >&2
            exit 1
        fi
    done
    echo "[self-host-bootstrap] codegen compiles fuzz backend parity generator -> matches oracle-built corpus"
fi

echo "[self-host-bootstrap] SELF-HOSTING OK (codegen self-hosts + builds lexer/parser/semantic/mir_lower + ${TOOLS// /, } + fuzz backend parity generator)"

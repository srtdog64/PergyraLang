#!/usr/bin/env bash
# Self-hosting fixpoint gate for the Pergyra-origin C codegen (2026-06-17).
#
# This proves the codegen tool *self-hosts*: a Pergyra-built copy of the tool,
# run on the tool's own source, reproduces its own source-compilation exactly.
#
#   gen0 = oracle-built tool        (pgy --backend=c main.pgy)
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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

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
CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-bootstrap] SKIP missing C compiler on PATH: $CC"
    exit 0
fi

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
B="$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap"
mkdir -p "$B"

emit() {  # emit <tool-exe> <out.c>
    (cd "$ROOT_DIR" && "$1" "$AST_REL" 2>/dev/null | tr -d '\r' > "$2")
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

if ! diff -q "$B/gen2.c" "$B/gen3.c" >/dev/null; then
    echo "[self-host-bootstrap] FIXPOINT BROKEN: gen2 != gen3" >&2
    diff "$B/gen2.c" "$B/gen3.c" | head -20 >&2
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
    o="$(cd "$ROOT_DIR" && "$B/gen0.exe" "${fa#$ROOT_DIR/}" 2>/dev/null | tr -d '\r')"
    g="$(cd "$ROOT_DIR" && "$B/gen2.exe" "${fa#$ROOT_DIR/}" 2>/dev/null | tr -d '\r')"
    if [[ "$o" != "$g" ]]; then
        echo "[self-host-bootstrap] $base: Pergyra-built tool emit differs from oracle-built" >&2
        exit 1
    fi
done
echo "[self-host-bootstrap] Pergyra-built tool emits identical C to oracle-built on ${SAMPLE// /, }"

# Breadth: the codegen also compiles the OTHER self-host components (lexer,
# parser). Build each via the codegen, gcc it, and check it produces the same
# output as the oracle-built component on a sample source.
SAMPLE_SRC="examples/hello.pgy"
for comp in lexer parser; do
    csrc="$ROOT_DIR/src/self_hosted/$comp/main.pgy"
    [[ -f "$csrc" ]] || continue
    crel=".tmp/self_hosted/codegen/bootstrap/${comp}_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$csrc")" 2>/dev/null \
        | tr -d '\r' > "$crel")
    (cd "$ROOT_DIR" && "$B/gen0.exe" "$crel" 2>/dev/null | tr -d '\r' > "$B/${comp}_via_codegen.c")
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
    via="$(cd "$ROOT_DIR" && "$B/${comp}_via_codegen.exe" "$SAMPLE_SRC" 2>/dev/null | tr -d '\r')"
    orc="$(cd "$ROOT_DIR" && "$B/${comp}_oracle.exe" "$SAMPLE_SRC" 2>/dev/null | tr -d '\r')"
    if [[ "$via" != "$orc" ]]; then
        echo "[self-host-bootstrap] $comp: codegen-built output differs from oracle-built on $SAMPLE_SRC" >&2
        exit 1
    fi
    echo "[self-host-bootstrap] codegen compiles $comp -> matches oracle-built on $SAMPLE_SRC"
done

# Wider breadth: audit tools (including namespace-imported ones) that read fixed
# files and take no args. The codegen-built binary must match the oracle-built.
TOOLS="air_graph_id_uniqueness module_manifest_resolver doc_link_checker backend_output_comparator runtime_boundary_checker examples_inventory_checker diagnostic_catalog_checker linter stable_subset_section_checker production_header_size_checker stdlib_dispatch_inventory_checker ast_read_surface_checker production_c_size_checker"
for name in $TOOLS; do
    tsrc="$ROOT_DIR/src/self_hosted/tools/$name/main.pgy"
    [[ -f "$tsrc" ]] || continue
    trel=".tmp/self_hosted/codegen/bootstrap/tool_${name}_ast.txt"
    (cd "$ROOT_DIR" && "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$tsrc")" 2>/dev/null \
        | tr -d '\r' > "$trel")
    (cd "$ROOT_DIR" && "$B/gen0.exe" "$trel" 2>/dev/null | tr -d '\r' > "$B/tool_${name}.c")
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
    via="$(cd "$ROOT_DIR" && "$B/tool_${name}_self.exe" 2>/dev/null | tr -d '\r')"
    orc="$(cd "$ROOT_DIR" && "$B/tool_${name}_oracle.exe" 2>/dev/null | tr -d '\r')"
    if [[ "$via" != "$orc" ]]; then
        echo "[self-host-bootstrap] tool $name: codegen-built output differs from oracle-built" >&2
        exit 1
    fi
    echo "[self-host-bootstrap] codegen compiles tool $name -> matches oracle-built"
done

echo "[self-host-bootstrap] SELF-HOSTING OK (codegen self-hosts + builds lexer/parser + ${TOOLS// /, })"

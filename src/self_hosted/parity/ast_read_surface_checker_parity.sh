#!/usr/bin/env bash
# Rung 2 parity for the AST read surface checker (2026-06-15).
#
# The shell smoke remains the coverage oracle: it verifies that the shared
# manifest accounts for every file under each measured scope. The Pergyra tool
# proves the same literal counts and ratchet verdict from the same manifest.

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
        echo "[self-host-parity:ast-read-surface] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:ast-read-surface] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/ast_read_surface_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/ast_read_surface_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/ast_read_surface_checker/expected/clean.json"
MANIFEST_FILE="$ROOT_DIR/tests/ast_read_surface_manifest.txt"
MANIFEST_REL="tests/ast_read_surface_manifest.txt"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$MANIFEST_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:ast-read-surface] missing input: $path" >&2
        exit 1
    fi
done

"$ROOT_DIR/tests/ast_read_surface_smoke.sh" >/dev/null

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

LIB_BUILD_DIR="$ROOT_DIR/.tmp/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:ast-read-surface] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.ast-read-surface.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] schema header missing" >&2
    exit 1
fi

SHELL_STATS="$(cd "$ROOT_DIR" && awk -F'|' '
    NF == 5 {
        cmd = "grep -F -o \"" $2 "\" \"" $5 "\" 2>/dev/null | wc -l"
        cmd | getline n
        close(cmd)
        gsub(/ /, "", n)
        if ($1 == "enum")
            enum_reads += n
        else if ($1 == "source_ast_codegen")
            codegen_reads += n
        else if ($1 == "source_ast_compiler")
            compiler_reads += n
        else if ($1 == "source_decl_codegen")
            source_decl_codegen_reads += n
        else if ($1 == "source_decl_compiler")
            source_decl_compiler_reads += n
        else if ($1 == "routine_source_decl_codegen")
            routine_source_decl_codegen_reads += n
    }
    END { printf "%d %d %d %d %d %d\n", enum_reads, codegen_reads, compiler_reads, source_decl_codegen_reads, source_decl_compiler_reads, routine_source_decl_codegen_reads }
' "$MANIFEST_REL")"
read -r SHELL_ENUM SHELL_CODEGEN SHELL_COMPILER SHELL_SOURCE_DECL_CODEGEN SHELL_SOURCE_DECL_COMPILER SHELL_ROUTINE_SOURCE_DECL_CODEGEN <<<"$SHELL_STATS"

if ! grep -Fq "\"enum\":${SHELL_ENUM}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] enum parity FAIL (shell=${SHELL_ENUM})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"source_ast_codegen\":${SHELL_CODEGEN}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] source_ast_codegen parity FAIL (shell=${SHELL_CODEGEN})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"source_ast_compiler\":${SHELL_COMPILER}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] source_ast_compiler parity FAIL (shell=${SHELL_COMPILER})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"source_decl_codegen\":${SHELL_SOURCE_DECL_CODEGEN}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] source_decl_codegen parity FAIL (shell=${SHELL_SOURCE_DECL_CODEGEN})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"source_decl_compiler\":${SHELL_SOURCE_DECL_COMPILER}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] source_decl_compiler parity FAIL (shell=${SHELL_SOURCE_DECL_COMPILER})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"routine_source_decl_codegen\":${SHELL_ROUTINE_SOURCE_DECL_CODEGEN}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:ast-read-surface] routine_source_decl_codegen parity FAIL (shell=${SHELL_ROUTINE_SOURCE_DECL_CODEGEN})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.ast-read-surface.v1' \
    | tail -n 1)"
PERGYRA_JSON="${PERGYRA_JSON%$'\r'}"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:ast-read-surface] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-ars.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/codegen"
mkdir -p "$NEG_ROOT/tests"
{
    for k in $(seq 1 1); do
        echo "source_ast /* synthetic growth $k */"
    done
} > "$NEG_ROOT/src/codegen/synthetic_source_ast.c"
echo "source_ast_codegen|source_ast|0|src/codegen|src/codegen/synthetic_source_ast.c" > "$NEG_ROOT/$MANIFEST_REL"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:ast-read-surface] growth fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"surface_growth"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:ast-read-surface] growth fixture expected surface_growth finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

echo "[self-host-parity:ast-read-surface] rung-2 parity ok (enum=$SHELL_ENUM codegen=$SHELL_CODEGEN compiler=$SHELL_COMPILER source_decl_codegen=$SHELL_SOURCE_DECL_CODEGEN source_decl_compiler=$SHELL_SOURCE_DECL_COMPILER routine_source_decl_codegen=$SHELL_ROUTINE_SOURCE_DECL_CODEGEN; growth-fixture rc=1)"

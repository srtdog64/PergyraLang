#!/usr/bin/env bash
# Rung 2 parity for the AST read surface checker (2026-06-15).
#
# The shell smoke remains the coverage oracle over the shared ratchet spec.
# The Pergyra tool proves the same literal counts by walking each metric scope
# through DirWalk.

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
RATCHET_FILE="$ROOT_DIR/tests/ast_read_surface_ratchet.txt"
RATCHET_REL="tests/ast_read_surface_ratchet.txt"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$RATCHET_FILE"; do
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

SHELL_ENUM=0
SHELL_CODEGEN=0
SHELL_COMPILER=0
SHELL_SOURCE_DECL_CODEGEN=0
SHELL_SOURCE_DECL_COMPILER=0
SHELL_ROUTINE_SOURCE_DECL_CODEGEN=0
while IFS='|' read -r kind pattern ceiling scope; do
    [[ -n "$kind" ]] || continue
    n="$((cd "$ROOT_DIR" && grep -R -F -o "$pattern" "$scope" --include='*.c' 2>/dev/null || true) \
        | wc -l | tr -d ' ')"
    case "$kind" in
        enum)
            SHELL_ENUM=$((SHELL_ENUM + n))
            ;;
        source_ast_codegen)
            SHELL_CODEGEN=$((SHELL_CODEGEN + n))
            ;;
        source_ast_compiler)
            SHELL_COMPILER=$((SHELL_COMPILER + n))
            ;;
        source_decl_codegen)
            SHELL_SOURCE_DECL_CODEGEN=$((SHELL_SOURCE_DECL_CODEGEN + n))
            ;;
        source_decl_compiler)
            SHELL_SOURCE_DECL_COMPILER=$((SHELL_SOURCE_DECL_COMPILER + n))
            ;;
        routine_source_decl_codegen)
            SHELL_ROUTINE_SOURCE_DECL_CODEGEN=$((SHELL_ROUTINE_SOURCE_DECL_CODEGEN + n))
            ;;
    esac
done < "$RATCHET_FILE"

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
echo "source_ast_codegen|source_ast|0|src/codegen" > "$NEG_ROOT/$RATCHET_REL"

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

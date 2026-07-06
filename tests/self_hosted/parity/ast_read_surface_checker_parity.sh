#!/usr/bin/env bash
# Rung 2 parity for the AST read surface checker (2026-06-15).
#
# The shell smoke remains the coverage oracle over the shared ratchet spec.
# The Pergyra tool proves the same literal counts by walking each metric scope
# through DirWalk.

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
        echo "[self-host-parity:ast-read-surface] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:ast-read-surface] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/ast_read_surface_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/ast_read_surface_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:ast-read-surface" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "ast-read-surface-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[self-host-parity:ast-read-surface] TestHarness manifest expected 3 ast-read-surface paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
RATCHET_REL="${harness_paths[2]}"
RATCHET_FILE="$ROOT_DIR/$RATCHET_REL"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$RATCHET_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:ast-read-surface] missing input: $path" >&2
        exit 1
    fi
done

bash "$ROOT_DIR/tests/ast_read_surface_smoke.sh" >/dev/null

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/ast_read_surface_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/ast_read_surface_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:ast-read-surface] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:ast-read-surface" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" 2>/dev/null)"
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
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:ast-read-surface" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

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
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" 2>&1)"
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

assert_llvm_leg "self-host-parity:ast-read-surface" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:ast-read-surface] rung-2 parity ok (enum=$SHELL_ENUM codegen=$SHELL_CODEGEN compiler=$SHELL_COMPILER source_decl_codegen=$SHELL_SOURCE_DECL_CODEGEN source_decl_compiler=$SHELL_SOURCE_DECL_COMPILER routine_source_decl_codegen=$SHELL_ROUTINE_SOURCE_DECL_CODEGEN; growth-fixture rc=1)"

#!/usr/bin/env bash
# Rung 2 parity for the doc link checker (2026-05-27).
#
# Pergyra is the origin (src/self_hosted/tools/doc_link_checker/main.pgy).
# Shell grep is the parity backend. Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - total/md link count parity vs `grep -oE`
#   - synthetic dead-link fixture: rc=1, missing_link finding
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
        echo "[self-host-parity:doc-link-checker] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:doc-link-checker] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/doc_link_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/doc_link_checker_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:doc-link-checker" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "doc-link-checker-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[self-host-parity:doc-link-checker] TestHarness manifest expected 3 doc-link paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
INDEX_PATH="${harness_paths[2]}"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$ROOT_DIR/$INDEX_PATH"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:doc-link-checker] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/doc_link_checker_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/doc_link_checker_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:doc-link-checker] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:doc-link-checker" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$INDEX_PATH" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:doc-link-checker] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.doc-link-checker.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:doc-link-checker] schema header missing" >&2
    exit 1
fi

# Shell drift detector.
SHELL_TOTAL="$(grep -oE '\]\(' "$ROOT_DIR/$INDEX_PATH" | wc -l | tr -d ' ')"
SHELL_MD="$(grep -oE '\]\([^)]+\.md\)' "$ROOT_DIR/$INDEX_PATH" | wc -l | tr -d ' ')"
if [[ -z "$SHELL_TOTAL" || "$SHELL_TOTAL" -eq 0 ]]; then
    echo "[self-host-parity:doc-link-checker] shell ground truth empty" >&2
    exit 1
fi

if ! grep -Fq "\"total_links\":${SHELL_TOTAL}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:doc-link-checker] total_links parity FAIL (shell=${SHELL_TOTAL})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"md_links\":${SHELL_MD}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:doc-link-checker] md_links parity FAIL (shell=${SHELL_MD})" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.doc-link-checker.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:doc-link-checker" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

# Synthetic dead-link fixture - rewrite one link target to a non-existent path.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-dlc.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/docs"
mkdir -p "$NEG_ROOT/.tmp"
# Mirror the docs subtree minimally - we only need INDEX.md + at least one
# real doc to exist so the tool sees a mix of live + dead links.
sed 's|](100_beta_readiness_checklist.md)|](XX_NONEXISTENT_FAKE_DRIFT.md)|' \
    "$ROOT_DIR/$INDEX_PATH" > "$NEG_ROOT/$INDEX_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" "$INDEX_PATH" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:doc-link-checker] dead-link fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"missing_link"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:doc-link-checker] dead-link fixture expected missing_link finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq 'XX_NONEXISTENT_FAKE_DRIFT.md' <<<"$NEG_OUT"; then
    echo "[self-host-parity:doc-link-checker] dead-link fixture expected drift path in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:doc-link-checker" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$INDEX_PATH"
echo "[self-host-parity:doc-link-checker] rung-2 parity ok (total=$SHELL_TOTAL md=$SHELL_MD missing=0; dead-link fixture rc=1)"

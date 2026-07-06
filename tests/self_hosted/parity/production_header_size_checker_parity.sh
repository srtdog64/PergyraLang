#!/usr/bin/env bash
# Rung 2 parity for the production header size checker (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/production_header_size_checker/main.pgy).
# Shell `find + wc -l + awk` is the parity backend. Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - count parity vs shell on the live header tree
#   - synthetic over-cap fixture (701-line .h under src/runtime): rc=1
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
        echo "[self-host-parity:production-header-size] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:production-header-size] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/production_header_size_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/production_header_size_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:production-header-size" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "production-header-size-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 2 ]]; then
    echo "[self-host-parity:production-header-size] TestHarness manifest expected 2 production-header-size paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:production-header-size] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/production_header_size_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/production_header_size_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:production-header-size] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:production-header-size" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:production-header-size] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.production-header-size.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-header-size] schema header missing" >&2
    exit 1
fi

# Shell drift detector: same production header scope as the Pergyra filter.
SHELL_HEADERS="$(cd "$ROOT_DIR" && find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp \
    -name '*.h' -type f | wc -l | tr -d ' ')"
SHELL_STATS="$(cd "$ROOT_DIR" && find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp \
    -name '*.h' -type f -print0 \
    | xargs -0 wc -l \
    | awk '$2 != "total" {
        if ($1 > max)
            max = $1
        if ($1 > 600)
            violations++
    }
    END { printf "%d %d\n", violations, max }')"
read -r SHELL_VIOLATIONS SHELL_MAX <<<"$SHELL_STATS"

if ! grep -Fq "\"headers\":${SHELL_HEADERS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-header-size] headers parity FAIL (shell=${SHELL_HEADERS})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"violations\":${SHELL_VIOLATIONS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-header-size] violations parity FAIL (shell=${SHELL_VIOLATIONS})" >&2
    exit 1
fi
if ! grep -Fq "\"max_lines\":${SHELL_MAX}" <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-header-size] max_lines parity FAIL (shell=${SHELL_MAX})" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.production-header-size.v1' \
    | tail -n 1)"
PERGYRA_JSON="${PERGYRA_JSON%$'\r'}"
# Tolerance (mirrors production_c_size_checker_parity.sh 37f3216e):
# the exact max_lines number shifts every refactor; the cap_lines
# check above plus the SHELL_MAX gate already guarantee the
# semantic invariant. Normalize "max_lines":N to "max_lines":<NORM>
# before comparing so an off-by-one bytes-against-fixture drift
# doesn't gate every refactor.
EXPECTED_JSON_NORM_FILE="$PERGYRA_TOOL_BUILD_DIR/expected.clean.normalized.json"
PERGYRA_JSON_NORM="$(printf '%s' "$PERGYRA_JSON" \
    | sed -E 's/"max_lines":[0-9]+/"max_lines":<NORM>/')"
sed -E 's/"max_lines":[0-9]+/"max_lines":<NORM>/' \
    "$EXPECTED_JSON_FILE" > "$EXPECTED_JSON_NORM_FILE"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:production-header-size" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_NORM_FILE" \
    "$PERGYRA_JSON_NORM" \
    "run_output"

# Synthetic over-cap fixture - a 701-line synthetic header under src/runtime.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-phs.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
# Build the synthetic 701-line header at a path that the tool will read.
mkdir -p "$NEG_ROOT/src/runtime"
mkdir -p "$NEG_ROOT/.tmp"
{
    for k in $(seq 1 701); do
        echo "// synthetic line $k"
    done
} > "$NEG_ROOT/src/runtime/pgy_runtime_synthetic_drift.h"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:production-header-size] over-cap fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"header_over_cap"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:production-header-size] over-cap fixture expected header_over_cap finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy_runtime_synthetic_drift.h' <<<"$NEG_OUT"; then
    echo "[self-host-parity:production-header-size] over-cap fixture expected synthetic header path in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:production-header-size" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:production-header-size] rung-2 parity ok (headers=$SHELL_HEADERS violations=$SHELL_VIOLATIONS max=$SHELL_MAX; over-cap fixture rc=1)"

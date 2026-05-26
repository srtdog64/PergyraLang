#!/usr/bin/env bash
# Rung 2 parity for the diagnostic catalog checker.
#
# The C diagnostic registry smoke remains the oracle while the Pergyra tool is
# a partial implementation. This script asserts exit-code agreement, stable
# counter parity, and exact clean-baseline JSON shape. See
# self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
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
        echo "[self-host-parity:diagnostic-catalog] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:diagnostic-catalog] missing compiler binary: $PGY" >&2
    exit 1
fi

C_ORACLE="$ROOT_DIR/tests/diagnostic_registry_smoke.sh"
PERGYRA_TOOL_SOURCE="$ROOT_DIR/self_hosted/tools/diagnostic_catalog_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/diagnostic_catalog_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json"
EXPECTED_MISSING_JSON_FILE="$ROOT_DIR/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json"
EXPECTED_INPUT_ERROR_JSON_FILE="$ROOT_DIR/self_hosted/tools/diagnostic_catalog_checker/expected/missing_input.json"

if [[ ! -x "$C_ORACLE" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing C oracle: $C_ORACLE" >&2
    exit 1
fi
if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_JSON_FILE" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing expected JSON: $EXPECTED_JSON_FILE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_MISSING_JSON_FILE" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing missing-code expected JSON: $EXPECTED_MISSING_JSON_FILE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_INPUT_ERROR_JSON_FILE" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing input-error expected JSON: $EXPECTED_INPUT_ERROR_JSON_FILE" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

set +e
(cd "$ROOT_DIR" && bash "$C_ORACLE") >/dev/null 2>&1
C_RC=$?
set -e

set +e
(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run) >/dev/null 2>&1
P_RC=$?
set -e

if [[ "$C_RC" -ne "$P_RC" ]]; then
    echo "[self-host-parity:diagnostic-catalog] exit-code parity FAIL (c=$C_RC pergyra=$P_RC)" >&2
    exit 1
fi

# Capture Pergyra stdout for header check and minimal count parity (rung 2).
set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
set -e
if ! grep -Fq 'pgy.selfhost.diagnostic-catalog.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:diagnostic-catalog] schema header missing from Pergyra stdout" >&2
    exit 1
fi

# Rung 2 minimal - count parity against shell grep ground truth.
# The C/shell side remains the oracle; shell grep is a drift detector for the
# macro count while the Pergyra side is still a candidate implementation.
SHELL_CODES="$(grep -c '#define PGY_CODE_' "$ROOT_DIR/src/semantic/diag_codes.h" || true)"
if [[ -z "$SHELL_CODES" || "$SHELL_CODES" -eq 0 ]]; then
    echo "[self-host-parity:diagnostic-catalog] shell grep ground truth empty for diag_codes.h" >&2
    exit 1
fi

if ! grep -Fq "\"codes\":${SHELL_CODES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:diagnostic-catalog] counts.codes parity FAIL (shell=${SHELL_CODES})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

SHELL_DOCUMENTED="$(grep -c '^#### `PGY_' "$ROOT_DIR/docs/72_diagnostic_codes.md" || true)"
if [[ -z "$SHELL_DOCUMENTED" || "$SHELL_DOCUMENTED" -eq 0 ]]; then
    echo "[self-host-parity:diagnostic-catalog] shell grep ground truth empty for docs catalog" >&2
    exit 1
fi

if ! grep -Fq "\"documented\":${SHELL_DOCUMENTED}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:diagnostic-catalog] counts.documented parity FAIL (shell=${SHELL_DOCUMENTED})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

if ! grep -Fq '"missing":0,' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:diagnostic-catalog] counts.missing parity FAIL (expected 0)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

# duplicates parity - shell ground truth via sort -u on emitted literal strings.
SHELL_DUP_TOTAL="$(grep -oE '"PGY_[A-Z0-9_]+"' "$ROOT_DIR/src/semantic/diag_codes.h" | grep -v '^"PGY_CAUSE_\|^"PGY_FIX_' | wc -l | tr -d ' ')"
SHELL_DUP_UNIQUE="$(grep -oE '"PGY_[A-Z0-9_]+"' "$ROOT_DIR/src/semantic/diag_codes.h" | grep -v '^"PGY_CAUSE_\|^"PGY_FIX_' | sort -u | wc -l | tr -d ' ')"
if [[ -z "$SHELL_DUP_TOTAL" || -z "$SHELL_DUP_UNIQUE" ]]; then
    echo "[self-host-parity:diagnostic-catalog] shell duplicates ground truth empty" >&2
    exit 1
fi
SHELL_DUPLICATES=$((SHELL_DUP_TOTAL - SHELL_DUP_UNIQUE))

if ! grep -Fq "\"duplicates\":${SHELL_DUPLICATES}" <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:diagnostic-catalog] counts.duplicates parity FAIL (shell=${SHELL_DUPLICATES})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

SHELL_ORPHANS=0
while IFS= read -r doc_code; do
    [[ -n "$doc_code" ]] || continue
    if ! grep -Fq "\"${doc_code}\"" "$ROOT_DIR/src/semantic/diag_codes.h"; then
        SHELL_ORPHANS=$((SHELL_ORPHANS + 1))
    fi
done < <(
    grep '^#### `PGY_' "$ROOT_DIR/docs/72_diagnostic_codes.md" \
        | sed -E 's/^#### `([^`]+)`.*/\1/'
)

if ! grep -Fq "\"orphans\":${SHELL_ORPHANS}" <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:diagnostic-catalog] counts.orphans parity FAIL (shell=${SHELL_ORPHANS})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.diagnostic-catalog.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:diagnostic-catalog] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-diag.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/semantic" "$NEG_ROOT/docs"
cp "$ROOT_DIR/src/semantic/diag_codes.h" "$NEG_ROOT/src/semantic/diag_codes.h"
cp "$ROOT_DIR/docs/72_diagnostic_codes.md" "$NEG_ROOT/docs/72_diagnostic_codes.md"
cat >> "$NEG_ROOT/src/semantic/diag_codes.h" <<'EOF'
#define PGY_CODE_FAKE_DRIFT_FOR_SELFHOST "PGY_FAKE_DRIFT_FOR_SELFHOST"
EOF

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing-code fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
NEG_JSON="$(printf '%s\n' "$NEG_OUT" \
    | grep -F 'pgy.selfhost.diagnostic-catalog.v1' \
    | tail -n 1)"
EXPECTED_MISSING_JSON="$(cat "$EXPECTED_MISSING_JSON_FILE")"
if [[ "$NEG_JSON" != "$EXPECTED_MISSING_JSON" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing-code JSON parity FAIL" >&2
    echo "expected: $EXPECTED_MISSING_JSON" >&2
    echo "actual:   $NEG_JSON" >&2
    exit 1
fi

INPUT_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-diag-input.XXXXXX")"
cleanup_input_root() {
    rm -rf "$INPUT_ROOT"
}
trap 'cleanup_neg_root; cleanup_input_root' EXIT
mkdir -p "$INPUT_ROOT/src/semantic" "$INPUT_ROOT/docs"
cp "$ROOT_DIR/docs/72_diagnostic_codes.md" "$INPUT_ROOT/docs/72_diagnostic_codes.md"

set +e
INPUT_OUT="$(cd "$INPUT_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
INPUT_RC=$?
set -e
if [[ "$INPUT_RC" -ne 1 ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing-input fixture expected rc=1, got rc=$INPUT_RC" >&2
    printf '%s\n' "$INPUT_OUT" >&2
    exit 1
fi
INPUT_JSON="$(printf '%s\n' "$INPUT_OUT" \
    | grep -F 'pgy.selfhost.diagnostic-catalog.v1' \
    | tail -n 1)"
EXPECTED_INPUT_ERROR_JSON="$(cat "$EXPECTED_INPUT_ERROR_JSON_FILE")"
if [[ "$INPUT_JSON" != "$EXPECTED_INPUT_ERROR_JSON" ]]; then
    echo "[self-host-parity:diagnostic-catalog] missing-input JSON parity FAIL" >&2
    echo "expected: $EXPECTED_INPUT_ERROR_JSON" >&2
    echo "actual:   $INPUT_JSON" >&2
    exit 1
fi

echo "[self-host-parity:diagnostic-catalog] rung-1 exit-code + rung-2 count/json parity ok (c=$C_RC pergyra=$P_RC codes=$SHELL_CODES documented=$SHELL_DOCUMENTED missing=0 duplicates=$SHELL_DUPLICATES orphans=$SHELL_ORPHANS; missing-fixture rc=$NEG_RC input-fixture rc=$INPUT_RC)"

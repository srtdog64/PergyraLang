#!/usr/bin/env bash
# Fact-owned impact plan smoke for self-host completeness.
#
# This does not decide changed files in shell. It proves that the Pergyra
# completeness owner exports the source-pattern -> env-knob -> proof-gate rows
# that later runners can consume instead of hard-coding impact lists.

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
        echo "[self-host-completeness-impact] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-completeness-impact] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/completeness_impact}"
MANIFEST_FILE="$BUILD_DIR/completeness_impact_plan.txt"
mkdir -p "$BUILD_DIR"

pgy_selfhost_read_test_harness_manifest \
    "self-host-completeness-impact" \
    "$BUILD_DIR" \
    "self-host-completeness-impact-plan" \
    "$MANIFEST_FILE"

if ! grep -Fxq "schema=pgy.selfhost.completeness-impact.v1" "$MANIFEST_FILE"; then
    echo "[self-host-completeness-impact] schema row missing" >&2
    cat "$MANIFEST_FILE" >&2
    exit 1
fi

impact_rows=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    [[ "$line" == schema=* ]] && continue
    impact_rows+=("$line")
done < "$MANIFEST_FILE"

if [[ "${#impact_rows[@]}" -ne 5 ]]; then
    echo "[self-host-completeness-impact] expected 5 impact rows, got ${#impact_rows[@]}" >&2
    cat "$MANIFEST_FILE" >&2
    exit 1
fi

seen_selfhost=0
seen_parser=0
seen_codegen=0
seen_air_graph=0
seen_contract=0
for row in "${impact_rows[@]}"; do
    IFS=$'\t' read -r impact_id source_pattern source_env stage_env stage_value proof_gate extra <<< "$row"
    if [[ -n "${extra:-}" || -z "$impact_id" || -z "$source_pattern" || -z "$proof_gate" ]]; then
        echo "[self-host-completeness-impact] malformed impact row: $row" >&2
        exit 1
    fi
    if ! grep -Fq "$proof_gate:" "$ROOT_DIR/Makefile"; then
        echo "[self-host-completeness-impact] proof gate is not a Make target: $proof_gate" >&2
        exit 1
    fi
    case "$impact_id" in
        selfhost-production-source|parser-source|codegen-source|air-graph-consumer-source)
            [[ "$source_env" == "PGY_SELFHOST_COMPLETENESS_SOURCES" ]] ||
                { echo "[self-host-completeness-impact] bad source filter env for $impact_id" >&2; exit 1; }
            [[ "$stage_env" == "PGY_SELFHOST_COMPLETENESS_STAGES" ]] ||
                { echo "[self-host-completeness-impact] bad stage filter env for $impact_id" >&2; exit 1; }
            [[ "$stage_value" == "lexer,parser,semantic,codegen" ]] ||
                { echo "[self-host-completeness-impact] bad stage filter value for $impact_id" >&2; exit 1; }
            ;;
        component-contract)
            [[ "$source_env" == "-" && "$stage_env" == "-" && "$stage_value" == "-" ]] ||
                { echo "[self-host-completeness-impact] component-contract must not claim completeness filters" >&2; exit 1; }
            ;;
        *)
            echo "[self-host-completeness-impact] unknown impact id: $impact_id" >&2
            exit 1
            ;;
    esac
    [[ "$impact_id" == "selfhost-production-source" ]] && seen_selfhost=1
    [[ "$impact_id" == "parser-source" ]] && seen_parser=1
    [[ "$impact_id" == "codegen-source" ]] && seen_codegen=1
    [[ "$impact_id" == "air-graph-consumer-source" ]] && seen_air_graph=1
    [[ "$impact_id" == "component-contract" ]] && seen_contract=1
done

if [[ "$seen_selfhost$seen_parser$seen_codegen$seen_air_graph$seen_contract" != "11111" ]]; then
    echo "[self-host-completeness-impact] required impact rows missing" >&2
    cat "$MANIFEST_FILE" >&2
    exit 1
fi

for required in \
    "PGY_SELFHOST_COMPLETENESS_SOURCES" \
    "PGY_SELFHOST_COMPLETENESS_STAGES"; do
    if ! grep -Fq "$required" "$ROOT_DIR/tests/self_hosted/parity/completeness_ledger.sh"; then
        echo "[self-host-completeness-impact] completeness runner does not consume $required" >&2
        exit 1
    fi
done

echo "[self-host-completeness-impact] impact manifest ok"

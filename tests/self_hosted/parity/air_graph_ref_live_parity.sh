#!/usr/bin/env bash
# Rung 1 parity for the live AIR graph referential-integrity checker.
#
# Pergyra is the origin (src/self_hosted/tools/air_graph_ref_live/main.pgy).
# It consumes the drift-guarded `pgy --air-json` fixture used by the AIR graph
# validator and checks live-schema back-references against summary counts.

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
        echo "[self-host-parity:air-ref-live] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-ref-live] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph_ref_live"
PERGYRA_TOOL_SOURCE="$TOOL_DIR/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_ref_live}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
AIR_GRAPH_SCAN_OWNER="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy"
EXPECTED_JSON_FILE="$TOOL_DIR/expected/clean.json"
FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"

for path in "$PERGYRA_TOOL_SOURCE" "$AIR_GRAPH_SCAN_OWNER" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-ref-live] missing input: $path" >&2
        exit 1
    fi
done

pgy_selfhost_tmp_root() {
    local tmp_root="${TMPDIR:-/tmp}"
    case "$tmp_root" in
        /*|[A-Za-z]:/*) ;;
        *) tmp_root="$ROOT_DIR/$tmp_root" ;;
    esac
    mkdir -p "$tmp_root"
    printf '%s\n' "$tmp_root"
}

count_dangling_refs() {
    local file="$1"
    local field="$2"
    local limit="$3"
    grep -oE "\"${field}\":(null|-?[0-9]+)" "$file" \
        | sed -E "s/\"${field}\"://" \
        | awk -v limit="$limit" '
            $0 != "null" {
                n = $0 + 0
                if (n < 0 || n >= limit)
                    bad++
            }
            END { print bad + 0 }
        '
}

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
AIR_SCAN_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/air_graph_json_validator"
mkdir -p "$AIR_SCAN_BUILD_DIR"
cp "$AIR_GRAPH_SCAN_OWNER" "$AIR_SCAN_BUILD_DIR/scan_owner.pgy"
LIB_BUILD_DIR="$ROOT_DIR/.tmp/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-ref-live] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-ref-live.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:air-ref-live" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "air_json"

SHELL_INTENTS="$(grep -oE '"intent_count":[0-9]+' "$FIXTURE_FILE" | grep -oE '[0-9]+' | head -n 1)"
SHELL_BOUNDARIES="$(grep -oE '"boundary_count":[0-9]+' "$FIXTURE_FILE" | grep -oE '[0-9]+' | head -n 1)"
SHELL_BOUNDARY_DANGLING="$(count_dangling_refs "$FIXTURE_FILE" "boundary" "$SHELL_BOUNDARIES")"
SHELL_INTENT_DANGLING="$(count_dangling_refs "$FIXTURE_FILE" "intent" "$SHELL_INTENTS")"
SHELL_DANGLING=$((SHELL_BOUNDARY_DANGLING + SHELL_INTENT_DANGLING))
if [[ "$SHELL_DANGLING" -ne 0 ]]; then
    echo "[self-host-parity:air-ref-live] shell ground truth expected 0 dangling refs, got $SHELL_DANGLING" >&2
    exit 1
fi
if ! grep -Fq '"dangling":0' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-ref-live] counts.dangling parity FAIL (shell=0)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

SELFHOST_TMP_ROOT="$(pgy_selfhost_tmp_root)"
NEG_ROOT="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-air-ref-live-neg.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture"
mkdir -p "$NEG_ROOT/.tmp"
sed -E 's/"boundary":0/"boundary":99/' "$FIXTURE_FILE" \
    > "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-ref-live] corrupted fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-ref-live] corrupted fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"dangling_reference"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-ref-live] corrupted fixture expected dangling_reference finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:air-ref-live" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:air-ref-live] rung-1 parity ok (live refs dangling=0 artifact-equal; corrupted fixture rc=1)"

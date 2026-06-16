#!/usr/bin/env bash
# Rung 1 parity for the AIR graph node-count integrity checker.
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_node_count_integrity/main.pgy).
# This is the first AIR consumer on the live `pgy --air-json` dump (reused from
# the drift-guarded validator fixture). Shell grep is the parity backend.
# Asserts: clean exit, JSON byte-equal vs expected/clean.json, ids/declared
# parity vs shell grep, and a corrupted-summary fixture (rc=1).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/src/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:air-node-count] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-node-count] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph_node_count_integrity"
PERGYRA_TOOL_SOURCE="$TOOL_DIR/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_node_count_integrity}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$TOOL_DIR/expected/clean.json"
FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-node-count] missing input: $path" >&2
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

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

# Clean run.
set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-node-count] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.air-node-count.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:air-node-count] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Shell ground truth: id count vs sum of declared counts on the live dump.
SHELL_IDS="$(grep -oE '"id":' "$FIXTURE_FILE" | wc -l | tr -d '[:space:]')"
SHELL_DECLARED="$(grep -oE '"(intent|boundary|evidence)_count":[0-9]+' "$FIXTURE_FILE" \
    | grep -oE '[0-9]+' | awk '{s+=$1} END {print s+0}')"
if ! grep -Fq "\"ids\":${SHELL_IDS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-node-count] counts.ids parity FAIL (shell=${SHELL_IDS})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"declared\":${SHELL_DECLARED}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-node-count] counts.declared parity FAIL (shell=${SHELL_DECLARED})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

# Negative fixture: corrupt a summary count so id_count != declared.
SELFHOST_TMP_ROOT="$(pgy_selfhost_tmp_root)"
NEG_ROOT="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-nodecount-neg.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture"
mkdir -p "$NEG_ROOT/.tmp"
sed -E 's/"evidence_count":[0-9]+/"evidence_count":99/' "$FIXTURE_FILE" \
    > "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-node-count] corrupted fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-node-count] corrupted fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"node_count_mismatch"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-node-count] corrupted fixture expected a node_count_mismatch finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:air-node-count" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:air-node-count] rung-1 parity ok (live dump ids=$SHELL_IDS==declared=$SHELL_DECLARED byte-equal; corrupted fixture rc=1)"

#!/usr/bin/env bash
# Rung 1 parity for the AIR graph referential-integrity checker.
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_ref_integrity/main.pgy).
# Shell grep is the parity backend. Asserts: clean exit, JSON byte-equal vs
# expected/clean.json, dangling-count parity vs a shell set-difference on the
# fixture, and a dangling fixture (one endpoint with no matching node, rc=1).

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
        echo "[self-host-parity:air-ref-integrity] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-ref-integrity] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph_ref_integrity"
PERGYRA_TOOL_SOURCE="$TOOL_DIR/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_ref_integrity}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
AIR_GRAPH_SCAN_OWNER="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy"
EXPECTED_JSON_FILE="$TOOL_DIR/expected/clean.json"
FIXTURE_FILE="$TOOL_DIR/fixture/graph_edges.json"
DANGLING_FIXTURE_FILE="$TOOL_DIR/fixture/graph_edges_dangling.json"

for path in "$PERGYRA_TOOL_SOURCE" "$AIR_GRAPH_SCAN_OWNER" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE" "$DANGLING_FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-ref-integrity] missing input: $path" >&2
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
AIR_SCAN_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/air_graph_json_validator"
mkdir -p "$AIR_SCAN_BUILD_DIR"
cp "$AIR_GRAPH_SCAN_OWNER" "$AIR_SCAN_BUILD_DIR/scan_owner.pgy"
LIB_BUILD_DIR="$ROOT_DIR/.tmp/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

# Clean run.
set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-ref-integrity] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-ref-integrity.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:air-ref-integrity] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Shell ground truth: endpoints not present in the node-id set (expect none).
ids_of() { grep -oE '"id":[0-9]+' "$1" | grep -oE '[0-9]+' | sort -u; }
endpoints_of() { grep -oE '"(from|to)":[0-9]+' "$1" | grep -oE '[0-9]+' | sort -u; }
SHELL_DANGLING="$(comm -23 <(endpoints_of "$FIXTURE_FILE") <(ids_of "$FIXTURE_FILE") | grep -c . || true)"
if [[ "$SHELL_DANGLING" != "0" ]]; then
    echo "[self-host-parity:air-ref-integrity] shell ground truth expected 0 dangling on clean fixture, got $SHELL_DANGLING" >&2
    exit 1
fi
if ! grep -Fq '"dangling":0' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-ref-integrity] counts.dangling parity FAIL (shell=0)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

# Negative fixture: one dangling endpoint, run in a temp root, expect rc=1.
SELFHOST_TMP_ROOT="$(pgy_selfhost_tmp_root)"
NEG_ROOT="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-ref-neg.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/self_hosted/tools/air_graph_ref_integrity/fixture"
mkdir -p "$NEG_ROOT/.tmp"
cp "$DANGLING_FIXTURE_FILE" \
    "$NEG_ROOT/src/self_hosted/tools/air_graph_ref_integrity/fixture/graph_edges.json"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-ref-integrity] dangling fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-ref-integrity] dangling fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"dangling_edge_endpoint"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-ref-integrity] dangling fixture expected a dangling_edge_endpoint finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:air-ref-integrity" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:air-ref-integrity] rung-1 parity ok (clean dangling=0 byte-equal; dangling fixture rc=1)"

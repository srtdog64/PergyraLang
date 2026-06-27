#!/usr/bin/env bash
# Rung 1 parity for the AIR graph node-id uniqueness checker.
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_id_uniqueness/main.pgy).
# Shell grep is the parity backend. Asserts: clean exit, JSON byte-equal vs
# expected/clean.json, duplicate count parity vs shell grep on the fixture, and
# a duplicate fixture (one repeated id, expect rc=1).

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
        echo "[self-host-parity:air-id-uniqueness] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-id-uniqueness] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph_id_uniqueness"
PERGYRA_TOOL_SOURCE="$TOOL_DIR/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_id_uniqueness}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$TOOL_DIR/expected/clean.json"
FIXTURE_FILE="$TOOL_DIR/fixture/graph_ids.json"
DUP_FIXTURE_FILE="$TOOL_DIR/fixture/graph_ids_dup.json"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE" "$DUP_FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-id-uniqueness] missing input: $path" >&2
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
    echo "[self-host-parity:air-id-uniqueness] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-id-uniqueness.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:air-id-uniqueness] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Shell ground truth: count duplicate id tokens on the clean fixture (expect 0).
SHELL_DUPS="$(grep -oE '"id":[^,}]*' "$FIXTURE_FILE" | sort | uniq -d | wc -l | tr -d '[:space:]')"
if [[ "$SHELL_DUPS" != "0" ]]; then
    echo "[self-host-parity:air-id-uniqueness] shell ground truth expected 0 dup ids on clean fixture, got $SHELL_DUPS" >&2
    exit 1
fi
if ! grep -Fq '"duplicates":0' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-id-uniqueness] counts.duplicates parity FAIL (shell=0)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

# Negative fixture: one repeated id, run in a temp root, expect rc=1, ok:false.
SELFHOST_TMP_ROOT="$(pgy_selfhost_tmp_root)"
NEG_ROOT="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-idu-neg.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/self_hosted/tools/air_graph_id_uniqueness/fixture"
mkdir -p "$NEG_ROOT/.tmp"
cp "$DUP_FIXTURE_FILE" \
    "$NEG_ROOT/src/self_hosted/tools/air_graph_id_uniqueness/fixture/graph_ids.json"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-id-uniqueness] dup fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-id-uniqueness] dup fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"duplicate_id"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-id-uniqueness] dup fixture expected a duplicate_id finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:air-id-uniqueness" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:air-id-uniqueness] rung-1 parity ok (clean dups=0 byte-equal; dup fixture rc=1)"

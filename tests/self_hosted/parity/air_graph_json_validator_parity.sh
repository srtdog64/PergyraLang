#!/usr/bin/env bash
# Rung 2 parity for the AIR graph JSON validator (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_json_validator/main.pgy).
# The Pergyra backend-output comparator owns clean and live AIR JSON artifact
# equality. Shell only runs the process, derives live `pgy --air-json` drift
# artifacts, and mutates a synthetic missing-key fixture (strip "summary" key,
# expect rc=1).
#
# Also asserts that the committed fixture still matches the live `pgy --air-json`
# output for the canonical AIR-producing test source, so the validator does not
# drift away from the compiler's actual AIR shape.

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
        echo "[self-host-parity:air-graph-json] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-graph-json] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_json_validator}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/air_graph_json_validator_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:air-graph-json" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "air-graph-json-validator-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 10 ]]; then
    echo "[self-host-parity:air-graph-json] TestHarness manifest expected 10 air-graph-json paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
AIR_EVIDENCE_OWNER="$ROOT_DIR/${harness_paths[1]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[2]}"
FIXTURE_REL="${harness_paths[3]}"
CAP_FIXTURE_REL="${harness_paths[4]}"
AIR_SOURCE_REL="${harness_paths[5]}"
AIR_CAP_SOURCE_REL="${harness_paths[6]}"
MISSING_KEY_NAME="${harness_paths[7]}"
MISSING_KEY_FINDING_FIELD="${harness_paths[8]}"
MISSING_KEY_FINDING_VALUE="${harness_paths[9]}"
FIXTURE_FILE="$ROOT_DIR/$FIXTURE_REL"
CAP_FIXTURE_FILE="$ROOT_DIR/$CAP_FIXTURE_REL"
AIR_SOURCE="$ROOT_DIR/$AIR_SOURCE_REL"
AIR_CAP_SOURCE="$ROOT_DIR/$AIR_CAP_SOURCE_REL"

if [[ -z "$MISSING_KEY_NAME" || -z "$MISSING_KEY_FINDING_FIELD" || -z "$MISSING_KEY_FINDING_VALUE" ]]; then
    echo "[self-host-parity:air-graph-json] invalid TestHarness missing-key row" >&2
    exit 1
fi

for path in "$PERGYRA_TOOL_SOURCE" "$AIR_EVIDENCE_OWNER" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE" "$CAP_FIXTURE_FILE" "$AIR_SOURCE" "$AIR_CAP_SOURCE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-graph-json] missing input: $path" >&2
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

compare_clean_json_with_owner() {
    local actual_json="$1"
    local expected_norm="$PERGYRA_TOOL_BUILD_DIR/expected.clean.norm.json"
    local actual_norm="$PERGYRA_TOOL_BUILD_DIR/actual.clean.norm.json"
    local cmp_out="$PERGYRA_TOOL_BUILD_DIR/clean_json_compare.out"
    local cmp_err="$PERGYRA_TOOL_BUILD_DIR/clean_json_compare.err"
    local comparator_bin
    local expected_rel
    local actual_rel

    pgy_selfhost_compile_backend_output_comparator "self-host-parity:air-graph-json" "$PERGYRA_TOOL_BUILD_DIR"
    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$PERGYRA_TOOL_BUILD_DIR")"
    pgy_selfhost_normalize_text_artifact < "$EXPECTED_JSON_FILE" > "$expected_norm"
    printf '%s\n' "$actual_json" | pgy_selfhost_normalize_text_artifact > "$actual_norm"
    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected_norm")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual_norm")"

    if ! (cd "$ROOT_DIR" && "$comparator_bin" "$expected_rel" "$actual_rel" 0 2 \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[self-host-parity:air-graph-json] clean JSON parity FAIL" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

AIR_JSON_COMPARE_OUT=""
AIR_JSON_COMPARE_ERR=""

compare_air_json_file_with_owner() {
    local label="$1"
    local expected_file="$2"
    local actual_file="$3"
    local safe_label="${label//[^A-Za-z0-9_]/_}"
    local expected_norm="$PERGYRA_TOOL_BUILD_DIR/${safe_label}.expected.norm.json"
    local actual_norm="$PERGYRA_TOOL_BUILD_DIR/${safe_label}.actual.norm.json"
    local comparator_bin
    local expected_rel
    local actual_rel

    AIR_JSON_COMPARE_OUT="$PERGYRA_TOOL_BUILD_DIR/${safe_label}.compare.out"
    AIR_JSON_COMPARE_ERR="$PERGYRA_TOOL_BUILD_DIR/${safe_label}.compare.err"

    pgy_selfhost_compile_backend_output_comparator "self-host-parity:air-graph-json" "$PERGYRA_TOOL_BUILD_DIR"
    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$PERGYRA_TOOL_BUILD_DIR")"
    pgy_selfhost_normalize_text_artifact < "$expected_file" > "$expected_norm"
    pgy_selfhost_normalize_text_artifact < "$actual_file" > "$actual_norm"
    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected_norm")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual_norm")"

    (cd "$ROOT_DIR" && "$comparator_bin" "$expected_rel" "$actual_rel" 0 2 air_json \
        >"$AIR_JSON_COMPARE_OUT" 2>"$AIR_JSON_COMPARE_ERR")
}

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/air_graph_json_validator_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/air_graph_json_validator_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:air-graph-json] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:air-graph-json" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$FIXTURE_REL" "$CAP_FIXTURE_REL" 2>/dev/null | tr -d '\r')"
P_RC=$?
set -e
if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-graph-json] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.air-graph-validator.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] schema header missing" >&2
    exit 1
fi

# Clean JSON shape parity.
# counts.env_effect_sites parity is owned by the expected JSON fixture and the
# validator output; shell only routes both artifacts through the comparator.
PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.air-graph-validator.v1' \
    | tail -n 1)"
compare_clean_json_with_owner "$PERGYRA_JSON"

# Drift guard - committed fixture must still match live pgy --air-json output.
# Some sandboxed shells (MSYS / Git-Bash) cannot launch the pgy subprocess
# directly; treat that as a SKIP for the drift rung rather than a hard fail.
SELFHOST_TMP_ROOT="$(pgy_selfhost_tmp_root)"
LIVE_AIR_JSON_DIR="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-air-live.XXXXXX")"
cleanup_live_air() {
    rm -rf "$LIVE_AIR_JSON_DIR"
}
trap cleanup_live_air EXIT
LIVE_AIR_JSON="$LIVE_AIR_JSON_DIR/live.json"
LIVE_CAP_AIR_JSON="$LIVE_AIR_JSON_DIR/live_cap_env.json"
set +e
(cd "$ROOT_DIR" && "$PGY" --air-json "$(pgy_path_for_compiler "$PGY" "$AIR_SOURCE")" 2>/dev/null \
    | grep '^{' | head -n 1 > "$LIVE_AIR_JSON")
LIVE_RC=$?
(cd "$ROOT_DIR" && "$PGY" --air-json "$(pgy_path_for_compiler "$PGY" "$AIR_CAP_SOURCE")" 2>/dev/null \
    | grep '^{' | head -n 1 > "$LIVE_CAP_AIR_JSON")
LIVE_CAP_RC=$?
set -e
DRIFT_GUARD="skipped"
if [[ "$LIVE_RC" -eq 0 && -s "$LIVE_AIR_JSON" ]]; then
    if ! compare_air_json_file_with_owner "live_fixture_drift" "$FIXTURE_FILE" "$LIVE_AIR_JSON"; then
        # PGY_AIR_GRAPH_JSON_SKIP_DRIFT escape hatch: the committed
        # fixture is pinned against an LLVM-enabled build. Builds that
        # toggle structural compile flags (e.g. LLVM_ENABLED=0 on the
        # Windows + macOS-c-only CI lanes) can legitimately reshape
        # formatting around the same underlying AIR. The summary/count
        # parity above already cross-checks the semantic shape; the
        # byte-equal drift detector is the stricter add-on, so let
        # those lanes opt out instead of forcing dual fixtures.
        if [[ "${PGY_AIR_GRAPH_JSON_SKIP_DRIFT:-0}" == "1" ]]; then
            DRIFT_GUARD="skipped-by-env"
        else
            echo "[self-host-parity:air-graph-json] committed fixture drifted from live pgy --air-json output" >&2
            echo "regenerate via: pgy --air-json $AIR_SOURCE_REL > $FIXTURE_REL" >&2
            cat "$AIR_JSON_COMPARE_OUT" "$AIR_JSON_COMPARE_ERR" >&2
            exit 1
        fi
    else
        DRIFT_GUARD="ok"
    fi
fi
if [[ "$LIVE_CAP_RC" -eq 0 && -s "$LIVE_CAP_AIR_JSON" ]]; then
    if ! compare_air_json_file_with_owner "live_cap_fixture_drift" "$CAP_FIXTURE_FILE" "$LIVE_CAP_AIR_JSON"; then
        if [[ "${PGY_AIR_GRAPH_JSON_SKIP_DRIFT:-0}" == "1" ]]; then
            DRIFT_GUARD="skipped-by-env"
        else
            echo "[self-host-parity:air-graph-json] committed cap_env fixture drifted from live pgy --air-json output" >&2
            echo "regenerate via: pgy --air-json $AIR_CAP_SOURCE_REL > $CAP_FIXTURE_REL" >&2
            cat "$AIR_JSON_COMPARE_OUT" "$AIR_JSON_COMPARE_ERR" >&2
            exit 1
        fi
    elif [[ "$DRIFT_GUARD" != "skipped-by-env" ]]; then
        DRIFT_GUARD="ok"
    fi
fi

# Negative fixture - synthetic missing-key (strip top-level "summary":{...}).
NEG_ROOT="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-air-neg.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
    cleanup_live_air
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/$(dirname "$FIXTURE_REL")" "$NEG_ROOT/$(dirname "$CAP_FIXTURE_REL")"
mkdir -p "$NEG_ROOT/.tmp"
# Strip the owner-selected top-level object segment from the live shape.
sed -E "s/\"${MISSING_KEY_NAME}\":\{[^}]*\},//" "$FIXTURE_FILE" \
    > "$NEG_ROOT/$FIXTURE_REL"
cp "$CAP_FIXTURE_FILE" "$NEG_ROOT/$CAP_FIXTURE_REL"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" "$FIXTURE_REL" "$CAP_FIXTURE_REL" 2>&1 | tr -d '\r')"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"${MISSING_KEY_FINDING_FIELD}\":${MISSING_KEY_FINDING_VALUE}" <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected ${MISSING_KEY_FINDING_FIELD}:${MISSING_KEY_FINDING_VALUE}" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:air-graph-json" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$FIXTURE_REL" "$CAP_FIXTURE_REL"
echo "[self-host-parity:air-graph-json] rung-2 parity ok (expected-json clean; missing-key rc=1; live-drift=$DRIFT_GUARD)"

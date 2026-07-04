#!/usr/bin/env bash
# Rung 2 parity for the AIR graph JSON validator (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_json_validator/main.pgy).
# Shell grep supplies count ground truth; the Pergyra backend-output comparator
# owns clean and live AIR JSON artifact equality. The script also
# checks a synthetic missing-key fixture (strip "summary" key, expect rc=1).
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

PERGYRA_TOOL_SOURCE_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator"
PERGYRA_TOOL_SOURCE="$PERGYRA_TOOL_SOURCE_DIR/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_json_validator}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
AIR_EVIDENCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/air_evidence_owner.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/expected/clean.json"
FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"
CAP_FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/cap_env.json"
AIR_SOURCE="$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy"
AIR_CAP_SOURCE="$ROOT_DIR/tests/capability/cap_env_demo.pgy"

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

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE_DIR"/*.pgy "$PERGYRA_TOOL_BUILD_DIR"/
mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../lib"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/../../lib/"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../compiler"
cp "$AIR_EVIDENCE_OWNER" "$PERGYRA_TOOL_BUILD_DIR/../../compiler/air_evidence_owner.pgy"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null | tr -d '\r')"
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

# Shell drift detector - extract counts from fixture.
extract_int() {
    local field="$1"
    grep -oE "\"${field}\":[0-9]+" "$FIXTURE_FILE" | head -n 1 | grep -oE '[0-9]+' || true
}
SHELL_INTENTS="$(extract_int intent_count)"
SHELL_BOUNDARIES="$(extract_int boundary_count)"
SHELL_EVIDENCE="$(extract_int evidence_count)"
SHELL_DRIFTS="$(extract_int drift_count)"
SHELL_EFFECT_SITES="$( (grep -ho '"capability_mask":"0x[0-9a-fA-F]*"' "$FIXTURE_FILE" "$CAP_FIXTURE_FILE" || true) | wc -l | tr -d '[:space:]')"
SHELL_ENV_EFFECT_SITES="$( (grep -ho '"op":"Args","effect":"ENV","capability_mask":"0x20"' "$CAP_FIXTURE_FILE" || true) | wc -l | tr -d '[:space:]')"

if [[ -z "$SHELL_INTENTS" || -z "$SHELL_BOUNDARIES" || -z "$SHELL_EVIDENCE" || -z "$SHELL_DRIFTS" ]]; then
    echo "[self-host-parity:air-graph-json] shell grep ground truth missing from fixture" >&2
    exit 1
fi
if [[ "$SHELL_ENV_EFFECT_SITES" -le 0 ]]; then
    echo "[self-host-parity:air-graph-json] shell grep ground truth missing Args/ENV effect site" >&2
    exit 1
fi

if ! grep -Fq "\"intents\":${SHELL_INTENTS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.intents parity FAIL (shell=${SHELL_INTENTS})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"boundaries\":${SHELL_BOUNDARIES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.boundaries parity FAIL (shell=${SHELL_BOUNDARIES})" >&2
    exit 1
fi
if ! grep -Fq "\"evidence\":${SHELL_EVIDENCE}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.evidence parity FAIL (shell=${SHELL_EVIDENCE})" >&2
    exit 1
fi
if ! grep -Fq "\"drifts\":${SHELL_DRIFTS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.drifts parity FAIL (shell=${SHELL_DRIFTS})" >&2
    exit 1
fi
if ! grep -Fq "\"effect_sites\":${SHELL_EFFECT_SITES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.effect_sites parity FAIL (shell=${SHELL_EFFECT_SITES})" >&2
    exit 1
fi
if ! grep -Fq "\"env_effect_sites\":${SHELL_ENV_EFFECT_SITES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.env_effect_sites parity FAIL (shell=${SHELL_ENV_EFFECT_SITES})" >&2
    exit 1
fi

# Clean JSON shape parity.
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
            echo "regenerate via: pgy --air-json $AIR_SOURCE > $FIXTURE_FILE" >&2
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
            echo "regenerate via: pgy --air-json $AIR_CAP_SOURCE > $CAP_FIXTURE_FILE" >&2
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
mkdir -p "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture"
mkdir -p "$NEG_ROOT/.tmp"
# Strip the "summary":{...}, segment - simple sed that matches the live shape.
sed -E 's/"summary":\{[^}]*\},//' "$FIXTURE_FILE" \
    > "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"
cp "$CAP_FIXTURE_FILE" \
    "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture/cap_env.json"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1 | tr -d '\r')"
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
if ! grep -Fq '"missing_keys":1' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected missing_keys:1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:air-graph-json" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:air-graph-json] rung-2 parity ok (intents=$SHELL_INTENTS boundaries=$SHELL_BOUNDARIES evidence=$SHELL_EVIDENCE drifts=$SHELL_DRIFTS effect_sites=$SHELL_EFFECT_SITES env_effect_sites=$SHELL_ENV_EFFECT_SITES; missing-key rc=1; live-drift=$DRIFT_GUARD)"

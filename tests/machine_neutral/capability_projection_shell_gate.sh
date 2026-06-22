#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"

pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-${1:-$ROOT_DIR/bin/pgy.exe}}"
PGY="$(pgy_path_for_bash_tool "$PGY")"
PGY="$(pgy_select_optional_exe_binary "$PGY")"

fail() {
    echo "[machine-neutral-shell] $*" >&2
    exit 1
}

require_air_term() {
    local fixture="$1"
    local term="$2"
    local label="$3"
    local out="$4"

    if ! grep -Fq -- "$term" "$out"; then
        echo "--- AIR JSON output for $fixture ---" >&2
        cat "$out" >&2
        fail "$label missing term: $term"
    fi
}

run_air() {
    local fixture="$1"
    local out="$2"

    "$PGY" --air-json "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$fixture")" \
        --backend=c >"$out" 2>&1 \
        || {
            cat "$out" >&2
            fail "pgy --air-json failed for $fixture"
        }
    require_air_term "$fixture" '"schema":"pgy.air.graph.v1"' "schema" "$out"
}

if [[ ! -x "$PGY" ]]; then
    fail "missing compiler binary: $PGY"
fi
pgy_require_runnable_binary_here "machine-neutral-shell" "$PGY"

tmp_base="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    tmp_base="$ROOT_DIR/.tmp"
    mkdir -p "$tmp_base"
fi
tmp_dir="$(mktemp -d "${tmp_base%/}/pgy-machine-neutral.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

random_out="$tmp_dir/cap_random_demo.air"
slot_out="$tmp_dir/secure_slot.air"
authority_out="$tmp_dir/zone_intent.air"

run_air "tests/capability/cap_random_demo.pgy" "$random_out"
require_air_term "tests/capability/cap_random_demo.pgy" \
    '"effects":["RANDOM"]' "effect inventory" "$random_out"
require_air_term "tests/capability/cap_random_demo.pgy" \
    '"effects_by_op":[{"op":"Random","effect":"RANDOM","capability_mask":"0x10","routine":"Main"}]' \
    "per-op capability mask" "$random_out"

run_air "tests/air_erasure/fixtures/03_secure_slot.pgy" "$slot_out"
require_air_term "tests/air_erasure/fixtures/03_secure_slot.pgy" \
    '"slots":[{"slot":"hp","op":"Write","routine":"Main"}' \
    "slot identity" "$slot_out"
require_air_term "tests/air_erasure/fixtures/03_secure_slot.pgy" \
    '{"slot":"hp","op":"Read","routine":"Main"}' \
    "slot read identity" "$slot_out"

run_air "tests/air_erasure/fixtures/05_zone_intent.pgy" "$authority_out"
require_air_term "tests/air_erasure/fixtures/05_zone_intent.pgy" \
    '"authority_names":["hero"]' "authority participant" "$authority_out"
require_air_term "tests/air_erasure/fixtures/05_zone_intent.pgy" \
    '"required_abilities":["Prepared"]' "authority contract binding" "$authority_out"

echo "[machine-neutral-shell] GREEN: AIR owns capability-machine projection fields"

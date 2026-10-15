#!/usr/bin/env bash
# Execute only the direct fact-owner unit; never emit/run supplied source.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
cd "$ROOT"
source tests/pgy_binary_path_helpers.sh
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT/bin/pgy}")"
pgy_require_runnable_binary_here collection-map-written-root "$PGY"
WORK="$(mktemp -d "$ROOT/.tmp/self_hosted/map-written-root.XXXXXX")"
REL="${WORK#"$ROOT/"}"
UNIT=tests/self_hosted/fixtures/collection_map_payload_written_root_unit.pgy
INPUT=tests/self_hosted/parity/fixture/collection_field_lifetime/map_payload_written_root_backedge_facts.pgy
sha256sum "$PGY" >"$WORK/native.sha256"
find src/self_hosted -type f -name '*.pgy' -print0 | sort -z | xargs -0 sha256sum >"$WORK/source.sha256"
sha256sum "$UNIT" "$INPUT" >"$WORK/inputs.sha256"
for backend in ${BACKENDS:-c llvm}; do
    timeout 120 "$PGY" --native-pipeline "$UNIT" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-unit.exe" >"$WORK/$backend-unit.compile" 2>&1
    sha256sum "$WORK/$backend-unit.exe" >>"$WORK/probes.sha256"
    timeout 30 "$WORK/$backend-unit.exe" "$INPUT" >"$WORK/$backend-unit.raw" 2>"$WORK/$backend-unit.err"
    tr -d '\r' <"$WORK/$backend-unit.raw" >"$WORK/$backend-unit.run"
    [[ ! -s "$WORK/$backend-unit.err" ]]
    grep -Fxq 'MAP PAYLOAD WRITTEN ROOT PASS' "$WORK/$backend-unit.run"
done
for manifest in native source inputs probes; do
    sha256sum -c "$WORK/$manifest.sha256" >"$WORK/$manifest-recheck.log"
done
echo "[collection-map-written-root] PASS: direct formal/local/nested/fresh/unrelated/outside and missing/stale/shape controls; $REL"

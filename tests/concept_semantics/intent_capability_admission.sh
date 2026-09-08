#!/usr/bin/env bash
# Purpose-owned capability use must cross Intent calls, never all-purpose union.
# Invalid inputs are checked before MIR/executable publication and never executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here intent-capability "$PGY"
pgy_require_runnable_binary_here intent-capability "$DRIVER"
case "$DRIVER" in
    *.exe) COMPANION="${DRIVER%.exe}.machine-layer-manifest.json" ;;
    *) COMPANION="$DRIVER.machine-layer-manifest.json" ;;
esac
[[ -s "$COMPANION" ]] || { echo '[intent-capability] missing driver machine-manifest companion (test setup)' >&2; exit 2; }
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/intent-capability.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
echo "[intent-capability] evidence: ${WORK#"$ROOT_DIR/"}"
sha256sum "$PGY" "$DRIVER" "$COMPANION" tests/concept_semantics/intent/capability_fixture.pgy >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name missing; do
    source="tests/concept_semantics/intent/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        status=0
        command=("$DRIVER" --emit-mir-json-verified "$source")
        diagnostic='Code: declared_capability_missing'
        if [[ "$origin" == native ]]; then
            command=("$PGY" --native-pipeline --mir-json --error-format=json "$source")
            diagnostic='PGY_SEM_EFFECT_CONFLICT'
        fi
        timeout 30 "${command[@]}" >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
        if [[ "$missing" == valid ]]; then
            if [[ "$status" != 0 ]] || ! grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out"; then
                echo "[intent-capability] FAIL $name/$origin: valid source refused" >&2
                failures=$((failures + 1))
            else
                echo "[intent-capability] PASS $name/$origin admission"
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" ||
            ! grep -Fq "missing declared capabilities: $missing" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err"; then
            echo "[intent-capability] FAIL $name/$origin: owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[intent-capability] PASS $name/$origin owned refusal"
        fi
    done
    if [[ "$missing" != valid ]]; then
        for backend in c llvm; do
            checks=$((checks + 1))
            stem="$name.public.$backend"
            status=0
            env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
                timeout 30 "$PGY" "$source" "--backend=$backend" --opt=dev -o "$REL/$stem.exe" \
                >"$WORK/$stem.reject" 2>&1 || status=$?
            if [[ "$status" != 1 || -e "$WORK/$stem.exe" ]] ||
                ! grep -Fq 'Code: declared_capability_missing' "$WORK/$stem.reject" ||
                ! grep -Fq "missing declared capabilities: $missing" "$WORK/$stem.reject" ||
                grep -Fq '[pipeline timing]' "$WORK/$stem.reject"; then
                echo "[intent-capability] FAIL $stem: public owned refusal absent (status $status)" >&2
                failures=$((failures + 1))
            else
                echo "[intent-capability] PASS $stem rejection before artifact publication"
            fi
        done
    fi
done <<'CASES'
single_step_exact|valid
capability_on_valid|valid
capability_nested_valid|valid
capability_isolated_valid|valid
capability_on_rejected|clock
capability_expect_rejected|clock
capability_compensate_rejected|io_write
capability_nested_rejected|clock
CASES
# This is an inventory ratchet, not a replacement for the executable controls.
checks=$((checks + 1))
if rg -q 'SemanticAstIntentExpressionOwnerNodeId\(' src/self_hosted/semantic; then
    echo '[intent-capability] FAIL downstream purpose ancestry query returned' >&2
    failures=$((failures + 1))
fi
echo "[intent-capability] $checks checks / $failures failures"
[[ "$failures" == 0 ]]

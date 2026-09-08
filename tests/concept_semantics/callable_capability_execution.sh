#!/usr/bin/env bash
# Positive execution only: admission parity does not prove callable lowering.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here callable-execution "$PGY"
pgy_require_runnable_binary_here callable-execution "$DRIVER"
case "$DRIVER" in
    *.exe) COMPANION="${DRIVER%.exe}.machine-layer-manifest.json" ;;
    *) COMPANION="$DRIVER.machine-layer-manifest.json" ;;
esac
[[ -s "$COMPANION" ]] || { echo '[callable-execution] missing driver machine-manifest companion (test setup)' >&2; exit 2; }
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/callable-execution.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
echo "[callable-execution] evidence: $REL"
sha256sum "$PGY" "$DRIVER" "$COMPANION" \
    tests/concept_semantics/authority_effect/callable_capability_fixture.pgy >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name source expected; do
    sha256sum "$source" >>"$WORK/inputs.sha256"
    printf '%s\n' "$expected" | tr ',' '\n' >"$WORK/$name.expected"
    for origin in native public; do
        for backend in c llvm; do
            checks=$((checks + 1))
            stem="$name.$origin.$backend"
            command=("$PGY" "$source" "--backend=$backend" --opt=dev -o "$REL/$stem.exe")
            [[ "$origin" == native ]] && command+=(--native-pipeline)
            if ! env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
                timeout 45 "${command[@]}" >"$WORK/$stem.compile" 2>&1; then
                echo "[callable-execution] FAIL $stem: valid source rejected" >&2
                failures=$((failures + 1))
                continue
            fi
            if [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; then
                echo "[callable-execution] FAIL $stem: native fallback" >&2
                failures=$((failures + 1))
                continue
            fi
            if ! timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err"; then
                echo "[callable-execution] FAIL $stem: runtime failure" >&2
                failures=$((failures + 1))
                continue
            fi
            tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.run"
            if [[ -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$name.expected" "$WORK/$stem.run"; then
                echo "[callable-execution] FAIL $stem: output mismatch" >&2
                failures=$((failures + 1))
            else
                echo "[callable-execution] PASS $stem execution"
            fi
        done
    done
done <<'CASES'
compose|tests/cases/backend_compare/compose_two_functions/main.pgy|16,13,6
shadow|tests/self_hosted/fixtures/callable_parameter_builtin_shadow.pgy|6
recursive|tests/concept_semantics/authority_effect/callable_capability_recursive_valid.pgy|42
higher|tests/concept_semantics/authority_effect/callable_capability_higher_valid.pgy|42
CASES
echo "[callable-execution] $checks checks / $failures failures"
[[ "$failures" == 0 ]]

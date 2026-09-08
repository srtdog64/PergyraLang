#!/usr/bin/env bash
# One capability owner admits source artifacts and supplies inspection masks.
# Invalid programs are never executed; every rejection pins its semantic cause.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here capability-admission "$PGY"
pgy_require_runnable_binary_here capability-admission "$DRIVER"
case "$DRIVER" in
    *.exe) COMPANION="${DRIVER%.exe}.machine-layer-manifest.json" ;;
    *) COMPANION="$DRIVER.machine-layer-manifest.json" ;;
esac
[[ -s "$COMPANION" ]] || { echo '[capability-admission] missing driver machine-manifest companion (test setup)' >&2; exit 2; }
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/capability-admission.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
sha256sum "$PGY" "$DRIVER" "$COMPANION" >"$WORK/binaries.sha256"
checks=0
failures=0
echo "[capability-admission] evidence: $REL"
while IFS='|' read -r name missing; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/sources.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        status=0
        if [[ "$origin" == native ]]; then
            timeout 30 "$PGY" --native-pipeline --mir-json --error-format=json "$source" \
                >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
            diagnostic='PGY_SEM_EFFECT_CONFLICT'
        else
            timeout 30 "$DRIVER" --emit-mir-json-verified "$source" \
                >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
            diagnostic='Code: declared_capability_missing'
        fi
        if [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" ||
            ! grep -Fq "missing declared capabilities: $missing" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err"; then
            echo "[capability-admission] FAIL $name/$origin: owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[capability-admission] PASS $name/$origin"
        fi
    done
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
            echo "[capability-admission] FAIL $stem: public owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[capability-admission] PASS $stem rejection before artifact publication"
        fi
    done
done <<'CASES'
clock_wrong_capability|clock
capability_transitive_rejected|clock
capability_declared_edge_rejected|random
capability_forward_rejected|clock
capability_forward_inferred_rejected|clock
capability_file_mode_rejected|io_write
CASES

for name in capability_inferred capability_broad capability_recursive; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/sources.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        command=("$DRIVER" --emit-mir-json-verified "$source")
        [[ "$origin" == native ]] && command=("$PGY" --native-pipeline --mir-json "$source")
        if ! timeout 30 "${command[@]}" >"$WORK/$name.$origin.json" 2>"$WORK/$name.$origin.admit.err" ||
            ! grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.json"; then
            echo "[capability-admission] FAIL $name/$origin: valid admission" >&2
            failures=$((failures + 1))
        else
            echo "[capability-admission] PASS $name/$origin admission"
        fi
    done
    for origin in native public; do
      for backend in c llvm; do
        checks=$((checks + 1))
        stem="$name.$origin.$backend"
        command=("$PGY" "$source" "--backend=$backend" --opt=dev -o "$REL/$stem.exe")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        if ! env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
            timeout 45 "${command[@]}" >"$WORK/$stem.compile" 2>&1; then
            echo "[capability-admission] FAIL $stem: valid source rejected" >&2
            failures=$((failures + 1))
            continue
        fi
        if [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; then
            echo "[capability-admission] FAIL $stem: native fallback" >&2
            failures=$((failures + 1))
            continue
        fi
        if ! timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err"; then
            echo "[capability-admission] FAIL $stem: runtime failure" >&2
            failures=$((failures + 1))
            continue
        fi
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.run"
        printf 'ok\n' >"$WORK/$stem.expected"
        if [[ -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$stem.expected" "$WORK/$stem.run"; then
            echo "[capability-admission] FAIL $stem: result mismatch" >&2
            failures=$((failures + 1))
        else
            echo "[capability-admission] PASS $stem execution"
        fi
      done
    done
done

# Match and foreach hoists are owned, call-free synthetic tails, not lost uses.
source='tests/concept_semantics/authority_effect/capability_synthetic.pgy'
sha256sum "$source" >>"$WORK/sources.sha256"
for origin in native self; do
    checks=$((checks + 1))
    command=("$DRIVER" --emit-mir-json-verified "$source")
    [[ "$origin" == native ]] && command=("$PGY" --native-pipeline --mir-json "$source")
    if ! timeout 30 "${command[@]}" >"$WORK/synthetic.$origin.json" 2>"$WORK/synthetic.$origin.err" ||
        ! grep -Fq '"pgy.mir.v1"' "$WORK/synthetic.$origin.json"; then
        echo "[capability-admission] FAIL synthetic/$origin: valid body rejected" >&2
        failures=$((failures + 1))
    else
        echo "[capability-admission] PASS synthetic/$origin admission"
    fi
done

# Inspection intentionally reports used masks even for a violated bound.
# It must use the same inferred fact, not recompute a manifest-only analysis.
for name in manifest_clean manifest_declared_ok manifest_violation manifest_interproc \
    file_handle_read_violation file_handle_write_violation file_handle_dynamic_mode_violation; do
    checks=$((checks + 1))
    source="tests/capability/$name.pgy"
    sha256sum "$source" >>"$WORK/sources.sha256"
    native_status=0
    self_status=0
    timeout 30 "$PGY" --native-pipeline --capability-manifest "$source" \
        >"$WORK/$name.native.manifest" 2>"$WORK/$name.native.err" || native_status=$?
    timeout 30 "$DRIVER" --emit-capability-manifest-verified "$source" \
        >"$WORK/$name.self.manifest" 2>"$WORK/$name.self.err" || self_status=$?
    expected=1
    [[ "$name" == manifest_clean || "$name" == manifest_declared_ok ]] && expected=0
    for origin in native self; do
        awk 'seen || /^\{/ { seen=1; print }' "$WORK/$name.$origin.manifest" |
            tr -d '\r' >"$WORK/$name.$origin.json"
    done
    if [[ "$native_status" != "$expected" || "$self_status" != "$expected" ]] ||
        ! grep -Fq '"pgy.capability.manifest.v1"' "$WORK/$name.self.json" ||
        ! cmp -s "$WORK/$name.native.json" "$WORK/$name.self.json"; then
        echo "[capability-admission] FAIL $name: manifest status/mask parity" >&2
        failures=$((failures + 1))
    else
        echo "[capability-admission] PASS $name manifest"
    fi
done
forbidden='SemanticAstCapabilityFactsFromAdmittedBody('
checks=$((checks + 1))
if grep -Fq "$forbidden" src/self_hosted/compiler/capability_manifest_owner.pgy ||
    ! grep -Fq "$forbidden" src/self_hosted/semantic/ast_body_type_bundle_owner.pgy; then
    echo '[capability-admission] FAIL manifest-only inference returned' >&2
    failures=$((failures + 1))
fi
checks=$((checks + 1))
if ! grep -Fq 'ast_func_declared_capabilities(stmt)' src/semantic/type_checker_program.c; then
    echo '[capability-admission] FAIL forward signature lost declared capabilities' >&2
    failures=$((failures + 1))
fi
echo "[capability-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]

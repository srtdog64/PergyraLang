#!/usr/bin/env bash
# A Zone's synchronization/identity state cannot be copied into a spawn
# wrapper. Native and public source-MIR admission must reject before either
# C/LLVM publishes an artifact; ordinary scalar spawn remains admitted.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || {
    echo "[zone-spawn-transport] compiler or driver is missing" >&2
    exit 1
}

mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/zone-spawn-transport.XXXXXX")"
fail() { echo "[zone-spawn-transport] $*" >&2; exit 1; }

for shape in slot shared; do
  for mode in ref own; do
    if [[ "$shape" == shared ]]; then
        source_rel="tests/self_hosted/parity/fixture/zone_spawn_shared_${mode}_boundary_rejected.pgy"
    else
        source_rel="tests/self_hosted/parity/fixture/zone_spawn_${mode}_boundary_rejected.pgy"
    fi
    label="$shape.$mode"
    if [[ "$shape" == shared ]]; then
        (cd "$ROOT_DIR" && "$SELF_DRIVER" --ast "$source_rel") \
            >"$WORK_DIR/$label.ast" 2>"$WORK_DIR/$label.ast.err" ||
            fail "public self-host shared Zone declaration did not parse"
        grep -Fq 'Shared: count: Int = 0' "$WORK_DIR/$label.ast" ||
            fail "public self-host shared Zone field lost its AST fact"
    fi
    for backend in c llvm; do
        artifact="$WORK_DIR/$label.$backend"
        args=("$source_rel" --native-pipeline --error-format=json)
        if [[ "$backend" == c ]]; then
            artifact="$artifact.c"
            args+=(--emit-c -o "$(pgy_path_for_compiler "$PGY" "$artifact")")
        else
            artifact="$artifact.ll"
            args+=(--emit-llvm -o "$(pgy_path_for_compiler "$PGY" "$artifact")")
        fi
        if (cd "$ROOT_DIR" && "$PGY" "${args[@]}") \
            >"$WORK_DIR/$label.$backend.out" \
            2>"$WORK_DIR/$label.$backend.err"; then
            fail "native $backend accepted $label Zone transport"
        fi
        [[ ! -e "$artifact" ]] ||
            fail "native $backend published $label Zone transport"
        for fact in '"code":"PGY_SEM_BORROW_ESCAPE"' \
                    '"cause_ir":"semantic:borrow_escape"'; do
            grep -Fq "$fact" "$WORK_DIR/$label.$backend.err" ||
                fail "native $backend lost owned $label diagnostic: $fact"
        done
    done

    if (cd "$ROOT_DIR" && "$SELF_DRIVER" \
        --emit-mir-json-diagnostic-verified "$source_rel") \
        >"$WORK_DIR/$label.self.out" 2>"$WORK_DIR/$label.self.err"; then
        fail "public self-host MIR accepted $label Zone transport"
    fi
    [[ ! -s "$WORK_DIR/$label.self.err" ]] ||
        fail "public self-host $label rejection changed diagnostic channel"
    grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
        "$WORK_DIR/$label.self.out" ||
        fail "public self-host $label rejection is silent"
    if [[ "$mode" == ref ]]; then
        expected_code='"code":"PGY_SEM_BORROW_ESCAPE"'
        expected_cause='"cause_ir":"semantic:zone:spawn_transport"'
    else
        expected_code='"code":"PGY_SEM_ANCHORED_HANDLE_COPY"'
        expected_cause='"cause_ir":"semantic:zone:parameter_carriage"'
    fi
    grep -Fq "$expected_code" "$WORK_DIR/$label.self.out" ||
        fail "public self-host $label code drifted"
    grep -Fq "$expected_cause" "$WORK_DIR/$label.self.out" ||
        fail "public self-host $label cause drifted"
  done
done

scalar_rel="tests/cases/backend_compare/async_spawn_await/main.pgy"
(cd "$ROOT_DIR" && "$PGY" "$scalar_rel" --native-pipeline --mir) \
    >"$WORK_DIR/scalar.native.mir" 2>"$WORK_DIR/scalar.native.err" ||
    fail "native scalar spawn was over-rejected"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$scalar_rel") >"$WORK_DIR/scalar.self.mir" \
    2>"$WORK_DIR/scalar.self.err" ||
    fail "self-host scalar spawn was over-rejected"
[[ -s "$WORK_DIR/scalar.native.mir" &&
   -s "$WORK_DIR/scalar.self.mir" ]] ||
    fail "scalar spawn lost a MIR fact"

PROBE_BIN="${PGY_ZONE_SPAWN_PROBE_BIN:-$WORK_DIR/zone-probe}"
if [[ "$PGY" == *.exe ]]; then PROBE_BIN="${PROBE_BIN%.exe}.exe"; fi
if [[ -z "${PGY_ZONE_SPAWN_PROBE_BIN:-}" ]]; then
    PROBE_C="$WORK_DIR/zone-probe.c"
    (cd "$ROOT_DIR" && "$PGY" \
        tests/self_hosted/fixtures/zone_spawn_transport_admission_fact.pgy \
        --native-pipeline --emit-c \
        -o "$(pgy_path_for_compiler "$PGY" "$PROBE_C")") \
        >"$WORK_DIR/zone-probe.emit.out" \
        2>"$WORK_DIR/zone-probe.emit.err" ||
        fail "Pergyra Zone spawn owner probe did not emit C"
    "${CC:-gcc}" -x c -std=c11 -O2 -fwrapv -fno-strict-aliasing \
        -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
        "$PROBE_C" -o "$PROBE_BIN" ||
        fail "Pergyra Zone spawn owner probe did not compile"
fi
[[ -x "$PROBE_BIN" ]] || fail "Pergyra Zone spawn owner probe is missing"
for shape in slot shared; do
  for mode in ref own; do
    if [[ "$shape" == shared ]]; then
        source_rel="tests/self_hosted/parity/fixture/zone_spawn_shared_${mode}_boundary_rejected.pgy"
    else
        source_rel="tests/self_hosted/parity/fixture/zone_spawn_${mode}_boundary_rejected.pgy"
    fi
    if [[ "$mode" == ref ]]; then
        owned_code="zone_spawn_transport_unavailable"
        oracle_code="PGY_SEM_BORROW_ESCAPE"
        cause="semantic:zone:spawn_transport"
    else
        owned_code="zone_value_parameter_requires_transfer"
        oracle_code="PGY_SEM_ANCHORED_HANDLE_COPY"
        cause="semantic:zone:parameter_carriage"
    fi
    (cd "$ROOT_DIR" && "$PROBE_BIN" "$source_rel" "$owned_code" \
        "$oracle_code" "$cause") \
        >"$WORK_DIR/$shape.$mode.probe" ||
        fail "Pergyra owner verdict failed for $shape/$mode"
    grep -Fq 'zone spawn semantic verdict and public receipt verified' \
        "$WORK_DIR/$shape.$mode.probe" ||
        fail "Pergyra owner did not confirm $shape/$mode receipt"
  done
done
(cd "$ROOT_DIR" && "$PROBE_BIN" \
    tests/self_hosted/parity/fixture/zone_spawn_ref_boundary_rejected.pgy \
    missing-identity PGY_SEM_TYPE_MISMATCH semantic:ast:admission) \
    >"$WORK_DIR/missing-identity.probe" ||
    fail "missing spawn target identity was admitted"
grep -Fq 'missing spawn callable identity refused with owned receipt' \
    "$WORK_DIR/missing-identity.probe" ||
    fail "missing spawn target identity lost its owned receipt"

echo "[zone-spawn-transport] four Zone refusals/no artifacts, missing target refusal, and scalar spawn: PASS"

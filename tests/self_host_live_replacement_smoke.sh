#!/usr/bin/env bash
# The public pgy launcher must execute the shipped bounded DRV-2 binary rather
# than silently falling back to the C semantic/codegen pipeline.
# Contract: hard_self_MIR_is_graph_owned_and_matches_the_explicit_C_oracle_bridge

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/live_replacement}"
mkdir -p "$WORK_DIR"

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] && pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || { echo "[self-host-live] missing pgy: $PGY" >&2; exit 1; }
[[ -x "$SELF_DRIVER" ]] || { echo "[self-host-live] missing self driver: $SELF_DRIVER" >&2; exit 1; }
command -v "$CC" >/dev/null 2>&1 || { echo "[self-host-live] missing C compiler: $CC" >&2; exit 1; }

positive="src/self_hosted/semantic/fixture/valid_call_int.pgy"
negative="src/self_hosted/semantic/fixture/bad_return_type.pgy"
mir_source="src/self_hosted/mir_lower/fixture/let_log.pgy"
array_mir_source="src/self_hosted/codegen/fixture/array_return_literal.pgy"
try_mir_source="src/self_hosted/codegen/fixture/option_try.pgy"
struct_mir_source="src/self_hosted/codegen/fixture/struct_point.pgy"
generic_mir_source="src/self_hosted/mir_lower/fixture/generic_struct_field_value_flow.pgy"
generic_multi_mir_source="src/self_hosted/tools/generic_return_probe/explicit_ok.pgy"
generic_member_mir_source="src/self_hosted/mir_lower/fixture/generic_member_inferred_flow.pgy"
generic_vessel_member_mir_source="src/self_hosted/mir_lower/fixture/generic_vessel_member_inferred_flow.pgy"
generic_constructed_member_mir_source="src/self_hosted/mir_lower/fixture/generic_member_constructed_return_flow.pgy"
generic_array_member_mir_source="src/self_hosted/mir_lower/fixture/generic_member_array_return_flow.pgy"
generic_record_array_member_mir_source="src/self_hosted/mir_lower/fixture/generic_member_record_array_return_flow.pgy"
cast_mir_source="src/self_hosted/mir_lower/fixture/cast_numeric.pgy"
intent_typed_mir_source="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
bad_mir="src/self_hosted/mir_lower/fixture/invalid_schema.json"

(cd "$ROOT_DIR" && "$SELF_DRIVER" "$positive" --emit-c-verified) >"$WORK_DIR/direct.c"
(cd "$ROOT_DIR" && "$PGY" --self-driver "$positive") >"$WORK_DIR/launcher.c"
cmp -s "$WORK_DIR/direct.c" "$WORK_DIR/launcher.c" || {
    echo "[self-host-live] launcher C artifact differs from direct DRV-2" >&2
    exit 1
}
"$CC" -x c -std=c11 "$WORK_DIR/launcher.c" -o "$WORK_DIR/launcher-program" \
    >"$WORK_DIR/cc.log" 2>&1 || {
        cat "$WORK_DIR/cc.log" >&2
        exit 1
    }

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$negative" --emit-c-verified) >"$WORK_DIR/direct.diag" 2>"$WORK_DIR/direct.err"
direct_rc=$?
(cd "$ROOT_DIR" && "$PGY" --self-driver "$negative") >"$WORK_DIR/launcher.diag" 2>"$WORK_DIR/launcher.err"
launcher_rc=$?
set -e
[[ "$direct_rc" -ne 0 && "$launcher_rc" -eq "$direct_rc" ]] || {
    echo "[self-host-live] negative exit code drift: direct=$direct_rc launcher=$launcher_rc" >&2
    exit 1
}
tr -d '\r' <"$WORK_DIR/direct.diag" >"$WORK_DIR/direct.norm"
tr -d '\r' <"$WORK_DIR/launcher.diag" >"$WORK_DIR/launcher.norm"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/launcher.norm" || {
    echo "[self-host-live] launcher diagnostic differs from direct DRV-2" >&2
    exit 1
}

check_live_mir_source() {
    local source="$1"
    local label="$2"
    local live_mir="$WORK_DIR/$label.live.mir.json"
    local direct_self="$WORK_DIR/$label.direct-self.mir.json"
    local launcher_self="$WORK_DIR/$label.launcher-self.mir.json"
    local live_arg
    local self_arg

    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
        "$source" 2>/dev/null) \
        | tr -d '\r' >"$live_mir"
    grep -Fq '"schema":"pgy.mir.v1"' "$live_mir" || {
        echo "[self-host-live] $label: C oracle did not produce MIR" >&2
        exit 1
    }
    live_arg="$(pgy_selfhost_path_relative_to_root "$live_mir")"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-verified "$source") \
        | tr -d '\r' >"$direct_self"
    (cd "$ROOT_DIR" && "$PGY" --self-driver \
        --emit-mir-json-verified "$source") \
        | tr -d '\r' >"$launcher_self"
    cmp -s "$direct_self" "$launcher_self" || {
        echo "[self-host-live] $label: launcher MIR producer drift" >&2
        exit 1
    }
    if grep -Fq '"ast":' "$launcher_self"; then
        echo "[self-host-live] $label: self MIR emitted AST text" >&2
        exit 1
    fi
    self_arg="$(pgy_selfhost_path_relative_to_root "$launcher_self")"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" \
        --canonicalize-oracle-mir-json "$live_arg") \
        | tr -d '\r' >"$WORK_DIR/$label.oracle.canonical.mir.json"
    (cd "$ROOT_DIR" && "$PGY" --self-driver \
        --canonicalize-mir-json "$self_arg") \
        | tr -d '\r' >"$WORK_DIR/$label.self.canonical.mir.json"
    cmp -s "$WORK_DIR/$label.oracle.canonical.mir.json" \
        "$WORK_DIR/$label.self.canonical.mir.json" || {
            echo "[self-host-live] $label: canonical MIR fact drift" >&2
            exit 1
        }
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --mir-json "$self_arg") \
        >"$WORK_DIR/$label.direct-mir.c"
    (cd "$ROOT_DIR" && "$PGY" --self-driver --mir-json "$self_arg") \
        >"$WORK_DIR/$label.launcher-mir.c"
    cmp -s "$WORK_DIR/$label.direct-mir.c" \
        "$WORK_DIR/$label.launcher-mir.c" || {
            echo "[self-host-live] $label: launcher C artifact drift" >&2
            exit 1
        }
    "$CC" -x c -std=c11 -I"$ROOT_DIR/src/runtime" \
        "$WORK_DIR/$label.launcher-mir.c" \
        -o "$WORK_DIR/$label.launcher-program" \
        >"$WORK_DIR/$label.cc.log" 2>&1 || {
            cat "$WORK_DIR/$label.cc.log" >&2
            exit 1
        }
    (cd "$ROOT_DIR" && "$PGY" "$source" --backend=c \
        -o "$(pgy_selfhost_path_relative_to_root "$WORK_DIR/$label.oracle-program")" \
        >"$WORK_DIR/$label.oracle.compile.log" 2>&1)
    "$WORK_DIR/$label.launcher-program" | tr -d '\r' \
        >"$WORK_DIR/$label.launcher.run"
    "$WORK_DIR/$label.oracle-program" | tr -d '\r' \
        >"$WORK_DIR/$label.oracle.run"
    cmp -s "$WORK_DIR/$label.oracle.run" "$WORK_DIR/$label.launcher.run" || {
        echo "[self-host-live] $label: integrated MIR run output differs from C oracle" >&2
        exit 1
    }
}

check_live_mir_source "$mir_source" "let-log"
check_live_mir_source "$array_mir_source" "array-return-literal"
check_live_mir_source "$try_mir_source" "option-try"
check_live_mir_source "$struct_mir_source" "struct-point"
check_live_mir_source "$generic_mir_source" "generic-struct-field"
check_live_mir_source "$generic_multi_mir_source" "generic-multi-actual"
check_live_mir_source "$generic_member_mir_source" "generic-member-inferred"
check_live_mir_source "$generic_vessel_member_mir_source" "generic-vessel-member-inferred"
check_live_mir_source "$generic_constructed_member_mir_source" "generic-member-constructed-return"
check_live_mir_source "$generic_array_member_mir_source" "generic-member-array-return"
check_live_mir_source "$generic_record_array_member_mir_source" "generic-member-record-array-return"
check_live_mir_source "$cast_mir_source" "cast-numeric"
check_live_mir_source "$intent_typed_mir_source" "intent-typed-outcome"

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --mir-json "$bad_mir") \
    >"$WORK_DIR/direct-bad-mir.out" 2>"$WORK_DIR/direct-bad-mir.err"
direct_bad_mir_rc=$?
(cd "$ROOT_DIR" && "$PGY" --self-driver --mir-json "$bad_mir") \
    >"$WORK_DIR/launcher-bad-mir.out" 2>"$WORK_DIR/launcher-bad-mir.err"
launcher_bad_mir_rc=$?
set -e
[[ "$direct_bad_mir_rc" -ne 0 && "$launcher_bad_mir_rc" -eq "$direct_bad_mir_rc" ]] || {
    echo "[self-host-live] bad MIR exit drift: direct=$direct_bad_mir_rc launcher=$launcher_bad_mir_rc" >&2
    exit 1
}
tr -d '\r' <"$WORK_DIR/direct-bad-mir.out" >"$WORK_DIR/direct-bad-mir.norm"
tr -d '\r' <"$WORK_DIR/launcher-bad-mir.out" >"$WORK_DIR/launcher-bad-mir.norm"
cmp -s "$WORK_DIR/direct-bad-mir.norm" "$WORK_DIR/launcher-bad-mir.norm" || {
    echo "[self-host-live] bad MIR diagnostic differs from direct DRV-2" >&2
    exit 1
}
grep -Fq "MIR-LOWER ERROR" "$WORK_DIR/launcher-bad-mir.norm" || {
    echo "[self-host-live] bad MIR did not fail through MIR owner" >&2
    exit 1
}

set +e
PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-driver" "$PGY" --self-driver "$positive" \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || {
    echo "[self-host-live] missing self driver silently fell back" >&2
    exit 1
}
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" || {
    echo "[self-host-live] missing-driver failure is not explicit" >&2
    exit 1
}

echo "[self-host-live] producer-first DRV-2 source/MIR replacement paths are artifact-equal and fail-closed"

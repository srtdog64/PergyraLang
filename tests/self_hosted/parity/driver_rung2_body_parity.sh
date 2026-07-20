#!/usr/bin/env bash
# DRV-2 parity: artifact-body semantic evidence is mandatory before C emission.
# native_MIR_JSON_rows_and_C_LLVM_forloop_foreach_rows is the registry witness.
# source_iteration_type_rescan and backend_iteration_type_guess are forbidden:
# both backend legs consume the carried iteration rows and negative mutations.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_resource_runtime_abi_negative_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_array_set_graph_negative_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_try_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_continue_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_index_expression_type_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_call_target_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_integer_literal_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_long_literal_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_bool_literal_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_string_literal_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_iteration_graph_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_foreach_call_type_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_enum_argument_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_enum_return_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_array_argument_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_struct_argument_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_struct_value_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_struct_value_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_inferred_generic_value_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_member_specialization_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_option_struct_value_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_mutation_graph_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_array_literal_graph_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_assign_instruction_graph_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_match_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_loop_phi_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_destructure_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_defer_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_else_if_graph_parity_owner.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:driver-rung2] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:driver-rung2" "$PGY"

CC="${CC:-cc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-parity:driver-rung2] missing C compiler: $CC" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_rung2}"
DRIVER_PATHS="$BUILD_DIR/driver_paths.txt"
SEMANTIC_PATHS="$BUILD_DIR/semantic_paths.txt"
FIXTURE_ROWS="$BUILD_DIR/fixture_rows.txt"
MIR_FIXTURE_ROWS="$BUILD_DIR/mir_fixture_rows.txt"
mkdir -p "$BUILD_DIR"
rm -f "$BUILD_DIR"/*.baseline.c

pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:driver-rung2" "$BUILD_DIR/manifest" \
    "driver-rung2-paths" "$DRIVER_PATHS"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:driver-rung2" "$BUILD_DIR/manifest" \
    "semantic-parity-paths" "$SEMANTIC_PATHS"

driver_paths=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && driver_paths+=("$line")
done <"$DRIVER_PATHS"
semantic_paths=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && semantic_paths+=("$line")
done <"$SEMANTIC_PATHS"
if [[ "${#driver_paths[@]}" -ne 1 || "${#semantic_paths[@]}" -ne 7 ]]; then
    echo "[self-host-parity:driver-rung2] TestHarness path cardinality mismatch" >&2
    exit 1
fi

DRIVER_SOURCE="$ROOT_DIR/${driver_paths[0]}"
FIXTURE_DIR="$ROOT_DIR/${semantic_paths[2]}"
EXPECTED_DIR="$ROOT_DIR/${semantic_paths[3]}"
for path in "$DRIVER_SOURCE" "$FIXTURE_DIR" "$EXPECTED_DIR"; do
    [[ -e "$path" ]] || {
        echo "[self-host-parity:driver-rung2] missing TestHarness input: $path" >&2
        exit 1
    }
done

compile_driver() {
    local backend="$1"
    local out_bin="$2"
    local log="$BUILD_DIR/driver_${backend}.compile.log"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$DRIVER_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" \
        >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[self-host-parity:driver-rung2] backend=$backend driver compile failed" >&2
        cat "$log" >&2
        return 1
    fi
}

C_DRIVER="$BUILD_DIR/driver_c.exe"
compile_driver c "$C_DRIVER"
if ! (cd "$ROOT_DIR" && "$C_DRIVER" --fixture-manifest >"$FIXTURE_ROWS"); then
    echo "[self-host-parity:driver-rung2] fixture manifest emission failed" >&2
    exit 1
fi
fixture_rows=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && fixture_rows+=("$line")
done <"$FIXTURE_ROWS"
if [[ "${#fixture_rows[@]}" -ne 20 ]]; then
    echo "[self-host-parity:driver-rung2] fixture count drifted: ${#fixture_rows[@]} != 20" >&2
    exit 1
fi
if ! (cd "$ROOT_DIR" && "$C_DRIVER" --mir-fixture-manifest \
    >"$MIR_FIXTURE_ROWS"); then
    echo "[self-host-parity:driver-rung2] MIR fixture manifest emission failed" >&2
    exit 1
fi
mir_fixture_rows=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && mir_fixture_rows+=("$line")
done <"$MIR_FIXTURE_ROWS"
if [[ "${#mir_fixture_rows[@]}" -ne 80 ]]; then
    echo "[self-host-parity:driver-rung2] MIR fixture count drifted: ${#mir_fixture_rows[@]} != 80" >&2
    exit 1
fi
MIR_FIXTURE_FILTER="${PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER:-}"
if [[ -n "$MIR_FIXTURE_FILTER" ]]; then
    filtered_mir_fixture_rows=()
    mir_fixture_filters=()
    IFS=',' read -r -a mir_fixture_filters <<<"$MIR_FIXTURE_FILTER"
    for wanted_fixture in "${mir_fixture_filters[@]}"; do
        matched_fixture=""
        for fixture_rel in "${mir_fixture_rows[@]}"; do
            if [[ "$(pgy_selfhost_driver_rung2_fixture_base \
                "$fixture_rel")" == "$wanted_fixture" ]]; then
                matched_fixture="$fixture_rel"
                break
            fi
        done
        if [[ -z "$wanted_fixture" || -z "$matched_fixture" ]]; then
            echo "[self-host-parity:driver-rung2] MIR fixture filter did not select a row: $wanted_fixture" >&2
            exit 1
        fi
        filtered_mir_fixture_rows+=("$matched_fixture")
    done
    mir_fixture_rows=("${filtered_mir_fixture_rows[@]}")
fi

pgy_selfhost_prepare_driver_rung2_mir_oracles

BACKENDS="${PGY_SELFHOST_DRIVER_BACKENDS:-c llvm}"
ran=0
for backend in $BACKENDS; do
    DRIVER_BIN="$BUILD_DIR/driver_${backend}.exe"
    if [[ "$backend" != "c" ]]; then
        set +e
        compile_driver "$backend" "$DRIVER_BIN"
        compile_rc=$?
        set -e
        if [[ "$compile_rc" -eq 2 ]]; then
            echo "[self-host-parity:driver-rung2] LLVM unavailable; skipping llvm-built driver"
            continue
        fi
        [[ "$compile_rc" -eq 0 ]] || exit "$compile_rc"
    fi

    for row in "${fixture_rows[@]}"; do
        IFS='|' read -r base status <<<"$row"
        fixture_rel="${semantic_paths[2]}/$base.pgy"
        fixture_abs="$ROOT_DIR/$fixture_rel"
        actual="$BUILD_DIR/${base}_${backend}.out"
        err="$BUILD_DIR/${base}_${backend}.err"
        [[ -f "$fixture_abs" ]] || {
            echo "[self-host-parity:driver-rung2] missing fixture: $fixture_rel" >&2
            exit 1
        }
        set +e
        (cd "$ROOT_DIR" && "$DRIVER_BIN" "$fixture_rel" --emit-c-verified \
            >"$actual.raw" 2>"$err")
        rc=$?
        set -e
        tr -d '\r' <"$actual.raw" >"$actual"
        rm -f "$actual.raw"

        if [[ "$status" == "ok" ]]; then
            if [[ "$rc" -ne 0 ]]; then
                echo "[self-host-parity:driver-rung2] $backend positive failed: $base rc=$rc" >&2
                cat "$actual" "$err" >&2
                exit 1
            fi
            if ! "$CC" -x c -std=c11 "$actual" -o "$BUILD_DIR/${base}_${backend}.program.exe" \
                >"$BUILD_DIR/${base}_${backend}.cc.log" 2>&1; then
                echo "[self-host-parity:driver-rung2] emitted C compile failed: $backend/$base" >&2
                cat "$BUILD_DIR/${base}_${backend}.cc.log" >&2
                exit 1
            fi
            baseline="$BUILD_DIR/${base}.baseline.c"
            if [[ ! -f "$baseline" ]]; then
                cp "$actual" "$baseline"
            else
                pgy_selfhost_compare_expected_text_artifact_file_with_owner \
                    "driver-rung2:$backend:$base" "$BUILD_DIR" \
                    "$baseline" "$actual" "emitted_c"
            fi
        else
            if [[ "$rc" -eq 0 ]]; then
                echo "[self-host-parity:driver-rung2] $backend negative accepted: $base" >&2
                exit 1
            fi
            expected="$EXPECTED_DIR/$base.diag"
            pgy_selfhost_compare_expected_text_artifact_file_with_owner \
                "driver-rung2:$backend:$base" "$BUILD_DIR" \
                "$expected" "$actual" "diagnostics"
        fi
    done
    pgy_selfhost_run_driver_rung2_mir_producer_parity "$backend" "$DRIVER_BIN"
    ran=$((ran + 1))
done

if [[ "$ran" -eq 0 ]]; then
    echo "[self-host-parity:driver-rung2] no backend ran" >&2
    exit 1
fi
echo "[self-host-parity:driver-rung2] producer-first source/MIR parity ok: backends=$ran body_fixtures=${#fixture_rows[@]} mir_fixtures=${#mir_fixture_rows[@]}"

#!/usr/bin/env bash
# DRV-2 parity: artifact-body semantic evidence is mandatory before C emission.
# native_MIR_JSON_rows_and_C_LLVM_forloop_foreach_rows is the registry witness.
# source_iteration_type_rescan, backend_iteration_type_guess,
# source_local_type_as_iteration_authority, and mir_foreach_collection_type_guess
# are forbidden: all consumers use carried iteration rows and negative gates.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_operator_kind_negative_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_defer_graph_negative_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_resource_runtime_abi_negative_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_array_set_graph_negative_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_try_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_continue_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_pipeline_step_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_canonical_declaration_order_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_action_contract_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_effect_declaration_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_domain_graph_producer_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_domain_topology_producer_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_assignment_binding_mode_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_callable_receiver_carriage_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_index_expression_type_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_call_target_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_recursive_call_target_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_integer_literal_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_long_literal_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_bool_literal_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_string_literal_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_iteration_graph_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_foreach_call_type_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_enum_argument_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_enum_return_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_class_enum_composition_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_nominal_builtin_collision_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_class_array_composition_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_enum_match_loop_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_option_coalesce_loop_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_coalesce_bool_loop_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_nested_coalesce_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_result_field_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_string_concat_alias_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_fieldless_class_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_spawn_await_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_spawn_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_string_spawn_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_spawn_mixed_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_default_contract_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_ability_bind_dispatch_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_multi_bound_defaults_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_nested_generic_containers_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_list_ops_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_list_int_loop_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_for_in_list_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_list_push_scalar_value_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_list_shadow_scope_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_list_literal_context_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_queue_ops_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_set_ops_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_set_literal_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_set_index_value_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_iteration_expression_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_array_argument_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_struct_argument_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_struct_value_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_struct_value_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_inferred_generic_value_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_member_specialization_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_option_struct_value_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_mutation_graph_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_array_literal_graph_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_assign_instruction_graph_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_match_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_wrapper_match_loop_phi_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_loop_phi_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_destructure_parity_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_defer_parity_owner.sh"; source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_else_if_graph_parity_owner.sh"
pgy_prepend_windows_runtime_paths

MIR_FIXTURE_FILTER="${PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER:-}"
if [[ -z "$MIR_FIXTURE_FILTER" ]] && command -v cygpath >/dev/null 2>&1; then
    driver_shell_command="$(command -v bash 2>/dev/null || true)"
    driver_shell_native="$(cygpath -am "$driver_shell_command" 2>/dev/null || true)"
    case "$driver_shell_native" in
        *"/Git/"*"/bash.exe")
            echo "[self-host-parity:driver-rung2] full matrix requires MSYS2 bash; Git Bash can orphan the long-running worker" >&2
            echo "[self-host-parity:driver-rung2] use PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER for a focused Git Bash gate" >&2
            exit 1
            ;;
    esac
fi

if [[ "$(pgy_selfhost_driver_rung2_fixture_base \
    tests/cases/backend_compare/class_as_strategy/main.pgy)" != \
    "class_as_strategy" ]]; then
    echo "[self-host-parity:driver-rung2] backend-compare fixture identity owner drifted" >&2
    exit 1
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:driver-rung2] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:driver-rung2" "$PGY"

PREBUILT_DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
if [[ -n "$PREBUILT_DRIVER" ]]; then
    PREBUILT_DRIVER="$(pgy_select_optional_exe_binary "$PREBUILT_DRIVER")"
    pgy_require_runnable_binary_here \
        "self-host-parity:driver-rung2:hard" "$PREBUILT_DRIVER" || exit 1
fi

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
    local source="${3:-$DRIVER_SOURCE}"
    local log="$BUILD_DIR/driver_${backend}.compile.log"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
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
MANIFEST_DRIVER="$C_DRIVER"
if [[ -n "$PREBUILT_DRIVER" ]]; then
    C_DRIVER="$PREBUILT_DRIVER"
    MANIFEST_DRIVER="$BUILD_DIR/driver_fixture_manifest.exe"
    compile_driver c "$MANIFEST_DRIVER" \
        "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_fixture_manifest_main.pgy"
else
    compile_driver c "$C_DRIVER"
fi
if ! (cd "$ROOT_DIR" && "$MANIFEST_DRIVER" --fixture-manifest >"$FIXTURE_ROWS"); then
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
if ! (cd "$ROOT_DIR" && "$MANIFEST_DRIVER" --mir-fixture-manifest \
    >"$MIR_FIXTURE_ROWS"); then
    echo "[self-host-parity:driver-rung2] MIR fixture manifest emission failed" >&2
    exit 1
fi
mir_fixture_rows=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && mir_fixture_rows+=("$line")
done <"$MIR_FIXTURE_ROWS"
if [[ "${#mir_fixture_rows[@]}" -ne 282 ]]; then
    echo "[self-host-parity:driver-rung2] MIR fixture count drifted: ${#mir_fixture_rows[@]} != 282" >&2
    exit 1
fi
if [[ -n "$MIR_FIXTURE_FILTER" ]]; then
    filtered_mir_fixture_rows=()
    mir_fixture_filters=()
    mir_fixture_bases=()
    mir_fixture_rows_by_base=()
    for fixture_rel in "${mir_fixture_rows[@]}"; do
        fixture_base=""
        pgy_selfhost_driver_rung2_fixture_base "$fixture_rel" fixture_base
        fixture_base_duplicate=0
        for existing_fixture_base in "${mir_fixture_bases[@]}"; do
            if [[ "$existing_fixture_base" == "$fixture_base" ]]; then
                fixture_base_duplicate=1
                break
            fi
        done
        if [[ -z "$fixture_base" || "$fixture_base_duplicate" -ne 0 ]]; then
            echo "[self-host-parity:driver-rung2] MIR fixture base is empty or duplicated: $fixture_base" >&2
            exit 1
        fi
        mir_fixture_bases+=("$fixture_base")
        mir_fixture_rows_by_base+=("$fixture_rel")
    done
    IFS=',' read -r -a mir_fixture_filters <<<"$MIR_FIXTURE_FILTER"
    for wanted_fixture in "${mir_fixture_filters[@]}"; do
        selected_fixture_row=""
        fixture_index=0
        while [[ "$fixture_index" -lt "${#mir_fixture_bases[@]}" ]]; do
            if [[ "${mir_fixture_bases[$fixture_index]}" == "$wanted_fixture" ]]; then
                selected_fixture_row="${mir_fixture_rows_by_base[$fixture_index]}"
                break
            fi
            fixture_index=$((fixture_index + 1))
        done
        if [[ -z "$wanted_fixture" || -z "$selected_fixture_row" ]]; then
            echo "[self-host-parity:driver-rung2] MIR fixture filter did not select a row: $wanted_fixture" >&2
            exit 1
        fi
        filtered_mir_fixture_rows+=("$selected_fixture_row")
    done
    mir_fixture_rows=("${filtered_mir_fixture_rows[@]}")
    # A MIR fixture filter owns a focused producer/consumer gate.  The
    # semantic fixture matrix is independent and must not prevent that focused
    # gate from reaching its selected MIR row.
    fixture_rows=()
fi

pgy_selfhost_prepare_driver_rung2_mir_oracles

BACKENDS="${PGY_SELFHOST_DRIVER_BACKENDS:-c llvm}"
ran=0
for backend in $BACKENDS; do
    DRIVER_BIN="$BUILD_DIR/driver_${backend}.exe"
    if [[ "$backend" == "hard" ]]; then
        if [[ -z "$PREBUILT_DRIVER" ]]; then
            echo "[self-host-parity:driver-rung2] hard lane has no Pergyra-built driver" >&2
            exit 1
        fi
        DRIVER_BIN="$PREBUILT_DRIVER"
    elif [[ "$backend" != "c" ]]; then
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
            if ! pgy_selfhost_driver_rung2_compile_emitted 0 "$actual" \
                "$BUILD_DIR/${base}_${backend}.program.exe" \
                "$BUILD_DIR/${base}_${backend}.cc.log"; then
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

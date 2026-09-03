#!/usr/bin/env bash
pgy_selfhost_prepare_driver_rung2_mir_oracles() {
    local fixture_rel fixture_abs base mir_json oracle_bin
    pgy_selfhost_driver_rung2_machine_manifest_init
    for fixture_rel in "${mir_fixture_rows[@]}"; do
        fixture_abs="$ROOT_DIR/$fixture_rel"
        base="$(pgy_selfhost_driver_rung2_fixture_base "$fixture_rel")"
        mir_json="$BUILD_DIR/${base}.mir.json"
        oracle_bin="$BUILD_DIR/${base}.oracle.exe"
        [[ -f "$fixture_abs" ]] || {
            echo "[self-host-parity:driver-rung2] missing MIR fixture: $fixture_rel" >&2
            exit 1
        }
        (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
            "$(pgy_path_for_compiler "$PGY" "$fixture_abs")" 2>/dev/null) \
            | tr -d '\r' >"$mir_json"
        grep -Fq '"schema":"pgy.mir.v1"' "$mir_json" || {
            echo "[self-host-parity:driver-rung2] missing MIR schema: $fixture_rel" >&2
            exit 1
        }
        if ! (cd "$ROOT_DIR" && "$PGY" \
            "$(pgy_path_for_compiler "$PGY" "$fixture_abs")" --backend=c \
            --native-pipeline -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
            >"$BUILD_DIR/${base}.oracle.compile.log" 2>&1); then
            echo "[self-host-parity:driver-rung2] C oracle compile failed: $fixture_rel" >&2
            cat "$BUILD_DIR/${base}.oracle.compile.log" >&2
            exit 1
        fi
        "$oracle_bin" >"$BUILD_DIR/${base}.oracle.run.raw"
        tr -d '\r' <"$BUILD_DIR/${base}.oracle.run.raw" \
            >"$BUILD_DIR/${base}.oracle.run"
    done; }
pgy_selfhost_run_driver_rung2_mir_producer_parity() {
    local backend="$1" driver_bin="$2" fixture_rel base mir_json mir_json_arg self_mir_json self_mir_json_arg oracle_canonical
    local oracle_canonical_arg self_canonical self_canonical_arg oracle_canonical_mode canonical_consume_arg materialized_match_delta actual err self_actual source_actual mir_baseline bare_call_missing_graph machine_fixture
    for fixture_rel in "${mir_fixture_rows[@]}"; do
        base="$(pgy_selfhost_driver_rung2_fixture_base "$fixture_rel")"
        machine_fixture=0
        if pgy_selfhost_driver_rung2_is_machine_fixture "$fixture_rel"; then
            machine_fixture=1
        fi
        mir_json="$BUILD_DIR/${base}.mir.json"
        mir_json_arg="$(pgy_selfhost_path_relative_to_root "$mir_json")"
        self_mir_json="$BUILD_DIR/${base}_${backend}.self.mir.json"
        self_mir_json_arg="$(pgy_selfhost_path_relative_to_root "$self_mir_json")"
        oracle_canonical="$BUILD_DIR/${base}_${backend}.oracle.canonical.mir.json"
        oracle_canonical_arg="$(pgy_selfhost_path_relative_to_root "$oracle_canonical")"
        self_canonical="$BUILD_DIR/${base}_${backend}.self.canonical.mir.json"
        self_canonical_arg="$(pgy_selfhost_path_relative_to_root "$self_canonical")"
        actual="$BUILD_DIR/${base}_${backend}.mir.c"
        err="$BUILD_DIR/${base}_${backend}.mir.err"
        if ! pgy_selfhost_driver_rung2_produce_self_mir \
            "$machine_fixture" "$backend" "$base" "$driver_bin" \
            "$fixture_rel" "$self_mir_json"; then
            echo "[self-host-parity:driver-rung2] $backend MIR producer failed: $base" >&2
            cat "$self_mir_json.raw" "$self_mir_json.err" >&2
            exit 1
        fi
        tr -d '\r' <"$self_mir_json.raw" >"$self_mir_json"
        rm -f "$self_mir_json.raw"
        grep -Fq '"schema":"pgy.mir.v1"' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend self MIR schema missing: $base" >&2
            exit 1
        }
        if grep -Fq '"ast":' "$self_mir_json"; then
            echo "[self-host-parity:driver-rung2] $backend self MIR reopened AST compatibility text: $base" >&2
            exit 1
        fi
        pgy_selfhost_verify_driver_rung2_action_contract "$backend" "$base" "$mir_json" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_effect_declaration "$backend" "$base" "$mir_json" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_domain_graph_producer "$backend" "$base" "$mir_json" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_domain_topology_producer "$backend" "$base" "$mir_json" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_callable_receiver_carriage "$backend" "$base" "$self_mir_json" "$driver_bin"; if [[ "$base" == "zone_layer_projection_runtime" ]]; then continue; fi
        pgy_selfhost_verify_driver_rung2_machine_facts \
            "$machine_fixture" "$backend" "$base" "$self_mir_json"
        pgy_selfhost_verify_driver_rung2_resource_runtime_abi_negative "$machine_fixture" "$backend" "$base" "$self_mir_json" "$driver_bin" "$DRIVER_RUNG2_MACHINE_MANIFEST_REL"
        pgy_selfhost_verify_driver_rung2_abi_layout_negative "$machine_fixture" "$backend" "$base" "$self_mir_json" "$driver_bin" "$DRIVER_RUNG2_MACHINE_MANIFEST_REL"
        pgy_selfhost_verify_driver_rung2_target_projection_negative "$machine_fixture" "$backend" "$base" "$self_mir_json" "$driver_bin" "$DRIVER_RUNG2_MACHINE_MANIFEST_REL"
        if [[ "$base" == "forloop" ]]; then
            grep -Fq '"loop_flow_summary_count":1' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend loop summary owner row was lost" >&2
                exit 1
            }
            grep -Eq '"loop_syntax_id":[1-9][0-9]*,"kind":"for","effect_base":0,"effect_delta":0,"flags":1,"entry_state_start":0,"entry_state_count":0,"exit_state_start":0,"exit_state_count":0' \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend loop summary owner row drifted" >&2
                exit 1
            }
        fi
        pgy_selfhost_verify_driver_rung2_call_target "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_iteration_graph "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_long_literal "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_bool_literal "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_string_literal "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_foreach_call_type "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_enum_argument "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_enum_return "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_class_enum_composition "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_nominal_builtin_collision "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_class_array_composition "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_collection_enum_match_loop "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_collection_option_coalesce_loop "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_coalesce_bool_loop "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_nested_coalesce "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_result_field "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_string_concat_alias "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_fieldless_class "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_spawn_await "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_generic_spawn "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_generic_string_spawn "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_generic_spawn_mixed "$backend" "$base" "$self_mir_json"; pgy_selfhost_verify_driver_rung2_generic_default_contract "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_ability_bind_dispatch \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_generic_multi_bound_defaults \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_nested_generic_containers "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_list_ops "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_list_int_loop "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_for_in_list "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_list_push_scalar_value "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_list_shadow_scope "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_list_literal_context "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_queue_ops "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_set_ops "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_set_literal "$backend" "$base" "$self_mir_json" "$driver_bin"; pgy_selfhost_verify_driver_rung2_set_index_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_iteration_expression "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_array_argument "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_struct_argument "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_struct_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_generic_struct_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_inferred_generic_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_generic_member_specialization "$backend" "$base" "$self_mir_json" "$mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_option_struct_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_collection_mutation_graph "$backend" "$base" "$self_mir_json"
        pgy_selfhost_verify_driver_rung2_array_literal_graph "$backend" "$base" "$self_mir_json"
        pgy_selfhost_verify_driver_rung2_indexed_assignment \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_assignment_binding_mode \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_index_expression_type \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        if [[ "$base" == "if_else_assign" ]]; then
            grep -Fq '"kind":"phi"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend branch phi fact was lost" >&2
                exit 1
            }
            grep -Fq '"uses":["value.3","value.4"]' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend branch incoming versions drifted" >&2
                exit 1
            }
        fi
        pgy_selfhost_verify_driver_rung2_match \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_wrapper_match_loop_phi "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_loop_phi \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_continue \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_destructure \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_else_if_graph \
            "$backend" "$base" "$self_mir_json"
        if [[ "$base" == "param_carriage" ]]; then
            grep -Eq '"name":"pair","source_syntax_id":[1-9][0-9]*,"type":"Pair","carriage":"readonly-ref","resource":"none","pass":"indirect"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend readonly-ref aggregate ABI fact drifted" >&2
                exit 1
            }
            grep -Eq '"name":"value","source_syntax_id":[1-9][0-9]*,"type":"Int","carriage":"value-result","resource":"none","pass":"direct"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend value-result ABI fact drifted" >&2
                exit 1
            }
            grep -Eq '"name":"values","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"owner-handle","resource":"none","pass":"direct"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend owner-handle ABI fact drifted" >&2
                exit 1
            }
            grep -Fq '"expr0":"Mutate(value)","expr0_graph":{' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend bare-call expression graph was lost" >&2
                exit 1
            }
            bare_call_missing_graph="$BUILD_DIR/${base}_${backend}.bare-call-missing-graph.mir.json"
            sed 's/"expr0":"Mutate(value)","expr0_graph"/"expr0":"Mutate(value)","expr0_graph_removed"/g' \
                "$self_mir_json" >"$bare_call_missing_graph"
            if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
                "$(pgy_selfhost_path_relative_to_root "$bare_call_missing_graph")" \
                >"$bare_call_missing_graph.out" 2>"$bare_call_missing_graph.err"); then
                echo "[self-host-parity:driver-rung2] $backend bare-call missing expression graph was accepted" >&2
                exit 1
            fi
            grep -Fq "MIR instruction expression graph is missing or invalid" \
                "$bare_call_missing_graph.err" "$bare_call_missing_graph.out" || {
                echo "[self-host-parity:driver-rung2] $backend bare-call missing graph diagnostic drifted" >&2
                cat "$bare_call_missing_graph.out" "$bare_call_missing_graph.err" >&2
                exit 1
            }
        fi
        if [[ "$base" == "pipe_carriage" ]]; then
            grep -Fq '"expr0":"Add(Double(5), 3)","expr0_graph":{"root":8' \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend pipe expression collapsed out of the call graph" >&2
                exit 1
            }
            grep -Fq '"kind":"call_argument","text":"Double(5)"' \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend inner pipe call spine was lost" >&2
                exit 1
            }
        fi
        pgy_selfhost_verify_driver_rung2_try_graph \
            "$backend" "$base" "$self_mir_json" "self-MIR"
        pgy_selfhost_verify_driver_rung2_defer \
            "$backend" "$base" "$self_mir_json"
        if [[ "$base" == "nested_member_access" ]]; then
            for nested_member in \
                '"kind":"member_access","text":"line.end"' \
                '"kind":"member_access","text":"line.end.x"' \
                '"kind":"member_access","text":"line.start"' \
                '"kind":"member_access","text":"line.start.x"'; do
                grep -Fq "$nested_member" "$self_mir_json" || {
                    echo "[self-host-parity:driver-rung2] $backend nested member graph edge was lost: $nested_member" >&2
                    exit 1
                }
            done
        fi
        if [[ "$base" == "nested_member_call" ]]; then
            for nested_call_edge in \
                '"kind":"member_access","text":"line.end"' \
                '"kind":"member_access","text":"line.end.LengthPlus"' \
                '"kind":"call","text":"line.end.LengthPlus()"'; do
                grep -Fq "$nested_call_edge" "$self_mir_json" || {
                    echo "[self-host-parity:driver-rung2] $backend nested member-call graph edge was lost: $nested_call_edge" >&2
                    exit 1
                }
            done
        fi
        oracle_canonical_mode="--canonicalize-oracle-mir-json"
        if [[ "$base" == "defer_scope" ]]; then
            oracle_canonical_mode="--canonicalize-mir-json"
        fi
        pgy_selfhost_driver_rung2_canonicalize "$machine_fixture" \
            "$driver_bin" "$oracle_canonical_mode" "$mir_json_arg" \
            "$oracle_canonical"
        pgy_selfhost_driver_rung2_canonicalize "$machine_fixture" \
            "$driver_bin" --canonicalize-mir-json "$self_mir_json_arg" \
            "$self_canonical"
        pgy_selfhost_verify_driver_rung2_try_graph \
            "$backend" "$base" "$oracle_canonical" "oracle-canonical"
        pgy_selfhost_verify_driver_rung2_try_graph \
            "$backend" "$base" "$self_canonical" "self-canonical"
        materialized_match_delta=0
        if pgy_selfhost_driver_rung2_match_materialization_delta \
            "$backend" "$base" "$oracle_canonical" "$self_mir_json"; then
            materialized_match_delta=1
        else
            pgy_selfhost_verify_driver_rung2_canonical_declaration_order "$backend" "$base" "$mir_json" "$self_mir_json" "$oracle_canonical" "$self_canonical"
            pgy_selfhost_compare_expected_text_artifact_file_with_owner \
                "driver-rung2:$backend:$base:mir-json" "$BUILD_DIR" \
                "$oracle_canonical" "$self_canonical" "mir_json"
        fi
        canonical_consume_arg="$oracle_canonical_arg"
        if [[ "$materialized_match_delta" -eq 1 ]]; then
            canonical_consume_arg="$self_canonical_arg"
        fi
        if ! pgy_selfhost_driver_rung2_consume_mir "$machine_fixture" \
            "$driver_bin" "$canonical_consume_arg" "$actual.raw" "$err"; then
            echo "[self-host-parity:driver-rung2] $backend MIR integration failed: $base" >&2
            cat "$actual.raw" "$err" >&2
            exit 1
        fi
        tr -d '\r' <"$actual.raw" >"$actual"
        rm -f "$actual.raw"
        self_actual="$BUILD_DIR/${base}_${backend}.self.mir.c"
        if ! pgy_selfhost_driver_rung2_consume_mir "$machine_fixture" \
            "$driver_bin" "$self_mir_json_arg" "$self_actual.raw" \
            "$self_actual.err"; then
            echo "[self-host-parity:driver-rung2] $backend self MIR consumer failed: $base" >&2
            cat "$self_actual.raw" "$self_actual.err" >&2
            exit 1
        fi
        tr -d '\r' <"$self_actual.raw" >"$self_actual"
        rm -f "$self_actual.raw"
        pgy_selfhost_verify_driver_rung2_option_struct_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_result_field_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_string_concat_alias_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_fieldless_class_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_role_implicit_self_emitted_c "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_spawn_await_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_generic_spawn_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_generic_string_spawn_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_generic_spawn_mixed_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_generic_default_contract_emitted_c "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_ability_bind_dispatch_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_generic_multi_bound_defaults_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_nested_generic_containers_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_list_ops_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_list_int_loop_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_for_in_list_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_list_push_scalar_value_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_list_shadow_scope_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_list_literal_context_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_queue_ops_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_set_ops_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_set_literal_emitted_c "$backend" "$base" "$self_actual"; pgy_selfhost_verify_driver_rung2_set_index_value_emitted_c "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_generic_struct_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_inferred_generic_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_generic_member_specialization_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_array_literal_emitted_c "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_owner_field "$backend" "$base" "$self_mir_json" "$self_actual" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_assign_instruction_graph \
            "$backend" "$base" "$mir_json" "$self_mir_json" \
            "$self_actual" "$driver_bin"
        pgy_selfhost_driver_rung2_verify_graph_negatives \
            "$machine_fixture" "$backend" "$base" "$self_mir_json" \
            "$driver_bin"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:self-mir-c" "$BUILD_DIR" \
            "$actual" "$self_actual" "emitted_c"
        source_actual="$BUILD_DIR/${base}_${backend}.source.mir.c"
        if ! pgy_selfhost_driver_rung2_emit_source "$machine_fixture" \
            "$driver_bin" "$fixture_rel" "$source_actual.raw" \
            "$source_actual.err"; then
            echo "[self-host-parity:driver-rung2] $backend producer-first source path failed: $base" >&2
            cat "$source_actual.raw" "$source_actual.err" >&2
            exit 1
        fi
        tr -d '\r' <"$source_actual.raw" >"$source_actual"
        rm -f "$source_actual.raw"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:source-mir-c" "$BUILD_DIR" \
            "$self_actual" "$source_actual" "emitted_c"
        if ! pgy_selfhost_driver_rung2_compile_emitted "$machine_fixture" \
            "$actual" "$BUILD_DIR/${base}_${backend}.mir.exe" \
            "$BUILD_DIR/${base}_${backend}.mir.cc.log"; then
            echo "[self-host-parity:driver-rung2] integrated MIR C compile failed: $backend/$base" >&2
            cat "$BUILD_DIR/${base}_${backend}.mir.cc.log" >&2
            exit 1
        fi
        "$BUILD_DIR/${base}_${backend}.mir.exe" \
            >"$BUILD_DIR/${base}_${backend}.mir.run.raw"
        tr -d '\r' <"$BUILD_DIR/${base}_${backend}.mir.run.raw" \
            >"$BUILD_DIR/${base}_${backend}.mir.run"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:mir-run" "$BUILD_DIR" \
            "$BUILD_DIR/${base}.oracle.run" \
            "$BUILD_DIR/${base}_${backend}.mir.run" "run_output"
        mir_baseline="$BUILD_DIR/${base}.mir.baseline.c"
        if [[ ! -f "$mir_baseline" ]]; then
            cp "$actual" "$mir_baseline"
        else
            pgy_selfhost_compare_expected_text_artifact_file_with_owner \
                "driver-rung2:$backend:$base:mir-c" "$BUILD_DIR" \
                "$mir_baseline" "$actual" "emitted_c"
        fi
    done
}

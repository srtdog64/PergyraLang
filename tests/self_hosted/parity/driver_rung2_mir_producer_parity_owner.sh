#!/usr/bin/env bash
# Owns DRV-2 source-to-MIR producer, consumer, artifact, and run parity.
pgy_selfhost_prepare_driver_rung2_mir_oracles() {
    local fixture_rel fixture_abs base mir_json oracle_bin
    for fixture_rel in "${mir_fixture_rows[@]}"; do
        fixture_abs="$ROOT_DIR/$fixture_rel"
        base="$(basename "$fixture_rel" .pgy)"
        mir_json="$BUILD_DIR/${base}.mir.json"
        oracle_bin="$BUILD_DIR/${base}.oracle.exe"
        [[ -f "$fixture_abs" ]] || {
            echo "[self-host-parity:driver-rung2] missing MIR fixture: $fixture_rel" >&2
            exit 1
        }
        (cd "$ROOT_DIR" && "$PGY" --mir-json \
            "$(pgy_path_for_compiler "$PGY" "$fixture_abs")" 2>/dev/null) \
            | tr -d '\r' >"$mir_json"
        grep -Fq '"schema":"pgy.mir.v1"' "$mir_json" || {
            echo "[self-host-parity:driver-rung2] missing MIR schema: $fixture_rel" >&2
            exit 1
        }
        if ! (cd "$ROOT_DIR" && "$PGY" \
            "$(pgy_path_for_compiler "$PGY" "$fixture_abs")" --backend=c \
            -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
            >"$BUILD_DIR/${base}.oracle.compile.log" 2>&1); then
            echo "[self-host-parity:driver-rung2] C oracle compile failed: $fixture_rel" >&2
            cat "$BUILD_DIR/${base}.oracle.compile.log" >&2
            exit 1
        fi
        "$oracle_bin" >"$BUILD_DIR/${base}.oracle.run.raw"
        tr -d '\r' <"$BUILD_DIR/${base}.oracle.run.raw" \
            >"$BUILD_DIR/${base}.oracle.run"
    done
}
pgy_selfhost_run_driver_rung2_mir_producer_parity() {
    local backend="$1"
    local driver_bin="$2"
    local fixture_rel base mir_json mir_json_arg self_mir_json
    local self_mir_json_arg oracle_canonical oracle_canonical_arg self_canonical
    local actual err missing_graph invalid_graph self_actual source_actual mir_baseline bare_call_missing_graph
    for fixture_rel in "${mir_fixture_rows[@]}"; do
        base="$(basename "$fixture_rel" .pgy)"
        mir_json="$BUILD_DIR/${base}.mir.json"
        mir_json_arg="$(pgy_selfhost_path_relative_to_root "$mir_json")"
        self_mir_json="$BUILD_DIR/${base}_${backend}.self.mir.json"
        self_mir_json_arg="$(pgy_selfhost_path_relative_to_root "$self_mir_json")"
        oracle_canonical="$BUILD_DIR/${base}_${backend}.oracle.canonical.mir.json"
        oracle_canonical_arg="$(pgy_selfhost_path_relative_to_root "$oracle_canonical")"
        self_canonical="$BUILD_DIR/${base}_${backend}.self.canonical.mir.json"
        actual="$BUILD_DIR/${base}_${backend}.mir.c"
        err="$BUILD_DIR/${base}_${backend}.mir.err"
        if ! (cd "$ROOT_DIR" && "$driver_bin" \
            --emit-mir-json-verified "$fixture_rel" \
            >"$self_mir_json.raw" 2>"$self_mir_json.err"); then
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
        pgy_selfhost_verify_driver_rung2_call_target "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_iteration_graph "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_foreach_call_type "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_enum_argument "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_array_argument "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_struct_argument "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_struct_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_generic_struct_value \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_inferred_generic_value \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_option_struct_value "$backend" "$base" "$self_mir_json" "$driver_bin"
        pgy_selfhost_verify_driver_rung2_collection_mutation_graph "$backend" "$base" "$self_mir_json"
        pgy_selfhost_verify_driver_rung2_array_literal_graph "$backend" "$base" "$self_mir_json"
        pgy_selfhost_verify_driver_rung2_indexed_assignment \
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
        if [[ "$base" == "param_carriage" ]]; then
            grep -Fq '"name":"pair","type":"Pair","carriage":"readonly-ref","pass":"indirect"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend readonly-ref aggregate ABI fact drifted" >&2
                exit 1
            }
            grep -Fq '"name":"value","type":"Int","carriage":"value-result","pass":"direct"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend value-result ABI fact drifted" >&2
                exit 1
            }
            grep -Fq '"name":"values","type":"Array<Int>","carriage":"owner-handle","pass":"direct"' "$self_mir_json" || {
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
        if [[ "$base" == "option_try" ]]; then
            grep -Fq '"kind":"try","text":"?Pick(x)"' \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend try expression collapsed out of the operand graph" >&2
                exit 1
            }
            grep -Fq '"kind":"call_argument","text":"Pick(x)"' \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend try operand call spine was lost" >&2
                exit 1
            }
        fi
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
        (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$mir_json_arg" | tr -d '\r' >"$oracle_canonical")
        (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$self_mir_json_arg" | tr -d '\r' >"$self_canonical")
        if [[ "$base" == "option_try" ]]; then
            for canonical_try_graph in "$oracle_canonical" "$self_canonical"; do
                grep -Fq '"kind":"try","text":"?Pick(x)"' \
                    "$canonical_try_graph" || {
                    echo "[self-host-parity:driver-rung2] $backend canonical try expression graph was lost" >&2
                    exit 1
                }
            done
        fi
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:mir-json" "$BUILD_DIR" \
            "$oracle_canonical" "$self_canonical" "mir_json"
        if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$oracle_canonical_arg" \
            >"$actual.raw" 2>"$err"); then
            echo "[self-host-parity:driver-rung2] $backend MIR integration failed: $base" >&2
            cat "$actual.raw" "$err" >&2
            exit 1
        fi
        tr -d '\r' <"$actual.raw" >"$actual"
        rm -f "$actual.raw"
        self_actual="$BUILD_DIR/${base}_${backend}.self.mir.c"
        if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$self_mir_json_arg" >"$self_actual.raw" 2>"$self_actual.err"); then
            echo "[self-host-parity:driver-rung2] $backend self MIR consumer failed: $base" >&2
            cat "$self_actual.raw" "$self_actual.err" >&2
            exit 1
        fi
        tr -d '\r' <"$self_actual.raw" >"$self_actual"
        rm -f "$self_actual.raw"
        pgy_selfhost_verify_driver_rung2_option_struct_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_generic_struct_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_inferred_generic_emitted_c \
            "$backend" "$base" "$self_actual"
        pgy_selfhost_verify_driver_rung2_array_literal_emitted_c \
            "$backend" "$base" "$self_actual"
        if grep -Fq '"expr0_graph":{' "$self_mir_json"; then
            missing_graph="$BUILD_DIR/${base}_${backend}.missing-graph.mir.json"
            sed 's/"expr0_graph"/"expr0_graph_removed"/g' \
                "$self_mir_json" >"$missing_graph"
            if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
                "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
                >"$missing_graph.out" 2>"$missing_graph.err"); then
                echo "[self-host-parity:driver-rung2] $backend missing expression graph was accepted: $base" >&2
                exit 1
            fi
            grep -Fq "MIR instruction expression graph is missing or invalid" \
                "$missing_graph.err" "$missing_graph.out" || {
                echo "[self-host-parity:driver-rung2] $backend missing expression graph diagnostic drifted: $base" >&2
                cat "$missing_graph.out" "$missing_graph.err" >&2
                exit 1
            }
            invalid_graph="$BUILD_DIR/${base}_${backend}.invalid-graph.mir.json"
            sed 's/"root":[0-9][0-9]*/"root":999/g' \
                "$self_mir_json" >"$invalid_graph"
            if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
                "$(pgy_selfhost_path_relative_to_root "$invalid_graph")" \
                >"$invalid_graph.out" 2>"$invalid_graph.err"); then
                echo "[self-host-parity:driver-rung2] $backend invalid expression graph was accepted: $base" >&2
                exit 1
            fi
            grep -Fq "MIR instruction expression graph is missing or invalid" \
                "$invalid_graph.err" "$invalid_graph.out" || {
                echo "[self-host-parity:driver-rung2] $backend invalid expression graph diagnostic drifted: $base" >&2
                cat "$invalid_graph.out" "$invalid_graph.err" >&2
                exit 1
            }
        fi
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:self-mir-c" "$BUILD_DIR" \
            "$actual" "$self_actual" "emitted_c"
        source_actual="$BUILD_DIR/${base}_${backend}.source.mir.c"
        if ! (cd "$ROOT_DIR" && "$driver_bin" "$fixture_rel" \
            --emit-c-verified >"$source_actual.raw" 2>"$source_actual.err"); then
            echo "[self-host-parity:driver-rung2] $backend producer-first source path failed: $base" >&2
            cat "$source_actual.raw" "$source_actual.err" >&2
            exit 1
        fi
        tr -d '\r' <"$source_actual.raw" >"$source_actual"
        rm -f "$source_actual.raw"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:source-mir-c" "$BUILD_DIR" \
            "$self_actual" "$source_actual" "emitted_c"
        if ! "$CC" -x c -std=c11 "$actual" \
            -o "$BUILD_DIR/${base}_${backend}.mir.exe" \
            >"$BUILD_DIR/${base}_${backend}.mir.cc.log" 2>&1; then
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

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
    local self_mir_json_arg oracle_canonical self_canonical actual err
    local self_actual source_actual mir_baseline

    for fixture_rel in "${mir_fixture_rows[@]}"; do
        base="$(basename "$fixture_rel" .pgy)"
        mir_json="$BUILD_DIR/${base}.mir.json"
        mir_json_arg="$(pgy_selfhost_path_relative_to_root "$mir_json")"
        self_mir_json="$BUILD_DIR/${base}_${backend}.self.mir.json"
        self_mir_json_arg="$(pgy_selfhost_path_relative_to_root "$self_mir_json")"
        oracle_canonical="$BUILD_DIR/${base}_${backend}.oracle.canonical.mir.json"
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
        if [[ "$base" == "indexed_assignment" ]]; then
            grep -Fq '"expr1":"values[i]"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend indexed target fact was lost" >&2
                exit 1
            }
            grep -Fq '"uses":["values.1","i.1","j.1"]' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend indexed target/RHS use facts drifted" >&2
                exit 1
            }
        fi
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
        fi
        if [[ "$base" == "for_each" ]]; then
            grep -Fq '"arg0":"n","arg1":null,"expr0":"nums","expr1":"nums"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend Int foreach fact drifted" >&2
                exit 1
            }
            grep -Fq '"arg0":"name","arg1":null,"expr0":"names","expr1":"names"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend String foreach fact drifted" >&2
                exit 1
            }
            grep -Fq '"name":"n","type":"Int"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend Int foreach binding type drifted" >&2
                exit 1
            }
            grep -Fq '"name":"name","type":"String"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend String foreach binding type drifted" >&2
                exit 1
            }
            grep -Fq '"uses":["nums.1"]' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend Int foreach use fact drifted" >&2
                exit 1
            }
            grep -Fq '"uses":["names.1"]' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend String foreach use fact drifted" >&2
                exit 1
            }
        fi
        (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$mir_json_arg" | tr -d '\r' >"$oracle_canonical")
        (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$self_mir_json_arg" | tr -d '\r' >"$self_canonical")
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:mir-json" "$BUILD_DIR" \
            "$oracle_canonical" "$self_canonical" "mir_json"
        if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json "$mir_json_arg" \
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

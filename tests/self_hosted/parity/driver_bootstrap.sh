#!/usr/bin/env bash
# Integrated parser/codegen driver fixed-point gate.
#
# The codegen bootstrap is the seed producer and must run first. This runner
# consumes its Pergyra-built gen2 codegen and self-parser artifacts, builds the
# shared driver pipeline, then proves gen2 and gen3 consume one oracle MIR fact.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-driver-bootstrap] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-driver-bootstrap" "$PGY"

CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-driver-bootstrap] missing C compiler on PATH: $CC" >&2
    exit 1
fi

CODEGEN_BUILD="$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/driver/bootstrap"
PATHS_FILE="$BUILD_DIR/driver_bootstrap_paths.txt"
CODEGEN_BIN="$CODEGEN_BUILD/gen2.exe"
PARSER_BIN="$CODEGEN_BUILD/parser_ast_producer.exe"
mkdir -p "$BUILD_DIR"

pgy_selfhost_read_test_harness_manifest \
    "self-host-driver-bootstrap" \
    "$BUILD_DIR" \
    "codegen-bootstrap-paths" \
    "$PATHS_FILE"

paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    paths+=("$line")
done <"$PATHS_FILE"
if [[ "${#paths[@]}" -ne 9 ]]; then
    echo "[self-host-driver-bootstrap] TestHarness expected 9 paths, got ${#paths[@]}" >&2
    exit 1
fi

DRIVER_SOURCE="$ROOT_DIR/${paths[8]}"
SAMPLE_SOURCE="$ROOT_DIR/${paths[7]}"
for path in "$DRIVER_SOURCE" "$SAMPLE_SOURCE" "$CODEGEN_BIN" "$PARSER_BIN"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-driver-bootstrap] missing bootstrap input: $path" >&2
        exit 1
    fi
done

driver_bootstrap_heartbeat_pid=""
driver_bootstrap_stop_heartbeat() {
    if [[ -n "$driver_bootstrap_heartbeat_pid" ]]; then
        kill "$driver_bootstrap_heartbeat_pid" 2>/dev/null || true
        wait "$driver_bootstrap_heartbeat_pid" 2>/dev/null || true
    fi
}
trap driver_bootstrap_stop_heartbeat EXIT
(
    while sleep 60; do
        echo "[self-host-driver-bootstrap] still running"
    done
) &
driver_bootstrap_heartbeat_pid=$!

compile_c() {
    local label="$1"
    local source="$2"
    local output="$3"
    local log="$BUILD_DIR/${label}.compile.log"

    echo "[self-host-driver-bootstrap] compiling $label"
    if ! "$CC" "$source" -o "$output" >"$log" 2>&1; then
        echo "[self-host-driver-bootstrap] $label C compile failed" >&2
        tail -c 65536 "$log" >&2 || true
        exit 1
    fi
}

run_driver_to_file() {
    local label="$1"
    local bin="$2"
    local source="$3"
    local output="$4"
    local stdout="$BUILD_DIR/${label}.out"
    local stderr="$BUILD_DIR/${label}.err"
    local source_rel
    local output_rel

    source_rel="$(pgy_selfhost_path_relative_to_root "$source")"
    output_rel="$(pgy_selfhost_path_relative_to_root "$output")"
    rm -f "$output"
    echo "[self-host-driver-bootstrap] running $label"
    if ! (cd "$ROOT_DIR" && "$bin" "$source_rel" "$output_rel" >"$stdout" 2>"$stderr"); then
        echo "[self-host-driver-bootstrap] $label failed for $source_rel" >&2
        cat "$stdout" "$stderr" >&2 || true
        exit 1
    fi
    if [[ ! -s "$output" ]]; then
        echo "[self-host-driver-bootstrap] $label emitted no artifact" >&2
        exit 1
    fi
}

run_driver_mode_to_file() {
    local label="$1"
    local bin="$2"
    local mode="$3"
    local input="$4"
    local output="$5"
    local stdout="$BUILD_DIR/${label}.out"
    local stderr="$BUILD_DIR/${label}.err"
    local input_rel
    local output_rel

    input_rel="$(pgy_selfhost_path_relative_to_root "$input")"
    output_rel="$(pgy_selfhost_path_relative_to_root "$output")"
    rm -f "$output"
    echo "[self-host-driver-bootstrap] running $label"
    if ! (cd "$ROOT_DIR" && \
        "$bin" "$mode" "$input_rel" "$output_rel" >"$stdout" 2>"$stderr"); then
        echo "[self-host-driver-bootstrap] $label failed for $input_rel" >&2
        cat "$stdout" "$stderr" >&2 || true
        exit 1
    fi
    if [[ ! -s "$output" ]]; then
        echo "[self-host-driver-bootstrap] $label emitted no artifact" >&2
        exit 1
    fi
}

driver_rel="$(pgy_selfhost_path_relative_to_root "$DRIVER_SOURCE")"
ast_rel=".tmp/self_hosted/driver/bootstrap/driver_bootstrap.ast.txt"
ast_abs="$ROOT_DIR/$ast_rel"
echo "[self-host-driver-bootstrap] parsing integrated driver"
if ! (cd "$ROOT_DIR" && "$PARSER_BIN" "$driver_rel" 2>"$BUILD_DIR/parser.err" | tr -d '\r' >"$ast_abs"); then
    echo "[self-host-driver-bootstrap] self-parser failed for $driver_rel" >&2
    cat "$BUILD_DIR/parser.err" >&2 || true
    exit 1
fi

echo "[self-host-driver-bootstrap] emitting integrated driver seed"
if ! (cd "$ROOT_DIR" && "$CODEGEN_BIN" "$ast_rel" 2>"$BUILD_DIR/seed_emit.err" | tr -d '\r' >"$BUILD_DIR/driver_seed.c"); then
    echo "[self-host-driver-bootstrap] Pergyra-built codegen failed to emit driver seed" >&2
    cat "$BUILD_DIR/seed_emit.err" >&2 || true
    exit 1
fi
if grep -q '^CODEGEN ERROR' "$BUILD_DIR/driver_seed.c"; then
    echo "[self-host-driver-bootstrap] integrated driver is outside the codegen subset" >&2
    grep '^CODEGEN ERROR' "$BUILD_DIR/driver_seed.c" | head -3 >&2
    exit 1
fi
compile_c "driver_seed" "$BUILD_DIR/driver_seed.c" "$BUILD_DIR/driver_seed.exe"

echo "[self-host-driver-bootstrap] compiling oracle driver"
if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$DRIVER_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$BUILD_DIR/driver_oracle.exe")" \
    >"$BUILD_DIR/driver_oracle.compile.log" 2>&1); then
    echo "[self-host-driver-bootstrap] oracle driver compile failed" >&2
    tail -c 65536 "$BUILD_DIR/driver_oracle.compile.log" >&2 || true
    exit 1
fi

run_driver_to_file "sample_self" "$BUILD_DIR/driver_seed.exe" "$SAMPLE_SOURCE" "$BUILD_DIR/sample_self.c"
run_driver_to_file "sample_oracle" "$BUILD_DIR/driver_oracle.exe" "$SAMPLE_SOURCE" "$BUILD_DIR/sample_oracle.c"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-driver-bootstrap:sample" \
    "$BUILD_DIR" \
    "$BUILD_DIR/sample_oracle.c" \
    "$BUILD_DIR/sample_self.c" \
    "emitted_c"

run_driver_mode_to_file \
    "bounded_mir_seed" \
    "$BUILD_DIR/driver_seed.exe" \
    "--emit-mir-json-verified" \
    "$SAMPLE_SOURCE" \
    "$BUILD_DIR/bounded_seed.mir.json"
run_driver_mode_to_file \
    "bounded_mir_oracle" \
    "$BUILD_DIR/driver_oracle.exe" \
    "--emit-mir-json-verified" \
    "$SAMPLE_SOURCE" \
    "$BUILD_DIR/bounded_oracle.mir.json"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-driver-bootstrap:bounded-mir-producer" \
    "$BUILD_DIR" \
    "$BUILD_DIR/bounded_oracle.mir.json" \
    "$BUILD_DIR/bounded_seed.mir.json" \
    "mir_json"
run_driver_mode_to_file \
    "bounded_seed" \
    "$BUILD_DIR/driver_seed.exe" \
    "--mir-json" \
    "$BUILD_DIR/bounded_seed.mir.json" \
    "$BUILD_DIR/bounded_seed.c"
run_driver_mode_to_file \
    "bounded_oracle" \
    "$BUILD_DIR/driver_oracle.exe" \
    "--mir-json" \
    "$BUILD_DIR/bounded_seed.mir.json" \
    "$BUILD_DIR/bounded_oracle.c"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-driver-bootstrap:bounded-mir-consumer" \
    "$BUILD_DIR" \
    "$BUILD_DIR/bounded_oracle.c" \
    "$BUILD_DIR/bounded_seed.c" \
    "emitted_c"
echo "[self-host-driver-bootstrap] integrated seed bounded MIR consumer parity ok"

if [[ "${PGY_SELFHOST_DRIVER_FULL_FIXPOINT:-0}" != "1" ]]; then
    echo "[self-host-driver-bootstrap] bounded integrated bootstrap ok; full stage2/gen3 fixed point is explicit"
    exit 0
fi

# Full-source self-host MIR production is covered by bounded DRV-2 producer
# parity until its allocation growth is closed. The explicit fixed point
# consumes one oracle-owned MIR fact so generation drift stays isolated.
run_driver_mode_to_file \
    "driver_mir_oracle" \
    "$BUILD_DIR/driver_oracle.exe" \
    "--emit-mir-json-verified" \
    "$DRIVER_SOURCE" \
    "$BUILD_DIR/driver_source.mir.json"
run_driver_mode_to_file \
    "gen2_emit" \
    "$BUILD_DIR/driver_seed.exe" \
    "--mir-json" \
    "$BUILD_DIR/driver_source.mir.json" \
    "$BUILD_DIR/driver_gen2.c"
compile_c "driver_gen2" "$BUILD_DIR/driver_gen2.c" "$BUILD_DIR/driver_gen2.exe"
run_driver_mode_to_file \
    "bounded_gen2" \
    "$BUILD_DIR/driver_gen2.exe" \
    "--mir-json" \
    "$BUILD_DIR/bounded_seed.mir.json" \
    "$BUILD_DIR/bounded_gen2.c"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-driver-bootstrap:generated-bounded-preflight" \
    "$BUILD_DIR" \
    "$BUILD_DIR/bounded_seed.c" \
    "$BUILD_DIR/bounded_gen2.c" \
    "emitted_c"
echo "[self-host-driver-bootstrap] generated driver bounded MIR preflight ok"

run_driver_mode_to_file \
    "gen3_emit" \
    "$BUILD_DIR/driver_gen2.exe" \
    "--mir-json" \
    "$BUILD_DIR/driver_source.mir.json" \
    "$BUILD_DIR/driver_gen3.c"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-driver-bootstrap:fixpoint" \
    "$BUILD_DIR" \
    "$BUILD_DIR/driver_gen2.c" \
    "$BUILD_DIR/driver_gen3.c" \
    "emitted_c"

echo "[self-host-driver-bootstrap] integrated MIR-consumer fixpoint ok: gen2 == gen3 ($(wc -l < "$BUILD_DIR/driver_gen2.c") lines)"

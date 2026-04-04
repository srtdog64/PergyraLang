#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

BACKENDS="${PGY_EXAMPLE_BACKENDS:-c llvm}"

normalize_output() {
    sed \
        -e '/^0 error(s), 0 warning(s)$/d' \
        -e '/^pgy: compiled/d' \
        -e '/^pgy: compiled (LLVM)/d' \
        -e '/^--- output ---$/d' \
        -e '/^--- end ---$/d'
}

pick_expected_file() {
    local base="$1"
    local backend="$2"

    if [[ -f "${base}.${backend}.txt" ]]; then
        printf '%s.%s.txt' "$base" "$backend"
        return 0
    fi
    if [[ -f "${base}.txt" ]]; then
        printf '%s.txt' "$base"
        return 0
    fi
    return 1
}

run_exact_output_if_present() {
    local name="$1"
    local backend="$2"
    local output="$3"
    local expected
    local cleaned_output
    local cleaned_expected

    if ! expected="$(pick_expected_file "$ROOT_DIR/examples/$name/expected_stdout" "$backend")"; then
        return 1
    fi

    cleaned_output="$(printf '%s' "$output" | normalize_output)"
    cleaned_expected="$(cat "$expected")"
    if ! diff -u <(printf '%s' "$cleaned_expected") <(printf '%s' "$cleaned_output") >/dev/null; then
        echo "[example-smoke] $name backend=$backend stdout mismatch" >&2
        diff -u <(printf '%s' "$cleaned_expected") <(printf '%s' "$cleaned_output") >&2 || true
        exit 1
    fi
    echo "[example-smoke] $name backend=$backend stdout exact ok"
    return 0
}

run_expect_lines() {
    local name="$1"
    local backend="$2"
    local file="$3"
    shift 3
    local output
    local out_bin="$WORK_DIR/${name}_${backend}"

    if [[ -d "$file" ]]; then
        file="$file/main.pgy"
    fi
    if [[ ! -f "$file" ]]; then
        echo "[example-smoke] $name backend=$backend missing entry: $file" >&2
        exit 1
    fi

    output="$("$PGY" "$file" --run --backend="$backend" -o "$out_bin" 2>&1)"
    if run_exact_output_if_present "$name" "$backend" "$output"; then
        return 0
    fi
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[example-smoke] $name backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[example-smoke] $name backend=$backend ok"
}

run_expect_file_lines() {
    local name="$1"
    local backend="$2"
    local file="$3"
    shift 3
    local content
    local expected

    expected=""
    if [[ "$file" == "$ROOT_DIR/examples/"*"/results.txt" ]]; then
        local example_dir
        example_dir="$(dirname "$file")"
        if expected="$(pick_expected_file "$example_dir/expected_results" "$backend" 2>/dev/null)"; then
            :
        else
            expected=""
        fi
    fi

    if [[ ! -f "$file" ]]; then
        echo "[example-smoke] $name missing output file $file" >&2
        exit 1
    fi
    content="$(cat "$file")"
    if [[ -n "$expected" ]]; then
        if ! diff -u "$expected" "$file" >/dev/null; then
            echo "[example-smoke] $name file mismatch" >&2
            diff -u "$expected" "$file" >&2 || true
            exit 1
        fi
        echo "[example-smoke] $name file exact ok"
        return 0
    fi
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$content"; then
            echo "[example-smoke] $name file missing '$expected'" >&2
            echo "--- file ---" >&2
            echo "$content" >&2
            echo "------------" >&2
            exit 1
        fi
    done
    echo "[example-smoke] $name file ok"
}

run_stable_examples() {
    local backend="$1"
    run_expect_lines "beta_resource_slots" "$backend" \
        "$ROOT_DIR/examples/beta_resource_slots.pgy" "42" "7" "3"
    run_expect_lines "beta_modules_generics" "$backend" \
        "$ROOT_DIR/examples/beta_modules_generics.pgy" "7"
    run_expect_lines "battle_simulator" "$backend" \
        "$ROOT_DIR/examples/battle_simulator" "BATTLE" "Hero" "Slime" "TOURNAMENT" "14" "true"
    run_expect_lines "biome_simulator" "$backend" \
        "$ROOT_DIR/examples/biome_simulator" "BIOME" "Red Deer" "Grey Wolf" "[World] Total migration pressure" "Day 6" "SAVING REPORT"
    run_expect_lines "fsm_factory" "$backend" \
        "$ROOT_DIR/examples/fsm_factory" "PERGYRA FACTORY FSM" "FACTORY SHIFT 1" "ALPHA CELL" "BETA CELL" "SAVING FSM REPORT"
    run_expect_lines "raid_graph_fsm" "$backend" \
        "$ROOT_DIR/examples/raid_graph_fsm" "RAID GRAPH + FSM" "RAID TURN 1" "[Raider] Iris" "[Room] VAULT" "SAVING RAID REPORT"
    run_expect_file_lines "battle_simulator" \
        "$backend" "$ROOT_DIR/examples/battle_simulator/results.txt" "TOURNAMENT" "Hero" "Knight" "projection_ready"
    run_expect_file_lines "biome_simulator" \
        "$backend" "$ROOT_DIR/examples/biome_simulator/results.txt" "BIOME SIMULATION FINAL REPORT" "Red Deer" "Lynx" "migration:"
    run_expect_file_lines "fsm_factory" \
        "$backend" "$ROOT_DIR/examples/fsm_factory/results.txt" "FACTORY FSM REPORT" "ALPHA" "BETA" "projection_ready=true"
    run_expect_file_lines "raid_graph_fsm" \
        "$backend" "$ROOT_DIR/examples/raid_graph_fsm/results.txt" "RAID GRAPH FSM REPORT" "FORGE" "SANCTUM" "Iris relics="
}

run_qubit_example() {
    local backend="$1"
    local output
    local values
    local line1
    local line2
    local line3
    local line4
    local line5
    local out_bin="$WORK_DIR/beta_qubit_experimental_${backend}"

    output="$("$PGY" "$ROOT_DIR/examples/beta_qubit_experimental.pgy" --run --backend="$backend" -o "$out_bin" 2>&1)"
    values="$(grep -E '^(0|1|2)$' <<<"$output" || true)"

    line1="$(sed -n '1p' <<<"$values")"
    line2="$(sed -n '2p' <<<"$values")"
    line3="$(sed -n '3p' <<<"$values")"
    line4="$(sed -n '4p' <<<"$values")"
    line5="$(sed -n '5p' <<<"$values")"

    if [[ "$line1" != "2" ]]; then
        echo "[example-smoke] beta_qubit_experimental backend=$backend missing initial superposition state" >&2
        echo "$output" >&2
        exit 1
    fi

    if [[ -z "$line2" || "$line2" != "$line3" ]]; then
        echo "[example-smoke] beta_qubit_experimental backend=$backend repeated measurement mismatch" >&2
        echo "$output" >&2
        exit 1
    fi

    if [[ -z "$line4" || "$line4" != "$line5" ]]; then
        echo "[example-smoke] beta_qubit_experimental backend=$backend entangled pair mismatch" >&2
        echo "$output" >&2
        exit 1
    fi

    echo "[example-smoke] beta_qubit_experimental backend=$backend ok"
}

for backend in $BACKENDS; do
    run_stable_examples "$backend"
done

for backend in $BACKENDS; do
    run_qubit_example "$backend"
done

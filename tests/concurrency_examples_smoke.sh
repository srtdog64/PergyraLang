#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate: native concurrency example execution and semantic
# rejection across C/LLVM, not self-host substitution.
# Reuse canonical fixtures/goldens. Compile and execute each positive once per
# backend; negatives must fail in source semantics without an artifact.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() { echo "[concurrency-examples] FAIL: $*" >&2; exit 1; }

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then PGY="${PGY}.exe"; fi
[[ -x "$PGY" ]] || fail "missing compiler: $PGY"
pgy_require_runnable_binary_here concurrency-examples "$PGY"
for tool in sha256sum cmp tr mktemp; do
    command -v "$tool" >/dev/null 2>&1 || fail "required tool missing: $tool"
done

mkdir -p "$ROOT_DIR/.tmp"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/concurrency_examples.XXXXXX")"
echo "[concurrency-examples] evidence: $WORK_DIR"
trap 'status=$?; printf "exit_status=%s\n" "$status" >>"$WORK_DIR/receipt.txt"; echo "[concurrency-examples] retained: $WORK_DIR"' EXIT
compiler_before="$(sha256sum "$PGY")"
printf 'head=%s\npipeline=native\ncompiler=%s\n' \
    "$(git -C "$ROOT_DIR" rev-parse HEAD)" "$compiler_before" \
    >"$WORK_DIR/receipt.txt"

# A single five-minute budget includes all subprocesses. No timeout extension,
# launch retry, compiler rebuild, installed-driver replacement, or quiet skip.
gate_deadline=$((SECONDS + 300))
run_with_gate_budget() {
    local seconds="$1" out="$2" err="$3" remaining
    shift 3
    remaining=$((gate_deadline - SECONDS))
    (( remaining > 0 )) || fail "five-minute gate budget exhausted"
    if (( seconds > remaining )); then seconds="$remaining"; fi
    pgy_run_with_timeout "$seconds" "$out" "$err" "$@"
}

positives=(
    parallel_snapshot_read
    parallel_disjoint_split_write
    parallel_pingpong_witness
    parallel_join_expr
    parallel_join_stencil
    parallel_join_reduce
    parallel_join_any_blocked
    parallel_scheduler_showcase
)
positive_runs=0
cd "$ROOT_DIR"
for case_name in "${positives[@]}"; do
    case_dir="$ROOT_DIR/tests/cases/backend_compare/$case_name"
    source_path="$case_dir/main.pgy"
    [[ -f "$source_path" && -s "$case_dir/expected.stdout" ]] \
        || fail "missing canonical source/golden: $case_name"
    source_before="$(sha256sum "$source_path" "$case_dir/expected.stdout")"
    printf '%s\n' "$source_before" >>"$WORK_DIR/receipt.txt"
    source_arg="$(pgy_path_for_compiler "$PGY" "$source_path")"
    tr -d '\r' <"$case_dir/expected.stdout" >"$WORK_DIR/$case_name.expected"
    for backend in c llvm; do
        stem="$WORK_DIR/${case_name}_${backend}"
        binary="$stem.exe"
        output_arg="$(pgy_path_for_compiler "$PGY" "$binary")"
        if run_with_gate_budget 45 "$stem.compile.out" "$stem.compile.err" \
            "$PGY" "$source_arg" "--backend=$backend" -o "$output_arg"; then
            compile_rc=0
        else
            compile_rc=$?
        fi
        [[ "$compile_rc" -eq 0 && -f "$binary" ]] \
            || fail "$case_name/$backend compile rc=$compile_rc (see $stem.compile.err)"
        pgy_require_runnable_binary_here concurrency-examples "$binary"
        if run_with_gate_budget 15 "$stem.stdout" "$stem.stderr" "$binary"; then
            run_rc=0
        else
            run_rc=$?
        fi
        [[ "$run_rc" -eq 0 ]] || fail "$case_name/$backend execution rc=$run_rc"
        [[ ! -s "$stem.stderr" ]] || fail "$case_name/$backend wrote runtime stderr"
        tr -d '\r' <"$stem.stdout" >"$stem.normalized"
        cmp -s "$WORK_DIR/$case_name.expected" "$stem.normalized" \
            || fail "$case_name/$backend disagrees with canonical expected.stdout"
        positive_runs=$((positive_runs + 1))
        printf 'positive=%s backend=%s exit=0 stderr=empty stdout=exact\n' \
            "$case_name" "$backend" >>"$WORK_DIR/receipt.txt"
        echo "[concurrency-examples] PASS $case_name/$backend"
    done
    cmp -s "$WORK_DIR/${case_name}_c.normalized" \
        "$WORK_DIR/${case_name}_llvm.normalized" || fail "$case_name C/LLVM mismatch"
    [[ "$source_before" == "$(sha256sum "$source_path" "$case_dir/expected.stdout")" ]] \
        || fail "$case_name source/golden changed during verification"
done

# These are existing, expected source-semantic rejections. Do not add the
# known aggregate-Future or malformed external-MIR findings as executable tests.
negative_sources=(
    tests/cases/structured_spawn_lifecycle/negative_fallthrough.pgy
    tests/cases/structured_spawn_lifecycle/negative_alias_binding.pgy
    tests/cases/parallel_join/reject_any_index_mode.pgy
    tests/cases/parallel_join/reject_stencil_inplace.pgy
    tests/cases/parallel_snapshot/reject_write_write.pgy
)
negative_needles=(
    PGY_SEM_TASK_LIFECYCLE
    PGY_SEM_TASK_LIFECYCLE
    'element mode only'
    'outside the index-disjoint form'
    'write-write race'
)
negative_checks=0
for index in "${!negative_sources[@]}"; do
    source_path="$ROOT_DIR/${negative_sources[$index]}"
    [[ -f "$source_path" ]] || fail "missing rejection fixture: $source_path"
    source_before="$(sha256sum "$source_path")"
    printf '%s\n' "$source_before" >>"$WORK_DIR/receipt.txt"
    source_arg="$(pgy_path_for_compiler "$PGY" "$source_path")"
    for backend in c llvm; do
        stem="$WORK_DIR/reject_${index}_${backend}"
        output_arg="$(pgy_path_for_compiler "$PGY" "$stem.exe")"
        if run_with_gate_budget 45 "$stem.out" "$stem.err" \
            "$PGY" "$source_arg" "--backend=$backend" --error-format=json \
            -o "$output_arg"; then
            reject_rc=0
        else
            reject_rc=$?
        fi
        [[ "$reject_rc" -eq 1 ]] || fail "negative $index/$backend returned $reject_rc, expected semantic rejection"
        for extension in exe c ll o; do
            [[ ! -e "$stem.$extension" ]] \
                || fail "negative $index/$backend published $stem.$extension (not executed)"
        done
        grep -Fq '"stage":"semantic"' "$stem.err" \
            || fail "negative $index/$backend lacks semantic diagnostic"
        grep -Fq "${negative_needles[$index]}" "$stem.err" \
            || fail "negative $index/$backend lacks its owned diagnostic"
        if grep -Fq '"stage":"internal"' "$stem.err"; then
            fail "negative $index/$backend reached an internal failure"
        fi
        negative_checks=$((negative_checks + 1))
        printf 'negative=%s backend=%s exit=1 artifact=absent diagnostic=owned\n' \
            "${negative_sources[$index]}" "$backend" >>"$WORK_DIR/receipt.txt"
        echo "[concurrency-examples] PASS rejection $index/$backend"
    done
    [[ "$source_before" == "$(sha256sum "$source_path")" ]] \
        || fail "negative fixture changed during verification: $source_path"
done

[[ "$compiler_before" == "$(sha256sum "$PGY")" ]] \
    || fail "compiler binary changed during verification; mixed evidence is not a pass"
printf 'positive_runs=%s\nnegative_checks=%s\n' "$positive_runs" "$negative_checks" \
    >>"$WORK_DIR/receipt.txt"
echo "[concurrency-examples] PASS: ${#positives[@]} unique programs, $positive_runs C/LLVM runs, $negative_checks artifact-free semantic rejections"

#!/usr/bin/env bash
# Complete ArrayString layout consumers share one admitted owner in C/LLVM.
# Every semantic base is issued once per run; adjacent inventory reuses it.
# Forbidden old-read inventory, enforced here and by the retained move/flow gates:
# backend_local_array_layout, layout_id_without_row_admission,
# array_runtime_symbol_guess, backend_string_element_reconstruction,
# capacity_as_length, post_issue_layout_mutation,
# scalar_program_preamble_layout_literal, complete_cross_family_row_acceptance,
# c_dir_walk_positional_array_string_layout,
# duplicate_scalar_program_array_string_projection_readiness,
# process_args_array_string_storage_alignment_literal,
# process_args_without_target_projection,
# llvm_readonly_ref_array_string_storage_alignment_literal,
# readonly_ref_array_string_without_target_projection,
# owner_handle_array_string_without_caller_move_fact,
# caller_cleanup_after_owner_handle_array_string_move,
# owner_handle_array_string_use_after_move,
# multiple_owner_moves_rejected_as_scalar_fact,
# orphan_or_repeated_consuming_expression, stale_owned_move_flow_receipt,
# duplicate_final_owned_move_cfg_proof,
# c_array_descriptor_field_order_literal, c_array_string_positional_initializer,
# legacy_array_string_storage_alignment_literal,
# legacy_array_string_element_alignment_literal,
# llvm_owned_array_string_copyin_alignment_literal,
# llvm_dir_walk_array_string_storage_alignment_literal,
# llvm_local_array_string_storage_alignment_literal,
# llvm_operation_array_string_storage_alignment_literal,
# llvm_expression_array_string_storage_alignment_literal,
# llvm_string_join_element_alignment_literal,
# llvm_string_slice_copy_element_size_literal,
# llvm_string_slice_copy_descriptor_reconstruction.
# The original owner-handle/terminal/sealed-flow gates remain independent
# enforcement obligations; this layout gate does not expand lifetime support.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=array-string-layout-consumer-closure
STAGE="${PGY_ARRAY_STRING_LAYOUT_STAGE:-all}"
PYTHON="${PYTHON_BIN:-python3}"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ "$STAGE" == all || "$STAGE" == static || "$STAGE" == general ]] || fail "unknown stage $STAGE"
"$PYTHON" "$ROOT_DIR/tests/self_hosted/parity/array_string_layout_consumer_ratchet.py"
[[ "$STAGE" != static ]] || exit 0
pgy_require_runnable_binary_here "$LABEL" "$DRIVER"
command -v timeout >/dev/null || fail "missing bounded execution tool"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/array-string-layout-closure.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
echo "[$LABEL] evidence=$REL driver=$DRIVER"
export PGY_SELF_DRIVER_BIN="$DRIVER"

# The existing sources are reused, not unrelated shape/ownership inventories.
# Adjacent gates retain their own stronger negative contracts.
"$CLANG" -std=c11 -O0 -DPGY_LLVM_ENABLED "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK/runtime.o" \
    >"$WORK/runtime.compile.log" 2>&1 || fail "runtime object compilation failed"
general=0; legacy=0
names=(indexed push int-push builtins dirwalk args owned owned-return copyout readonly slice clone)
[[ "$STAGE" != all ]] || names+=(foreach legacy-storage pop reverse)
for name in "${names[@]}"; do
    run_args=()
    case "$name" in
        indexed) source_rel=src/self_hosted/codegen/fixture/str_array.pgy; expected=$'alice\nbob\ncarol\nBOB\n' ;;
        push) source_rel=src/self_hosted/codegen/fixture/str_array_push.pgy; expected=$'abbccc\n3\n' ;;
        foreach) source_rel=src/self_hosted/codegen/fixture/for_each.pgy; expected=$'60\nabbccc\n' ;;
        legacy-storage) source_rel=tests/self_hosted/fixtures/direct_mir_array_string_legacy_storage_consumers.pgy; expected=$'alice\nBOB\ncarol\ndave\nBOB\n4\n60\n' ;;
        pop) source_rel=src/self_hosted/codegen/fixture/array_pop.pgy; expected=$'30\n2\n2\na\n' ;;
        int-push) source_rel=src/self_hosted/codegen/fixture/array_push.pgy; expected=$'30\n5\n' ;;
        reverse) source_rel=src/self_hosted/codegen/fixture/array_reverse.pgy; expected=$'3\n2\n1\n3\n' ;;
        builtins) source_rel=src/self_hosted/codegen/fixture/str_builtins2.pgy; expected=$'yes\n3\nbb\na|bb|c\n43\n1\n2\nleft\nleft/right\n' ;;
        dirwalk) source_rel=tests/self_hosted/fixtures/direct_mir_dir_walk_direct_call.pgy; expected=$'2\n' ;;
        args) source_rel=tests/self_hosted/fixtures/direct_mir_process_args_direct_call.pgy; expected=$'first-value\n2\n'; run_args=(first-value second-value) ;;
        owned) source_rel=tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter.pgy; expected=$'released\n' ;;
        owned-return) source_rel=tests/self_hosted/fixtures/direct_mir_owned_array_string_return.pgy; expected=$'1\n2\n0\n1\n2\n0\n1\n2\n' ;;
        copyout) source_rel=tests/self_hosted/fixtures/direct_mir_bool_two_array_string_two_array_int_value_result.pgy; expected=$'mixed-four-copyouts-ready\nmixed-interleaved-ready\n1\n1\n1\n1\n' ;;
        readonly) source_rel=tests/self_hosted/fixtures/direct_mir_scalar_array_string_readonly_ref.pgy; expected=$'array-string-readonly-ref-ready\n' ;;
        slice) source_rel=tests/cases/backend_compare/slice_copy/main.pgy; expected=$'2\n20\n30\n2\nred\nblue\n0\n' ;;
        clone) source_rel=tests/concept_semantics/hashmap/string_array_clone_independence.pgy; expected=$'alpha\n' ;;
    esac
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source_rel" \
        -o "$REL/$name.mir.json") >"$WORK/$name.producer.log" 2>&1 || fail "$name producer refused"
    input_hash="$(sha256sum "$WORK/$name.mir.json" | cut -d' ' -f1)"
    printf '%s' "$expected" >"$WORK/$name.expected"
    if [[ "$name" == dirwalk ]]; then
        tree="$ROOT_DIR/.tmp/self_hosted/direct_mir_scalar_dir_walk_direct_call/tree"
        mkdir -p "$tree/sub"
        printf 'a\n' >"$tree/a.txt"; printf 'b\n' >"$tree/sub/b.txt"
    fi
    for backend in c llvm; do
        suffix=c; [[ "$backend" == llvm ]] && suffix=ll
        artifact="$REL/$name.$suffix"
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$REL/$name.mir.json" -o "$artifact") >"$WORK/$name-$backend.project.log" 2>&1 ||
            fail "$name $backend projection refused"
        command=("$CC" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing "$ROOT_DIR/$artifact")
        if [[ "$backend" == llvm ]]; then
            command=("$CLANG" -x ir "$ROOT_DIR/$artifact" -x none "$WORK/runtime.o" -pthread)
        elif pgy_selfhost_emitted_c_uses_runtime_headers "$ROOT_DIR/$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$WORK/$name-$backend.exe")
        "${command[@]}" >"$WORK/$name-$backend.compile.log" 2>&1 || fail "$name $backend compilation failed"
        if [[ "$name" == dirwalk ]]; then rm -f "$tree/written.txt"; fi
        (cd "$ROOT_DIR" && timeout 30 "$WORK/$name-$backend.exe" "${run_args[@]}") | tr -d '\r' >"$WORK/$name-$backend.run"
        cmp -s "$WORK/$name.expected" "$WORK/$name-$backend.run" || fail "$name $backend output drifted"
        [[ "$(sha256sum "$WORK/$name.mir.json" | cut -d' ' -f1)" == "$input_hash" ]] ||
            fail "$name $backend rewrote producer MIR"
        if [[ "$name" == dirwalk ]]; then
            [[ "$(tr -d '\r\n' <"$tree/written.txt")" == written ]] || fail "$backend DirWalk/WriteFile effect drifted"
        fi
    done
    # Attribute actual route markers, not historical fixture labels.
    if grep -Fq 'pgy_r0_block_' "$WORK/$name.c"; then
        grep -Fq 'pgy.r0.block.' "$WORK/$name.ll" || fail "$name route attribution disagrees"
        ((general+=1))
    else
        grep -Fq 'pgy.block.0' "$WORK/$name.ll" || fail "$name legacy route attribution is absent"
        ((legacy+=1))
    fi
    echo "[$LABEL] executed=$name C/LLVM"
done
if [[ "$STAGE" == all ]]; then
    ((general > 0 && legacy > 0)) || fail "both general and legacy consumers must execute"
    grep -Fq '%pgy.op.7.pop.length' "$WORK/pop.ll" || fail "legacy String pop leaf did not execute"
    [[ "$(grep -Fc 'sizeof(pgy_ai) ==' "$WORK/pop.c")" == 1 ]] ||
        fail "prefix pop must consume exactly one foreach-owned Int descriptor"
    ! grep -Fq 'pgy_r0_block_' "$WORK/legacy-storage.c" || fail "legacy storage coverage selected the general route"
    grep -Fq '@.pgy.scalar.cfg.indexed.push.' "$WORK/legacy-storage.ll" || fail "legacy String push leaf did not execute"
    grep -Fq '@.pgy.scalar.cfg.indexed.set.' "$WORK/legacy-storage.ll" || fail "legacy String set leaf did not execute"
    grep -Fq 'Array storage size receipt' "$WORK/foreach.c" || fail "shared C descriptor assertion omitted"
    route_dir="$WORK/route-mutations"
    mkdir -p "$route_dir"
    "$PYTHON" "$ROOT_DIR/tests/self_hosted/parity/array_string_layout_route_admission_mutations.py" \
        "$WORK/foreach.mir.json" "$route_dir" >"$route_dir/generation.log" 2>&1 ||
        fail "reached route mutation generation failed"
    route_refusals=0
    while IFS= read -r mutation; do
        mutation="${mutation%$'\r'}"
        for backend in c llvm; do
            output="$REL/route-mutations/$mutation.$backend"
            printf 'preserved:%s:%s\n' "$mutation" "$backend" >"$ROOT_DIR/$output"
            if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$REL/route-mutations/$mutation.mir.json" -o "$output") \
                >"$route_dir/$mutation-$backend.log" 2>&1; then
                fail "$backend accepted reached route $mutation"
            fi
            [[ "$(tr -d '\r\n' <"$ROOT_DIR/$output")" == "preserved:$mutation:$backend" ]] ||
                fail "$backend replaced rejected reached-route artifact $mutation"
            grep -Eq '(direct MIR|MIR-LOWER ERROR|Code:|CODEGEN ERROR)' \
                "$route_dir/$mutation-$backend.log" || fail "route refusal lost diagnostic"
            ((route_refusals+=1))
        done
    done <"$route_dir/mutations.list"
    echo "[$LABEL] reached-route refusals=$route_refusals; preserved artifacts"
fi
grep -Fq 'Array storage size receipt' "$WORK/slice.c" || fail "private C descriptor assertion omitted"

# Reuse producer-issued bases above. Repair every ABI row ID together, so a
# stale ID or contradictory occurrence is not the only reason for rejection.
bases=("$WORK/indexed.mir.json" "$WORK/copyout.mir.json"
    "$WORK/slice.mir.json" "$WORK/clone.mir.json")
[[ "$STAGE" != all ]] || bases+=("$WORK/legacy-storage.mir.json")
index=0; refusals=0
for input in "${bases[@]}"; do
    input_hash="$(sha256sum "$input" | cut -d' ' -f1)"
    mutation_dir="$WORK/mutation-$index"
    mkdir -p "$mutation_dir"
    "$PYTHON" "$ROOT_DIR/tests/self_hosted/parity/array_string_layout_consumer_mutations.py" \
        "$input" "$mutation_dir" >"$mutation_dir/generation.log" 2>&1 || {
        cat "$mutation_dir/generation.log" >&2; fail "coherent ABI mutation generation failed";
    }
    while IFS= read -r mutation; do
        mutation="${mutation%$'\r'}"
        for backend in c llvm; do
            output="$REL/mutation-$index/$mutation.$backend"
            printf 'preserved:%s:%s:%s\n' "$index" "$mutation" "$backend" >"$ROOT_DIR/$output"
            if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$REL/mutation-$index/$mutation.mir.json" -o "$output") \
                >"$mutation_dir/$mutation-$backend.log" 2>&1; then
                fail "$backend accepted $index:$mutation"
            fi
            [[ "$(tr -d '\r\n' <"$ROOT_DIR/$output")" == "preserved:$index:$mutation:$backend" ]] ||
                fail "$backend replaced rejected artifact $index:$mutation"
            grep -Eq '(direct MIR|MIR-LOWER ERROR|Code:|CODEGEN ERROR)' \
                "$mutation_dir/$mutation-$backend.log" || fail "refusal lost owned diagnostic"
            ((refusals+=1))
        done
    done <"$mutation_dir/mutations.list"
    [[ "$(sha256sum "$input" | cut -d' ' -f1)" == "$input_hash" ]] || fail "mutation changed its base"
    ((index+=1))
done
echo "[$LABEL] PASS stage=$STAGE: ${#names[@]} unique bases (general=$general legacy=$legacy); C/LLVM execution; $refusals coherent ABI refusals with preserved artifacts"

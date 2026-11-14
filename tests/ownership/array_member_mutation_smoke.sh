#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here array-member-mutation "$PGY"
cd "$ROOT_DIR"

mkdir -p "$ROOT_DIR/.tmp"
WORK="$(mktemp -d .tmp/array-member-mutation.XXXXXX)"
FIXTURE="tests/cases/backend_compare/array_member_mutation/main.pgy"
INVALID="tests/concept_semantics/array_member_mutation/temporary_receiver_invalid.pgy"
printf 'ARRAY MEMBER MUTATION PASS\n1234\n' >"$WORK/expected"

for backend in c llvm; do
    "$PGY" --native-pipeline --backend="$backend" "$FIXTURE" \
        -o "$WORK/$backend.exe" >"$WORK/$backend.compile.log" 2>&1 || {
            cat "$WORK/$backend.compile.log" >&2
            echo "[array-member-mutation] native $backend compile failed" >&2
            exit 1
        }
    "$WORK/$backend.exe" >"$WORK/$backend.raw" 2>"$WORK/$backend.err" || {
        cat "$WORK/$backend.raw" >&2
        cat "$WORK/$backend.err" >&2
        echo "[array-member-mutation] native $backend execution failed" >&2
        exit 1
    }
    tr -d '\r' <"$WORK/$backend.raw" >"$WORK/$backend.out"
    cmp -s "$WORK/expected" "$WORK/$backend.out" || {
        echo "[array-member-mutation] native $backend violated the fixed oracle" >&2
        diff -u "$WORK/expected" "$WORK/$backend.out" >&2 || true
        exit 1
    }
    [[ ! -s "$WORK/$backend.err" ]]
done

for name in indexed_member_store; do
    printf 'zz\n99\n12\n2\n2\n' >"$WORK/$name.expected"
    for backend in c llvm; do
        output="$WORK/$name.$backend"
        "$PGY" --native-pipeline --backend="$backend" \
            "tests/cases/backend_compare/$name/main.pgy" -o "$output.exe" \
            >"$output.compile.log" 2>&1 || {
                cat "$output.compile.log" >&2
                echo "[array-member-mutation] $name/$backend compile failed" >&2
                exit 1
            }
        "$output.exe" >"$output.raw" 2>"$output.err"
        tr -d '\r' <"$output.raw" >"$output.out"
        cmp -s "$WORK/$name.expected" "$output.out"
        [[ ! -s "$output.err" ]]
    done
done

for name in indexed_member_negative_index indexed_member_past_end; do
    for backend in c llvm; do
        output="$WORK/$name.$backend"
        "$PGY" --native-pipeline --backend="$backend" \
            "tests/cases/backend_compare/$name/main.pgy" -o "$output.exe" \
            >"$output.compile.log" 2>&1 || {
                cat "$output.compile.log" >&2
                exit 1
            }
        status=0
        "$output.exe" >"$output.out" 2>"$output.err" || status=$?
        [[ "$status" != 0 ]] && [[ ! -s "$output.out" ]] &&
            grep -Fq 'array index out of bounds' "$output.err" || {
                echo "[array-member-mutation] $name/$backend missed the checked target before RHS" >&2
                exit 1
            }
    done
done

# Capturing an indexed target creates a temporary loan of its backing until
# the store. A target-index or RHS call may read that backing, but cannot take
# mutable access that could invalidate the captured address.
for name in indexed_target_rhs_growth_invalid indexed_target_index_growth_invalid; do
    for backend in c llvm; do
        output="$WORK/$name.$backend"
        status=0
        "$PGY" --native-pipeline --backend="$backend" --error-format=json \
            "tests/concept_semantics/array_member_mutation/$name.pgy" \
            -o "$output.exe" >"$output.out" 2>"$output.err" || status=$?
        if [[ "$status" != 1 ]] || [[ -e "$output.exe" ]] ||
            ! grep -Fq 'PGY_SEM_BORROW_ESCAPE' "$output.out" "$output.err" ||
            ! grep -Fq 'indexed assignment keeps its backing address' "$output.out" "$output.err"; then
            echo "[array-member-mutation] $name/$backend did not fail closed before artifact publication" >&2
            cat "$output.out" "$output.err" >&2
            exit 1
        fi
    done
done

printf '7\n' >"$WORK/indexed_target_readonly_valid.expected"
for backend in c llvm; do
    output="$WORK/indexed_target_readonly_valid.$backend"
    "$PGY" --native-pipeline --backend="$backend" \
        tests/concept_semantics/array_member_mutation/indexed_target_readonly_valid.pgy \
        -o "$output.exe" >"$output.compile.log" 2>&1 || {
            cat "$output.compile.log" >&2
            echo "[array-member-mutation] readonly target loan/$backend compile failed" >&2
            exit 1
        }
    "$output.exe" >"$output.raw" 2>"$output.err"
    tr -d '\r' <"$output.raw" >"$output.out"
    cmp -s "$WORK/indexed_target_readonly_valid.expected" "$output.out"
    [[ ! -s "$output.err" ]]
done

# Keep a structural ratchet against the original C failure mode: ordered-call
# lowering must capture the exact member address, never a value descriptor.
"$PGY" --native-pipeline --backend=c --emit-c "$FIXTURE" \
    -o "$WORK/member.c" >"$WORK/emit-c.log" 2>&1
if ! grep -Eq '__auto_type __pgy_seq_[0-9]+_0 = &\(buffer\.values\);' \
    "$WORK/member.c"; then
    echo '[array-member-mutation] C lowering omitted one-level member address capture' >&2
    exit 1
fi
if ! grep -Eq '__auto_type __pgy_seq_[0-9]+_0 = &\(frame\.buffer\.values\);' \
    "$WORK/member.c"; then
    echo '[array-member-mutation] C lowering omitted nested member address capture' >&2
    exit 1
fi
if grep -Eq '__auto_type __pgy_seq_[0-9]+_0 = \((buffer|frame\.buffer)\.values\);' \
    "$WORK/member.c"; then
    echo '[array-member-mutation] C lowering copied a mutable member descriptor' >&2
    exit 1
fi

status=0
"$PGY" --native-pipeline --mir-json --error-format=json "$INVALID" \
    >"$WORK/temporary.out" 2>"$WORK/temporary.err" || status=$?
if [[ "$status" != 1 ]] ||
    ! grep -Fq 'PGY_SEM_BUILTIN_ARGS_INVALID' \
        "$WORK/temporary.out" "$WORK/temporary.err" ||
    ! grep -Fq 'requires addressable array storage rooted in a named binding' \
        "$WORK/temporary.out" "$WORK/temporary.err"; then
    echo "[array-member-mutation] temporary receiver did not fail closed (status $status)" >&2
    cat "$WORK/temporary.out" "$WORK/temporary.err" >&2
    exit 1
fi

echo '[array-member-mutation] exact nested/indexed backing writes, temporary backing loan, ref receiver value ABI, order/bounds, C/LLVM oracle, address capture and temporary refusal PASS'

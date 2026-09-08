#!/usr/bin/env bash
# Callee identity, not declaration order, owns a last-use Array<String> move.
# Callee-order permutation and independent returns preserve last-use C/LLVM moves with owned ABI and returned-use refusals
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-owned-array-string-callee-identity"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned-array-callee-identity.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $WORK_REL"

for name in first_callee late_callee independent_return real_join; do
    source_rel="tests/self_hosted/fixtures/direct_mir_owned_array_string_$name.pgy"
    mir_rel="$WORK_REL/$name.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source_rel" \
        -o "$mir_rel") >"$WORK_DIR/$name.producer.log" 2>&1 || {
        cat "$WORK_DIR/$name.producer.log" >&2; fail "$name MIR production failed";
    }
    case "$name" in
        first_callee|late_callee) printf '7\nreleased\n' ;;
        independent_return) printf '7\nreturned\n' ;;
        real_join) printf 'actual-owner\n' ;;
    esac >"$WORK_DIR/$name.expected.run"
    for backend in c llvm; do
        artifact_rel="$WORK_REL/$name.$backend"
        artifact="$ROOT_DIR/$artifact_rel"
        bin="$WORK_DIR/$name-$backend.exe"
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mir_rel" -o "$artifact_rel") >"$WORK_DIR/$name.$backend.project.log" 2>&1 || {
            cat "$WORK_DIR/$name.$backend.project.log" >&2; fail "$name/$backend projection failed";
        }
        [[ -s "$artifact" ]] || fail "$name/$backend emitted no artifact"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 "$artifact")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$bin")
        else
            command=("$CLANG" -x ir "$artifact" -o "$bin")
        fi
        "${command[@]}" >"$WORK_DIR/$name.$backend.compile.log" 2>&1 || {
            cat "$WORK_DIR/$name.$backend.compile.log" >&2; fail "$name/$backend compile failed";
        }
        "$bin" | tr -d '\r' >"$WORK_DIR/$name.$backend.run"
        cmp -s "$WORK_DIR/$name.expected.run" "$WORK_DIR/$name.$backend.run" ||
            fail "$name/$backend runtime output drifted"
    done
done

negative_rel="$WORK_REL/return-after-move.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    tests/self_hosted/fixtures/direct_mir_owned_array_string_return_after_move.pgy \
    -o "$negative_rel") >"$WORK_DIR/return-after-move.producer.log" 2>&1 || {
    cat "$WORK_DIR/return-after-move.producer.log" >&2; fail "negative MIR production failed";
}
for mutation in return-after-move owned-array-string-parameter-carriage \
    owned-array-string-parameter-pass owned-array-string-parameter-abi-layout \
    owned-array-string-parameter-abi-missing owned-array-string-parameter-call-target; do
    mutated_rel="$negative_rel"
    if [[ "$mutation" != return-after-move ]]; then
        mutated_rel="$WORK_REL/$mutation.mir.json"
        python "$MUTATIONS" "$WORK_DIR/late_callee.mir.json" \
            "$mutation" "$ROOT_DIR/$mutated_rel"
    fi
    for backend in c llvm; do
        artifact_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$artifact_rel") >"$WORK_DIR/$mutation.$backend.log" 2>&1; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$artifact_rel" ]] || fail "$backend published $mutation"
        grep -Eq '(CODEGEN|MIR-LOWER) ERROR:' "$WORK_DIR/$mutation.$backend.log" || {
            cat "$WORK_DIR/$mutation.$backend.log" >&2
            fail "$backend rejected $mutation without an owned diagnostic"
        }
    done
done
echo "[$LABEL] order permutation + independent return + real join C/LLVM runtime and negatives: PASS"

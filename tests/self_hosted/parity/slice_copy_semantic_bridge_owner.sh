#!/usr/bin/env bash
# Array.Slice and SliceCopy have one exact builtin identity, one direct-MIR
# owner, and the same observable result on native/public C/LLVM. An Array is
# not a SliceCopy operand, and a rejected compile may not publish an artifact.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:slice-copy-semantic-bridge"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/slice_copy_semantic_bridge"
WORK_DIR="$ROOT_DIR/$WORK_REL"
VALID_REL="tests/cases/backend_compare/slice_copy/main.pgy"
INVALID_REL="tests/self_hosted/fixtures/slice_copy_reject_array_operand.pgy"
CALL_IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_member_callee_identity_owner.pgy"
SLICE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_slice_builtin_owner.pgy"
LLVM_SLICE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_slice_expression_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
DRIVER="$(cd "$(dirname "$DRIVER")" && pwd -P)/$(basename "$DRIVER")"
suffix=""
if [[ "$PGY" == *.exe ]]; then suffix=".exe"; fi

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
printf '2\n20\n30\n2\nred\nblue\n0\n' >"$WORK_DIR/expected.run"

# Static negative ratchets: syntax-id zero is admitted only by the canonical
# builtin registry, and aggregate Slice returns remain inside one LLVM module
# instead of crossing a platform-dependent C ABI boundary.
grep -Fq 'SemanticBuiltinMemberCallableMatches(' "$CALL_IDENTITY_OWNER" ||
    fail "builtin member admission no longer consults the canonical registry"
grep -Fq 'name != "Array_Slice" && name != "SliceCopy"' "$SLICE_OWNER" ||
    fail "direct-MIR Slice owner lost its exact builtin boundary"
grep -Fq 'define internal %pgy.array.int @pgy.self.slice.copy.Int' "$LLVM_SLICE_OWNER" ||
    fail "LLVM Int SliceCopy no longer owns its aggregate-return ABI internally"
grep -Fq 'define internal %pgy.array.string @pgy.self.slice.copy.String' "$LLVM_SLICE_OWNER" ||
    fail "LLVM String SliceCopy no longer owns its aggregate-return ABI internally"
if grep -Eq '@pgy_(array_slice|slice_copy)_' "$LLVM_SLICE_OWNER"; then
    fail "LLVM Slice projection reopened the external aggregate-return ABI"
fi

for origin in native public; do
    for backend in c llvm; do
        stem="$origin-$backend"
        output_rel="$WORK_REL/$stem$suffix"
        command=("$PGY" "$VALID_REL" "--backend=$backend" --opt=dev -o "$output_rel")
        if [[ "$origin" == native ]]; then command+=(--native-pipeline); fi
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            timeout 90 "${command[@]}") \
            >"$WORK_DIR/$stem.compile.out" 2>"$WORK_DIR/$stem.compile.err" || {
            cat "$WORK_DIR/$stem.compile.out" "$WORK_DIR/$stem.compile.err" >&2
            fail "$stem valid Slice program did not compile"
        }
        if [[ "$origin" == public ]]; then
            ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.compile.err" ||
                fail "$stem re-entered the native pipeline"
        fi
        timeout 30 "$WORK_DIR/$stem$suffix" \
            >"$WORK_DIR/$stem.raw" 2>"$WORK_DIR/$stem.run.err" || {
            cat "$WORK_DIR/$stem.run.err" >&2
            fail "$stem valid Slice program did not run"
        }
        tr -d '\r' <"$WORK_DIR/$stem.raw" >"$WORK_DIR/$stem.run"
        [[ ! -s "$WORK_DIR/$stem.run.err" ]] ||
            fail "$stem valid Slice program emitted runtime diagnostics"
        cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$stem.run" || {
            diff -u "$WORK_DIR/expected.run" "$WORK_DIR/$stem.run" >&2
            fail "$stem Slice observation drifted"
        }

        rejected_rel="$WORK_REL/$stem-rejected$suffix"
        rejected="$WORK_DIR/$stem-rejected$suffix"
        rm -f "$rejected"
        reject_command=("$PGY" "$INVALID_REL" "--backend=$backend" --opt=dev -o "$rejected_rel")
        if [[ "$origin" == native ]]; then reject_command+=(--native-pipeline); fi
        set +e
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            timeout 90 "${reject_command[@]}") \
            >"$WORK_DIR/$stem.reject.out" 2>"$WORK_DIR/$stem.reject.err"
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] || fail "$stem accepted Array as a SliceCopy operand"
        [[ ! -e "$rejected" ]] || fail "$stem published a rejected artifact"
        if [[ "$origin" == native ]]; then
            grep -Fq 'SliceCopy requires Slice<T>' "$WORK_DIR/$stem.reject.err" || {
                cat "$WORK_DIR/$stem.reject.err" >&2
                fail "$stem rejection lost the native SliceCopy diagnostic"
            }
        else
            grep -Fq 'Code: ast_artifact_invalid' "$WORK_DIR/$stem.reject.out" &&
                grep -Fq 'owner: SemanticAstStatementTypeFacts:' "$WORK_DIR/$stem.reject.out" || {
                cat "$WORK_DIR/$stem.reject.out" "$WORK_DIR/$stem.reject.err" >&2
                fail "$stem rejection escaped the self-host semantic owner"
            }
            ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.reject.err" ||
                fail "$stem rejected through native fallback"
        fi
        echo "[$LABEL] $stem: valid observation + invalid operand rejection PASS"
    done
done

echo "[$LABEL] exact builtin identity and native/public C/LLVM bridge: PASS"

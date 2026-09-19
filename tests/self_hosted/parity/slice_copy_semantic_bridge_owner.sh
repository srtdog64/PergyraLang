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
GROWTH_REJECT_REL="tests/self_hosted/fixtures/slice_growth_after_borrow_reject.pgy"
GROWTH_SAFE_REL="tests/self_hosted/fixtures/slice_growth_before_borrow.pgy"
MEMBER_GROWTH_REJECT_REL="tests/self_hosted/fixtures/slice_member_growth_after_borrow_reject.pgy"
MEMBER_GROWTH_SAFE_REL="tests/self_hosted/fixtures/slice_member_growth_before_borrow.pgy"
MEMBER_SIBLING_SAFE_REL="tests/self_hosted/fixtures/slice_member_sibling_growth_while_borrowed.pgy"
CALL_IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_member_callee_identity_owner.pgy"
SLICE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_slice_builtin_owner.pgy"
LLVM_SLICE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_slice_expression_owner.pgy"
LLVM_ROUTE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_control_flow_route_owner.pgy"
RUNTIME_ALLOCATOR_OWNER="$ROOT_DIR/src/runtime/pgy_runtime_lib_allocator_exports.h"
NATIVE_SYMBOL_OWNER="$ROOT_DIR/src/semantic/symbol_table.h"
NATIVE_BORROW_OWNER="$ROOT_DIR/src/semantic/type_checker_ownership_let_slice.c"
PGY_PLACE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_storage_place_identity_owner.pgy"
PGY_MUTATION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_collection_mutation_owner.pgy"
CALL_TARGET_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_call_target_fact_owner.pgy"
CALL_ARGUMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy"

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
printf '99\n99\n' >"$WORK_DIR/growth-safe.expected"
printf '3\n' >"$WORK_DIR/member-growth-safe.expected"
printf '2\n2\n' >"$WORK_DIR/member-sibling-safe.expected"

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
grep -Fq 'pgy_alloc_export' "$LLVM_SLICE_OWNER" &&
    grep -Fq 'pgy_runtime_panic_internal_invariant_export' "$LLVM_SLICE_OWNER" ||
    fail "LLVM SliceCopy does not preserve allocation/invariant failure classes"
! grep -Fq 'call void @abort()' "$LLVM_SLICE_OWNER" ||
    fail "LLVM SliceCopy collapsed a typed failure back into abort"
grep -Fq 'pgy_alloc_export' "$RUNTIME_ALLOCATOR_OWNER" ||
    fail "runtime allocator export for LLVM SliceCopy is missing"
grep -Fq 'DirectMirScalarCfgSourceLocalTypeSupported(' "$LLVM_ROUTE_OWNER" ||
    fail "single-routine control-flow routing excludes collection locals"
grep -Fq 'ASTNode* slice_borrow_base_place;' "$NATIVE_SYMBOL_OWNER" ||
    fail "native Slice borrow lost its exact storage-place identity"
grep -Fq 'semantic_find_active_slice_borrow_for_array_place(' "$NATIVE_BORROW_OWNER" ||
    fail "native Array mutation no longer compares exact borrowed places"
grep -Fq 'func SemanticAstExpressionStoragePlaceEqual(' "$PGY_PLACE_OWNER" ||
    fail "self-host exact storage-place identity owner is missing"
grep -Fq 'SemanticAstExpressionStoragePlaceEqual(' "$PGY_MUTATION_OWNER" ||
    fail "self-host mutation admission bypasses exact storage-place identity"
grep -Fq 'SemanticExpressionGraphCallTargetSyntaxId(graph, call.call_node)' \
    "$CALL_TARGET_OWNER" ||
    fail "Slice target resolution lost the parser-carried builtin identity"
grep -Fq 'SemanticBuiltinMemberCallableMatches(' "$CALL_TARGET_OWNER" ||
    fail "Slice target resolution bypasses the builtin member registry"
grep -Fq 'SemanticBuiltinMemberSourceParameterOffset(' "$CALL_ARGUMENT_OWNER" ||
    fail "Slice argument admission lost the builtin receiver offset owner"

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

        growth_rejected="$WORK_DIR/$stem-growth-rejected$suffix"
        rm -f "$growth_rejected"
        growth_reject_command=("$PGY" "$GROWTH_REJECT_REL" \
            "--backend=$backend" --opt=dev \
            -o "$WORK_REL/$stem-growth-rejected$suffix")
        if [[ "$origin" == native ]]; then
            growth_reject_command+=(--native-pipeline)
        fi
        set +e
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            timeout 90 "${growth_reject_command[@]}") \
            >"$WORK_DIR/$stem.growth.reject.out" \
            2>"$WORK_DIR/$stem.growth.reject.err"
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] || fail "$stem admitted Array growth through a live Slice"
        [[ ! -e "$growth_rejected" ]] || fail "$stem published the rejected growth artifact"
        if [[ "$origin" == native ]]; then
            grep -Fq 'cannot change Array storage' \
                "$WORK_DIR/$stem.growth.reject.err" &&
                grep -Fq 'while Slice' "$WORK_DIR/$stem.growth.reject.err" ||
                fail "$stem growth rejection lost the native borrow diagnostic"
        else
            grep -Fq 'Code: slice_storage_invalidation' \
                "$WORK_DIR/$stem.growth.reject.out" ||
                fail "$stem growth rejection escaped the self-host Slice owner"
            ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.growth.reject.err" ||
                fail "$stem growth rejection retried the native pipeline"
        fi

        member_growth_rejected="$WORK_DIR/$stem-member-growth-rejected$suffix"
        rm -f "$member_growth_rejected"
        member_growth_reject_command=("$PGY" "$MEMBER_GROWTH_REJECT_REL" \
            "--backend=$backend" --opt=dev \
            -o "$WORK_REL/$stem-member-growth-rejected$suffix")
        if [[ "$origin" == native ]]; then
            member_growth_reject_command+=(--native-pipeline)
        fi
        set +e
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            timeout 90 "${member_growth_reject_command[@]}") \
            >"$WORK_DIR/$stem.member.growth.reject.out" \
            2>"$WORK_DIR/$stem.member.growth.reject.err"
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] ||
            fail "$stem admitted member Array growth through a live Slice"
        [[ ! -e "$member_growth_rejected" ]] ||
            fail "$stem published the rejected member-growth artifact"
        if [[ "$origin" == native ]]; then
            grep -Fq "cannot change Array storage for 'bag.values'" \
                "$WORK_DIR/$stem.member.growth.reject.err" &&
                grep -Fq "while Slice 'view' is live" \
                    "$WORK_DIR/$stem.member.growth.reject.err" ||
                fail "$stem member growth rejection lost exact place identity"
        else
            grep -Fq 'Code: slice_storage_invalidation' \
                "$WORK_DIR/$stem.member.growth.reject.out" ||
                fail "$stem member growth escaped the self-host Slice owner"
            ! grep -Fq '[pipeline timing]' \
                "$WORK_DIR/$stem.member.growth.reject.err" ||
                fail "$stem member growth retried the native pipeline"
        fi

        growth_safe="$WORK_DIR/$stem-growth-safe$suffix"
        safe_command=("$PGY" "$GROWTH_SAFE_REL" "--backend=$backend" \
            --opt=dev -o "$WORK_REL/$stem-growth-safe$suffix")
        if [[ "$origin" == native ]]; then safe_command+=(--native-pipeline); fi
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$DRIVER" timeout 90 "${safe_command[@]}") \
            >"$WORK_DIR/$stem.growth.safe.out" \
            2>"$WORK_DIR/$stem.growth.safe.err" || {
            cat "$WORK_DIR/$stem.growth.safe.out" \
                "$WORK_DIR/$stem.growth.safe.err" >&2
            fail "$stem failed safe growth-before-borrow compilation"
        }
        timeout 30 "$growth_safe" | tr -d '\r' \
            >"$WORK_DIR/$stem.growth.safe.run"
        cmp -s "$WORK_DIR/growth-safe.expected" \
            "$WORK_DIR/$stem.growth.safe.run" ||
            fail "$stem safe growth-before-borrow observation drifted"

        if [[ "$origin" == native ]]; then
            for member_case in growth-safe sibling-safe; do
                if [[ "$member_case" == growth-safe ]]; then
                    member_source="$MEMBER_GROWTH_SAFE_REL"
                    member_expected="$WORK_DIR/member-growth-safe.expected"
                else
                    member_source="$MEMBER_SIBLING_SAFE_REL"
                    member_expected="$WORK_DIR/member-sibling-safe.expected"
                fi
                member_output="$WORK_DIR/$stem-member-$member_case$suffix"
                (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
                    PGY_SELF_DRIVER_BIN="$DRIVER" timeout 90 \
                    "$PGY" "$member_source" "--backend=$backend" --opt=dev \
                    -o "$WORK_REL/$stem-member-$member_case$suffix" \
                    --native-pipeline) \
                    >"$WORK_DIR/$stem.member.$member_case.out" \
                    2>"$WORK_DIR/$stem.member.$member_case.err" || {
                    cat "$WORK_DIR/$stem.member.$member_case.out" \
                        "$WORK_DIR/$stem.member.$member_case.err" >&2
                    fail "$stem failed member-place $member_case compilation"
                }
                timeout 30 "$member_output" | tr -d '\r' \
                    >"$WORK_DIR/$stem.member.$member_case.run"
                cmp -s "$member_expected" \
                    "$WORK_DIR/$stem.member.$member_case.run" ||
                    fail "$stem member-place $member_case observation drifted"
            done
        fi
        echo "[$LABEL] $stem: valid observation + invalid operand rejection PASS"
    done
done

# Member Slice materialization is not yet a public-backend claim. The installed
# self-host semantic/MIR entrypoint must nevertheless admit both non-conflicting
# controls, proving the member-place comparison does not collapse every field
# of one root into one backing identity.
for admitted in "$MEMBER_GROWTH_SAFE_REL" "$MEMBER_SIBLING_SAFE_REL"; do
    admitted_name="$(basename "$admitted" .pgy)"
    (cd "$ROOT_DIR" && timeout 90 "$DRIVER" --emit-mir-json-verified \
        "$admitted" -o "$WORK_REL/$admitted_name.mir.json") \
        >"$WORK_DIR/$admitted_name.mir.out" \
        2>"$WORK_DIR/$admitted_name.mir.err" || {
        cat "$WORK_DIR/$admitted_name.mir.out" \
            "$WORK_DIR/$admitted_name.mir.err" >&2
        fail "self-host rejected non-conflicting member-place case $admitted_name"
    }
done

echo "[$LABEL] exact builtin identity, member-place lifetime, and native/public C/LLVM bridge: PASS"

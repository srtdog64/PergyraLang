#!/usr/bin/env bash
# One source-owned subject/action/zone/intent program reaches the public
# self-host LLVM route. Native LLVM is a comparator, not the semantic oracle;
# malformed admitted facts must fail without publishing an artifact.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

LABEL="self-host-direct-mir-legacy-intent-program-llvm"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
WORK_BASE="$ROOT_DIR/.tmp/self_hosted/direct_mir_legacy_intent_program_llvm"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE/run.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
SOURCE_REL="tests/self_hosted/parity/fixture/direct_mir_legacy_intent_program_llvm.pgy"
SELF_MIR_REL="$WORK_REL/self.mir.json"
SELF_MIR="$ROOT_DIR/$SELF_MIR_REL"
LLVM_REL="$WORK_REL/self.ll"
LLVM="$ROOT_DIR/$LLVM_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "missing Python"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$SELF_MIR_REL") \
    >"$WORK_DIR/self-mir.out" 2>"$WORK_DIR/self-mir.err" || {
        cat "$WORK_DIR/self-mir.out" "$WORK_DIR/self-mir.err" >&2
        fail "self MIR production failed"
    }
for fact in \
    '"kind":"class","nominal_kind":"subject","name":"Counter"' \
    '"kind":"class","nominal_kind":"zone","name":"CounterZone"' \
    '"field_kind":"subject_slot"' \
    '"name":"IncrementOnce","kind":"intent"' \
    '"name":"IntentMode"' \
    '"arg0":"exclusive","arg1":"IncrementOnce"' \
    '"name":"IntentEval"' \
    '"arg0":"priority","arg1":"IncrementOnce"' \
    '"expr0":"4"'; do
    grep -Fq "$fact" "$SELF_MIR" || fail "MIR omitted $fact"
done

(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$SELF_MIR_REL" -o "$LLVM_REL") \
    >"$WORK_DIR/self-llvm.out" 2>"$WORK_DIR/self-llvm.err" || {
        cat "$WORK_DIR/self-llvm.out" "$WORK_DIR/self-llvm.err" >&2
        fail "self direct-MIR LLVM projection failed"
    }
grep -Fq 'atomicrmw add ptr %generation, i32 1 release' "$LLVM" ||
    fail "zone generation synchronization is missing"
grep -Fq 'call i32 @pgy_intent_enter_export(ptr @.pgy.intent.name, ptr %subject.slot, i32 1, i1 false, i32 4)' "$LLVM" ||
    fail "mode/priority did not reach the runtime admission call"
grep -Fq 'call i1 @IncrementOnce(ptr %zone, ptr %subject)' "$LLVM" ||
    fail "Main did not reach the intent routine"
[[ "$(grep -Fc 'call void @pgy_mir_cleanup_op_export' "$LLVM")" -eq 9 ]] ||
    fail "rollback/invalidation cleanup emission drifted"

suffix=""
if [[ "$PGY" == *.exe ]]; then suffix=".exe"; fi
SELF_BIN="$WORK_DIR/self$suffix"
NATIVE_BIN="$WORK_DIR/native$suffix"
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE_REL" --backend=llvm \
    -o "$WORK_REL/self$suffix") \
    >"$WORK_DIR/self-compile.out" 2>"$WORK_DIR/self-compile.err" || {
        cat "$WORK_DIR/self-compile.out" "$WORK_DIR/self-compile.err" >&2
        fail "public self-host LLVM compile failed"
    }
! grep -Fq '[pipeline timing]' "$WORK_DIR/self-compile.err" ||
    fail "public self-host LLVM route re-entered the native compiler"
(cd "$ROOT_DIR" && "$PGY" "$SOURCE_REL" --native-pipeline --backend=llvm \
    -o "$WORK_REL/native$suffix") \
    >"$WORK_DIR/native-compile.out" 2>"$WORK_DIR/native-compile.err" || {
        cat "$WORK_DIR/native-compile.out" "$WORK_DIR/native-compile.err" >&2
        fail "native LLVM comparator compile failed"
    }
"$SELF_BIN" | tr -d '\r' >"$WORK_DIR/self.run"
"$NATIVE_BIN" | tr -d '\r' >"$WORK_DIR/native.run"
printf 'true\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/self.run" ||
    fail "public self-host LLVM runtime result drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/native.run" ||
    fail "native LLVM independent expected observation drifted"

# Scope renaming and formal ordering must not change the binder relation.
# These remain in the existing claimant envelope; no fixture-specific route.
for control in order names spelling; do
    source_rel="tests/self_hosted/parity/fixture/direct_mir_legacy_intent_binding_$control.pgy"
    expected=true
    if [[ "$control" == names ]]; then expected=false; fi
    if [[ "$control" == spelling ]]; then
        source_rel="tests/self_hosted/parity/fixture/intent_carrier_spelling_local.pgy"; expected=6
    fi
    printf '%s\n' "$expected" >"$WORK_DIR/binding-$control.expected"
    for origin in native public; do
        for backend in c llvm; do
            stem="binding-$control-$origin-$backend"
            command=("$PGY" "$source_rel" "--backend=$backend" --opt=dev
                -o "$WORK_REL/$stem$suffix")
            if [[ "$origin" == native ]]; then command+=(--native-pipeline); fi
            (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
                timeout 60 "${command[@]}") \
                >"$WORK_DIR/$stem.compile.out" 2>"$WORK_DIR/$stem.compile.err" ||
                fail "$stem compilation failed (see $WORK_DIR/$stem.compile.err)"
            if [[ "$origin" == public ]]; then
                ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.compile.err" ||
                    fail "$stem re-entered the native compiler"
            fi
            timeout 10 "$WORK_DIR/$stem$suffix" \
                >"$WORK_DIR/$stem.raw" 2>"$WORK_DIR/$stem.run.err" ||
                fail "$stem valid control execution failed"
            tr -d '\r' <"$WORK_DIR/$stem.raw" >"$WORK_DIR/$stem.run"
            [[ ! -s "$WORK_DIR/$stem.run.err" ]] || fail "$stem emitted runtime diagnostics"
            cmp -s "$WORK_DIR/binding-$control.expected" "$WORK_DIR/$stem.run" ||
                fail "$stem independent expected observation drifted"
        done
    done
done

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --mir-json "$SOURCE_REL") \
    >"$WORK_DIR/native.mir.json" 2>"$WORK_DIR/native.mir.err" ||
    fail "native MIR production failed"
# Both producer block-role/binder owners are checked without code emission.
# General native Main execution is still a separate LLVM obligation.
PROBE="$WORK_DIR/binder-probe$suffix"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --backend=c \
    tests/self_hosted/parity/fixture/intent_binding_identity_probe.pgy \
    -o "$WORK_REL/binder-probe$suffix") \
    >"$WORK_DIR/binder-probe.compile.out" 2>"$WORK_DIR/binder-probe.compile.err" ||
    fail "binder owner probe compilation failed"

negative_failures=0
negative_checks=0
for producer in self native; do
    mutation_dir="$WORK_DIR/$producer-negatives"
    mutation_rel="$WORK_REL/$producer-negatives"
    mkdir -p "$mutation_dir"
    "$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/direct_mir_legacy_intent_mutations.py" \
        "$WORK_DIR/$producer.mir.json" "$mutation_dir" || fail "$producer binder wire assertions failed"
    (cd "$ROOT_DIR" && "$PROBE" "$WORK_REL/$producer.mir.json") \
        >"$mutation_dir/baseline.out" 2>"$mutation_dir/baseline.err" ||
        fail "$producer binder owner baseline failed"
    grep -Fq 'Intent binder identity verified: 2' "$mutation_dir/baseline.out" ||
        fail "$producer binder projection lost source identities"
    for mutation in old-subject-wire crossed-nominal-kind duplicate-mode invalid-priority zone-field-drift \
        action-target-drift missing-invalidation-cleanup missing-call-target-id crossed-call-target-id \
        crossed-call-binder-id crossed-formal-order crossed-call-arguments missing-binding-id zero-binding-id \
        string-binding-id duplicate-binding-id crossed-mirror-id missing-mirror-id orphan-binding-mirror \
        nonbinding-identity duplicate-binding-key missing-block-role mistyped-block-role unknown-block-role \
        duplicate-root-block-role crossed-cleanup-block-role cyclic-body-block-role nonintent-block-role duplicate-block-role-key; do
        output_rel="$mutation_rel/$mutation.ll"
        output="$ROOT_DIR/$output_rel"
        command=("$DRIVER" --mir-json-backend=llvm "$mutation_rel/$mutation.mir.json" -o "$output_rel")
        diagnostic='^(CODEGEN|MIR-LOWER) ERROR:'
        if [[ "$producer" == native ]]; then
            case "$mutation" in
                *binding-id|crossed-mirror-id|missing-mirror-id|orphan-binding-mirror|nonbinding-identity|duplicate-binding-key|*block-role|duplicate-block-role-key) ;;
                *) continue ;;
            esac
            command=("$PROBE" "$mutation_rel/$mutation.mir.json")
            diagnostic='^CODEGEN ERROR: (MIR (Intent |intent |nonbinding carrier)|binder probe routine index)'
        fi
        [[ ! -e "$output" ]] || fail "negative artifact path already exists: $output"
        set +e
        (cd "$ROOT_DIR" && "${command[@]}") \
            >"$mutation_dir/$mutation.out" 2>"$mutation_dir/$mutation.err"
        rc=$?
        set -e
        if [[ "$rc" -eq 1 && ! -e "$output" ]] &&
            grep -Eq "$diagnostic" "$mutation_dir/$mutation.out" "$mutation_dir/$mutation.err"; then
            echo "[$LABEL] controlled refusal: $producer/$mutation"
        else
            echo "[$LABEL] FAIL: $producer/$mutation (exit $rc; see $mutation_dir/$mutation.{out,err})" >&2
            negative_failures=$((negative_failures + 1))
        fi
        negative_checks=$((negative_checks + 1))
    done
done
[[ "$negative_failures" -eq 0 ]] || fail "$negative_failures malformed-MIR checks failed"

echo "[$LABEL] baseline LLVM parity + twelve binding/spelling observations across four source legs + both producer binding owners + $negative_checks refusal-only checks: PASS"

#!/usr/bin/env bash
# Installed composition-root proof for one exact argv request owner. Read-only
# stdout and explicit artifact effects must not share a positional shape.
# Registry forbidden-fallback inventory enforced by this request-owner gate and
# the specialized public-mode gates named by the same registry row:
# raw_argv_reparse,
# optional_third_position_guess, same_argv_different_effect,
# implicit_default_source, artifact_without_explicit_output_token,
# unknown_option_as_path, test_fixture_manifest_in_production_root,
# public_token_native_fallback, public_token_oracle_self_compare,
# public_ast_native_fallback, public_ast_oracle_self_compare,
# public_machine_manifest_native_fallback,
# public_machine_manifest_reconstruction,
# public_capability_manifest_native_fallback,
# public_capability_manifest_oracle_self_compare,
# missing_capability_driver_native_retry,
# public_dir_native_fallback, public_dir_oracle_self_compare,
# missing_dir_driver_native_retry,
# source_c_artifact_machine_manifest_omission,
# source_c_machine_manifest_missing_or_corrupt_native_retry.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
source "$ROOT_DIR/tests/self_hosted/parity/compiler_root_intent_takeover_gate.sh"

DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/installed-driver-cli-mode"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/semantic/fixture/valid_call_int.pgy"
LLVM_SOURCE_REL="examples/hello.pgy"
MODE="--emit-c-artifact-verified"
FLAG_PATH="$ROOT_DIR/--emit-c-verified"
UNKNOWN_FLAG="--unknown-installed-driver-mode"
UNKNOWN_PATH="$ROOT_DIR/$UNKNOWN_FLAG"
OUTPUT_FLAG="--invalid-output-option"
OUTPUT_FLAG_PATH="$ROOT_DIR/$OUTPUT_FLAG"

fail() {
    echo "[self-host-installed-cli-mode] $*" >&2
    exit 1
}

if [[ "$DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${DRIVER}.exe"; then
    DRIVER="${DRIVER}.exe"
fi
[[ -x "$DRIVER" ]] || fail "installed self-host driver is missing: $DRIVER"
for path in "$FLAG_PATH" "$UNKNOWN_PATH" "$OUTPUT_FLAG_PATH"; do
    [[ ! -e "$path" ]] || fail "pre-existing flag-named path blocks proof: $path"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified \
    >"$WORK_DIR/stdout-1.c" 2>"$WORK_DIR/stdout-1.err") ||
    fail "explicit stdout mode failed"
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified \
    >"$WORK_DIR/stdout-2.c" 2>"$WORK_DIR/stdout-2.err") ||
    fail "repeated stdout mode failed"
[[ -s "$WORK_DIR/stdout-1.c" ]] || fail "stdout mode emitted empty C"
cmp -s "$WORK_DIR/stdout-1.c" "$WORK_DIR/stdout-2.c" ||
    fail "stdout mode is not byte-stable"
grep -Fq '#include' "$WORK_DIR/stdout-1.c" ||
    fail "stdout mode did not emit a C artifact"
[[ ! -e "$FLAG_PATH" ]] || fail "stdout option became a flag-named artifact"

(cd "$ROOT_DIR" && "$DRIVER" "$MODE" "$SOURCE_REL" \
    "$WORK_REL/artifact.c" >"$WORK_DIR/artifact.out" \
    2>"$WORK_DIR/artifact.err") || fail "explicit artifact mode failed"
[[ -s "$WORK_DIR/artifact.c" ]] || fail "artifact mode emitted no C"
[[ ! -s "$WORK_DIR/artifact.out" ]] || fail "artifact mode leaked payload to stdout"
grep -Fq '#include' "$WORK_DIR/artifact.c" ||
    fail "artifact mode published a non-C payload"
tr -d '\r' <"$WORK_DIR/stdout-1.c" >"$WORK_DIR/stdout.normalized.c"
tr -d '\r' <"$WORK_DIR/artifact.c" >"$WORK_DIR/artifact.normalized.c"
cmp -s "$WORK_DIR/stdout.normalized.c" "$WORK_DIR/artifact.normalized.c" ||
    fail "stdout and artifact modes emitted different C payloads"
if compgen -G "$WORK_DIR/artifact.c.pgy-tmp-*" >/dev/null; then
    fail "artifact transaction left a temporary publication"
fi

source_hash_before="$(sha256sum "$ROOT_DIR/$SOURCE_REL" | awk '{print $1}')"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    >"$WORK_DIR/source.mir.json" 2>"$WORK_DIR/source-mir.err") ||
    fail "source-MIR stdout mode remained shadowed"
grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/source.mir.json" ||
    fail "source-MIR stdout mode emitted no canonical MIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/source.artifact.mir.json" \
    >"$WORK_DIR/source-artifact.out" \
    2>"$WORK_DIR/source-artifact.err") ||
    fail "explicit source-MIR artifact mode failed"
[[ -s "$WORK_DIR/source.artifact.mir.json" ]] ||
    fail "source-MIR artifact mode emitted no artifact"
[[ ! -s "$WORK_DIR/source-artifact.out" ]] ||
    fail "source-MIR artifact mode leaked payload to stdout"
tr -d '\r\n' <"$WORK_DIR/source.mir.json" >"$WORK_DIR/source.stdout.normalized.json"
tr -d '\r\n' <"$WORK_DIR/source.artifact.mir.json" >"$WORK_DIR/source.artifact.normalized.json"
cmp -s "$WORK_DIR/source.stdout.normalized.json" \
    "$WORK_DIR/source.artifact.normalized.json" ||
    fail "source-MIR stdout and artifact payloads differ"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$LLVM_SOURCE_REL" \
    -o "$WORK_REL/llvm-source.mir.json" \
    >"$WORK_DIR/llvm-source-mir.out" 2>"$WORK_DIR/llvm-source-mir.err") ||
    fail "LLVM fixture source-MIR production failed"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$WORK_REL/llvm-source.mir.json" -o "$WORK_REL/direct-source.ll" \
    >"$WORK_DIR/direct-source.out" 2>"$WORK_DIR/direct-source.err") ||
    fail "direct source-MIR LLVM oracle failed"
(cd "$ROOT_DIR" && "$DRIVER" --emit-source-llvm-ir-verified \
    "$LLVM_SOURCE_REL" -o "$WORK_REL/intent-source.ll" \
    >"$WORK_DIR/intent-source.out" 2>"$WORK_DIR/intent-source.err") ||
    fail "canonical compiler intent execution failed"
cmp -s "$WORK_DIR/direct-source.ll" "$WORK_DIR/intent-source.ll" ||
    fail "compiler intent LLVM differs from the admitted direct-MIR projection"
[[ ! -s "$WORK_DIR/intent-source.out" ]] ||
    fail "compiler intent artifact mode leaked payload to stdout"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    >"$WORK_DIR/mir.c" 2>"$WORK_DIR/mir.err") ||
    fail "MIR-C stdout mode remained shadowed"
grep -Fq '#include' "$WORK_DIR/mir.c" ||
    fail "MIR-C stdout mode emitted no C"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    -o "$WORK_REL/mir.artifact.c" >"$WORK_DIR/mir-artifact.out" \
    2>"$WORK_DIR/mir-artifact.err") ||
    fail "explicit MIR-C artifact mode failed"
[[ -s "$WORK_DIR/mir.artifact.c" ]] ||
    fail "MIR-C artifact mode emitted no artifact"
[[ ! -s "$WORK_DIR/mir-artifact.out" ]] ||
    fail "MIR-C artifact mode leaked payload to stdout"
tr -d '\r' <"$WORK_DIR/mir.c" >"$WORK_DIR/mir.stdout.normalized.c"
tr -d '\r' <"$WORK_DIR/mir.artifact.c" >"$WORK_DIR/mir.artifact.normalized.c"
cmp -s "$WORK_DIR/mir.stdout.normalized.c" \
    "$WORK_DIR/mir.artifact.normalized.c" ||
    fail "MIR-C stdout and artifact payloads differ"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    --observe-mir-consumer-stages -o "$WORK_REL/mir.observed.c" \
    >"$WORK_DIR/mir-observed.out" 2>"$WORK_DIR/mir-observed.err") ||
    fail "pressure-observed MIR-C artifact mode failed"
[[ -s "$WORK_DIR/mir.observed.c" ]] ||
    fail "pressure-observed MIR-C mode emitted no artifact"
tr -d '\r' <"$WORK_DIR/mir.observed.c" >"$WORK_DIR/mir.observed.normalized.c"
cmp -s "$WORK_DIR/mir.stdout.normalized.c" \
    "$WORK_DIR/mir.observed.normalized.c" ||
    fail "pressure observation changed the MIR-C artifact payload"
for stage in 'consumer:input:start' 'consumer:input:done' \
    'consumer:c-emission:start' 'consumer:c-emission:done'; do
    grep -Fq -- "[driver-pressure-stage] $stage" \
        "$WORK_DIR/mir-observed.out" "$WORK_DIR/mir-observed.err" ||
        fail "pressure-observed MIR-C mode lost stage: $stage"
done
source "$ROOT_DIR/tests/self_hosted/parity/driver_mir_c_stdout_execution_action_gate.sh"
source_hash_after="$(sha256sum "$ROOT_DIR/$SOURCE_REL" | awk '{print $1}')"
[[ "$source_hash_before" == "$source_hash_after" ]] ||
    fail "stdout mode overwrote its source operand"

set +e
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" "$WORK_REL/legacy.c" \
    >"$WORK_DIR/legacy.out" 2>"$WORK_DIR/legacy.err")
legacy_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" "$UNKNOWN_FLAG" \
    >"$WORK_DIR/unknown.out" 2>"$WORK_DIR/unknown.err")
unknown_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" "$MODE" "$SOURCE_REL" \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err")
missing_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" "$MODE" "$SOURCE_REL" "$OUTPUT_FLAG" \
    >"$WORK_DIR/output-flag.out" 2>"$WORK_DIR/output-flag.err")
output_flag_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    "$WORK_REL/legacy-source.mir.json" \
    >"$WORK_DIR/legacy-source-mir.out" \
    2>"$WORK_DIR/legacy-source-mir.err")
legacy_source_mir_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    "$WORK_REL/legacy-mir.c" >"$WORK_DIR/legacy-mir-c.out" \
    2>"$WORK_DIR/legacy-mir-c.err")
legacy_mir_c_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o \
    >"$WORK_DIR/missing-source-output.out" \
    2>"$WORK_DIR/missing-source-output.err")
missing_source_output_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" -o \
    >"$WORK_DIR/missing-mir-output.out" \
    2>"$WORK_DIR/missing-mir-output.err")
missing_mir_output_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    -o "$WORK_REL/mir-c-missing-parent/out.c" \
    >"$WORK_DIR/rejected-mir-c.out" 2>"$WORK_DIR/rejected-mir-c.err")
rejected_mir_c_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --emit-source-llvm-ir-verified \
    "src/self_hosted/compiler/driver_bootstrap_main.pgy" \
    -o "$WORK_REL/rejected-source.ll" \
    >"$WORK_DIR/rejected-source-llvm.out" \
    2>"$WORK_DIR/rejected-source-llvm.err")
rejected_source_llvm_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" --emit-source-llvm-ir-verified \
    "$LLVM_SOURCE_REL" -o "$WORK_REL/llvm-missing-parent/out.ll" \
    >"$WORK_DIR/rejected-projection.out" \
    2>"$WORK_DIR/rejected-projection.err")
rejected_projection_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" >"$WORK_DIR/empty.out" 2>"$WORK_DIR/empty.err")
empty_rc=$?
set -e

[[ "$legacy_rc" -ne 0 && ! -e "$WORK_DIR/legacy.c" ]] ||
    fail "removed positional artifact form was accepted"
grep -Fq 'source C mode requires' "$WORK_DIR/legacy.out" ||
    fail "removed positional form lost its owned diagnostic"
[[ "$unknown_rc" -ne 0 && ! -e "$UNKNOWN_PATH" ]] ||
    fail "unknown option was accepted as an output path"
grep -Fq 'source C mode requires' "$WORK_DIR/unknown.out" ||
    fail "unknown option lost its owned diagnostic"
[[ "$missing_rc" -ne 0 ]] || fail "artifact mode accepted a missing output"
grep -Fq 'requires source and output paths' "$WORK_DIR/missing.out" ||
    fail "missing artifact output lost its owned diagnostic"
[[ "$output_flag_rc" -ne 0 && ! -e "$OUTPUT_FLAG_PATH" ]] ||
    fail "artifact mode accepted an option token as output"
grep -Fq 'output path must be a non-option path' \
    "$WORK_DIR/output-flag.out" || fail "output-option rejection lost its diagnostic"

[[ "$legacy_source_mir_rc" -ne 0 && \
    ! -e "$WORK_DIR/legacy-source.mir.json" ]] ||
    fail "ambiguous positional source-MIR artifact form was accepted"
grep -Fq 'source MIR mode requires' "$WORK_DIR/legacy-source-mir.out" ||
    fail "positional source-MIR rejection lost its owned diagnostic"
[[ "$legacy_mir_c_rc" -ne 0 && ! -e "$WORK_DIR/legacy-mir.c" ]] ||
    fail "ambiguous positional MIR-C artifact form was accepted"
grep -Fq 'MIR C mode requires' "$WORK_DIR/legacy-mir-c.out" ||
    fail "positional MIR-C rejection lost its owned diagnostic"
[[ "$missing_source_output_rc" -ne 0 ]] ||
    fail "source-MIR -o accepted a missing output"
[[ "$missing_mir_output_rc" -ne 0 ]] ||
    fail "MIR-C -o accepted a missing output"
[[ "$rejected_mir_c_rc" -ne 0 && \
    ! -e "$WORK_DIR/mir-c-missing-parent/out.c" ]] ||
    fail "MIR-C transaction rejection published an artifact"
grep -Fq 'artifact transaction rejected:' \
    "$WORK_DIR/rejected-mir-c.out" "$WORK_DIR/rejected-mir-c.err" ||
    fail "MIR-C transaction rejection lost its typed diagnostic"
[[ "$rejected_source_llvm_rc" -ne 0 && \
    ! -e "$WORK_DIR/rejected-source.ll" ]] ||
    fail "compiler intent source rejection published an LLVM artifact"
grep -Fq 'full driver MIR production requires pressure observation' \
    "$WORK_DIR/rejected-source-llvm.out" "$WORK_DIR/rejected-source-llvm.err" ||
    fail "compiler intent source-step rejection lost its typed diagnostic"
[[ "$rejected_projection_rc" -ne 0 && \
    ! -e "$WORK_DIR/llvm-missing-parent/out.ll" ]] ||
    fail "compiler intent projection rejection published an LLVM artifact"
grep -Fq 'artifact transaction rejected:' \
    "$WORK_DIR/rejected-projection.out" "$WORK_DIR/rejected-projection.err" ||
    fail "compiler intent projection-step rejection lost its typed diagnostic"
[[ "$empty_rc" -ne 0 ]] || fail "empty argv recovered an implicit source"
grep -Fq 'requires an explicit source or mode' "$WORK_DIR/empty.out" ||
    fail "empty argv rejection lost its owned diagnostic"

for artifact in artifact.c source.artifact.mir.json llvm-source.mir.json direct-source.ll \
    intent-source.ll mir.artifact.c mir.observed.c; do
    if compgen -G "$WORK_DIR/$artifact.pgy-tmp-*" >/dev/null; then
        fail "$artifact left a temporary publication"
    fi
done

source "$ROOT_DIR/tests/self_hosted/parity/public_mir_diagnostic_installed_self_host_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_artifact_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_return_type_mismatch_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_logical_operand_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_condition_not_bool_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_not_operand_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_compare_type_mismatch_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_let_type_mismatch_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_assign_type_mismatch_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_call_arg_type_mismatch_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_builtin_arg_type_mismatch_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_parser_callable_contract_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_tokens_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_ast_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_llvm_ir_json_diagnostic_receipt_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_native_ir_explicit_opt_in_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_repl_installed_self_host_compile_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_fmt_installed_self_host_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/public_device_slot_machine_manifest_installed_self_host_owner.sh"
echo "[self-host-installed-driver-cli] one typed argv owner keeps public MIR diagnostic, source-C, source-MIR, and MIR-C effects disjoint"
echo "[self-host-installed-driver-cli] general MIR-C world/action artifact parity, pressure observation, and transaction rejection: PASS"

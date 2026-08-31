#!/usr/bin/env bash
# Static ratchet for the bounded source-to-LLVM compiler-purpose takeover.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
WORLD="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
PROTOCOL="$ROOT_DIR/src/self_hosted/compiler/driver_source_llvm_intent_protocol_owner.pgy"
ACTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
PUBLICATION="$ROOT_DIR/src/self_hosted/compiler/driver_source_llvm_projection_publication_owner.pgy"
INTENT_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_source_llvm_intent_execution_owner.pgy"
ROOT_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/compiler_root_intent_execution_owner.pgy"
ARTIFACT_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_artifact_request_execution_owner.pgy"
REQUEST="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
INSTALLED="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
C_OWNER="$ROOT_DIR/src/compiler/self_host_llvm_driver.c"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"

fail() { echo "[compiler-root-intent-takeover] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }

for owner in "$WORLD" "$PROTOCOL" "$ACTION" "$PUBLICATION" "$INTENT_EXECUTION" "$ROOT_EXECUTION" \
    "$ARTIFACT_EXECUTION" "$REQUEST" "$INSTALLED" "$C_OWNER" "$PROCESS_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done

root_intent="$(awk '/^intent CompilePergyraProgram\(/{inside=1} inside{print}' "$WORLD")"
[[ "$(grep -Ec '^intent CompilePergyraProgram\(' "$WORLD")" -eq 1 ]] ||
    fail "canonical compiler intent must have one declaration"
[[ "$(grep -Ec '^[[:space:]]+step [A-Za-z]' <<<"$root_intent")" -eq 1 ]] ||
    fail "canonical compiler purpose must contain exactly one real-purpose action"
for term in 'intent_zone: DriverSourceLlvmIntentZone' \
    'intent_execution: DriverSourceLlvmIntentExecution' \
    'step Compile {' 'using: intent_zone;' \
    'on action_succeeded: intent_execution.Compile(' \
    'source_execution, direct_execution,' \
    'source_path, output_path, machine_declaration' \
    'expect: action_succeeded;' 'success: true;' 'failure: false;'; do
    grep -Fq -- "$term" <<<"$root_intent" || fail "root intent lost: $term"
done
grep -Eq 'FrontendPipeline|MiddleEndPipeline|BackendPipeline|SelfProofPipeline' \
    <<<"$root_intent" && fail "readiness-only root intent scaffold returned"
grep -Eq 'step ProduceMir|step PublishLlvm|success: DriverSourceMirPayloadAdmitted|failure: DriverLlvmPayloadPublicationRejected' \
    <<<"$root_intent" && fail "fixed source-MIR/LLVM lifecycle returned to the purpose intent"

for term in 'enum DriverSourceLlvmIntentOutcome' \
    'DriverSourceLlvmPublished(DriverRung2ExecutionReceipt)' \
    'DriverSourceLlvmSourceRejected(DriverSourceMirExecutionRejection)' \
    'DriverSourceLlvmProjectionRejected(DriverLlvmPayloadPublicationFailure)' \
    'func DriverSourceLlvmPublishedOutcome(' \
    'func DriverSourceLlvmSourceRejectedOutcome(' \
    'func DriverSourceLlvmProjectionRejectedOutcome(' \
    'func DriverSourceLlvmIntentOutcomeReadyFor(' \
    'func DriverSourceLlvmIntentOutcomeDiagnostic('; do
    require_text "$PROTOCOL" "$term"
done
for term in 'func DriverRung2PublishMirPayloadLlvmArtifactForIdentity(' \
    'source_receipt: DriverSourceMirPayloadReceipt' \
    'DriverSourceMirPayloadReceiptReadyFor(' \
    'with caps io_write {' \
    'CompileMirJsonTextToDirectBackendVerifiedObserved(' \
    'DriverRung2CommitArtifactForTarget('; do
    require_text "$PUBLICATION" "$term"
done
require_text "$ACTION" 'func DriverRung2CommitArtifactForTarget('
grep -Fq 'action PublishMirPayloadLlvmArtifact(' "$ACTION" &&
    fail "retired source-LLVM pass-through action returned"
grep -Eq 'DriverLlvmArtifactPublished|llvm_artifact_published' "$ACTION" "$PUBLICATION" &&
    fail "atomic artifact publication was duplicated as a domain lifecycle effect"

for term in 'subject DriverSourceLlvmIntentExecution {' \
    'let mut outcome: Option<DriverSourceLlvmIntentOutcome>;' \
    'action Compile(' 'within DriverSourceLlvmIntentZone' \
    'authorized by self' 'with caps io_read, io_write {' \
    'DriverSourceMirProducePayloadAdmitted(' \
    'DriverRung2PublishMirPayloadLlvmArtifactForIdentity(' \
    'case DriverSourceMirPayloadDenied(rejection):' \
    'case DriverLlvmPayloadPublicationRejected(failure):' \
    'public zone DriverSourceLlvmIntentZone {'; do
    require_text "$INTENT_EXECUTION" "$term"
done
grep -Eq '\.ProduceSourceMir\(|\.PublishMirPayloadLlvmArtifact\(' "$INTENT_EXECUTION" &&
    fail "compiler-purpose action bypassed its function owners through nested zone actions"

for term in 'func CompileSourceToLlvmThroughPgyCompilerWorld(' \
    'inout compiler_world: PgyCompilerWorld,' \
    'return compiler_world.CompileSourceToLlvm('; do
    require_text "$ROOT_EXECUTION" "$term"
done
for term in 'func CompileSourceToLlvm(self, source_path: String' \
    'zone source_llvm: DriverSourceLlvmIntentZone' \
    'let intent_zone: DriverSourceLlvmIntentZone = Clone(source_llvm);' \
    'Clone(source_llvm.execution);' \
    'Clone(source_mir.execution);' \
    'Clone(direct_mir.execution);' \
    'let completed: Bool = CompilePergyraProgram(' \
    'intent_zone, intent_execution, source_execution, direct_execution' \
    'if !IsSome(intent_execution.outcome)' \
    'completed != DriverSourceLlvmIntentOutcomeReadyFor('; do
    require_text "$WORLD" "$term"
done
grep -Fq 'PgyCompilerWorldMaterializeExecutableZones' "$ROOT_EXECUTION" &&
    fail "compiler intent execution returned to a by-value world factory"
for term in 'func DriverRung2InstalledPublishSourceLlvm(' \
    'CompileSourceToLlvmThroughPgyCompilerWorld(' \
    'IntentHistoryCount()' 'IntentLastName() != "CompilePergyraProgram"' \
    'IntentLastFailed()'; do
    require_text "$ARTIFACT_EXECUTION" "$term"
done
for term in 'DriverCliSourceLlvmArtifact(String, String, Bool)' \
    'args[0] == "--emit-source-llvm-ir-verified"' \
    'args[0] == "--emit-source-llvm-ir-json-diagnostic-verified"'; do
    require_text "$REQUEST" "$term"
done
require_text "$INSTALLED" 'case DriverCliSourceLlvmArtifact(source_path, output_path, emit_json):'

[[ "$(grep -Fc 'driver_run_self_host_artifact_process(' "$C_OWNER")" -eq 1 ]] ||
    fail "C host must invoke exactly one installed compiler intent"
require_text "$C_OWNER" '"--emit-source-llvm-ir-verified"'
require_text "$C_OWNER" '"--emit-source-llvm-ir-json-diagnostic-verified"'
require_text "$PROCESS_OWNER" 'pgy_exec_argv_capture_stdout('
grep -Eq 'driver_materialize_self_host_mir_artifact|--mir-json-backend=llvm|mir_output_path' \
    "$C_OWNER" && fail "C host regained source-MIR/backend orchestration"
grep -R -Eq 'driver_materialize_self_host_llvm_artifacts\(' "$ROOT_DIR/src" &&
    fail "retired plural C orchestration entrypoint returned"

echo "[compiler-root-intent-takeover] one Pergyra purpose intent/action owns source-to-LLVM coordination"

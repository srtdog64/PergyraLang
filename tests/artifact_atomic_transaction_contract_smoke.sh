#!/usr/bin/env bash
# Static ratchet for the compiler artifact transaction substitution seam.
# Forbidden fallbacks: raw_final_writer,
# duplicate_backend_transaction_algorithm, commit_without_typed_receipt,
# second_whole_graph_validation, crash_durability_without_sync,
# failure_tag_only, outcome_bool_collapse_before_last_consumer,
# receipt_as_authority_or_projection_source, unknown_failure_status,
# known_wrong_target_projection, source_mir_main_direct_commit,
# source_mir_file_helper_fallback.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT_DIR/src/runtime/pgy_runtime_artifact_transaction_core.h"
WRITER="$ROOT_DIR/src/self_hosted/mir/program_json_artifact_writer_owner.pgy"
INSTRUCTION_WRITER="$ROOT_DIR/src/self_hosted/mir/instruction_json_artifact_writer_owner.pgy"
TX_OWNER="$ROOT_DIR/src/self_hosted/mir/artifact_transaction_owner.pgy"
JSON_EMIT="$ROOT_DIR/src/self_hosted/lib/json_emit.pgy"
DRIVER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
ACTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
MAIN="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
CLI="$ROOT_DIR/src/self_hosted/compiler/driver_cli_owner.pgy"
SOURCE_ACTION_GATE="$ROOT_DIR/tests/self_hosted/parity/driver_source_mir_execution_action_gate.sh"

for path in "$CORE" "$WRITER" "$INSTRUCTION_WRITER" "$TX_OWNER" "$JSON_EMIT" \
    "$DRIVER" "$ACTION" "$MAIN" "$CLI" "$SOURCE_ACTION_GATE"; do
    [[ -f "$path" ]] || { echo "missing artifact transaction owner: $path" >&2; exit 1; }
done

for term in 'O_EXCL' 'CREATE_NEW' 'fflush(entry->stream)' \
    'MoveFileExA(temp_path, final_path, MOVEFILE_REPLACE_EXISTING)' \
    'rename(temp_path, final_path)' 'PGY_COMPILER_ARTIFACT_COMMIT_CLEANUP_FAILED' \
    'PGY_COMPILER_ARTIFACT_TXN_HANDLE_TAG'; do
    grep -Fq -- "$term" "$CORE" || {
        echo "artifact runtime core lost invariant: $term" >&2
        exit 1
    }
done
grep -Fq '#ifdef PGY_RUNTIME_ARTIFACT_TESTING' "$CORE"
if grep -Fq 'remove(entry->final_path)' "$CORE"; then
    echo "artifact transaction must never remove the final path before publish" >&2
    exit 1
fi

for term in 'tobject SelfMirArtifactReceipt' 'tobject SelfMirArtifactFailure' \
    'SelfMirArtifactCommitted(SelfMirArtifactReceipt)' \
    'atomic_visibility' 'crash_durable' 'CompilerArtifactAbort(artifact_handle)' \
    'CompilerArtifactCommit(artifact_handle)'; do
    grep -Fq -- "$term" "$TX_OWNER" || {
        echo "typed artifact transaction owner lost: $term" >&2
        exit 1
    }
done

for migrated in "$WRITER" "$INSTRUCTION_WRITER" "$JSON_EMIT" "$ACTION" "$CLI"; do
    if grep -Eq -- '(^|[^[:alnum:]_])(FileOpen|FileWrite|FileClose|WriteFile)\(' "$migrated"; then
        echo "raw file artifact bypass returned: ${migrated#"$ROOT_DIR/"}" >&2
        exit 1
    fi
done
if grep -Eq -- 'WriteFile\(' "$MAIN"; then
    echo "bootstrap compiler output bypassed artifact transaction" >&2
    exit 1
fi
grep -Fq -- 'args[0] == "--emit-c-artifact-verified"' "$MAIN" || {
    echo "source C artifact transaction lost its explicit mode" >&2
    exit 1
}
if grep -Fq -- 'if ArrayLength(args) == 2 {' "$MAIN"; then
    echo "generic source/output artifact fallback returned" >&2
    exit 1
fi

[[ "$(grep -F -c 'SelfMirProgramFactsReady(facts)' "$WRITER")" -eq 1 ]] || {
    echo "compatibility writer must validate raw MIR facts exactly once" >&2
    exit 1
}
grep -Fq 'SelfMirProgramJsonWriteArtifactVerified(' "$DRIVER"
if grep -Fq 'SelfMirProgramJsonWriteFile(projection.facts' "$DRIVER"; then
    echo "production source-to-MIR path reintroduced the validating wrapper" >&2
    exit 1
fi
grep -Fq 'DriverRung2Executed(DriverRung2ExecutionReceipt)' "$ACTION"
for term in 'match committed {' 'case SelfMirArtifactCommitted(receipt):' \
    'SelfMirArtifactReceiptReadyFor(receipt, output_path)' \
    'case SelfMirArtifactRejected(failure):' \
    'return DriverRung2ArtifactRejected(failure);'; do
    grep -Fq -- "$term" "$ACTION" || {
        echo "action collapsed a typed artifact outcome: $term" >&2
        exit 1
    }
done
if grep -Fq 'SelfMirArtifactCommitOutcomeReady(committed)' "$ACTION"; then
    echo "action collapsed artifact failure payload to Bool" >&2
    exit 1
fi
for term in 'failure.stage' 'failure.status' 'failure.prior_final_preserved' \
    'failure.temp_removed' 'failure.final_path'; do
    grep -Fq -- "$term" "$TX_OWNER" || {
        echo "artifact failure diagnostic dropped payload field: $term" >&2
        exit 1
    }
done
grep -Fq 'SelfMirArtifactCommitStatusKnown(failure.status)' "$TX_OWNER" || {
    echo "artifact failure admitted an unknown status identity" >&2
    exit 1
}

bash "$SOURCE_ACTION_GATE" >/dev/null

echo "[artifact-atomic-contract] one runtime owner; typed receipt; no raw final writer; single production validation"

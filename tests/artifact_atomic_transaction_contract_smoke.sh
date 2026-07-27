#!/usr/bin/env bash
# Static ratchet for the compiler artifact transaction substitution seam.
# Forbidden fallbacks: raw_final_writer,
# duplicate_backend_transaction_algorithm, commit_without_typed_receipt,
# second_whole_graph_validation, crash_durability_without_sync.
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

for path in "$CORE" "$WRITER" "$INSTRUCTION_WRITER" "$TX_OWNER" "$JSON_EMIT" \
    "$DRIVER" "$ACTION" "$MAIN" "$CLI"; do
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

[[ "$(grep -F -c 'SelfMirProgramFactsReady(facts)' "$WRITER")" -eq 1 ]] || {
    echo "compatibility writer must validate raw MIR facts exactly once" >&2
    exit 1
}
grep -Fq 'SelfMirProgramJsonWriteArtifactVerified(' "$DRIVER"
if grep -Fq 'SelfMirProgramJsonWriteFile(projection.facts' "$DRIVER"; then
    echo "production source-to-MIR path reintroduced the validating wrapper" >&2
    exit 1
fi
grep -Fq 'ArtifactCommitted,' "$ACTION"
grep -Fq 'SelfMirArtifactCommitOutcomeReady(committed)' "$ACTION"

echo "[artifact-atomic-contract] one runtime owner; typed receipt; no raw final writer; single production validation"

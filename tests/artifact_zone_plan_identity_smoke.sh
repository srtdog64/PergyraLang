#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="${PGY_VERIFIED_PROJECTION_PLAN_PROBE:-$ROOT_DIR/build/verified_projection_plan_probe.exe}"

require_text() {
    local file="$1"
    local needle="$2"
    if ! grep -Fq "$needle" "$ROOT_DIR/$file"; then
        echo "[artifact-zone] missing '$needle' in $file" >&2
        exit 1
    fi
}

require_text src/compiler/compiler.h "artifact_plan_revision"
require_text src/compiler/compiler.h "artifact_plan_digest"
require_text src/compiler/compiler_result.c "compiler_result_bind_artifact_identity"
require_text src/compiler/compiler_result.c "verified plan revision/digest is missing"
require_text src/compiler/compiler.c '"emitted_c"'
require_text src/compiler/compiler_llvm.c '"emitted_llvm"'
require_text src/compiler/c_runner.c "compiler_result_artifact_identity_ready"
require_text src/compiler/llvm_runner.c "compiler_result_artifact_identity_ready"
require_text src/compiler/verified_projection_plan.c "pgy_verified_projection_plan_identity_ready"
require_text src/self_hosted/compiler/artifact_zone_owner.pgy "CompilerArtifactPlanRevision"
require_text src/self_hosted/compiler/artifact_zone_owner.pgy "CompilerArtifactIdentityReady"

# path_as_artifact_identity and backend_output_without_plan_digest are
# forbidden: every runnable artifact must carry the verified plan identity.

if [[ ! -f "$PROBE" ]]; then
    echo "[artifact-zone] verified projection probe is missing: $PROBE" >&2
    exit 1
fi

probe_output="$($PROBE)"
grep -Fq "OBS0 erase and OBS1 materialize rows verified" <<<"$probe_output"
echo "[artifact-zone] native_C_LLVM_artifact_plan_identity: anchored revision and digest"

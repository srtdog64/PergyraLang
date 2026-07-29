#!/usr/bin/env bash
set -euo pipefail

# Owns the production source-to-MIR action boundary and its no-bypass ratchet.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
SOURCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"
WORLD_OWNER="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
COMPOSITION_OWNER="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
MIR_MANIFEST="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy"

fail() { echo "[driver-source-mir-execution-action] $1" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }

for owner in "$MAIN_OWNER" "$SOURCE_OWNER" "$WORLD_OWNER" "$COMPOSITION_OWNER" "$MIR_MANIFEST"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
for term in \
    'enum DriverSourceMirRequest' 'SourceMirVerified' 'SourceMirPressureObserved' \
    'tobject DriverSourceMirExecutionReceipt' 'tobject DriverSourceMirExecutionRejection' \
    'enum DriverSourceMirExecutionOutcome' 'DriverSourceMirExecuted(' \
    'DriverSourceMirRejected(' 'DriverSourceMirArtifactRejected(SelfMirArtifactFailure)' \
    'func DriverSourceMirRequestObservesPressure(' 'func DriverSourceMirIsFullDriverSource(' \
    'func DriverSourceMirExecutionOwnerIdentity(' 'func DriverSourceMirExecutionTopologyIdentity(' \
    'subject DriverSourceMirExecution' 'action EmitSourceMir(' 'within DriverSourceMirZone' \
    'authorized by self' 'public zone DriverSourceMirZone' \
    'subject slot execution: DriverSourceMirExecution' 'authority execution' \
    'full driver MIR production requires pressure observation' \
    'pressure-observed source MIR is only valid for the full driver' \
    'source MIR execution subject identity is invalid' \
    'source MIR execution topology identity is invalid' \
    'SelfMirArtifactCommitPayload(output_path, payload);' \
    'case SelfMirArtifactCommitted(receipt):' 'case SelfMirArtifactRejected(failure):'; do
    require_text "$SOURCE_OWNER" "$term"
done
[[ "$(grep -Ec -- '^[[:space:]]*subject DriverSourceMirExecution[[:space:]]*\{' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR owner must declare exactly one execution subject"
[[ "$(grep -Ec -- '^[[:space:]]*action EmitSourceMir\(' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR owner must declare exactly one execution action"
[[ "$(grep -Ec -- '^[[:space:]]*public zone DriverSourceMirZone[[:space:]]*\{' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR owner must declare exactly one execution zone"
[[ "$(grep -F -c -- 'SelfMirArtifactCommitPayload(output_path, payload);' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR action must own exactly one atomic payload commit"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonVerified(' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR action must consume the verified payload owner exactly once"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonPressureObserved(' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR action must consume the pressure payload owner exactly once"
if grep -Eq -- '(^|[^[:alnum:]_])(WriteFile|FileOpen|FileWrite|FileClose)\(' "$SOURCE_OWNER" ||
    grep -Eq -- '(SelfMirArtifactBegin|CompilerArtifactWrite|SelfMirArtifactCommit|SelfMirArtifactAbort)\(' "$SOURCE_OWNER"; then
    fail "source-MIR action bypassed the atomic payload owner"
fi
grep -Fq -- 'Fallback' "$SOURCE_OWNER" && fail "source-MIR action regained a fallback branch"

world_zones="$(
    awk '/^world PgyCompilerWorld[[:space:]]*\{/{inside=1; next} inside && /^}/{exit} inside && /^[[:space:]]+zone /{print}' "$WORLD_OWNER"
)"
world_zone_count="$(printf '%s\n' "$world_zones" | awk 'NF { count++ } END { print count + 0 }')"
first_world_zone="$(printf '%s\n' "$world_zones" | sed -n '1p')"
second_world_zone="$(printf '%s\n' "$world_zones" | sed -n '2p')"
[[ "$world_zone_count" -eq 2 ]] || fail "PgyCompilerWorld must expose exactly two executable zone fields"
[[ "$first_world_zone" == *'zone direct_mir: DriverRung2DirectMirZone'* ]] ||
    fail "direct-MIR zone is not the first world field"
[[ "$second_world_zone" == *'zone source_mir: DriverSourceMirZone'* ]] ||
    fail "source-MIR zone is not the second world field"
require_text "$WORLD_OWNER" 'func EmitSourceMir('
require_text "$WORLD_OWNER" 'self.source_mir.execution.EmitSourceMir('
world_declarations="$(
    rg -n -g '*.pgy' '^[[:space:]]*(public[[:space:]]+)?world[[:space:]]+' "$ROOT_DIR/src/self_hosted/compiler" || true
)"
world_declaration_count="$(printf '%s\n' "$world_declarations" | awk 'NF { count++ } END { print count + 0 }')"
[[ "$world_declaration_count" -eq 1 ]] || fail "compiler tree declared a second world"
world_declaration="$(printf '%s\n' "$world_declarations" | sed -n '1p' | tr '\\' '/')"
[[ "$world_declaration" == *'/src/self_hosted/compiler/world.pgy:'* ]] ||
    fail "PgyCompilerWorld moved from its canonical owner"
constructor_sites="$(
    rg -l -g '*.pgy' '(^|[^[:alnum:]_])PgyCompilerWorld\(' "$ROOT_DIR/src/self_hosted/compiler" || true
)"
constructor_site_count="$(printf '%s\n' "$constructor_sites" | awk 'NF { count++ } END { print count + 0 }')"
[[ "$constructor_site_count" -eq 1 ]] || fail "compiler tree must have one world materialization root"
constructor_site="$(printf '%s\n' "$constructor_sites" | sed -n '1p' | tr '\\' '/')"
[[ "$constructor_site" == *'/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy' ]] ||
    fail "world materialization escaped its composition owner"
[[ "$(grep -Ec -- '(^|[^[:alnum:]_])PgyCompilerWorld\(' "$COMPOSITION_OWNER")" -eq 1 ]] ||
    fail "composition root must materialize the world exactly once"
for term in 'DriverRung2DirectMirZone(' 'DriverSourceMirZone(' 'func EmitSourceMirThroughPgyCompilerWorld('; do
    require_text "$COMPOSITION_OWNER" "$term"
done
for term in 'EmitSourceMirThroughPgyCompilerWorld(' 'DriverSourceMirExecutionOutcomeReadyFor(' \
    'DriverSourceMirExecutionOutcomeDiagnostic(' 'SourceMirPressureObserved' 'SourceMirVerified' \
    'args[3] != "--pressure-owned-full-fixpoint"'; do
    require_text "$MAIN_OWNER" "$term"
done
[[ "$(grep -F -c -- 'EmitSourceMirThroughPgyCompilerWorld(' "$MAIN_OWNER")" -eq 1 ]] ||
    fail "Main must delegate source-MIR execution exactly once"
source_branch="$(awk '/args\[0\] == "--emit-mir-json-verified"/{inside=1} inside{print} inside && /^[[:space:]]*return;/{exit}' "$MAIN_OWNER")"
[[ -n "$source_branch" ]] || fail "Main source-MIR branch is missing"
grep -Fq -- 'SelfMirArtifactCommitPayload(' <<<"$source_branch" &&
    fail "Main source-MIR branch retained a direct artifact commit"
grep -Eq -- 'CompileSourceToMirJson(Verified|PressureObserved)\(' <<<"$source_branch" &&
    fail "Main source-MIR branch retained direct payload compilation"
if rg -n -g '*.pgy' 'CompileSourceToMirJsonFile(Verified|PressureObserved)\(' "$ROOT_DIR/src/self_hosted" >/dev/null; then
    fail "retired source-MIR file helper definition or call returned"
fi
require_text "$MIR_MANIFEST" 'examples/function_clause_order_minimal.pgy'
require_text "$ROOT_DIR/Makefile" 'function_clause_order_minimal'
echo "[driver-source-mir-execution-action] source-MIR world/action/pressure/commit ratchet PASS"

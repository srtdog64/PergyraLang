#!/usr/bin/env bash
# Fast recursive-topology contract for the Pergyra self-host compiler world.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORLD="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
STAGES="$ROOT_DIR/src/self_hosted/compiler/stage_intents.pgy"
EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
COMPOSITION="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
MAIN="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
COMPILER_ROOT="$ROOT_DIR/src/self_hosted/compiler"
ARCH="$ROOT_DIR/docs/self_hosted/11_compiler_world_architecture.md"

fail() {
    echo "[self-host-compiler-topology] $*" >&2
    exit 1
}

[[ -f "$WORLD" ]] || fail "missing compiler world"
[[ -f "$STAGES" ]] || fail "missing stage intent owner"
[[ -f "$EXECUTION" ]] || fail "missing direct-MIR execution owner"
[[ -f "$COMPOSITION" ]] || fail "missing direct-MIR world composition owner"
[[ -f "$MAIN" ]] || fail "missing production compiler entrypoint"
[[ -f "$ARCH" ]] || fail "missing compiler world architecture"

awk '
    /^(public[[:space:]]+)?zone[[:space:]]+[A-Za-z0-9_]+[[:space:]]*\{/ {
        zones++
        if ($0 ~ /^(public[[:space:]]+)?zone[[:space:]]+DriverRung2DirectMirZone[[:space:]]*\{/) {
            direct_zone++
        }
    }
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ {
        worlds++
        in_world = 1
        next
    }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        members++
        if (members == 1 &&
            $0 ~ /^[[:space:]]+zone[[:space:]]+direct_mir:[[:space:]]+DriverRung2DirectMirZone[[:space:]]*$/) {
            direct_member_is_first++
        }
        if ($0 ~ /^[[:space:]]+zone[[:space:]]+direct_mir:[[:space:]]+DriverRung2DirectMirZone[[:space:]]*$/) {
            direct_member++
        }
    }
    in_world && /^}/ { in_world = 0 }
    /intent[[:space:]]+CompilePergyraProgram[[:space:]]*\(/ { compile_intent++ }
    /step[[:space:]]+Frontend[[:space:]]*\{/ { frontend++ }
    /step[[:space:]]+MiddleEnd[[:space:]]*\{/ { middle_end++ }
    /step[[:space:]]+Evidence[[:space:]]*\{/ { evidence++ }
    /step[[:space:]]+Backend[[:space:]]*\{/ { backend++ }
    /step[[:space:]]+SelfProof[[:space:]]*\{/ { self_proof++ }
    /SelfHostCompiler/ { duplicate_aggregate++ }
    END {
        if (zones != 19 || worlds != 1 || members != 19 ||
            direct_zone != 1 || direct_member != 1 ||
            direct_member_is_first != 1 ||
            compile_intent != 1 || frontend != 1 || middle_end != 1 ||
            evidence != 1 || backend != 1 || self_proof != 1 ||
            duplicate_aggregate != 0) {
            exit 1
        }
    }
' "$WORLD" "$EXECUTION" || fail "world/resource topology drifted"

first_world_member="$(awk '
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ {
        in_world = 1
        next
    }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        print $2 "|" $3
        exit
    }
' "$WORLD")"
[[ "$first_world_member" == \
    'direct_mir:|DriverRung2DirectMirZone' ]] ||
    fail "direct_mir must remain PgyCompilerWorld's first positional field"

compiler_world_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?world[[:space:]]+' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$compiler_world_count" -eq 1 ]] ||
    fail "compiler graph must declare exactly one world"

for owner_term in \
    "$EXECUTION|public zone DriverRung2DirectMirZone" \
    "$EXECUTION|within DriverRung2DirectMirZone" \
    "$EXECUTION|authorized by self" \
    "$EXECUTION|subject slot execution: DriverRung2Execution" \
    "$EXECUTION|authority execution" \
    "$WORLD|zone direct_mir: DriverRung2DirectMirZone" \
    "$WORLD|return self.direct_mir.execution.EmitDirectMir(" \
    "$COMPOSITION|import \"world.pgy\";" \
    "$COMPOSITION|func EmitDirectMirThroughPgyCompilerWorld(" \
    "$COMPOSITION|let compiler_world = PgyCompilerWorld(" \
    "$COMPOSITION|DriverRung2DirectMirZone(" \
    "$COMPOSITION|return compiler_world.EmitDirectMir(" \
    "$MAIN|import \"compiler_world_direct_mir_owner.pgy\";" \
    "$MAIN|EmitDirectMirThroughPgyCompilerWorld("; do
    owner="${owner_term%%|*}"
    term="${owner_term#*|}"
    [[ "$(grep -F -c -- "$term" "$owner")" -eq 1 ]] ||
        fail "direct-MIR topology fact must occur exactly once: $term"
done

if grep -Fq -- 'import "driver_rung2_execution_owner.pgy";' "$MAIN" ||
    grep -Fq -- 'import "world.pgy";' "$MAIN" ||
    grep -Fq -- '.EmitDirectMir(' "$MAIN"; then
    fail "production Main bypassed the single compiler-world composition path"
fi

direct_zone_decl_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+DriverRung2DirectMirZone[[:space:]]*\{' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$direct_zone_decl_count" -eq 1 ]] ||
    fail "DriverRung2DirectMirZone must have exactly one declaration"
[[ "$(grep -F -c -- 'DriverRung2DirectMirZone' "$WORLD")" -eq 1 ]] ||
    fail "PgyCompilerWorld must bind DriverRung2DirectMirZone exactly once"

awk '
    /^intent[[:space:]]+FrontendPipeline[[:space:]]*\(/ { frontend++ }
    /^intent[[:space:]]+MiddleEndPipeline[[:space:]]*\(/ { middle_end++ }
    /^intent[[:space:]]+BackendPipeline[[:space:]]*\(/ { backend++ }
    /^intent[[:space:]]+SelfProofPipeline[[:space:]]*\(/ { self_proof++ }
    END {
        if (frontend != 1 || middle_end != 1 || backend != 1 ||
            self_proof != 1) {
            exit 1
        }
    }
' "$STAGES" || fail "derived stage-intent topology drifted"

awk '
    /^Status: `hard-self-host-shape-contract \/ direct-MIR REACHABLE BRIDGE`$/ { status++ }
    /## Recursive Topology Rule/ { rule++ }
    /## Target Resource Facade/ { facade++ }
    /resource zone -> intent transition -> typed owner fact -> final consumer/ {
        chain++
    }
    /carriage decision remains positional by default/ { positional++ }
    /not the literal golden ratio/ { balance++ }
    /^\| `FrontendResources\.Intake` \| `SourceIntakeZone` \|$/ { intake++ }
    /^\| `FrontendResources\.Token` \| `TokenStreamZone` \|$/ { token++ }
    /^\| `FrontendResources\.AST` \| `AstTreeZone` \|$/ { ast++ }
    /^\| `MiddleEndResources\.Semantic` \| `SemanticVerdictZone`, `TypeEnvZone` \|$/ { semantic++ }
    /^\| `MiddleEndResources\.MIR` \| `MirFactGraphZone` \|$/ { mir++ }
    /^\| `EvidenceResources\.AIR` \| `AirEvidenceZone` \|$/ { air++ }
    /^\| `EvidenceResources\.Compatibility` \| `CompatibilityEvolutionZone` \|$/ { compatibility++ }
    /^\| `EvidenceResources\.ABI` \| `AbiLayoutZone`, `AbiRowProjectionZone` \|$/ { abi++ }
    /^\| `BackendResources\.Target` \| `TargetCapabilityZone`, `SandboxCapabilityZone` \|$/ { target++ }
    /^\| `BackendResources\.Emission` \| `SymbolFactTableZone`, `EmissionZone` \|$/ { emission++ }
    /^\| `BackendResources\.Artifact` \| `ArtifactZone`, `TestHarnessZone`, `SubprocessRunnerZone`, `ParityZone` \|$/ { artifact++ }
    /^\| `BackendResources\.DirectMIR` \| `DriverRung2DirectMirZone` \|$/ { direct_mir++ }
    /driver_bootstrap_main\.Main -> EmitDirectMirThroughPgyCompilerWorld/ { reachable_path++ }
    /exactly 19 concrete resource zones and 19 world members/ { exact_topology++ }
    /target facade projection, not a claim that four new aggregate zones/ {
        projection++
    }
    /It must not be emitted as a nested value bundle/ { no_bundle++ }
    END {
        if (status != 1 || rule != 1 || facade != 1 || chain != 1 || positional != 1 ||
            balance != 1 || intake != 1 || token != 1 || ast != 1 ||
            semantic != 1 || mir != 1 || air != 1 || compatibility != 1 ||
            abi != 1 || target != 1 || emission != 1 || artifact != 1 ||
            direct_mir != 1 || reachable_path != 1 || exact_topology != 1 ||
            projection != 1 || no_bundle != 1) {
            exit 1
        }
    }
' "$ARCH" || fail "recursive topology contract documentation drifted"

echo "[self-host-compiler-topology] recursive owner topology ok"

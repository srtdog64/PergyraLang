#!/usr/bin/env bash
# Fast recursive-topology contract for the Pergyra self-host compiler world.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORLD="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
STAGES="$ROOT_DIR/src/self_hosted/compiler/stage_intents.pgy"
ARCH="$ROOT_DIR/docs/self_hosted/11_compiler_world_architecture.md"

fail() {
    echo "[self-host-compiler-topology] $*" >&2
    exit 1
}

[[ -f "$WORLD" ]] || fail "missing compiler world"
[[ -f "$STAGES" ]] || fail "missing stage intent owner"
[[ -f "$ARCH" ]] || fail "missing compiler world architecture"

awk '
    /^zone[[:space:]]+[A-Za-z0-9_]+[[:space:]]*\{/ { zones++ }
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ {
        worlds++
        in_world = 1
        next
    }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        members++
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
        if (zones != 18 || worlds != 1 || members != 18 ||
            compile_intent != 1 || frontend != 1 || middle_end != 1 ||
            evidence != 1 || backend != 1 || self_proof != 1 ||
            duplicate_aggregate != 0) {
            exit 1
        }
    }
' "$WORLD" || fail "world/resource topology drifted"

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
    /target facade projection, not a claim that four new aggregate zones/ {
        projection++
    }
    /It must not be emitted as a nested value bundle/ { no_bundle++ }
    END {
        if (rule != 1 || facade != 1 || chain != 1 || positional != 1 ||
            balance != 1 || intake != 1 || token != 1 || ast != 1 ||
            semantic != 1 || mir != 1 || air != 1 || compatibility != 1 ||
            abi != 1 || target != 1 || emission != 1 || artifact != 1 ||
            projection != 1 || no_bundle != 1) {
            exit 1
        }
    }
' "$ARCH" || fail "recursive topology contract documentation drifted"

echo "[self-host-compiler-topology] recursive owner topology ok"

#!/usr/bin/env bash
# Gates the self-hosted compiler source shape. Hard substitution should grow as
# Pergyra world/intent-owned flow, not as a copy of the C compiler folder graph.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-compiler-world] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

require_max_lines() {
    local rel="$1"
    local cap="$2"
    local count
    count="$(wc -l < "$ROOT_DIR/$rel" | tr -d ' ')"
    [[ "$count" -le "$cap" ]] ||
        fail "$rel has $count lines; cap is $cap"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden term: $term"
    fi
}

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"
pgy_prepend_windows_runtime_paths

require_file "src/self_hosted/compiler/README.md"
require_file "src/self_hosted/compiler/world.pgy"
require_file "src/self_hosted/compiler/stage_intents.pgy"
require_file "docs/self_hosted/11_compiler_world_architecture.md"
require_file "docs/self_hosted/12_intent_zone_self_host_architecture.md"
require_file "docs/self_hosted/13_compiler_substrate_architecture.md"
require_file "tests/self_host_compiler_world_contract_smoke.sh"
require_file "tests/self_hosted/compiler_world_manifest.sh"

pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
    fail "compiler world path manifest is incomplete"
pgy_compiler_world_require_stage_conformance "$ROOT_DIR" ||
    fail "compiler world stages do not conform to the on-disk stage owners"
require_max_lines "src/self_hosted/compiler/world.pgy" 600

for term in \
    "world PgyCompilerWorld" \
    "zone SelfHostCompiler" \
    "zone SourceIntakeZone" \
    "zone TokenStreamZone" \
    "zone AstTreeZone" \
    "zone SemanticVerdictZone" \
    "zone MirFactGraphZone" \
    "zone TypeEnvZone" \
    "zone EmissionZone" \
    "zone ParityZone" \
    "intent CompilePergyraProgram" \
    "step Frontend" \
    "step MiddleEnd" \
    "step Backend" \
    "step SelfProof" \
    "FrontendPipeline" \
    "MiddleEndPipeline" \
    "BackendPipeline" \
    "SelfProofPipeline" \
    "import \"stage_intents.pgy\"" \
    "intent IntakeSource" \
    "intent LexSource" \
    "intent ParseTokens" \
    "intent CheckProgramSemantics" \
    "intent LowerProgramFacts" \
    "intent EmitProgramArtifact" \
    "intent ProveSelfHostedParity" \
    "subject SourceUnit" \
    "subject StageOwner" \
    "subject ProgramEmitter" \
    "action Emit" \
    "subject OraclePair" \
    "object SourceBatch" \
    "object StagePathManifest" \
    "compiler_world: String" \
    "source_dir: String" \
    "test_dir: String" \
    "parity_dir: String" \
    "object TokenStream" \
    "object AstTree" \
    "object SemanticVerdict" \
    "object MirFactGraph" \
    "object TypeEnvironment" \
    "object EmittedC" \
    "tobject ParityVerdict" \
    "subject slot emitter: ProgramEmitter"; do
    require_text "src/self_hosted/compiler/world.pgy" "$term"
done

for term in \
    "intent FrontendPipeline" \
    "intent MiddleEndPipeline" \
    "intent BackendPipeline" \
    "intent SelfProofPipeline" \
    "step Intake" \
    "step Lex" \
    "step Parse" \
    "step Check" \
    "step Lower" \
    "step Emit" \
    "step Prove" \
    "IntakeSource(intake, source)" \
    "LexSource(tokens, owner)" \
    "ParseTokens(ast, owner)" \
    "CheckProgramSemantics(semantic_zone, owner)" \
    "LowerProgramFacts(lower_zone, owner)" \
    "EmitProgramArtifact(emit_zone, types, emitter)" \
    "ProveSelfHostedParity(parity_zone, oracle)"; do
    require_text "src/self_hosted/compiler/stage_intents.pgy" "$term"
done

for term in \
    "zone ProgramEmitZone" \
    "zone FunctionEmitZone" \
    "zone StmtEmitZone" \
    "zone ExprRewriteZone" \
    "zone StructValueEmitZone"; do
    forbid_text "src/self_hosted/compiler/world.pgy" "$term"
done

require_text "src/self_hosted/compiler/README.md" "Compiler World"
require_text "src/self_hosted/compiler/README.md" "PgyCompilerWorld"
require_text "src/self_hosted/compiler/README.md" "world.pgy"
require_text "src/self_hosted/compiler/README.md" '`world.pgy` stays under the same 600-line cap'
require_text "src/self_hosted/compiler/README.md" "resource-owned intent cluster"
require_text "src/self_hosted/compiler/README.md" "does this boundary own a distinct resource"
forbid_text "src/self_hosted/compiler/README.md" "mirrors the C-side"
forbid_text "src/self_hosted/compiler/README.md" "intentionally empty"
forbid_text "src/self_hosted/compiler/README.md" "??"

require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/world.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/stage_intents.pgy"
require_text "src/self_hosted/README.md" "compiler/world.pgy"
require_text "docs/self_hosted/README.md" "11_compiler_world_architecture.md"
require_text "docs/self_hosted/README.md" "12_intent_zone_self_host_architecture.md"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "## Compiler World Rule"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "PgyCompilerWorld"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "No Compiler World exception exists for the 600-line cap"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "resource-owned intent cluster"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "resource ownership boundary"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "not a module, folder, phase, or helper"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "TypeEnvZone"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "ProgramEmitter"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "object slot c_output: EmittedC"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "backend resource cluster"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "not a semantic zone split"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "compiler flow owner"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "stage_intents.pgy"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "Codegen Shape"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "Path And Source Intake"
require_text "src/self_hosted/compiler/README.md" "resource ownership boundary"
require_text "src/self_hosted/compiler/README.md" "ProgramEmitter"
require_text "src/self_hosted/codegen/intent.md" "EmissionZone"
require_text "src/self_hosted/codegen/intent.md" "TypeEnvZone"
require_text "src/self_hosted/codegen/intent.md" "ProgramEmitter"
require_text "src/self_hosted/codegen/intent.md" "participants in the emission action graph"
require_text "src/self_hosted/codegen/intent.md" "does this boundary own a distinct resource"
require_text "src/self_hosted/codegen/intent.md" "recursive participants over the same output/type resources"
require_text "docs/INDEX.md" "self_hosted/11_compiler_world_architecture.md"
require_text "docs/INDEX.md" "self_hosted/12_intent_zone_self_host_architecture.md"
require_text "Makefile" "self-host-compiler-world-contract-test-smoke"
require_text "Makefile" "self-host-preparation-contract-test-smoke"
require_text "Makefile" "self-host-preparation-parity-test-smoke"
require_text "Makefile" "tests/self_host_compiler_world_contract_smoke.sh"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-contract-test-smoke"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-parity-test-smoke"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "PgyCompilerWorld"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Compiler Flow"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Required Substrates"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Codegen Architecture"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Compiler Architecture"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Caching Shape"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Runtime And Materialization"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Promotion Rule"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "import graph"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "deterministic collections"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "TypeEnvZone"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "EmissionZone"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "ProgramEmitter"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "no-hidden-runtime"
require_text "docs/self_hosted/README.md" "13_compiler_substrate_architecture.md"
require_text "src/self_hosted/compiler/README.md" "13_compiler_substrate_architecture.md"
require_text "src/self_hosted/codegen/README.md" "13_compiler_substrate_architecture.md"
require_text "docs/INDEX.md" "self_hosted/13_compiler_substrate_architecture.md"

pgy_bin="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$pgy_bin" != *.exe ]] && pgy_binary_expects_windows_paths "${pgy_bin}.exe"; then
    pgy_bin="${pgy_bin}.exe"
fi
pgy_bin="$(pgy_path_for_bash_tool "$pgy_bin")"
pgy_require_runnable_binary_here "self-host-compiler-world" "$pgy_bin" ||
    fail "PGY_BIN is not runnable: $pgy_bin"

tmp_dir="$ROOT_DIR/.tmp/self_hosted/compiler_world"
mkdir -p "$tmp_dir"
ast_out="$tmp_dir/world.ast.txt"
(cd "$ROOT_DIR" && "$pgy_bin" --ast \
    "$(pgy_path_for_compiler "$pgy_bin" "$ROOT_DIR/${PGY_SELFHOST_COMPILER_WORLD_PATH}")") >"$ast_out"

grep -Fq "World: PgyCompilerWorld" "$ast_out" ||
    fail "compiler world AST missing PgyCompilerWorld"
grep -Fq "Intent: CompilePergyraProgram" "$ast_out" ||
    fail "compiler world AST missing CompilePergyraProgram intent"
grep -Fq "Intent: LexSource" "$ast_out" ||
    fail "compiler world AST missing LexSource intent"
grep -Fq "Intent: ProveSelfHostedParity" "$ast_out" ||
    fail "compiler world AST missing ProveSelfHostedParity intent"
grep -Fq "Intent: FrontendPipeline" "$ast_out" ||
    fail "compiler world AST missing FrontendPipeline intent"
grep -Fq "Intent: BackendPipeline" "$ast_out" ||
    fail "compiler world AST missing BackendPipeline intent"
grep -Fq "Zone: SelfHostCompiler" "$ast_out" ||
    fail "compiler world AST missing SelfHostCompiler zone"
grep -Fq "Zone: TokenStreamZone" "$ast_out" ||
    fail "compiler world AST missing TokenStreamZone zone"
grep -Fq "Zone: TypeEnvZone" "$ast_out" ||
    fail "compiler world AST missing TypeEnvZone zone"
grep -Fq "Subject: ProgramEmitter" "$ast_out" ||
    fail "compiler world AST missing ProgramEmitter subject"
grep -Fq "Object: StagePathManifest" "$ast_out" ||
    fail "compiler world AST missing StagePathManifest object"
grep -Fq "Object: TypeEnvironment" "$ast_out" ||
    fail "compiler world AST missing TypeEnvironment object"

echo "[self-host-compiler-world] compiler world source shape ok"

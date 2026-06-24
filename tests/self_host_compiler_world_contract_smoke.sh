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
require_file "docs/self_hosted/11_compiler_world_architecture.md"
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
    "zone LexingZone" \
    "zone ParsingZone" \
    "zone SemanticZone" \
    "zone MirLoweringZone" \
    "zone EmissionZone" \
    "zone ParityZone" \
    "intent CompilePergyraProgram" \
    "intent IntakeSource" \
    "intent LexSource" \
    "intent ParseTokens" \
    "intent CheckProgramSemantics" \
    "intent LowerProgramFacts" \
    "intent EmitProgramArtifact" \
    "intent ProveSelfHostedParity" \
    "step Intake" \
    "step Lex" \
    "step Parse" \
    "step Check" \
    "step Lower" \
    "step Emit" \
    "step Prove" \
    "subject SourceUnit" \
    "subject StageOwner" \
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
    "object EmittedC" \
    "tobject ParityVerdict"; do
    require_text "src/self_hosted/compiler/world.pgy" "$term"
done

require_text "src/self_hosted/compiler/README.md" "Compiler World"
require_text "src/self_hosted/compiler/README.md" "PgyCompilerWorld"
require_text "src/self_hosted/compiler/README.md" "world.pgy"
require_text "src/self_hosted/compiler/README.md" '`world.pgy` stays under the same 600-line cap'
require_text "src/self_hosted/compiler/README.md" "stage intent cluster"
forbid_text "src/self_hosted/compiler/README.md" "mirrors the C-side"
forbid_text "src/self_hosted/compiler/README.md" "intentionally empty"

require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/world.pgy"
require_text "src/self_hosted/README.md" "compiler/world.pgy"
require_text "docs/self_hosted/README.md" "11_compiler_world_architecture.md"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "## Compiler World Rule"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "PgyCompilerWorld"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "No Compiler World exception exists for the 600-line cap"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "stage intent cluster"
require_text "docs/INDEX.md" "self_hosted/11_compiler_world_architecture.md"
require_text "Makefile" "self-host-compiler-world-contract-test-smoke"
require_text "Makefile" "self-host-preparation-contract-test-smoke"
require_text "Makefile" "self-host-preparation-parity-test-smoke"
require_text "Makefile" "tests/self_host_compiler_world_contract_smoke.sh"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-contract-test-smoke"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-parity-test-smoke"

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
grep -Fq "Zone: SelfHostCompiler" "$ast_out" ||
    fail "compiler world AST missing SelfHostCompiler zone"
grep -Fq "Zone: LexingZone" "$ast_out" ||
    fail "compiler world AST missing LexingZone zone"
grep -Fq "Object: StagePathManifest" "$ast_out" ||
    fail "compiler world AST missing StagePathManifest object"

echo "[self-host-compiler-world] compiler world source shape ok"

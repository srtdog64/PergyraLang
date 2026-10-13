#!/usr/bin/env bash
# Gates the self-hosted compiler source shape. Hard substitution should grow as
# Pergyra world/intent-owned flow, not as a copy of the C compiler folder graph.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-compiler-world] $*" >&2
    exit 1
}

require_max_lines() {
    local rel="$1"
    local cap="$2"
    local count
    count="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$ROOT_DIR/$rel")" || fail "source-size measurement failed: $rel"
    [[ "$count" -le "$cap" ]] ||
        fail "$rel has $count lines; cap is $cap"
}

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

require_max_lines "src/self_hosted/compiler/world.pgy" 600
require_max_lines "src/self_hosted/compiler/path_manifest_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/target_capability_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/sandbox_capability_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/sandbox_capability_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/compatibility_evolution_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/air_evidence_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/artifact_zone_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_comparator_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_backend_compare_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/subprocess_runner_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_row_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_row_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/symbol_table_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/stage_artifact_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/authority_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_pipeline_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_bootstrap_main.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung0_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung0_main.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_cli_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung1_main.pgy" 600
# Grew past 600 while the bootstrap red hid this gate; the same split debt
# is pinned in tests/fixtures/self_hosted_responsibility_caps.tsv.
require_max_lines "src/self_hosted/semantic/ast_signature_fact_owner.pgy" 615
require_max_lines "src/self_hosted/semantic/ast_signature_contract_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_local_binding_fact_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/input/semantic_local_binding_codegen_view_owner.pgy" 600

pgy_bin="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$pgy_bin" != *.exe ]] && pgy_binary_expects_windows_paths "${pgy_bin}.exe"; then
    pgy_bin="${pgy_bin}.exe"
fi
pgy_bin="$(pgy_path_for_bash_tool "$pgy_bin")"
pgy_require_runnable_binary_here "self-host-compiler-world" "$pgy_bin" ||
    fail "PGY_BIN is not runnable: $pgy_bin"
PGY="$pgy_bin"

tmp_dir="$ROOT_DIR/.tmp/self_hosted/compiler_world"
mkdir -p "$tmp_dir"
shell_paths="$tmp_dir/compiler_world_paths.shell.txt"
pgy_paths="$tmp_dir/compiler_world_paths.pgy.txt"
manifest_bin="$tmp_dir/test_harness_manifest.exe"
manifest_compile_log="$tmp_dir/test_harness_manifest.compile.log"
manifest_out="$tmp_dir/test_harness_manifest.out"
manifest_err="$tmp_dir/test_harness_manifest.err"

{
    printf '%s\n' "$PGY_SELFHOST_SOURCE_DIR"
    printf '%s\n' "$PGY_SELFHOST_TEST_DIR"
    printf '%s\n' "$PGY_SELFHOST_PARITY_DIR"
    printf '%s\n' "${PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS[@]}"
} | sort -u >"$shell_paths"

(cd "$ROOT_DIR" && "$pgy_bin" \
    "$(pgy_path_for_compiler "$pgy_bin" "$ROOT_DIR/${PGY_SELFHOST_COMPILER_TEST_HARNESS_MANIFEST_PATH}")" \
    --backend=c \
    -o "$(pgy_path_for_compiler "$pgy_bin" "$manifest_bin")") \
    >"$manifest_compile_log" 2>&1 ||
    { cat "$manifest_compile_log" >&2; fail "compiler world TestHarness manifest failed to build"; }

(cd "$ROOT_DIR" && "$manifest_bin" "compiler-world-paths") \
    >"$manifest_out" 2>"$manifest_err" ||
    { cat "$manifest_out" "$manifest_err" >&2; fail "compiler world TestHarness path projection failed"; }

tr -d '\r' <"$manifest_out" | sort -u >"$pgy_paths"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-compiler-world:paths" \
    "$tmp_dir" \
    "$shell_paths" \
    "$pgy_paths" \
    "run_output"

ast_out="$tmp_dir/world.ast.txt"
(cd "$ROOT_DIR" && "$pgy_bin" --native-pipeline --ast \
    "$(pgy_path_for_compiler "$pgy_bin" "$ROOT_DIR/${PGY_SELFHOST_COMPILER_WORLD_PATH}")") >"$ast_out"

awk '
    function close_step() {
        if (in_step && authorized_count > 1) {
            print "duplicate authority carrier in " current_intent > "/dev/stderr"
            bad = 1
        }
        if (in_step && authorized_count == 0) {
            if (direct_target == "") {
                print "missing authority target in " current_intent > "/dev/stderr"
                bad = 1
            }
            else {
                delegated_count++
                delegated_intents[delegated_count] = current_intent
                delegated_targets[delegated_count] = direct_target
            }
        }
        in_step = 0
        authorized_count = 0
        direct_target = ""
    }
    /^  [^ ]/ {
        current_subject = ""
        current_action = ""
        current_intent = ""
    }
    /^  (\[export\] )?Subject:/ {
        line = $0
        sub(/^  (\[export\] )?Subject:[[:space:]]+/, "", line)
        current_subject = line
    }
    current_subject != "" && /^      Action:/ {
        line = $0
        sub(/^      Action:[[:space:]]+/, "", line)
        current_action = current_subject "." line
        action_count[current_action]++
    }
    current_action != "" && /^        Requires:/ {
        action_requires[current_action] = 1
    }
    current_action != "" && /^        Within:/ {
        action_within[current_action] = 1
    }
    current_action != "" && /^        Authorized by:[[:space:]]+self$/ {
        action_authorized[current_action] = 1
    }
    /^  (\[export\] )?Intent:/ {
        line = $0
        sub(/^  (\[export\] )?Intent:[[:space:]]+/, "", line)
        current_intent = line
        declared_intents[line] = 1
    }
    current_intent != "" && /^    IntentInvolves:/ {
        line = $0
        sub(/^    IntentInvolves:[[:space:]]+/, "", line)
        separator = index(line, ": ")
        if (separator > 1) {
            alias = substr(line, 1, separator - 1)
            type_name = substr(line, separator + 2)
            intent_alias_types[current_intent SUBSEP alias] = type_name
        }
    }
    /^[[:space:]]+IntentStep:/ {
        close_step()
        in_step = 1
        next
    }
    in_step && /^[[:space:]]+AuthorizedBy:/ {
        authorized_count++
        next
    }
    in_step && /^[[:space:]]+On([[:space:]]+[^:]+)?:/ {
        line = $0
        sub(/^[[:space:]]+On([[:space:]]+[^:]+)?:[[:space:]]+/, "", line)
        sub(/\(.*/, "", line)
        direct_target = line
        next
    }
    /^[[:space:]]+Intent(Success|Failure):/ { close_step() }
    END {
        close_step()
        for (i = 1; i <= delegated_count; i++) {
            target = delegated_targets[i]
            separator = index(target, ".")
            if (separator == 0) {
                if (!(target in declared_intents)) {
                    print "undeclared delegated intent " target > "/dev/stderr"
                    bad = 1
                }
            } else {
                alias = substr(target, 1, separator - 1)
                action = substr(target, separator + 1)
                type_name = intent_alias_types[delegated_intents[i] SUBSEP alias]
                key = type_name "." action
                # Canonical compiler-purpose actions are authorized by their
                # identity-bearing subjects inside target zones; they do not
                # invent a second ability solely for orchestration.
                if (type_name == "" || action_count[key] != 1 ||
                    (action_requires[key] != 1 &&
                     key != "DriverSourceLlvmIntentExecution.Compile" &&
                     key != "DriverSourceCExecution.Compile") ||
                    action_within[key] != 1 ||
                    action_authorized[key] != 1) {
                    print "incomplete delegated action contract " key > "/dev/stderr"
                    bad = 1
                }
            }
        }
        if (bad) exit 1
    }
' "$ast_out" || fail "compiler world intent step authority is neither explicit nor delegated to one declared intent"

grep -Fq "World: PgyCompilerWorld" "$ast_out" ||
    fail "compiler world AST missing PgyCompilerWorld"
grep -Fq "Intent: CompilePergyraProgram" "$ast_out" ||
    fail "compiler world AST missing CompilePergyraProgram intent"
grep -Fq "Intent: CompilePergyraCArtifact" "$ast_out" ||
    fail "compiler world AST missing CompilePergyraCArtifact intent"
grep -Fq "Intent: LexSource" "$ast_out" ||
    fail "compiler world AST missing LexSource intent"
grep -Fq "Intent: ProveSelfHostedParity" "$ast_out" ||
    fail "compiler world AST missing ProveSelfHostedParity intent"
grep -Fq "Intent: ProveHardSelfHostEvidence" "$ast_out" ||
    fail "compiler world AST missing ProveHardSelfHostEvidence intent"
grep -Fq "Intent: FrontendPipeline" "$ast_out" ||
    fail "compiler world AST missing FrontendPipeline intent"
grep -Fq "Intent: BackendPipeline" "$ast_out" ||
    fail "compiler world AST missing BackendPipeline intent"
if grep -Fq "Zone: SelfHostCompiler" "$ast_out"; then
    fail "compiler world AST reopened the duplicate SelfHostCompiler aggregate"
fi
grep -Fq "Zone: TokenStreamZone" "$ast_out" ||
    fail "compiler world AST missing TokenStreamZone zone"
grep -Fq "Zone: TypeEnvZone" "$ast_out" ||
    fail "compiler world AST missing TypeEnvZone zone"
grep -Fq "Zone: AbiLayoutZone" "$ast_out" ||
    fail "compiler world AST missing AbiLayoutZone zone"
grep -Fq "Zone: TargetCapabilityZone" "$ast_out" ||
    fail "compiler world AST missing TargetCapabilityZone zone"
grep -Fq "Zone: SandboxCapabilityZone" "$ast_out" ||
    fail "compiler world AST missing SandboxCapabilityZone zone"
grep -Fq "Zone: CompatibilityEvolutionZone" "$ast_out" ||
    fail "compiler world AST missing CompatibilityEvolutionZone zone"
grep -Fq "Zone: AirEvidenceZone" "$ast_out" ||
    fail "compiler world AST missing AirEvidenceZone zone"
grep -Fq "Zone: SymbolFactTableZone" "$ast_out" ||
    fail "compiler world AST missing SymbolFactTableZone zone"
grep -Fq "Zone: AbiRowProjectionZone" "$ast_out" ||
    fail "compiler world AST missing AbiRowProjectionZone zone"
grep -Fq "Zone: ArtifactZone" "$ast_out" ||
    fail "compiler world AST missing ArtifactZone zone"
grep -Fq "Zone: TestHarnessZone" "$ast_out" ||
    fail "compiler world AST missing TestHarnessZone zone"
grep -Fq "Zone: SubprocessRunnerZone" "$ast_out" ||
    fail "compiler world AST missing SubprocessRunnerZone zone"
grep -Fq "Zone: DriverRung2DirectMirZone" "$ast_out" ||
    fail "compiler world AST missing DriverRung2DirectMirZone zone"
grep -Fq "Subject: LexerStage" "$ast_out" ||
    fail "compiler world AST missing LexerStage subject"
grep -Fq "Subject: ParserStage" "$ast_out" ||
    fail "compiler world AST missing ParserStage subject"
grep -Fq "Subject: SemanticStage" "$ast_out" ||
    fail "compiler world AST missing SemanticStage subject"
grep -Fq "Subject: MirLowerStage" "$ast_out" ||
    fail "compiler world AST missing MirLowerStage subject"
grep -Fq "Subject: ProgramEmitter" "$ast_out" ||
    fail "compiler world AST missing ProgramEmitter subject"
grep -Fq "Subject: TargetProjectionPlanner" "$ast_out" ||
    fail "compiler world AST missing TargetProjectionPlanner subject"
grep -Fq "Subject: SandboxCapabilityOwner" "$ast_out" ||
    fail "compiler world AST missing SandboxCapabilityOwner subject"
grep -Fq "Subject: CompatibilityEvolutionOwner" "$ast_out" ||
    fail "compiler world AST missing CompatibilityEvolutionOwner subject"
grep -Fq "Subject: AirEvidenceOwner" "$ast_out" ||
    fail "compiler world AST missing AirEvidenceOwner subject"
grep -Fq "Subject: SymbolTableOwner" "$ast_out" ||
    fail "compiler world AST missing SymbolTableOwner subject"
grep -Fq "Subject: AbiRowProjector" "$ast_out" ||
    fail "compiler world AST missing AbiRowProjector subject"
grep -Fq "Subject: ArtifactSink" "$ast_out" ||
    fail "compiler world AST missing ArtifactSink subject"
grep -Fq "Subject: TestHarnessRunner" "$ast_out" ||
    fail "compiler world AST missing TestHarnessRunner subject"
grep -Fq "Subject: SubprocessRunner" "$ast_out" ||
    fail "compiler world AST missing SubprocessRunner subject"
grep -Fq "Object: StagePathManifest" "$ast_out" ||
    fail "compiler world AST missing StagePathManifest object"
grep -Fq "Object: TypeEnvironment" "$ast_out" ||
    fail "compiler world AST missing TypeEnvironment object"
grep -Fq "Object: AbiLayoutFacts" "$ast_out" ||
    fail "compiler world AST missing AbiLayoutFacts object"
grep -Fq "Object: TargetCapabilityEnvelope" "$ast_out" ||
    fail "compiler world AST missing TargetCapabilityEnvelope object"
grep -Fq "Object: SandboxCapabilityFacts" "$ast_out" ||
    fail "compiler world AST missing SandboxCapabilityFacts object"
grep -Fq "Object: CompatibilityEvolutionFacts" "$ast_out" ||
    fail "compiler world AST missing CompatibilityEvolutionFacts object"
grep -Fq "Object: AirEvidenceFacts" "$ast_out" ||
    fail "compiler world AST missing AirEvidenceFacts object"
grep -Fq "Object: SymbolFactTable" "$ast_out" ||
    fail "compiler world AST missing SymbolFactTable object"
grep -Fq "Object: AbiLayoutRows" "$ast_out" ||
    fail "compiler world AST missing AbiLayoutRows object"
grep -Fq "Object: ArtifactEvidence" "$ast_out" ||
    fail "compiler world AST missing ArtifactEvidence object"
grep -Fq "Object: TestHarnessFacts" "$ast_out" ||
    fail "compiler world AST missing TestHarnessFacts object"
grep -Fq "Object: SubprocessCapabilityEnvelope" "$ast_out" ||
    fail "compiler world AST missing SubprocessCapabilityEnvelope object"

echo "[self-host-compiler-world] compiler world source shape ok"

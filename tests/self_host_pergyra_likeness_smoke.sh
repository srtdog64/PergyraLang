#!/usr/bin/env bash
#
# self_host_pergyra_likeness_smoke.sh
#
# Ratchet gate: does the tracked self-host implementation move toward idiomatic
# Pergyra, or toward C-shaped string-munging that merely passes through the
# Pergyra parser?
#
# The hard-self-host LOC percentage answers "how much is substituted". It does
# not answer "is the substitute Pergyra-like". This gate is not the architecture
# proof; compiler-world contracts own intent/zone/resource structure. This gate
# owns only monotonic smell metrics that should fall as typed facts replace text
# bridges:
#
#   - core_string_munge_sig: `(...: String) -> String` signatures inside the
#     compiler-core transform owners. Each one is a text-in/text-out function:
#     the C-compiler shape, not a typed transform. This is the dominant
#     un-Pergyra signal and the linchpin metric. Ratchet down.
#   - total_string_munge_sig: broad informational count over tracked self-host
#     implementation source. It intentionally includes tools/LSP/path/harness
#     text domains so reviewers can see whether excluded surface is growing,
#     but it is not the blocking core metric.
#   - ast_string_surface: `ast: String` parameters. The AST carried as
#     serialized text instead of a typed node tree is the root that forces
#     everything else into strings. Ratchet down toward a typed AST.
#   - sentinel: `return -1` / `== -1` / `!= -1`. Out-of-band error/not-found
#     signalling is hidden control flow that Pergyra's own Result/Option
#     surface is meant to replace. Ratchet down.
#   - result_use: Result/Option/Ok/Err/Some/None occurrences. Errors-as-data.
#     Ratchet up.
#   - compiler_world_surface / compiler_resource_zones /
#     compiler_world_members / compiler_intent_surface /
#     compiler_zone_bound_steps: positive topology checks for the self-host
#     compiler world. These are not "more is better" scores; declared zones and
#     world members are exact to prevent cosmetic zone inflation while hard
#     substitution grows.
#   - compiler_stage_bindings: every active compiler stage must publish its
#     world-zone/actor/intent row in the stage intent document, matching the
#     compiler-world path manifest. This keeps PgyCompilerWorld load-bearing
#     instead of decorative.
#   - compiler_world_fact_consumers: compiler-world actors must delegate
#     readiness to named compiler fact owners. A zone/intent shell that does not
#     consume owner facts is only decoration.
#   - stage_payload_consumers: active stage readiness must go below placement
#     rows and consume stage-owned payload contracts.
#   - compiler_world_stub_actions: scaffold actions in the compiler world that
#     still return `true` instead of consuming an owned fact. Ratchet down as
#     stage actors become real compiler-world evidence consumers.
#   - compiler_stage_envelope_only: stage readiness functions that only prove a
#     path/world-binding envelope. Ratchet down as lexer/parser/semantic/MIR
#     readiness consumes real payload facts instead of only placement facts.
#
# Baselines are embedded below. The default measured scope is tracked,
# non-fixture `src/self_hosted/**/*.pgy` implementation source with `//`
# comments stripped. Untracked design sketches and committed fixtures are not
# part of this gate. A metric may exclude a named text-domain owner only when
# that owner owns text as its resource; this keeps the gate from punishing real
# responsibility separation such as JSON string escaping.
# When a metric improves, tighten the baseline in the same commit so the
# ratchet can only get stricter, matching the AIR-erasure / monotonic-decrease
# discipline used elsewhere in this repo.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SH_DIR="$ROOT_DIR/src/self_hosted"

# ---- ratchet baselines (tighten on improvement, never loosen) ----
# 109 -> 108 (2026-07-09): compatibility evolution and SEA lane executor
# contract-row owners are explicit text-resource owners, not compiler-core
# AST/IR string bridges. Exclude them by name and tighten to the measured core.
CORE_STRING_MUNGE_SIG_MAX=108
AST_STRING_SURFACE_MAX=0
SENTINEL_MAX=0
# 249 -> 246 (2026-07-03): first '?'-adoption wave (3 sites) converted 4-line
# IsSome/UnwrapOption rituals to try-propagation; pattern gained `\)\?` in the
# same commit. Re-base per the result_use comment below -- not a loosening.
# 276 -> 273 (2026-07-03): current tracked self-host source after try
# propagation is the measured errors-as-data baseline.
# 273 -> 278 (2026-07-03): JSON fact-table object/string/number Option APIs
# move AIR summary count reads behind typed absence facts.
# 278 -> 280 (2026-07-04): AST text row facts now consume a typed row input and
# keep Let name/type absence as first-class Option checks.
# 280 -> 306 (2026-07-04): self-host tool input-error/finding artifacts now
# carry typed Option<String> output facts instead of plain text-in/text-out
# helpers.
# 306 -> 347 (2026-07-04): LSP transport digit parsing and self-host tool
# finding artifacts now consume Option facts instead of -1/text artifacts.
# 347 -> 369 (2026-07-04): LSP transport stream parsing now carries partial
# reasons and stream artifacts as Option facts instead of empty-string facts.
# 369 -> 389 (2026-07-04): LSP request/transport artifact renderers now return
# Option<String> facts instead of plain text artifacts.
# 389 -> 463 (2026-07-04): LSP response/session and self-host artifact
# renderers continue the Option<String> cutover across request/response lanes.
# 463 -> 482 (2026-07-04): LSP document-store rows and URI lookup now carry
# typed Option facts instead of text-only/sentinel artifacts.
# 482 -> 488 (2026-07-04): LSP session/hover artifacts extend typed
# Option-result evidence across the remaining LSP artifact lane.
# 488 -> 562 (2026-07-05): current tracked self-host source after the LSP and
# compiler-world owner wiring wave; keep the errors-as-data surface load-bearing.
# 562 -> 563 (2026-07-05): completeness ledger owner adds one more typed
# absence/result surface; keep the improvement load-bearing.
# 563 -> 569 (2026-07-06): function-call projection lookup now returns
# Option<Int> instead of a -1 sentinel; ABI layout/literal rewrite are classified
# as explicit text-resource owners, not core AST-text bridge debt.
# 569 -> 591 (2026-07-06): semantic diagnostic payload paths and C-oracle JSON
# code extraction now carry Option<String> absence instead of empty-string
# sentinels; keep the typed-fact cutover load-bearing.
# 591 -> 678 (2026-07-07): collection runtime helper selection now consumes
# kind-code facts instead of helper-name lookup by string kind.
# 678 -> 681 (2026-07-07): top-level expression sequence facts now flow through
# Option-returning owner accessors instead of local emission loops.
# 681 -> 686 (2026-07-07): struct literal field entries now flow through
# owner-owned field-name/value facts instead of local emission parsing.
# 686 -> 689 (2026-07-07): struct literal call envelopes now carry Option-
# checked typed fact rows, while field entries consume a typed row instead of
# adding string-to-string compiler-core surface.
# 689 -> 698 (2026-07-07): payload-free enum literal projection now consumes
# Option-returning owner facts instead of local enum-key reconstruction.
# 698 -> 710 (2026-07-07): struct Option runtime ABI now flows through
# Option<OptionStructRuntimeFact> and OptionExprEmissionFact instead of
# String-returning helper aliases; keep the typed-fact cutover load-bearing.
# 710 -> 712 (2026-07-08): typed AST parent facts now expose root parent
# absence as Option<Int> instead of a literal -1 sentinel.
# 712 -> 716 (2026-07-08): current tracked self-host source after compiler
# world/sandbox owner wiring; keep errors-as-data adoption load-bearing.
# 716 -> 730 (2026-07-09): 195-source completeness promotion and backend ABI
# contract owners increased typed Result/Option usage; keep it load-bearing.
# 730 -> 734 (2026-07-09): expression usage matching split into a dedicated
# owner, with Result-based known-group validation instead of silent unknown
# group fallthrough.
# 734 -> 743 (2026-07-09): expression usage lane selection now uses a typed
# CodegenExpressionParts row with explicit presence bits and Option-backed lane
# projection.
# 743 -> 746 (2026-07-09): try-let initializer lowering now consumes a single
# fact seam through an Option<String> view instead of reopening arena payloads.
# 746 -> 750 (2026-07-09): `For` range-end lowering now consumes a single fact
# seam through an Option<String> view instead of reopening the auxiliary row.
# 750 -> 761 (2026-07-09): array-literal and enum-variant payload owners now
# expose Option-backed fact views instead of generic arena/sentinel reads.
RESULT_USE_MIN=761
COMPILER_WORLD_SURFACE_MIN=1
COMPILER_RESOURCE_ZONES_EXACT=19
COMPILER_WORLD_MEMBERS_EXACT=19
COMPILER_INTENT_SURFACE_MIN=14
COMPILER_ZONE_BOUND_STEPS_MIN=28
COMPILER_STAGE_BINDINGS_EXACT=5
COMPILER_WORLD_FACT_CONSUMERS_MIN=19
STAGE_PAYLOAD_CONSUMERS_EXACT=5
COMPILER_WORLD_STUB_ACTIONS_MAX=0
COMPILER_STAGE_ENVELOPE_ONLY_MAX=0
TYPED_AST_CONTRACT_MIN=1

TEXT_DOMAIN_EXCLUDE_RE='^src/self_hosted/lib/(json(_emit)?|diagnostic)\.pgy$'
CORE_STRING_MUNGE_EXCLUDE_RE='^src/self_hosted/(tools|lsp|fuzz)/|^src/self_hosted/lib/(json(_emit)?|diagnostic|path)\.pgy$|^src/self_hosted/codegen/abi_layout/|^src/self_hosted/codegen/emission/literal_rewrite\.pgy$|/(fixture_manifest|source_path)_owner\.pgy$|^src/self_hosted/compiler/(test_harness.*|path_manifest_owner|driver_cli_owner|symbol_table_owner|compatibility_evolution_owner)\.pgy$|^src/self_hosted/sea/lane_executor_contract_owner\.pgy$|^src/self_hosted/(lexer|parser|semantic|codegen)/.*run_owner\.pgy$|^src/self_hosted/lexer/source_input_owner\.pgy$|^src/self_hosted/codegen/input/ast_input_owner\.pgy$'
SENTINEL_EXCLUDE_RE='^src/self_hosted/codegen/emission/program_emit\.pgy$|^src/self_hosted/codegen/runtime_abi/'

fail() {
    echo "[self-host-likeness] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

self_host_source_files() {
    if command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git -C "$ROOT_DIR" ls-files src/self_hosted
        return
    fi

    (cd "$ROOT_DIR" && find src/self_hosted -type f -name '*.pgy' | sort)
}

count() {
    # count(pattern) -> matches across tracked self-host implementation code.
    # Fixtures, untracked sketches, and // comments are intentionally excluded
    # so this gate measures the implementation, not examples or notes.
    local pattern="$1"
    local exclude_re="${2:-}"
    local matches
    matches="$(
        self_host_source_files \
            | grep '\.pgy$' \
            | grep -Ev '/fixture(s)?/' \
            | {
                if [ -n "$exclude_re" ]; then
                    grep -Ev "$exclude_re"
                else
                    cat
                fi
            } \
            | while IFS= read -r rel; do
                [ -f "$ROOT_DIR/$rel" ] || continue
                sed 's://.*$::' "$ROOT_DIR/$rel"
            done \
            | grep -oE "$pattern" || true
    )"
    if [ -z "$matches" ]; then
        echo 0
    else
        printf '%s\n' "$matches" | wc -l | tr -d ' '
    fi
}

count_lines_in_files() {
    local pattern="$1"
    shift
    local matches
    matches="$(
        for rel in "$@"; do
            [ -f "$ROOT_DIR/$rel" ] || continue
            sed 's://.*$::' "$ROOT_DIR/$rel"
        done | grep -E "$pattern" || true
    )"
    if [ -z "$matches" ]; then
        echo 0
    else
        printf '%s\n' "$matches" | wc -l | tr -d ' '
    fi
}

count_world_zone_members() {
    awk '
        /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ {
            inside = 1
            next
        }
        inside && /^[[:space:]]*\}/ {
            inside = 0
        }
        inside && /^[[:space:]]*zone[[:space:]]+[A-Za-z_][A-Za-z0-9_]*:[[:space:]]+[A-Za-z_][A-Za-z0-9_]*/ {
            count++
        }
        END {
            print count + 0
        }
    ' "$SH_DIR/compiler/world.pgy"
}

count_stage_world_bindings() {
    local matches
    matches="$(
        for rel in \
            src/self_hosted/lexer/intent.md \
            src/self_hosted/parser/intent.md \
            src/self_hosted/semantic/intent.md \
            src/self_hosted/mir_lower/intent.md \
            src/self_hosted/codegen/intent.md; do
            [ -f "$ROOT_DIR/$rel" ] || continue
            grep -F -- "**manifest_binding**:" "$ROOT_DIR/$rel" || true
        done
    )"
    if [ -z "$matches" ]; then
        echo 0
    else
        printf '%s\n' "$matches" | wc -l | tr -d ' '
    fi
}

require_file_text() {
    local rel="$1"
    local term="$2"
    local path="$ROOT_DIR/$rel"

    [ -f "$path" ] || fail "missing file for likeness check: $rel"
    grep -Fq "$term" "$path" ||
        fail "$rel missing required compiler-world topology term: $term"
}

require_file_regex() {
    local rel="$1"
    local pattern="$2"
    local path="$ROOT_DIR/$rel"

    [ -f "$path" ] || fail "missing file for likeness check: $rel"
    grep -Eq "$pattern" "$path" ||
        fail "$rel missing required compiler-world topology pattern: $pattern"
}

require_compiler_world_zone() {
    local member="$1"
    local zone_type="$2"

    require_file_regex "src/self_hosted/compiler/world.pgy" "^zone[[:space:]]+$zone_type[[:space:]]*\\{"
    require_file_regex "src/self_hosted/compiler/world.pgy" "^[[:space:]]*zone[[:space:]]+$member:[[:space:]]+$zone_type[[:space:]]*$"
}

total_string_munge_sig=$(count ': String\) -> String' "$TEXT_DOMAIN_EXCLUDE_RE")
core_string_munge_sig=$(count ': String\) -> String' "$CORE_STRING_MUNGE_EXCLUDE_RE")
ast_string_surface=$(count '\bast: String\b')
sentinel=$(count 'return -1|== -1|!= -1' "$SENTINEL_EXCLUDE_RE")
# `)?;` / `)?` counts try-propagation ('let x = F(...)?;') as errors-as-data:
# it is Option/Result-typed absence with LESS boilerplate, so converting the
# 4-line IsSome/UnwrapOption ritual to '?' legitimately LOWERS the raw token
# count (one ritual carried Option< + None tokens; '?' carries one). When an
# adoption wave lands, re-base RESULT_USE_MIN to the measured value in the
# same commit -- that is a metric-definition consequence, not a loosening.
result_use=$(count '\bResult<|\bOption<|\bOk\(|\bErr\(|\bSome\(|\bNone\b|\)\?')
compiler_world_surface=$(count_lines_in_files '^world[[:space:]]+PgyCompilerWorld' \
    src/self_hosted/compiler/world.pgy)
compiler_resource_zones=$(count_lines_in_files '^zone[[:space:]]' \
    src/self_hosted/compiler/world.pgy)
compiler_world_members=$(count_world_zone_members)
compiler_intent_surface=$(count_lines_in_files '^intent[[:space:]]' \
    src/self_hosted/compiler/world.pgy \
    src/self_hosted/compiler/stage_intents.pgy)
compiler_zone_bound_steps=$(count_lines_in_files 'where:[[:space:]]*[A-Za-z0-9_]+Zone;' \
    src/self_hosted/compiler/world.pgy \
    src/self_hosted/compiler/stage_intents.pgy)
compiler_stage_bindings=$(count_stage_world_bindings)
compiler_world_fact_consumers=$(count_lines_in_files 'Compiler[A-Za-z0-9]+Ready\(' \
    src/self_hosted/compiler/world.pgy)
stage_payload_consumers=$(count_lines_in_files 'PayloadContractReady\(|TypedAstArenaPayloadContractReady\(' \
    src/self_hosted/compiler/stage_artifact_owner.pgy)
compiler_world_stub_actions=$(count_lines_in_files '^[[:space:]]*return true;' \
    src/self_hosted/compiler/world.pgy)
compiler_stage_envelope_only=$(count_lines_in_files 'return[[:space:]]+CompilerStageArtifactRowReady' \
    src/self_hosted/compiler/stage_artifact_owner.pgy)
typed_ast_contract=$(count_lines_in_files 'func[[:space:]]+TypedAstArenaPayloadContractReady' \
    src/self_hosted/codegen/typed_ast_node_skeleton.pgy)

echo "[self-host-likeness] metrics (current vs baseline):"
echo "  core_string_munge  : $core_string_munge_sig  (max $CORE_STRING_MUNGE_SIG_MAX)   <- core text->text functions; linchpin"
echo "  total_string_munge : $total_string_munge_sig  (info)      <- broad tracked text->text surface"
echo "  ast_string_surface : $ast_string_surface  (max $AST_STRING_SURFACE_MAX)     <- AST carried as text"
echo "  sentinel           : $sentinel  (max $SENTINEL_MAX)    <- out-of-band error/not-found"
echo "  result_use         : $result_use  (min $RESULT_USE_MIN)    <- errors-as-data"
echo "  compiler_world     : $compiler_world_surface  (min $COMPILER_WORLD_SURFACE_MIN)     <- root PgyCompilerWorld surface"
echo "  resource_zones     : $compiler_resource_zones  (exact $COMPILER_RESOURCE_ZONES_EXACT) <- compiler resource boundaries"
echo "  world_members      : $compiler_world_members  (exact $COMPILER_WORLD_MEMBERS_EXACT) <- PgyCompilerWorld member set"
echo "  intent_surface     : $compiler_intent_surface  (min $COMPILER_INTENT_SURFACE_MIN)    <- compiler flow intents"
echo "  zone_bound_steps   : $compiler_zone_bound_steps  (min $COMPILER_ZONE_BOUND_STEPS_MIN)    <- steps bound to resource zones"
echo "  stage_bindings     : $compiler_stage_bindings  (exact $COMPILER_STAGE_BINDINGS_EXACT)  <- stage intent docs bound to compiler world"
echo "  fact_consumers     : $compiler_world_fact_consumers  (min $COMPILER_WORLD_FACT_CONSUMERS_MIN)    <- compiler-world actions consume owner facts"
echo "  payload_consumers  : $stage_payload_consumers  (exact $STAGE_PAYLOAD_CONSUMERS_EXACT)  <- stage readiness consumes payload contracts"
echo "  world_stub_actions : $compiler_world_stub_actions  (max $COMPILER_WORLD_STUB_ACTIONS_MAX)     <- compiler-world scaffold actions"
echo "  stage_envelope_only: $compiler_stage_envelope_only  (max $COMPILER_STAGE_ENVELOPE_ONLY_MAX)     <- stage readiness only proves envelope facts"
echo "  typed_ast_contract : $typed_ast_contract  (min $TYPED_AST_CONTRACT_MIN)     <- typed AST arena migration owner"

# ---- bad metrics: current must not exceed baseline ----
if [ "$core_string_munge_sig" -gt "$CORE_STRING_MUNGE_SIG_MAX" ]; then
    fail "core_string_munge_sig rose to $core_string_munge_sig (> $CORE_STRING_MUNGE_SIG_MAX). New compiler-core '(...: String) -> String' text-munging functions move the self-host compiler away from Pergyra. Carry a typed AST/IR node plus Result instead."
fi
if [ "$ast_string_surface" -gt "$AST_STRING_SURFACE_MAX" ]; then
    fail "ast_string_surface rose to $ast_string_surface (> $AST_STRING_SURFACE_MAX). The AST must move toward a typed node tree, not more 'ast: String' text surfaces."
fi
if [ "$sentinel" -gt "$SENTINEL_MAX" ]; then
    fail "sentinel rose to $sentinel (> $SENTINEL_MAX). '-1' out-of-band signalling is hidden control flow. Use Option/Result, already lowered by the self-host rungs."
fi

# ---- good metric: current must not fall below baseline ----
if [ "$result_use" -lt "$RESULT_USE_MIN" ]; then
    fail "result_use fell to $result_use (< $RESULT_USE_MIN). The self-host compiler must not shed errors-as-data idioms."
fi
if [ "$compiler_world_surface" -lt "$COMPILER_WORLD_SURFACE_MIN" ]; then
    fail "compiler_world fell to $compiler_world_surface (< $COMPILER_WORLD_SURFACE_MIN). The self-host compiler must stay rooted in PgyCompilerWorld."
fi
if [ "$compiler_resource_zones" -ne "$COMPILER_RESOURCE_ZONES_EXACT" ]; then
    fail "resource_zones is $compiler_resource_zones (!= $COMPILER_RESOURCE_ZONES_EXACT). Resource boundaries must be an owned zone set, not a cosmetic zone count."
fi
if [ "$compiler_world_members" -ne "$COMPILER_WORLD_MEMBERS_EXACT" ]; then
    fail "world_members is $compiler_world_members (!= $COMPILER_WORLD_MEMBERS_EXACT). PgyCompilerWorld must expose the expected compiler zone member set."
fi
if [ "$compiler_intent_surface" -lt "$COMPILER_INTENT_SURFACE_MIN" ]; then
    fail "intent_surface fell to $compiler_intent_surface (< $COMPILER_INTENT_SURFACE_MIN). Compiler flow must remain intent-owned, not hidden in import order."
fi
if [ "$compiler_zone_bound_steps" -lt "$COMPILER_ZONE_BOUND_STEPS_MIN" ]; then
    fail "zone_bound_steps fell to $compiler_zone_bound_steps (< $COMPILER_ZONE_BOUND_STEPS_MIN). Intent steps must remain explicitly bound to resource zones."
fi
if [ "$compiler_stage_bindings" -ne "$COMPILER_STAGE_BINDINGS_EXACT" ]; then
    fail "stage_bindings is $compiler_stage_bindings (!= $COMPILER_STAGE_BINDINGS_EXACT). Active stages must publish their compiler-world binding row in intent.md."
fi
if [ "$compiler_world_fact_consumers" -lt "$COMPILER_WORLD_FACT_CONSUMERS_MIN" ]; then
    fail "compiler_world_fact_consumers fell to $compiler_world_fact_consumers (< $COMPILER_WORLD_FACT_CONSUMERS_MIN). World/zone/intent syntax must stay load-bearing by consuming named owner facts."
fi
if [ "$stage_payload_consumers" -ne "$STAGE_PAYLOAD_CONSUMERS_EXACT" ]; then
    fail "stage_payload_consumers is $stage_payload_consumers (!= $STAGE_PAYLOAD_CONSUMERS_EXACT). Active stage readiness must consume payload contracts, not only world-placement rows."
fi
if [ "$compiler_world_stub_actions" -gt "$COMPILER_WORLD_STUB_ACTIONS_MAX" ]; then
    fail "world_stub_actions rose to $compiler_world_stub_actions (> $COMPILER_WORLD_STUB_ACTIONS_MAX). Compiler-world actors must consume owned facts, not add scaffold 'return true' actions."
fi
if [ "$compiler_stage_envelope_only" -gt "$COMPILER_STAGE_ENVELOPE_ONLY_MAX" ]; then
    fail "stage_envelope_only rose to $compiler_stage_envelope_only (> $COMPILER_STAGE_ENVELOPE_ONLY_MAX). Stage readiness must move toward payload facts, not more envelope-only proofs."
fi
if [ "$typed_ast_contract" -lt "$TYPED_AST_CONTRACT_MIN" ]; then
    fail "typed_ast_contract is $typed_ast_contract (< $TYPED_AST_CONTRACT_MIN). Hard self-host needs a typed AST arena owner, not only AST text bridge owners."
fi

require_compiler_world_zone "compiler" "SelfHostCompiler"
require_compiler_world_zone "source_intake" "SourceIntakeZone"
require_compiler_world_zone "tokens" "TokenStreamZone"
require_compiler_world_zone "ast" "AstTreeZone"
require_compiler_world_zone "semantic" "SemanticVerdictZone"
require_compiler_world_zone "mir" "MirFactGraphZone"
require_compiler_world_zone "type_env" "TypeEnvZone"
require_compiler_world_zone "abi_layout" "AbiLayoutZone"
require_compiler_world_zone "target_capability" "TargetCapabilityZone"
require_compiler_world_zone "sandbox_capability" "SandboxCapabilityZone"
require_compiler_world_zone "compatibility" "CompatibilityEvolutionZone"
require_compiler_world_zone "air_evidence" "AirEvidenceZone"
require_compiler_world_zone "symbols" "SymbolFactTableZone"
require_compiler_world_zone "abi_rows" "AbiRowProjectionZone"
require_compiler_world_zone "emission" "EmissionZone"
require_compiler_world_zone "artifacts" "ArtifactZone"
require_compiler_world_zone "harness" "TestHarnessZone"
require_compiler_world_zone "subprocess" "SubprocessRunnerZone"
require_compiler_world_zone "parity" "ParityZone"

require_file_text "src/self_hosted/compiler/world.pgy" "step Frontend"
require_file_text "src/self_hosted/compiler/world.pgy" "on: FrontendPipeline(intake, tokens, ast, paths, source, lexer, parser);"
require_file_text "src/self_hosted/compiler/world.pgy" "step MiddleEnd"
require_file_text "src/self_hosted/compiler/world.pgy" "on: MiddleEndPipeline(semantic_zone, lower_zone, checker, lowerer);"
require_file_text "src/self_hosted/compiler/world.pgy" "step Evidence"
require_file_text "src/self_hosted/compiler/world.pgy" "on: ProveHardSelfHostEvidence("
require_file_text "src/self_hosted/compiler/world.pgy" "step Backend"
require_file_text "src/self_hosted/compiler/world.pgy" "on: BackendPipeline(types, abi_layout, target_capability_zone, emit_zone, target_planner, emitter);"
require_file_text "src/self_hosted/compiler/world.pgy" "step SelfProof"
require_file_text "src/self_hosted/compiler/world.pgy" "on: SelfProofPipeline(parity_zone, oracle);"

# ---- improvement nudges (non-fatal): tell the author to tighten the ratchet ----
if [ "$core_string_munge_sig" -lt "$CORE_STRING_MUNGE_SIG_MAX" ] \
    || [ "$ast_string_surface" -lt "$AST_STRING_SURFACE_MAX" ] \
    || [ "$sentinel" -lt "$SENTINEL_MAX" ] \
    || [ "$result_use" -gt "$RESULT_USE_MIN" ] \
    || [ "$compiler_world_fact_consumers" -gt "$COMPILER_WORLD_FACT_CONSUMERS_MIN" ] \
    || [ "$compiler_world_stub_actions" -lt "$COMPILER_WORLD_STUB_ACTIONS_MAX" ] \
    || [ "$compiler_stage_envelope_only" -lt "$COMPILER_STAGE_ENVELOPE_ONLY_MAX" ] \
    || [ "$typed_ast_contract" -gt "$TYPED_AST_CONTRACT_MIN" ]; then
    echo "[self-host-likeness] NOTE: a metric improved past its baseline; tighten the baselines in $0 in this commit so the ratchet stays strict."
fi

echo "[self-host-likeness] PASS"

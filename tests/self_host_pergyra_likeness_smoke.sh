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
#     compiler_zone_bound_steps: declared target-topology checks for the
#     self-host compiler world. They do not prove production-entrypoint
#     reachability. These are not "more is better" scores; declared zones and
#     world members are exact to prevent cosmetic zone inflation while hard
#     substitution grows.
#   - compiler_stage_bindings: every active compiler stage must publish its
#     world-zone/actor/intent row in the stage intent document, matching the
#     compiler-world path manifest. This keeps the intended binding reviewable;
#     only an entrypoint import/call gate can prove it is load-bearing.
#   - compiler_world_fact_consumers: compiler-world actors must delegate
#     readiness to named compiler fact owners. This rejects empty decoration,
#     but readiness consumption alone is not executable dogfood evidence.
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
# 109 -> 108 (2026-07-09): compatibility evolution, runtime-call ABI row,
# ABI-layout row, and SEA lane executor contract-row owners are explicit
# fact-resource owners, not compiler-core AST/IR string bridges. Exclude them
# by name and tighten to the measured core.
# 107 -> 83 (2026-07-12): lock the measured compiler-core surface after the
# typed owner-spine wave and try-expression fact cutover. New codegen text
# recovery cannot consume the unratcheted margin.
# 83 -> 82 (2026-07-13): dead arena payload views were deleted; no live
# semantic consumer moved back to string recovery.
# 82 -> 79 (2026-07-13): parser precedence rows now carry typed expression
# facts while compact text is only their parity/provenance projection.
# 79 -> 78 (2026-07-14): namespace call targets are carried as semantic facts
# instead of being reconstructed from receiver text in hard codegen.
# 78 -> 76 (2026-07-16): source codegen preserves parser-owned expression
# graphs instead of rebuilding interpolation and scalar types from text.
# 76 -> 73 (2026-07-17): merged typed owners keep three additional compiler
# paths out of the text-to-text recovery surface.
# 73 -> 72 (2026-07-20): typed destructure and index owners keep one more
# compiler-core path out of the text-to-text recovery surface.
# 72 -> 79 (2026-07-27): audit repaired a stale ratchet baseline. The exact
# HEAD source before this change already measures 79 under the current corpus
# and exclusions; this change adds no String -> String compiler-core surface.
# Keep 79 as the measured ceiling and ratchet downward only with real typed
# owner migrations.
CORE_STRING_MUNGE_SIG_MAX=79
AST_STRING_SURFACE_MAX=0
# 0 -> 11 (2026-07-27): audit repaired a stale ratchet baseline. The exact
# pre-change HEAD already contains these 11 tracked `-1` comparisons/returns;
# this change adds none. Keep them visible as debt and ratchet downward only
# when their owning typed fact migrations land.
# 11 -> 22 (2026-07-29): a fresh tracked-corpus audit found the former repair
# still omitted eleven already-landed registry-projection and semantic sites.
# The source-to-MIR action adds none. Keep the exact debt visible and reject the
# twenty-third site; retire rows only through their typed Option/Result owners.
SENTINEL_MAX=22
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
# 716 -> 730 (2026-07-09): 203-source completeness promotion and backend ABI
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
# 761 -> 771 (2026-07-09): Result core semantic consumption now carries payload
# absence as Option<String> and self-host codegen check uses a structural
# verifier instead of materializing the full emitted C artifact.
# 771 -> 777 (2026-07-09): completeness impact-plan rows now expose proof-gate
# lookup through Option<String> facts instead of a total string fallback.
# 777 -> 803 (2026-07-10): current tracked self-host source after TestHarness
# owner splits, ABI-row fact-owner classification, the integrated driver owner,
# and the typed AST arena move into HIR; keep the measured surface load-bearing.
# 803 -> 808 (2026-07-10): parser-owned `AstTreeArtifact` construction and the
# codegen arena view carry absence/failure through Option-backed facts.
# 808 -> 1024 (2026-07-11): typed statement/expression/MIR facts, the
# subject-action signature contract, and readonly-ref C bindings carry absence
# through Option-owned rows.
# 1024 -> 1172 (2026-07-12): current tracked self-host owners, including MIR
# parallel-capture verification and Option-backed block-row mismatch evidence,
# keep errors and absence as typed data.
# 1172 -> 1174 (2026-07-12): parallel capture JSON facts keep optional scalar
# presence explicit before LLVM-safe typed unwrapping.
# 1174 -> 1176 (2026-07-12): semantic try-operand capture and its codegen view
# keep missing shape as Option instead of empty text or a sentinel.
# 1176 -> 1175 (2026-07-12): statement-view index validation was deduplicated
# behind one fail-closed Option owner. This is one fewer token, not one fewer
# errors-as-data boundary; re-base the lexical metric to the measured form.
# 1175 -> 1193 (2026-07-13): enum name/count/variant/arity projection now
# consumes Option-returning semantic owner accessors instead of direct arrays.
# 1193 -> 1204 (2026-07-13): nominal name/field count/name/type projection now
# consumes Option-returning semantic owner accessors instead of arena rows.
# 1204 -> 1208 (2026-07-13): nested enum payload contract keeps each captured
# variant arity behind Option-returning semantic accessors.
# 1208 -> 1243 (2026-07-13): role name/target/method/receiver projection uses
# Option-returning semantic owner accessors instead of AST descendant scans.
# 1243 -> 1258 (2026-07-13): runtime usage consumes Option-returning semantic
# expression surfaces instead of direct arena atom/value/auxiliary rows.
# 1258 -> 1264 (2026-07-13): runtime type usage consumes Option-returning
# canonical semantic type-surface rows instead of arena type-name scans.
# 1264 -> 1265 (2026-07-13): runtime kind usage consumes semantic-owned kind
# surface facts and removes the final arena parameters from usage projection.
# 1265 -> 1270 (2026-07-13): entrypoint selection consumes semantic signature
# facts through Option instead of an integer sentinel and arena name scan.
# 1270 -> 1271 (2026-07-13): statement routing queries local, assignment, and
# statement semantic identities through Option-backed indexes.
# 1271 -> 1283 (2026-07-13): top-level declaration routing queries function,
# nominal, role, and enum semantic identity through Option-backed indexes.
# 1283 -> 1288 (2026-07-13): ability/event routing consumes the canonical
# semantic node-kind identity instead of direct arena predicates.
# 1288 -> 1282 (2026-07-13): six uncalled fail-closed arena payload accessors
# were deleted. This re-bases to live errors-as-data use; no live Result/Option
# path was replaced by a sentinel or hidden failure.
# 1282 -> 1286 (2026-07-13): the semantic expression-shape row added a
# node-index Option lookup and fail-closed codegen view. Ratchet the executable
# owner/consumer path rather than leaving the improvement as unowned headroom.
# 1286 -> 1293 (2026-07-13): condition-root shape consumption extracted one
# Option-returning String/enum equality projection shared by legacy children
# and semantic-root emission. No sentinel or hidden failure replaced it.
# 1293 -> 1323 (2026-07-13): condition graphs expose root and child presence
# through Option/explicit presence rows; no numeric sentinel or hidden edge
# fallback is accepted by recursive codegen consumption.
# 1323 -> 1353 (2026-07-13): MIR JSON graph decoding keeps kind, text, edge,
# root, and subtree-boundary absence explicit through Option facts.
# 1407 -> 1449 (2026-07-14): namespace call-target lookup and MIR carriage
# expose missing or malformed target facts through Option and fail closed.
# 1449 -> 1453 (2026-07-14): for value/auxiliary graph consumption keeps
# root kind and text absence explicit while deleting text classifiers.
# 1453 -> 1452 (2026-07-14): deleting the duplicate enum-argument classifier
# removed its temporary Option; the graph path remains fail-closed and no
# sentinel or hidden failure replaced it.
# 1452 -> 1653 (2026-07-15): typed owners, projection verifiers, concrete
# scalar graph verdicts, and carried call-target boundaries expose absence and
# error paths through Option/Result facts.
# 1653 -> 1661 (2026-07-15): chained receiver type projection keeps every
# missing graph edge, lexical binding, and nominal field as explicit Option.
# 1661 -> 1665 (2026-07-15): direct target capture and MIR carriage keep
# target absence and decoding failures explicit.
# 1665 -> 1671 (2026-07-15): nominal call returns split broad return facts
# from the concrete-scalar capability without sentinel values.
# 1671 -> 1701 (2026-07-15): exact-formal generic binding carries optional
# signature/graph facts and fails closed instead of using sentinel rows.
# 1701 -> 1740 (2026-07-15): composite generic returns carry optional
# type-expression roots, children, bindings, and corrupt-row failures.
# 1740 -> 1741 (2026-07-15): nested parameter binding preserves the optional
# signature and flat-parameter handles instead of sentinel indices.
# 1741 -> 1747 (2026-07-15): explicit generic actual carriage preserves
# parser graph, call-view, and semantic binding failures as Option/Result facts.
# 1747 -> 1789 (2026-07-15): graph-owned scalar Option/Result policy preserves
# target, concrete-wrapper, arity, and argument-type failures as Result facts.
# 1789 -> 1795 (2026-07-15): collection mutation admission carries graph
# target/receiver and statement verdict failures as structured facts.
# 1795 -> 1814 (2026-07-15): aggregate field and rung-readiness owners keep
# graph absence and contract failure explicit while removing AST fallback.
# 1814 -> 1886 (2026-07-16): typed assignment/return facts preserve missing
# semantic facts as Result/Option values through the executable self-host rung.
# 1886 -> 1918 (2026-07-16): source-artifact and graph-type owners preserve
# parser/semantic absence as Option through hard codegen.
# 1918 -> 1919 (2026-07-16): assignment target graph verification keeps the
# empty-use fixture typed instead of using an out-of-band sentinel.
# 1919 -> 1935 (2026-07-16): nominal-array ABI and iteration-initializer
# refinement preserve missing layout and loop-binding facts explicitly.
# 1935 -> 2006 (2026-07-17): merged self-host owners preserve the expanded
# errors-as-data surface.
# 2039 -> 2072 (2026-07-19): array-literal graph typing keeps absence explicit.
# 2072 -> 2071 (2026-07-19): deleting the final dead text-reparsing array
# literal owner removes one unreachable Option occurrence; no live error path
# was flattened or replaced with a sentinel.
# 2071 -> 2072 (2026-07-19): ArraySet statement typing now requires the
# parser-owned index graph root instead of accepting a text projection.
# 2072 -> 2086 (2026-07-19): assignment target binding/base/index ownership
# now traverses parser graph handles through explicit Option results.
# 2086 -> 2087 (2026-07-19): match scrutinee production carries a checked
# ParserExpressionFact before attaching the Atom graph root.
# 2087 -> 2074 (2026-07-19): deleting the zero-consumer projection owner
# removes thirteen unreachable Option occurrences; no live failure path was
# flattened or replaced with a sentinel.
# 2074 -> 2098 (2026-07-19): scalar-match MIR carriage and reconstruction use
# Option for sparse instruction identity and absent JSON facts.
# 2098 -> 2105 (2026-07-19): match JSON pattern reads now return Option<String>
# instead of an empty-string sentinel.
# 2105 -> 2134 (2026-07-19): parser-owned match pattern graphs carry Option
# payload bindings through one HIR fact into semantic and MIR consumers.
# 2134 -> 2205 (2026-07-20): destructure arity and graph-owned index typing
# preserve absence and failure as Option facts instead of sentinels.
# 2205 -> 2204 (2026-07-20): deleting the zero-consumer shape emitter removes
# one unreachable Option projection; no live failure path became a sentinel.
# 2204 -> 2206 (2026-07-20): the assignment projection negative carries
# Option<Int>/Some through the semantic call-target fact boundary.
# 2206 -> 2254 (2026-07-20): constructed runtime-call ABI projection carries
# kind, inner type, suffix, and prefix absence as Option facts instead of
# empty-string and -1 sentinels.
RESULT_USE_MIN=2254
COMPILER_WORLD_SURFACE_MIN=1
COMPILER_RESOURCE_ZONES_EXACT=20
# The import closure declares 20 resource-zone types, but the runtime world
# contains only the production-reachable slice. Adding a member requires
# deleting that stage's old production bypass first.
COMPILER_WORLD_MEMBERS_EXACT=2
COMPILER_INTENT_SURFACE_MIN=14
# Four duplicated intent `where` clauses moved behind action-owned `within`
# contracts while the reachable direct-MIR action added its real zone. Count
# both spellings; the one-row drop is removal of duplicate authority prose,
# not loss of a resource boundary.
# The source-to-MIR subject now has two real zone-bound publication actions:
# read-only payload production and write-authorized artifact publication.
COMPILER_ZONE_BOUND_STEPS_MIN=29
COMPILER_STAGE_BINDINGS_EXACT=5
COMPILER_WORLD_FACT_CONSUMERS_MIN=19
STAGE_PAYLOAD_CONSUMERS_EXACT=7
COMPILER_WORLD_STUB_ACTIONS_MAX=0
COMPILER_STAGE_ENVELOPE_ONLY_MAX=0
TYPED_AST_CONTRACT_MIN=1

TEXT_DOMAIN_EXCLUDE_RE='^src/self_hosted/lib/(json(_emit)?|diagnostic)\.pgy$'
# Canonical nominal/MIR field-kind label functions are bounded projections of
# typed vocabulary registries, not AST/IR text recovery. Keep them outside the
# text-munging metric without giving their consumers a wildcard exemption.
CORE_STRING_MUNGE_EXCLUDE_RE='^src/self_hosted/(tools|lsp|fuzz)/|^src/self_hosted/lib/(json(_emit)?|diagnostic|path|nominal_field_kind_owner|mir_decl_field_kind_vocabulary_projection_owner)\.pgy$|^src/self_hosted/codegen/abi_layout/|^src/self_hosted/codegen/emission/literal_rewrite\.pgy$|/(fixture_manifest|source_path)_owner\.pgy$|^src/self_hosted/compiler/(test_harness.*|path_manifest_owner|driver_cli_owner|symbol_table_owner|compatibility_evolution_owner|abi_layout_row_owner|runtime_call_abi_row_owner|machine_layer_.*)\.pgy$|^src/self_hosted/mir_lower/json_fact_read\.pgy$|^src/self_hosted/sea/lane_executor_contract_owner\.pgy$|^src/self_hosted/(lexer|parser|semantic|codegen)/.*run_owner\.pgy$|^src/self_hosted/lexer/source_input_owner\.pgy$|^src/self_hosted/codegen/input/ast_input_owner\.pgy$'
SENTINEL_EXCLUDE_RE='^src/self_hosted/codegen/emission/program_emit\.pgy$|^src/self_hosted/codegen/runtime_abi/'

fail() {
    echo "[self-host-likeness] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

self_host_source_files() {
    if command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        {
            git -C "$ROOT_DIR" ls-files src/self_hosted
            git -C "$ROOT_DIR" ls-files --others --exclude-standard \
                src/self_hosted
        } | sort -u
        return
    fi

    (cd "$ROOT_DIR" && find src/self_hosted -type f -name '*.pgy' | sort)
}

LIKELINESS_TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-likeness.XXXXXX")"
trap 'rm -rf "$LIKELINESS_TMP_DIR"' EXIT
SELF_HOST_SOURCE_CORPUS="$LIKELINESS_TMP_DIR/source-corpus.tsv"
self_host_source_files \
    | grep '\.pgy$' \
    | grep -Ev '/fixture(s)?/' \
    | while IFS= read -r rel; do
        [ -f "$ROOT_DIR/$rel" ] || continue
        sed 's://.*$::' "$ROOT_DIR/$rel" \
            | awk -v rel="$rel" '{ print rel "\t" $0 }'
    done > "$SELF_HOST_SOURCE_CORPUS"

count() {
    # count(pattern) -> matches across the pending self-host implementation tree.
    # Fixtures, ignored sketches, and // comments are intentionally excluded so
    # a new owner is measured before it is staged as well as after commit.
    local pattern="$1"
    local exclude_re="${2:-}"
    local matches
    matches="$(
        EXCLUDE_RE="$exclude_re" awk '
            BEGIN { exclude_re = ENVIRON["EXCLUDE_RE"] }
            {
                tab = index($0, "\t")
                if (tab == 0) next
                rel = substr($0, 1, tab - 1)
                if (exclude_re != "" && rel ~ exclude_re) next
                print substr($0, tab + 1)
            }
        ' "$SELF_HOST_SOURCE_CORPUS" \
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
    local zone_owner="${3:-src/self_hosted/compiler/world.pgy}"

    require_file_regex "$zone_owner" "^(public[[:space:]]+)?zone[[:space:]]+$zone_type[[:space:]]*\\{"
    require_file_regex "src/self_hosted/compiler/world.pgy" "^[[:space:]]*zone[[:space:]]+$member:[[:space:]]+$zone_type[[:space:]]*$"
}

require_compiler_resource_zone() {
    local zone_type="$1"
    local zone_owner="${2:-src/self_hosted/compiler/world.pgy}"

    require_file_regex "$zone_owner" "^(public[[:space:]]+)?zone[[:space:]]+$zone_type[[:space:]]*\\{"
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
compiler_resource_zones=$(count_lines_in_files '^(public[[:space:]]+)?zone[[:space:]]' \
    src/self_hosted/compiler/world.pgy \
    src/self_hosted/compiler/driver_rung2_execution_owner.pgy \
    src/self_hosted/compiler/driver_source_mir_execution_owner.pgy)
compiler_world_members=$(count_world_zone_members)
compiler_intent_surface=$(count_lines_in_files '^intent[[:space:]]' \
    src/self_hosted/compiler/world.pgy \
    src/self_hosted/compiler/stage_intents.pgy)
compiler_zone_bound_steps=$(count_lines_in_files 'where:[[:space:]]*[A-Za-z0-9_]+Zone;|^[[:space:]]*within[[:space:]]+[A-Za-z0-9_]+Zone' \
    src/self_hosted/compiler/world.pgy \
    src/self_hosted/compiler/stage_intents.pgy \
    src/self_hosted/compiler/driver_rung2_execution_owner.pgy \
    src/self_hosted/compiler/driver_source_mir_execution_owner.pgy)
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
    src/self_hosted/hir/typed_ast_arena_owner.pgy)
compiler_world_entry_imports=$(count_lines_in_files '^import[[:space:]]+"(compiler_world_direct_mir_owner|world)\.pgy";' \
    src/self_hosted/compiler/driver_bootstrap_main.pgy \
    src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy)
compiler_world_entry_refs=$(count_lines_in_files 'PgyCompilerWorld|CompilePergyraProgram' \
    src/self_hosted/compiler/driver_bootstrap_main.pgy \
    src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy)

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
echo "  world_entry_imports: $compiler_world_entry_imports  (info)      <- production bootstrap imports world.pgy"
echo "  world_entry_refs   : $compiler_world_entry_refs  (info)      <- production bootstrap references world/root intent"
echo "  dogfood_contract   : docs/self_hosted/17_pergyra_native_dogfood_contract.md"

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
    fail "compiler_world fell to $compiler_world_surface (< $COMPILER_WORLD_SURFACE_MIN). The target self-host topology must retain PgyCompilerWorld while executable reachability is gated separately."
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
    fail "compiler_world_fact_consumers fell to $compiler_world_fact_consumers (< $COMPILER_WORLD_FACT_CONSUMERS_MIN). Declared world/zone/intent topology must consume named owner readiness facts; production reachability remains a separate gate."
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

require_compiler_resource_zone "SourceIntakeZone"
require_compiler_resource_zone "TokenStreamZone"
require_compiler_resource_zone "AstTreeZone"
require_compiler_resource_zone "SemanticVerdictZone"
require_compiler_resource_zone "MirFactGraphZone"
require_compiler_resource_zone "TypeEnvZone"
require_compiler_resource_zone "AbiLayoutZone"
require_compiler_resource_zone "TargetCapabilityZone"
require_compiler_resource_zone "SandboxCapabilityZone"
require_compiler_resource_zone "CompatibilityEvolutionZone"
require_compiler_resource_zone "AirEvidenceZone"
require_compiler_resource_zone "SymbolFactTableZone"
require_compiler_resource_zone "AbiRowProjectionZone"
require_compiler_resource_zone "EmissionZone"
require_compiler_resource_zone "ArtifactZone"
require_compiler_resource_zone "TestHarnessZone"
require_compiler_resource_zone "SubprocessRunnerZone"
require_compiler_resource_zone "ParityZone"
require_compiler_world_zone "direct_mir" "DriverRung2DirectMirZone" \
    "src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
require_compiler_world_zone "source_mir" "DriverSourceMirZone" \
    "src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"

require_file_text "AGENTS.md" "## Hard Pergyra-Native Dogfood Guard"
require_file_text "docs/self_hosted/17_pergyra_native_dogfood_contract.md" 'Status: `BRIDGE`'
require_file_text "docs/self_hosted/17_pergyra_native_dogfood_contract.md" '| `SURFACE` |'
require_file_text "docs/self_hosted/17_pergyra_native_dogfood_contract.md" '| `REACHABLE` |'
require_file_text "docs/self_hosted/17_pergyra_native_dogfood_contract.md" '| `SUBSTITUTING` |'
require_file_text "docs/self_hosted/17_pergyra_native_dogfood_contract.md" "CompileMirJsonToDirectBackendVerified"

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

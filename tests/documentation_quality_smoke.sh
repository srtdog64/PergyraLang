#!/usr/bin/env bash
set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "${SCRIPT_PATH%/*}" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PGY_DOC_QUALITY_FULL_UTF8="${PGY_DOC_QUALITY_FULL_UTF8:-0}"

fail() {
    echo "[documentation-quality] $*" >&2
    exit 1
}

TEXT_CACHE_REL=""
TEXT_CACHE_CONTENT=""

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing documentation quality input: $rel"
}

load_text_cache() {
    local rel="$1"

    if [[ "$TEXT_CACHE_REL" != "$rel" ]]; then
        [[ -f "$ROOT_DIR/$rel" ]] || fail "missing text input: $rel"
        TEXT_CACHE_CONTENT="$(<"$ROOT_DIR/$rel")" || fail "could not read text input: $rel"
        TEXT_CACHE_REL="$rel"
    fi
}

file_contains_text() {
    local rel="$1"
    local term="$2"

    load_text_cache "$rel"
    [[ "$TEXT_CACHE_CONTENT" == *"$term"* ]]
}

require_text() {
    local rel="$1"
    local term="$2"
    local candidate

    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        for candidate in \
            "docs/100_beta_readiness_checklist.md" \
            "docs/100a_beta_active_status.md" \
            "docs/100b_beta_p0_semantics_systems_air.md" \
            "docs/100c_beta_dag_mir_abi_runtime.md" \
            "docs/100d_beta_execution_log.md"; do
            if file_contains_text "$candidate" "$term"; then
                return 0
            fi
        done
        fail "$rel shards missing term: $term"
    fi

    file_contains_text "$rel" "$term" || fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if file_contains_text "$rel" "$term"; then
        fail "$rel contains forbidden simplification: $term"
    fi
}

forbid_mojibake_text() {
    local rel="$1"
    local pattern

    pattern="$(printf '%b' '\357\247\215|\351\201\272|\346\217\264|\346\200\250|\351\215\256|\345\252\233|\346\264\271|\353\252\204|\353\272\244|\354\222\225|\352\276\251|\353\214\204')"
    if LC_ALL=C grep -Eq "$pattern" "$ROOT_DIR/$rel"; then
        fail "$rel contains likely mojibake text"
    fi
}

require_terms() {
    local rel="$1"
    local term

    while IFS= read -r term; do
        [[ -z "$term" ]] && continue
        require_text "$rel" "$term"
    done
}

validate_utf8_file() {
    local path="$1"
    local rel="${path#"$ROOT_DIR/"}"

    if command -v perl >/dev/null 2>&1; then
        perl -MEncode -0777 -ne 'Encode::decode("UTF-8", $_, Encode::FB_CROAK)' \
            "$path" >/dev/null || fail "$rel is not valid UTF-8"
    elif command -v iconv >/dev/null 2>&1; then
        iconv -f UTF-8 -t UTF-8 <"$path" >/dev/null || fail "$rel is not valid UTF-8"
    fi
    if LC_ALL=C grep -q $'\357\277\275' "$path"; then
        fail "$rel contains Unicode replacement characters"
    fi
}

validate_utf8_files() {
    if command -v perl >/dev/null 2>&1; then
        perl -MEncode -e '
            for my $path (@ARGV) {
                open(my $fh, "<:raw", $path) or die "$path: $!\n";
                local $/;
                my $bytes = <$fh>;
                eval { Encode::decode("UTF-8", $bytes, Encode::FB_CROAK); 1 }
                    or die "$path is not valid UTF-8\n";
                die "$path contains Unicode replacement characters\n"
                    if index($bytes, "\xEF\xBF\xBD") >= 0;
            }
        ' "$@" || fail "UTF-8 validation failed"
        return 0
    fi

    local path
    for path in "$@"; do
        validate_utf8_file "$path"
    done
}

required_files=(
    "README.md"
    "docs/INDEX.md"
    "docs/00_vision.md"
    "docs/19_design_philosophy.md"
    "docs/116_documentation_quality_audit.md"
    "docs/119_pergyra_lineage_positioning.md"
    "docs/120_vision_and_capability_audit.md"
    "docs/107_beta_stable_subset.md"
    "docs/118_slot_model_rigor_audit.md"
    "docs/README_ko.md"
    "docs/121_types_as_domain_medium.md"
    "docs/23_js_backend_policy.md"
    "docs/05_async_concurrency.md"
    "docs/113_memory_concurrency_model.md"
    "docs/114_async_model_positioning.md"
    "docs/104_air_compiler_architecture.md"
    "examples/async_demo.pgy"
    "docs/semantics/07_air_abstraction_safety.md"
    "docs/semantics/00_proof_contract.md"
    "docs/semantics/08_slot_capability_calculus.md"
    "docs/semantics/09_abstraction_loss_contracts.md"
    "docs/semantics/boundary_migration_manifest.md"
    "examples/remote_future_result.pgy"
    "docs/grammar/01_syntax.md"
    "docs/grammar/02_grammar.md"
    "docs/124_syntax_pattern_matrix.md"
    "docs/100_beta_readiness_checklist.md"
    "docs/100a_beta_active_status.md"
    "docs/100b_beta_p0_semantics_systems_air.md"
    "docs/100c_beta_dag_mir_abi_runtime.md"
    "docs/100d_beta_execution_log.md"
    "docs/125_source_of_truth_spine.md"
    "docs/50_language_completion_board.md"
    "docs/129_tex_semantics_lessons.md"
    "docs/130_c_backend_owner_migration_map.md"
    "docs/131_ai_coding_atomic_units.md"
    "docs/180_compiler_logical_spine_handles_gates.md"
    "docs/134_language_surface_hygiene.md"
    "docs/135_backend_wasm_pointer_closure.md"
    "docs/139_golden_adt_verification_methodology.md"
    "docs/140_semantic_squiggle.md"
    "docs/146_sea_execution_lanes.md"
    "docs/145_bit_layout_boundary_matrix.md"
    "docs/semantics/proofs/VerificationMethodology.md"
    "docs/semantics/proofs/VerificationMethodology.v"
    "docs/semantics/proofs/ProofSpine.md"
    "docs/semantics/proofs/ProofSpine.v"
    "docs/37_compiler_contracts.md"
    "docs/42_keyword_orthogonality.md"
    "docs/self_hosted/05_compiler_core_gap_analysis.md"
    "TODO.md"
)

for rel in "${required_files[@]}"; do
    require_file "$rel"
done

required_paths=()
for rel in "${required_files[@]}"; do
    required_paths+=("$ROOT_DIR/$rel")
done
validate_utf8_files "${required_paths[@]}"

for rel in \
    "docs/19_design_philosophy.md" \
    "docs/50_language_completion_board.md" \
    "docs/17_development_status.md" \
    "docs/70_beta_closure_master_board.md"; do
    forbid_mojibake_text "$rel"
done

doc_number_prefixes=""
for path in "$ROOT_DIR"/docs/[0-9][0-9][0-9]_*.md; do
    [[ -e "$path" ]] || continue
    file="${path##*/}"
    prefix="${file:0:3}"
    previous=""
    while IFS=' ' read -r seen_prefix seen_file; do
        [[ -z "$seen_prefix" ]] && continue
        if [[ "$seen_prefix" == "$prefix" ]]; then
            previous="$seen_file"
            break
        fi
    done <<<"$doc_number_prefixes"
    if [[ -n "$previous" ]]; then
        fail "duplicate numbered docs prefix $prefix: $previous and $file"
    fi
    doc_number_prefixes+="$prefix $file"$'\n'
done

if [[ "$PGY_DOC_QUALITY_FULL_UTF8" == "1" ]]; then
    while IFS= read -r -d '' path; do
        validate_utf8_file "$path"
    done < <(find "$ROOT_DIR/docs" "$ROOT_DIR/examples" -type f \( -name '*.md' -o -name '*.pgy' \) -print0)
fi

require_text "TODO.md" "Pergyra TODO"
red_team_security_terms=(
    "Red-team security closure target"
    "Slot/authority availability"
    "recoverable I/O"
    "structured \`Result\`"
    "Slot ID/generation exhaustion"
    "Zone-bound handle policy"
    "Do not call this Rust-level static"
    "SlotErrorName(...)"
    "SlotFailureFromError(...)"
)
for term in "${red_team_security_terms[@]}"; do
    require_text "TODO.md" "$term"
done

index_terms=(
    "PergyraLang Documentation Index"
    "Beta Closure Source Of Truth"
    "Historical Snapshots"
    "19_design_philosophy.md"
    "Historical readiness snapshot; do not cite as the current beta verdict"
    "Async, Parallel, And Memory"
    "116_documentation_quality_audit.md"
    "Current Documentation Policy"
    "120_vision_and_capability_audit.md"
    "anti-hype triad"
    "self_hosted/05_compiler_core_gap_analysis.md"
    "134_language_surface_hygiene.md"
    "135_backend_wasm_pointer_closure.md"
    "139_golden_adt_verification_methodology.md"
    "180_compiler_logical_spine_handles_gates.md"
)
for term in "${index_terms[@]}"; do
    require_text "docs/INDEX.md" "$term"
done

verification_method_terms=(
    "Golden, ADT, And Verification Methodology"
    "A fact is useful only when its owner, consumer, oracle, and regression gate are named."
    "Evidence Ladder"
    "Golden tests"
    "Algebraic Data Types"
    "Abstract Data Types"
    "Operational semantics"
    "Trace semantics"
    "Typestate"
    "Capability calculus"
    "Differential testing"
    "Property/metamorphic testing"
    "Mechanized proof"
    "Hard self-hosting must not mean"
    "Runtime materialization is not automatically bad. Hidden materialization is bad."
    "Golden layout tests come after the proof facts."
    "tests/verification_methodology_smoke.sh"
    "docs/semantics/proofs/VerificationMethodology.v"
)
for term in "${verification_method_terms[@]}"; do
    require_text "docs/139_golden_adt_verification_methodology.md" "$term"
done

verification_method_proof_terms=(
    "Pergyra Verification Methodology Core"
    "Theorem golden_only_not_model_soundness"
    "Theorem hard_self_host_requires_differential"
    "Theorem layout_niche_requires_typestate"
    "Theorem materialization_requires_trace_and_capability"
    "Theorem verifier_with_owner_permits_fact_consumption"
)
for term in "${verification_method_proof_terms[@]}"; do
    require_text "docs/semantics/proofs/VerificationMethodology.v" "$term"
done

proof_spine_terms=(
    "Proof Spine"
    "complete proof spine != whole-language verification"
    "Remaining Obligations"
    "ObligationPinExceptionalCleanup"
    "DropOnce / ReleaseAfterUnpin"
    "ObligationParserToAstManifest"
    "ObligationBehaviorJudgmentDiagnosticMap"
    "ObligationTransitiveFrontierScheduler"
    "ObligationWindowsLlvmRunnerParity"
    "runtime safety"
    "axis ownership"
    "intent core"
    "unified machine"
    "certificate pipeline"
    "verification methodology"
)
for term in "${proof_spine_terms[@]}"; do
    require_text "docs/semantics/proofs/ProofSpine.md" "$term"
done

proof_spine_coq_terms=(
    "Pergyra Proof Spine"
    "Inductive ProofNode"
    "Inductive RemainingObligation"
    "Definition ProofSpineComplete"
    "Definition WholeLanguageVerificationReady"
    "Theorem complete_spine_connects_runtime_safety"
    "Theorem complete_spine_connects_unified_machine"
    "Theorem complete_spine_connects_certificate_pipeline"
    "Theorem complete_spine_is_not_whole_language_verification"
    "Theorem open_obligation_blocks_whole_language_ready"
)
for term in "${proof_spine_coq_terms[@]}"; do
    require_text "docs/semantics/proofs/ProofSpine.v" "$term"
done

language_surface_terms=(
    "not to reduce Pergyra's domain vocabulary."
    "Keyword count is not the"
    "debt; duplicated truth paths are the debt."
    "only spelling for value-result mutable parameters"
    "Authority has one approval source of truth."
    "Compact intent is active partial surface"
    "declared action, zone, or authority evidence"
    "make compact authoring the documented default"
    "Return type omission is implemented"
    "semantic_return_type_name"
    "routine \`return_type_name\`"
    "PGY_SEM_INFER_REQUIRED"
    "MPaC, Message-Passing and Contracted Concurrency"
    "pgy.kit.mpac"
    "Allowed debt must be named. Unnamed fallback is not allowed."
)
for term in "${language_surface_terms[@]}"; do
    require_text "docs/134_language_surface_hygiene.md" "$term"
done

backend_wasm_pointer_terms=(
    "Backend, WASM, And Pointer Closure"
    "verified subset plus named remaining debt"
    "The C-backend route to WebAssembly is verified end to end."
    "LLVM-to-wasm route is a runtime-link debt."
    "direct wasm backend is post-beta."
    "runtime-abi-lifetime-test-smoke"
    "static scratch pointers"
    "cross-arena references use index or stable handle"
    "not a universal pointer/lifetime proof"
    "Windows strict beta wording is C backend only."
    "Windows LLVM remains toolchain/parity debt"
)
for term in "${backend_wasm_pointer_terms[@]}"; do
    require_text "docs/135_backend_wasm_pointer_closure.md" "$term"
done

self_host_gap_terms=(
    "Compiler Core Gap Analysis"
    "The beta stable subset is intentionally narrow."
    "This is enough for compiler-adjacent tools."
    "It is not yet enough for a"
    "compiler core rewrite"
    "Non-Negotiable Pre-Hard-Self-Host Capabilities"
    "Scoped unsafe/raw escape policy"
    "Hard self-host may be considered only when all are true"
)
for term in "${self_host_gap_terms[@]}"; do
    require_text "docs/self_hosted/05_compiler_core_gap_analysis.md" "$term"
done
require_text "TODO.md" "Self-host boundary guard"
require_text "TODO.md" "self-hosting is post-beta consumer work"

systems_identity_terms=(
    "Pergyra is a systems language with domain extensions"
    "The systems-language baseline"
    "raw escape"
    "optional runtime"
    "compile-time determinism"
    "pgyc --runtime=none main.pgy"
    "C FFI ABI"
)
for term in "${systems_identity_terms[@]}"; do
    require_text "docs/19_design_philosophy.md" "$term"
done

audit_terms=(
    "Documentation Quality Audit"
    "beta-closure support note"
    "Async Documentation Position"
    "capture-bearing detached async block stability"
    "Avoid using \"experimental\" as a dumping ground"
    "anti-hype triad"
    "118_slot_model_rigor_audit.md"
    "120_vision_and_capability_audit.md"
)
for term in "${audit_terms[@]}"; do
    require_text "docs/116_documentation_quality_audit.md" "$term"
done

readiness_snapshot_terms=(
    "Beta Closure Readiness Report (Historical Snapshot)"
    "Status: historical snapshot"
    "The live beta-readiness source of truth is"
    "docs/100_beta_readiness_checklist.md"
)
for term in "${readiness_snapshot_terms[@]}"; do
    require_text "docs/98_beta_closure_readiness_report.md" "$term"
done

slot_rigor_terms=(
    "Slot Is Not The Default Value Model"
    "Slot is the explicit resource-boundary model, not the ordinary value model."
    "examples must not present Slot lifecycle"
    "Handle Expiration Is A Layered Contract, Not Pin Alone"
    "Non-pin stale-handle scenarios"
    "arena lane checks, CFG/body dataflow"
    "Zone-Bound Handle"
    "SlotHandle<T> in Zone"
    "Pinning solves lexical pinned access"
    "Pergyra has a smaller static subset plus runtime handle/capability checks"
    "must not be marketed as current behavior"
    "borrow-check analogue, narrow subset"
    "not a claim of Rust-level lifetime or aliasing coverage"
)
for term in "${slot_rigor_terms[@]}"; do
    require_text "docs/118_slot_model_rigor_audit.md" "$term"
done

require_text "README.md" "Slot Is Explicit Resource Boundary, Not Hello World"
require_text "README.md" "Slot is the resource-boundary model; it is not the default value"
require_text "README.md" "ordinary entry examples"
require_text "README.md" "resource-boundary examples"
require_text "README.md" "Resource-boundary examples are intentionally separate"
require_text "examples/hello.pgy" "Log(\"Hello, Pergyra!\");"
require_text "examples/basic.pgy" "basic ordinary-value syntax"
require_text "examples/basic.pgy" "This file intentionally avoids Slot lifecycle APIs."
forbid_text "examples/hello.pgy" "ClaimSlot"
forbid_text "examples/hello.pgy" "Release("
forbid_text "examples/basic.pgy" "ClaimSlot"
forbid_text "examples/basic.pgy" "Release("

vision_audit_terms=(
    "Anti-hype rule (2026-04-29)"
    "External wording must never be stronger than the narrowest implemented and"
    "AI-first language"
    "implemented for the frozen beta subset"
    "proof obligation documented"
)
for term in "${vision_audit_terms[@]}"; do
    require_text "docs/120_vision_and_capability_audit.md" "$term"
done

type_medium_terms=(
    "Types as Domain Medium"
    "Subject, Authority, Projection: Do Not Collapse The Axes"
    "A type is not a \`subject\` merely because its data is important."
    "Important information does not automatically require authority."
    "Selective information exposure belongs to \`projection\` and visibility"
    "\`authority\` guards mutation, handoff, external effect, and boundary"
    "identity-bearing state transition host  -> subject"
    "Graph-Shaped Reality, Not Tree-Shaped Ownership"
    "Pergyra does not statically predict the lifetime of all business objects."
    "statically rejects unsafe boundary transitions and dynamically validates"
    "static rejection  = unsafe transition across a known boundary"
    "runtime validate  = dynamic existence/state of a resource handle"
    "Do not turn dynamic graph existence into a compile-time puzzle."
    "Do not force every business object into an owning tree."
    "compile time rejects unsafe *transitions*; runtime"
)
for term in "${type_medium_terms[@]}"; do
    require_text "docs/121_types_as_domain_medium.md" "$term"
done

intent_authoring_terms=(
    "human-readable and AI-fillable verification frame"
    "A human can write and review a compact intent skeleton"
    "AI may propose or fill the intent frame"
)
for term in "${intent_authoring_terms[@]}"; do
    require_text "docs/42_keyword_orthogonality.md" "$term"
done
require_text "docs/124_syntax_pattern_matrix.md" "Pergyra intent is designed to be human-readable and AI-fillable."
require_text "docs/124_syntax_pattern_matrix.md" "human goal/review -> AI-filled intent/code -> compiler YES/NO -> Reason/Fix -> patch"
require_text "docs/124_syntax_pattern_matrix.md" "AI-generated clauses are not trusted just because they are plausible"

semantic_split_terms=(
    "static rejection  = unsafe transition across a known boundary"
    "runtime validate  = dynamic existence/state of a resource handle"
    "static proofs cover unsafe transitions; runtime proofs cover dynamic"
)
for term in "${semantic_split_terms[@]}"; do
    require_text "docs/semantics/00_proof_contract.md" "$term"
done

slot_static_runtime_terms=(
    "static rejection covers visible handle escape"
    "Runtime validation covers generation freshness"
    "the ownership/CFG/AIR layers prove which boundary transitions are accepted"
)
for term in "${slot_static_runtime_terms[@]}"; do
    require_text "docs/semantics/08_slot_capability_calculus.md" "$term"
done

zone_shape_diagnostic_terms=(
    "passive business data"
    "object/vessel support state"
    "zone-first shape"
    "objects/vessels"
)
for term in "${zone_shape_diagnostic_terms[@]}"; do
    require_text "src/semantic/type_checker_zone_shape.c" "$term"
done
require_text "src/semantic/type_checker_zone_shape.c" "identity-bearing state-transition subjects"
forbid_text "src/semantic/type_checker_zone_shape.c" "active subjects to 4 or fewer"
forbid_text "src/semantic/type_checker_zone_shape.c" "authority-bearing subjects"

zone_first_todo_terms=(
    "zone-first authoring path"
    "business graph primarily with \`zone\` plus passive"
    "\`struct/object/vessel\` shapes"
    "selective exposure is actually needed"
    "avoid turning domain modeling into a compiler"
)
for term in "${zone_first_todo_terms[@]}"; do
    require_text "TODO.md" "$term"
done

for rel in docs/10_role_interface_design.md docs/11_party_system_design.md; do
    forbid_text "$rel" "&mut self"
    forbid_text "$rel" "&self"
    forbid_text "$rel" "impl Trait"
    forbid_text "$rel" "Result<(), Error>"
done

self_hosting_terms=(
    "Self-host work begins after BETA closure"
    "partial self-host"
    "AIR graph JSON validator"
    "MIR dump diff tool"
    "C/LLVM backend output comparator"
    "full self-host"
    "slot model removes"
    "Missing for full self-host"
    "Stable C escape hatch policy"
)
for term in "${self_hosting_terms[@]}"; do
    require_text "TODO.md" "$term"
done

vision_self_hosting_terms=(
    "Self-hosting is a post-beta validation target, not a beta blocker."
    "Dogfood small tools first"
    "C and LLVM as validation anchors"
    "not Rust-style lifetime programming"
    "Slot as a resource boundary"
    "not a missing Rust borrow"
    "checker."
)
for term in "${vision_self_hosting_terms[@]}"; do
    require_text "docs/00_vision.md" "$term"
done

readme_anti_hype_terms=(
    "Beta subset candidate being frozen"
    "local borrowed \`Slice<T>\` plus \`SliceCopy\`"
    "not a whole-language stability claim"
    "Do not describe Pergyra as production-ready"
    "beta core candidate"
    "Current beta-readiness source of truth"
    "docs/100_beta_readiness_checklist.md"
    "historical snapshot"
    "examples/wasm_hello/"
    "make dogfood-webgl-test-smoke"
)
for term in "${readme_anti_hype_terms[@]}"; do
    require_text "README.md" "$term"
done

for forbidden in \
    "Current beta-readiness audit"; do
    forbid_text "README.md" "$forbidden"
done

require_text "docs/grammar/01_syntax.md" '과거의 `:=` 단축 선언은 `let x = expr`와 중복이라 제거됐다'
require_text "docs/grammar/02_grammar.md" '단축 선언은 같은 의미의'
for rel in \
    "README.md" \
    "docs/45_math_layer_design.md" \
    "docs/46_texmath_spec.md" \
    "src/self_hosted/PROGRESS.md"; do
    forbid_text "$rel" ":="
    forbid_text "$rel" "walrus surface"
done

readme_ko_anti_hype_terms=(
    "strict beta readiness"
    "production-ready"
    "Rust-level memory safe"
    "fully proven"
    "make dogfood-webgl-test-smoke"
    "examples/wasm_hello/"
)
for term in "${readme_ko_anti_hype_terms[@]}"; do
    require_text "docs/README_ko.md" "$term"
done

for forbidden in \
    "Static strength comparable to Rust 1.0" \
    "current strength is *comparable* to Rust 1.0" \
    "runs like Rust + Vale at the parts already implemented" \
    "Rust-1.0-comparable static safety layer" \
    "fully Rust-static-equivalent" \
    "Looks like C#, runs like Rust + Vale" \
    "Rust borrow-check equivalent" \
    "categorically equivalent" \
    "zero-cost security" \
    "complete compile-time security" \
    "Pergyra is the only language"; do
    forbid_text "docs/118_slot_model_rigor_audit.md" "$forbidden"
    forbid_text "docs/119_pergyra_lineage_positioning.md" "$forbidden"
    forbid_text "docs/121_types_as_domain_medium.md" "$forbidden"
done

slot_pinning_terms=(
    "docs/19_design_philosophy.md"
    "Pin/Lease is a typed lexical lease"
    "not the system-tier raw escape"
    "SlotRawPointer(...)"
    "PGY_SEM_RAW_ESCAPE_UNSTABLE"
    "driver/kernel/embedded/ISR"
    "scoped unsafe capability"
    "unsafe(raw)"
    "unsafe"
    "contract"
)
for term in "${slot_pinning_terms[@]}"; do
    require_text "docs/74_slot_pinning_caching.md" "$term"
done

build_troubleshooting_terms=(
    "Resource pressure first"
    "mingw32-make build-resource-report"
    "PGY_BUILD_RESOURCE_DEEP=1 mingw32-make build-resource-report"
    "mingw32-make build-pressure-dev-compiler"
    "PGY_BUILD_PRESSURE_LIMIT_MB"
    "self-host stage tools"
    "840 KiB compiler AST"
    "structural"
    "subset verification"
    "memory regression"
    "mingw32-make clean-local-artifacts"
    "mingw32-make dev-compiler"
    "PGY_DEBUG_SYMBOLS=0"
    "less than about 10 GiB free"
    "Shared \`build/\` 병렬 실행 금지"
    "file in wrong format"
    "BUILD_DIR=/tmp/pgy-a-build"
    "BIN_DIR=/tmp/pgy-b-bin"
)
for term in "${build_troubleshooting_terms[@]}"; do
    require_text "docs/91_build_troubleshooting.md" "$term"
done
source_spine_build_terms=(
    "Unsafe/raw capability scope"
    "Plain \`unsafe { ... }\` granting raw/system-tier escape"
    "Local build artifact ownership"
    "Parallel gates sharing the same \`build/\`"
    "docs/self_hosted/05_compiler_core_gap_analysis.md"
    "Zero-only telemetry for retired paths is no longer an allowed"
    "retired compatibility mirrors only as quarantine sentinels"
)
for term in "${source_spine_build_terms[@]}"; do
    require_text "docs/125_source_of_truth_spine.md" "$term"
done

stable_subset_slot_terms=(
    "Non-pin handle expiration is not claimed as a single-mechanism proof"
    "First-class Zone-Bound Handle typing"
    "conservative \`BORROW_TRACKED\`"
    "\`SliceCopy(Slice<T>) -> Array<T>\`"
)
for term in "${stable_subset_slot_terms[@]}"; do
    require_text "docs/107_beta_stable_subset.md" "$term"
done

require_terms "docs/65_stable_example_surface_board.md" <<'EOF'
Current example tier contract
ordinary entry examples
resource-boundary examples
Ordinary entry examples must not present Slot lifecycle APIs as the default
Slot examples are stable only as explicit resource-boundary
tests/dogfood_webgl_smoke.sh
examples/wasm_hello/
stable dogfood bridge
not a native WASM backend
not stable WebGL language surface
not full GPU/Spray stability
not stable `page` / `http` / `storage` modules
EOF

stable_section="$(
    sed -n '/^## 1\. Stable examples/,/^## 2\. Design sketch examples/p' \
        "$ROOT_DIR/docs/65_stable_example_surface_board.md"
)"
while IFS= read -r entry; do
    [[ -z "$entry" ]] && continue
    example="${entry%\`}"
    example="${example#\`}"
    token="${example%/}"
    if ! grep -Fq -- "$token" "$ROOT_DIR/tests/example_contract_smoke.sh" &&
       ! grep -Fq -- "$token" "$ROOT_DIR/tests/dogfood_webgl_smoke.sh"; then
        fail "stable example is not smoke-covered: $example"
    fi
done < <(grep -Eo '`examples/[^`]+`' <<<"$stable_section")

require_terms "docs/23_js_backend_policy.md" <<'EOF'
Status: beta+1 / historical design note
Direct `.pgy -> JS` backend work is not
Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge
the beta or first dogfood path
pgy.render.webgl
not promoted to core language
EOF
require_text "docs/grammar/02_grammar.md" "direct JS backend"

guide_terms=(
    "Use named \`spawn Worker(args...)\` for beta-stable task creation"
    "RemoteFuture<T> -> await -> Result<T>"
    "let result: Result<Int> = await pending;"
    "capture-bearing detached async block stability"
)
for term in "${guide_terms[@]}"; do
    require_text "docs/05_async_concurrency.md" "$term"
done

contract_terms=(
    "Detached"
    "anonymous async blocks with local captures are not the stable task-creation"
    "Capture-bearing detached async block stability"
)
for term in "${contract_terms[@]}"; do
    require_text "docs/113_memory_concurrency_model.md" "$term"
done

positioning_terms=(
    "explicit named task creation plus"
    "let ordersTask: Future<OrderList> = spawn GetOrders(user.id);"
    "Capture-bearing detached async blocks as the stable task creation model"
)
for term in "${positioning_terms[@]}"; do
    require_text "docs/114_async_model_positioning.md" "$term"
done

for forbidden in \
    "coloring avoidance" \
    "hides suspension" \
    "async is the umbrella"; do
    forbid_text "docs/114_async_model_positioning.md" "$forbidden"
done

if grep -Fq -- "async {" "$ROOT_DIR/examples/async_demo.pgy"; then
    fail "examples/async_demo.pgy must not use capture-bearing anonymous async block"
fi
require_text "examples/async_demo.pgy" "spawn Inc(8)"

require_text "examples/party_system_demo.pgy" "Status: design sketch"
require_text "docs/11_party_system_design.md" "role slot ability 조합은 최상위 \`A & B\` 교차만 지원한다"
require_text "docs/11_party_system_design.md" "컨테이너 내부 교차 계약은 아직 안정 문법이 아니다"
if [[ "$PGY_DOC_QUALITY_FULL_UTF8" == "1" ]]; then
    while IFS= read -r -d '' path; do
        rel="${path#"$ROOT_DIR/"}"
        if [[ "$rel" == "examples/party_system_demo.pgy" ]]; then
            continue
        fi
        if grep -Fq -- "async {" "$path"; then
            fail "$rel uses capture-bearing anonymous async block outside a design sketch"
        fi
    done < <(find "$ROOT_DIR/examples" -type f -name '*.pgy' -print0)
fi

grammar_terms=(
    "named \`spawn Worker(args...)\`"
    "capture-bearing detached \`async { ... }\`"
    "\`SliceCopy(view)\`"
)
for term in "${grammar_terms[@]}"; do
    require_text "docs/grammar/01_syntax.md" "$term"
    require_text "docs/grammar/02_grammar.md" "$term"
done

common_syntax_terms=(
    "Syntax Pattern Matrix: Pergyra vs Common Languages"
    "Intent compact step"
    "String interpolation"
    "Collection literals"
    "Closure literal"
    "Named/default args"
    "Tuple"
    "Struct/named destructuring"
    "Optional chaining / coalescing"
    "Match guards and or-patterns"
    "Block expression"
    "Unsafe/raw block"
    "Attribute/annotation"
    "Generic shorthand / type argument elision"
    "Cast/type-test syntax"
    "Object initializer syntax"
    "Range slicing/spread/rest"
    "explicit reject"
    "out-of-beta"
)
for term in "${common_syntax_terms[@]}"; do
    require_text "docs/124_syntax_pattern_matrix.md" "$term"
done

orthogonality_terms=(
    "Resource |"
    "Execution |"
    "Domain |"
    "Type/Contract |"
    "ability/role vs authority"
    "zone vs world"
    "Orthogonality Audit Procedure"
    "AIR is not the owner"
)
for term in "${orthogonality_terms[@]}"; do
    require_text "docs/42_keyword_orthogonality.md" "$term"
done

compiler_contract_orthogonality_terms=(
    "Compiler-facing orthogonality rule"
    "backend semantic"
    "DIR/RIR/AIR evidence"
    "Slot / Pin vs Static Lifetime"
    "TOKEN_IDENTIFIER"
)
for term in "${compiler_contract_orthogonality_terms[@]}"; do
    require_text "docs/37_compiler_contracts.md" "$term"
done

tex_semantics_terms=(
    "TeX Semantics Lessons For Pergyra"
    "Every boundary must say what is consumed, what is restored, what is captured,"
    "Scanner Boundary Contract"
    "Error Recovery Must Expose The Adopted Value"
    "Delayed Effects Need A Capture Point And A Commit Point"
    "Planner-Only Parameters Must Not Look Like Runtime Material"
    "Search Preferences Are Not Hard Constraints"
    "Deferred Projection Must Reveal The Chosen Branch"
    "Accounting And Materialization Can Diverge"
    "Exit Hooks Need State-View And Ordering Contracts"
    "Test Artifacts Must Be Reviewable, Not Just Passing"
    "Token Identity, Late Freezing, And Arithmetic Drift"
    "Lattice Witnesses Beat Anecdotal Edge Cases"
    "Shared Tables Need Consumer-Specific Trigger Semantics"
    "candidate_order"
    "selection_rule"
    "recovery_artifact"
    "same visible output / different trigger path"
)
for term in "${tex_semantics_terms[@]}"; do
    require_text "docs/129_tex_semantics_lessons.md" "$term"
done

source_truth_input_output_terms=(
    "Inputter / Outputter Boundary Rule"
    "what source bytes/tokens/facts were adopted"
    "what artifact node was built"
    "marker/payload/residue"
    "docs/129_tex_semantics_lessons.md"
    "scanner owner, stop condition, lookahead policy, and adopted recovery value"
    "capture point, planner point, commit point, rollback/cancel point"
    "semantic equality or canonicalization rule"
    "deterministic side-effect trace"
    "What is adopted at the input boundary?"
    "What is built but not yet committed at the output boundary?"
    "Which artifact layer is the oracle"
)
for term in "${source_truth_input_output_terms[@]}"; do
    require_text "docs/125_source_of_truth_spine.md" "$term"
done

beta_io_boundary_terms=(
    "codegen outputter-owner split"
    "not the AIR/RIR resource-boundary inputter"
    "\`Print\` and \`Log*\` remain observability outputter artifact calls"
    "explicitly excluded from \`io_boundary_builtin.c\`"
)
for term in "${beta_io_boundary_terms[@]}"; do
    require_text "docs/100_beta_readiness_checklist.md" "$term"
done

beta_progress_terms=(
    "strict beta readiness is now about 83%"
    "Do not call this beta-complete until"
    "The five closure targets are:"
    "CFG/body safety source-of-truth"
    "AIR abstraction-boundary verification"
    "DAG recursive compatibility seam removal"
    "MIR/LLVM declaration bootstrap parity"
    "ABI/Slot/Pin ownership freeze"
)
for term in "${beta_progress_terms[@]}"; do
    require_text "docs/100_beta_readiness_checklist.md" "$term"
done
for term in "${beta_progress_terms[@]}"; do
    require_text "TODO.md" "$term"
done
for rel in \
    "TODO.md" \
    "docs/100_beta_readiness_checklist.md" \
    "docs/70_beta_closure_master_board.md" \
    "docs/README_ko.md"; do
    forbid_text "$rel" "strict beta readiness is now about 70-72%"
    forbid_text "$rel" 'strict beta readiness `약 70-72%`'
    forbid_text "$rel" "strict beta readiness 약 70-72%"
    forbid_text "$rel" "about 70-72%"
    forbid_text "$rel" "strict beta readiness is now about 75%"
    forbid_text "$rel" "약 70-72%"
done
for rel in "TODO.md" "docs/100_beta_readiness_checklist.md"; do
    forbid_text "$rel" "strict beta readiness 100%"
    forbid_text "$rel" "All five targets are fully closed"
done

for rel in \
    "README.md" \
    "docs/37_compiler_contracts.md" \
    "docs/42_keyword_orthogonality.md"; do
    forbid_text "$rel" "??"
done

air_terms=(
    "DIR -> step -> intent_node"
    "intent_step_ast -> execution_boundary -> boundary_node"
    "AIR -> no_drift"
)
for term in "${air_terms[@]}"; do
    require_text "docs/semantics/07_air_abstraction_safety.md" "$term"
done

air_architecture_terms=(
    "Epsilon-Loss Isolation Contract"
    "AIR does not pretend that abstraction lowering is lossless."
    "AIR's job is to isolate that epsilon-loss at the boundary."
    "epsilon quarantine layer, not an epsilon elimination layer"
    "Semantic truth remains with the owning layer"
    "cross-layer boundary with explicit evidence"
    "The general version of this rule is the abstraction loss contract"
)
for term in "${air_architecture_terms[@]}"; do
    require_text "docs/104_air_compiler_architecture.md" "$term"
done

semantic_squiggle_terms=(
    'Status: `slices 1-5 landed`'
    "slice 5(AIR BLUE/erasure 배선) **완료·검증**"
    "slice 5: BLUE"
    "slice 5 (landed)"
)
for term in "${semantic_squiggle_terms[@]}"; do
    require_text "docs/140_semantic_squiggle.md" "$term"
done
forbid_text "docs/140_semantic_squiggle.md" 'Status: `slices 1-3 landed`'
forbid_text "docs/140_semantic_squiggle.md" "⏳ (R&D) BLUE"

sea_execution_lane_terms=(
    "BoundaryCaptureFact"
    "boundary_capture"
    "captures_pin"
    "captures_raw_channel"
    "ExecutionLaneFact"
    "Precise capture plumbing"
)
for term in "${sea_execution_lane_terms[@]}"; do
    require_text "docs/146_sea_execution_lanes.md" "$term"
done

loss_contract_terms=(
    "Abstraction Loss Contracts"
    "Loss is not automatically a bug. Hidden loss is the bug."
    "An abstraction loss contract has seven fields:"
    "Loss budget classes:"
    "consumer forbidden_to_recover fact from source"
    "Theorem: Loss Visibility"
    "Theorem: Preservation Carry"
    "Theorem: Bounded Approximation Soundness"
    "That syntax is a design sketch only."
)
for term in "${loss_contract_terms[@]}"; do
    require_text "docs/semantics/09_abstraction_loss_contracts.md" "$term"
done
require_text "docs/37_compiler_contracts.md" "### Loss Contracts"
require_text "docs/125_source_of_truth_spine.md" "## 9. Loss Contract Rule"

compiler_spine_terms=(
    "Stable handles, movable boundaries, gated migrations."
    "Verified Projection Plan"
    "Projection Plan Gate"
    "Boundary Migration Protocol"
    "Boundary Migration Gate"
    "Source/Parser Artifact Gate"
    "Stable Identity Gate"
    "Build Resource Budget Gate"
)
for term in "${compiler_spine_terms[@]}"; do
    require_text "docs/180_compiler_logical_spine_handles_gates.md" "$term"
done
require_text "AGENTS.md" "docs/180_compiler_logical_spine_handles_gates.md"

remote_future_terms=(
    "RemoteFuture<T> -> await -> Result<T>"
    "let result: Result<Int> = await pending;"
    "let value: Int = Unwrap(result);"
)
for term in "${remote_future_terms[@]}"; do
    require_text "examples/remote_future_result.pgy" "$term"
done

echo "[documentation-quality] UTF, docs index, async wording, and executable example surface ok"

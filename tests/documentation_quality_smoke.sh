#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_DOC_QUALITY_FULL_UTF8="${PGY_DOC_QUALITY_FULL_UTF8:-0}"

fail() {
    echo "[documentation-quality] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing documentation quality input: $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden simplification: $term"
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

    if command -v iconv >/dev/null 2>&1; then
        iconv -f UTF-8 -t UTF-8 "$path" >/dev/null || fail "$rel is not valid UTF-8"
    fi
    if LC_ALL=C grep -q $'\357\277\275' "$path"; then
        fail "$rel contains Unicode replacement characters"
    fi
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
    "examples/remote_future_result.pgy"
    "docs/grammar/01_syntax.md"
    "docs/grammar/02_grammar.md"
    "docs/124_syntax_pattern_matrix.md"
    "docs/125_source_of_truth_spine.md"
    "docs/129_tex_semantics_lessons.md"
    "docs/37_compiler_contracts.md"
    "docs/42_keyword_orthogonality.md"
    "TODO.md"
)

for rel in "${required_files[@]}"; do
    require_file "$rel"
done

for rel in "${required_files[@]}"; do
    validate_utf8_file "$ROOT_DIR/$rel"
done
validate_utf8_file "$ROOT_DIR/TODO.md"

if [[ "$PGY_DOC_QUALITY_FULL_UTF8" == "1" ]]; then
    while IFS= read -r -d '' path; do
        validate_utf8_file "$path"
    done < <(find "$ROOT_DIR/docs" "$ROOT_DIR/examples" -type f \( -name '*.md' -o -name '*.pgy' \) -print0)
fi

require_text "TODO.md" "Pergyra TODO"

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
)
for term in "${index_terms[@]}"; do
    require_text "docs/INDEX.md" "$term"
done

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
    "separate"
    "unsafe"
    "contract"
)
for term in "${slot_pinning_terms[@]}"; do
    require_text "docs/74_slot_pinning_caching.md" "$term"
done

stable_subset_slot_terms=(
    "Non-pin handle expiration is not claimed as a single-mechanism proof"
    "First-class Zone-Bound Handle typing"
    "conservative \`BORROW_TRACKED\`"
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
    "Slicing/spread/rest"
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
    "strict beta readiness is fixed at 67%"
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
)
for term in "${air_architecture_terms[@]}"; do
    require_text "docs/104_air_compiler_architecture.md" "$term"
done

remote_future_terms=(
    "RemoteFuture<T> -> await -> Result<T>"
    "let result: Result<Int> = await pending;"
    "let value: Int = Unwrap(result);"
)
for term in "${remote_future_terms[@]}"; do
    require_text "examples/remote_future_result.pgy" "$term"
done

echo "[documentation-quality] UTF, docs index, async wording, and executable example surface ok"

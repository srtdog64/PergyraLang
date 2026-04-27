#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

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
    "docs/INDEX.md"
    "docs/19_design_philosophy.md"
    "docs/116_documentation_quality_audit.md"
    "docs/107_beta_stable_subset.md"
    "docs/118_slot_model_rigor_audit.md"
    "docs/05_async_concurrency.md"
    "docs/113_memory_concurrency_model.md"
    "docs/114_async_model_positioning.md"
    "examples/async_demo.pgy"
    "docs/semantics/07_air_abstraction_safety.md"
    "examples/remote_future_result.pgy"
    "docs/grammar/01_syntax.md"
    "docs/grammar/02_grammar.md"
    "TODO.md"
)

for rel in "${required_files[@]}"; do
    require_file "$rel"
done

while IFS= read -r -d '' path; do
    validate_utf8_file "$path"
done < <(find "$ROOT_DIR/docs" "$ROOT_DIR/examples" -type f \( -name '*.md' -o -name '*.pgy' \) -print0)
validate_utf8_file "$ROOT_DIR/TODO.md"

require_text "TODO.md" "Pergyra TODO (배포 준비)"

index_terms=(
    "PergyraLang Documentation Index"
    "Beta Closure Source Of Truth"
    "19_design_philosophy.md"
    "Async, Parallel, And Memory"
    "116_documentation_quality_audit.md"
    "Current Documentation Policy"
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
    "intent/zone/world의 어떤 변경도 C FFI ABI를 깨면 안 된다"
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
)
for term in "${audit_terms[@]}"; do
    require_text "docs/116_documentation_quality_audit.md" "$term"
done

slot_rigor_terms=(
    "Handle Expiration Is A Layered Contract, Not Pin Alone"
    "Non-pin stale-handle scenarios"
    "arena lane checks, CFG/body dataflow"
    "Zone-Bound Handle"
    "SlotHandle<T> in Zone"
    "Pinning solves lexical pinned access"
)
for term in "${slot_rigor_terms[@]}"; do
    require_text "docs/118_slot_model_rigor_audit.md" "$term"
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

while IFS= read -r -d '' path; do
    rel="${path#"$ROOT_DIR/"}"
    if [[ "$rel" == "examples/party_system_demo.pgy" ]]; then
        require_text "$rel" "Status: design sketch"
        continue
    fi
    if grep -Fq -- "async {" "$path"; then
        fail "$rel uses capture-bearing anonymous async block outside a design sketch"
    fi
done < <(find "$ROOT_DIR/examples" -type f -name '*.pgy' -print0)

grammar_terms=(
    "named \`spawn Worker(args...)\`"
    "capture-bearing detached \`async { ... }\`"
)
for term in "${grammar_terms[@]}"; do
    require_text "docs/grammar/01_syntax.md" "$term"
    require_text "docs/grammar/02_grammar.md" "$term"
done

air_terms=(
    "DIR -> step -> intent_node"
    "intent_step_ast -> execution_boundary -> boundary_node"
    "AIR -> no_drift"
)
for term in "${air_terms[@]}"; do
    require_text "docs/semantics/07_air_abstraction_safety.md" "$term"
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

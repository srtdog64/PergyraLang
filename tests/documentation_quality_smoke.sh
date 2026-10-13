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

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing documentation quality input: $rel"
}

forbid_mojibake_text() {
    local rel="$1"
    local pattern

    pattern="$(printf '%b' '\357\247\215|\351\201\272|\346\217\264|\346\200\250|\351\215\256|\345\252\233|\346\264\271|\353\252\204|\353\272\244|\354\222\225|\352\276\251|\353\214\204')"
    if LC_ALL=C grep -Eq "$pattern" "$ROOT_DIR/$rel"; then
        fail "$rel contains likely mojibake text"
    fi
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
    "docs/Main.md"
    "docs/language/Main.md"
    "docs/compiler/Main.md"
    "docs/runtime/Main.md"
    "docs/concurrency/Main.md"
    "docs/semantics/Main.md"
    "docs/security/Main.md"
    "docs/self_hosted/Main.md"
    "docs/stdlib/Main.md"
    "docs/tooling/Main.md"
    "docs/release/Main.md"
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
    "docs/semantics/23_compiler_stable_identity.md"
    "docs/semantics/24_semantic_declaration_identity.md"
    "docs/semantics/25_hir_routine_identity.md"
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

echo "[documentation-quality] UTF, docs index, async wording, and executable example surface ok"

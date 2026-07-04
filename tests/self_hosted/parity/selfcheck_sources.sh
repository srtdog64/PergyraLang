#!/usr/bin/env bash
# Real-source self-application for the Pergyra-origin semantic checker.
#
# The semantic_parity.sh gate proves the checker on bounded toy fixtures. This
# script is the next rung: it runs the compiled checker on ACTUAL self-host
# source files and asserts each is accepted (a clean `Status: ok` diagnostic).
# It is the bridge from rung-2 toward true self-hosting -- as the checker's
# bounded subset grows to cover more constructs, add the now-checkable real
# sources to SELF_SOURCES.
#
#     make pgy
#     tests/self_hosted/parity/selfcheck_sources.sh
#
# Seed: import-aware semantic self-application now checks imported real
# entrypoints through the same source-bundle owner used by the tool. Sources
# that use unsupported constructs stay out of the list until the checker covers
# them.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    if [[ -z "${PGY_BIN:-}" ]]; then
        echo "[self-host-selfcheck] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-selfcheck] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/semantic_selfcheck}"
TOOL="$BUILD_DIR/main.pgy"
RUN_ID="${PGY_SELFHOST_RUN_ID:-$$}"

mkdir -p "$BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/semantic/"*.pgy "$BUILD_DIR/"
LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

# Curated list of real self-host sources the checker is expected to accept.
# Imported entrypoints must be checked through source_bundle_owner.pgy, not by
# deleting import lines or concatenating temporary units.
SELF_SOURCES=(
    "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy"
    "src/self_hosted/codegen/input/ast_input_owner.pgy"
    "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy"
    "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy"
    "src/self_hosted/codegen/input/ast_text_statement_owner.pgy"
    "src/self_hosted/codegen/input/ast_usage_owner.pgy"
    "src/self_hosted/codegen/main.pgy"
    "src/self_hosted/codegen/typed_ast_node_skeleton.pgy"
    "src/self_hosted/codegen/emission/expr_rewrite.pgy"
    "src/self_hosted/codegen/emission/function_emit.pgy"
    "src/self_hosted/codegen/emission/program_emit.pgy"
    "src/self_hosted/codegen/emission/stmt_emit.pgy"
    "src/self_hosted/codegen/emission/struct_value_emit.pgy"
    "src/self_hosted/codegen/run/codegen_run_owner.pgy"
    "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy"
    "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy"
    "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy"
    "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy"
    "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy"
    "src/self_hosted/codegen/text/text_owner.pgy"
    "src/self_hosted/codegen/type_facts/type_env.pgy"
    "src/self_hosted/compiler/path_manifest_owner.pgy"
    "src/self_hosted/compiler/stage_intents.pgy"
    "src/self_hosted/compiler/target_capability_owner.pgy"
    "src/self_hosted/compiler/air_evidence_owner.pgy"
    "src/self_hosted/compiler/artifact_zone_owner.pgy"
    "src/self_hosted/compiler/test_harness_owner.pgy"
    "src/self_hosted/compiler/subprocess_runner_owner.pgy"
    "src/self_hosted/compiler/abi_layout_row_owner.pgy"
    "src/self_hosted/compiler/symbol_table_owner.pgy"
    "src/self_hosted/compiler/stage_artifact_owner.pgy"
    "src/self_hosted/compiler/test_harness_manifest.pgy"
    "src/self_hosted/compiler/world.pgy"
    "src/self_hosted/fuzz/backend_parity_generator/main.pgy"
    "src/self_hosted/lexer/char_owner.pgy"
    "src/self_hosted/lib/path.pgy"
    "src/self_hosted/lib/text_scan.pgy"
    "src/self_hosted/lib/diagnostic.pgy"
    "src/self_hosted/lib/json_scan.pgy"
    "src/self_hosted/lib/json_emit.pgy"
    "src/self_hosted/lib/json.pgy"
    "src/self_hosted/lib/json_fact_table.pgy"
    "src/self_hosted/lsp/diagnostics_owner.pgy"
    "src/self_hosted/lsp/document_store_owner.pgy"
    "src/self_hosted/lsp/feature_owner.pgy"
    "src/self_hosted/lsp/hover_content_owner.pgy"
    "src/self_hosted/lsp/request_owner.pgy"
    "src/self_hosted/lsp/response_owner.pgy"
    "src/self_hosted/lsp/session_owner.pgy"
    "src/self_hosted/lsp/session_state_owner.pgy"
    "src/self_hosted/lsp/squiggle_owner.pgy"
    "src/self_hosted/lsp/transport_owner.pgy"
    "src/self_hosted/lsp/main.pgy"
    "src/self_hosted/lexer/main.pgy"
    "src/self_hosted/lexer/scan_owner.pgy"
    "src/self_hosted/lexer/source_input_owner.pgy"
    "src/self_hosted/lexer/token_owner.pgy"
    "src/self_hosted/mir_lower/decl_lower.pgy"
    "src/self_hosted/mir_lower/error_owner.pgy"
    "src/self_hosted/mir_lower/json_fact_read.pgy"
    "src/self_hosted/mir_lower/main.pgy"
    "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy"
    "src/self_hosted/mir_lower/mir_json_input_owner.pgy"
    "src/self_hosted/mir_lower/program_lower.pgy"
    "src/self_hosted/mir_lower/routine_inventory_owner.pgy"
    "src/self_hosted/mir_lower/routine_lower.pgy"
    "src/self_hosted/mir_lower/stmt_render.pgy"
    "src/self_hosted/parser/cursor_owner.pgy"
    "src/self_hosted/parser/decl_ability_owner.pgy"
    "src/self_hosted/parser/decl_dispatch_owner.pgy"
    "src/self_hosted/parser/decl_effect_relation_owner.pgy"
    "src/self_hosted/parser/decl_enum_owner.pgy"
    "src/self_hosted/parser/decl_event_owner.pgy"
    "src/self_hosted/parser/decl_intent_owner.pgy"
    "src/self_hosted/parser/decl_nominal_owner.pgy"
    "src/self_hosted/parser/decl_role_owner.pgy"
    "src/self_hosted/parser/decl_type_owner.pgy"
    "src/self_hosted/parser/decl_zone_owner.pgy"
    "src/self_hosted/parser/error_owner.pgy"
    "src/self_hosted/parser/expr_owner.pgy"
    "src/self_hosted/parser/function_decl_owner.pgy"
    "src/self_hosted/parser/main.pgy"
    "src/self_hosted/parser/program_parse_owner.pgy"
    "src/self_hosted/parser/source_path_owner.pgy"
    "src/self_hosted/parser/stmt_owner.pgy"
    "src/self_hosted/parser/tree_text_owner.pgy"
    "src/self_hosted/parser/type_name_owner.pgy"
    "src/self_hosted/semantic/diagnostic_code_owner.pgy"
    "src/self_hosted/semantic/diagnostic_owner.pgy"
    "src/self_hosted/semantic/env_owner.pgy"
    "src/self_hosted/semantic/body_check_owner.pgy"
    "src/self_hosted/semantic/call_check_owner.pgy"
    "src/self_hosted/semantic/expr_type_owner.pgy"
    "src/self_hosted/semantic/expr_validation_owner.pgy"
    "src/self_hosted/semantic/main.pgy"
    "src/self_hosted/semantic/program_check_owner.pgy"
    "src/self_hosted/semantic/semantic_run_owner.pgy"
    "src/self_hosted/semantic/source_bundle_owner.pgy"
    "src/self_hosted/semantic/text_scan_owner.pgy"
    "src/self_hosted/sea/execution_lane.pgy"
    "src/self_hosted/tools/air_graph_id_uniqueness/main.pgy"
    "src/self_hosted/tools/air_graph_json_validator/main.pgy"
    "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy"
    "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy"
    "src/self_hosted/tools/air_graph_node_count_integrity/main.pgy"
    "src/self_hosted/tools/air_graph_reachability/main.pgy"
    "src/self_hosted/tools/air_graph_ref_integrity/main.pgy"
    "src/self_hosted/tools/air_graph_ref_live/main.pgy"
    "src/self_hosted/tools/ast_read_surface_checker/main.pgy"
    "src/self_hosted/tools/backend_output_comparator/main.pgy"
    "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy"
    "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy"
    "src/self_hosted/tools/diagnostic_catalog_checker/scan_owner.pgy"
    "src/self_hosted/tools/doc_link_checker/main.pgy"
    "src/self_hosted/tools/examples_inventory_checker/main.pgy"
    "src/self_hosted/tools/linter/main.pgy"
    "src/self_hosted/tools/module_manifest_resolver/main.pgy"
    "src/self_hosted/tools/production_c_size_checker/main.pgy"
    "src/self_hosted/tools/production_header_size_checker/main.pgy"
    "src/self_hosted/tools/runtime_boundary_checker/main.pgy"
    "src/self_hosted/tools/stable_subset_section_checker/main.pgy"
    "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy"
)

BACKENDS="${PGY_SELFHOST_SEMANTIC_BACKENDS:-c llvm}"
self_source_count="${#SELF_SOURCES[@]}"
for backend in $BACKENDS; do
    TOOL_BIN="$BUILD_DIR/main_selfcheck_${backend}_${RUN_ID}.exe"
    echo "[self-host-selfcheck] compiling checker backend=$backend..."
    rm -f "$TOOL_BIN"
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$TOOL")" \
        --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$TOOL_BIN")" >/dev/null)

    source_index=0
    for src in "${SELF_SOURCES[@]}"; do
        source_index=$((source_index + 1))
        echo "[self-host-selfcheck] backend=$backend checking $source_index/$self_source_count $src"
        out="$(cd "$ROOT_DIR" && "$TOOL_BIN" "$src" 2>/dev/null | tr -d '\r')"
        if ! grep -Fq 'Diagnostic: pgy.selfhost.semantic.v1' <<<"$out"; then
            echo "[self-host-selfcheck] backend=$backend $src: no diagnostic block" >&2
            printf '%s\n' "$out" >&2
            exit 1
        fi
        if ! grep -Fq 'Status: ok' <<<"$out"; then
            echo "[self-host-selfcheck] backend=$backend $src: checker rejected real source" >&2
            printf '%s\n' "$out" >&2
            exit 1
        fi
    done
    echo "[self-host-selfcheck] backend=$backend ok: $self_source_count real sources accepted"
done

echo "[self-host-selfcheck] real-source self-application ok: $self_source_count sources; backends=$BACKENDS"

#!/usr/bin/env bash
# Gates compiler-stage self-host substitute contracts.
#
# Heavy parity scripts prove behavior. This smoke proves the self-hosted
# compiler-stage surface itself is wired correctly: each active stage has an
# intent-verification pair, a Makefile target, and fixture/expected files that
# are actually listed by its parity harness.

set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "${SCRIPT_PATH%/*}" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SELF_HOST_DIR="$ROOT_DIR/src/self_hosted"
PARITY_DIR="$ROOT_DIR/tests/self_hosted/parity"

fail() {
    echo "[self-host-component-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_dir() {
    local rel="$1"
    [[ -d "$ROOT_DIR/$rel" ]] || fail "missing directory $rel"
}

TEXT_CACHE_REL=""
TEXT_CACHE_CONTENT=""

load_text_cache() {
    local rel="$1"

    if [[ "$TEXT_CACHE_REL" != "$rel" ]]; then
        [[ -f "$ROOT_DIR/$rel" ]] || fail "missing text input: $rel"
        TEXT_CACHE_CONTENT="$(<"$ROOT_DIR/$rel")" || fail "could not read text input: $rel"
        TEXT_CACHE_REL="$rel"
    fi
}

require_text() {
    local rel="$1"
    local term="$2"
    load_text_cache "$rel"
    [[ "$TEXT_CACHE_CONTENT" == *"$term"* ]] ||
        fail "$rel missing term: $term"
}

reject_text() {
    local rel="$1"
    local term="$2"
    load_text_cache "$rel"
    if [[ "$TEXT_CACHE_CONTENT" == *"$term"* ]]; then
        fail "$rel must not contain retired term: $term"
    fi
}

require_make_target_recipe_line() {
    local target="$1"
    local recipe_line="$2"

    awk -v target="$target" -v recipe_line="$recipe_line" '
        {
            line = $0
            sub(/\r$/, "", line)
            if (index(line, target ":") == 1) {
                inside = 1
                next
            }
            if (inside && line !~ /^[[:space:]]/ && line ~ /:/) {
                inside = 0
            }
            if (inside && line == "\t" recipe_line) {
                found = 1
            }
        }
        END {
            exit found ? 0 : 1
        }
    ' "$ROOT_DIR/Makefile" ||
        fail "Makefile target $target missing recipe line: $recipe_line"
}

contains_line() {
    local haystack="$1"
    local needle="$2"
    local line

    while IFS= read -r line; do
        [[ "$line" == "$needle" ]] && return 0
    done <<<"$haystack"
    return 1
}

find_stage_owner_sources() {
    local stage="$1"
    local stage_dir="$SELF_HOST_DIR/$stage"
    find "$stage_dir" -type f -name '*.pgy' \
        ! -path "$stage_dir/fixture/*" \
        ! -path "$stage_dir/expected/*" \
        | sort
}

extract_shell_array_items() {
    local file="$1"
    local array_name="$2"
    awk -v array_name="$array_name" '
        $0 ~ "^" array_name "=\\(" { inside = 1; next }
        inside && $0 ~ "^[[:space:]]*\\)" { inside = 0; next }
        inside {
            line = $0
            sub(/[[:space:]]*#.*/, "", line)
            gsub(/"/, "", line)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
            if (line != "") {
                print line
            }
        }
    ' "$file"
}

require_owner_surface() {
    local stage="$1"
    shift
    local stage_dir="$SELF_HOST_DIR/$stage"
    local count
    count="$(find_stage_owner_sources "$stage" | wc -l | tr -d ' ')"
    [[ "$count" -gt 1 ]] ||
        fail "$stage must not be a monolithic main.pgy-only compiler stage"

    local owner
    for owner in "$@"; do
        require_file "src/self_hosted/$stage/$owner"
        require_text "src/self_hosted/$stage/main.pgy" "import \"$owner\";"
    done
}

require_stage_world_binding() {
    local stage="$1"
    local zone="$2"
    local actor="$3"
    local intent="$4"
    local cluster="$5"
    local payload="$6"
    local rel="src/self_hosted/$stage/intent.md"

    require_text "$rel" "## Compiler World Binding"
    require_text "$rel" "- **world_zone**: \`$zone\`"
    require_text "$rel" "- **stage_actor**: \`$actor\`"
    require_text "$rel" "- **stage_intent**: \`$intent\`"
    require_text "$rel" "- **intent_cluster**: \`$cluster\`"
    require_text "$rel" "- **payload_contract**: \`$payload\`"
    require_text "$rel" "- **manifest_binding**: \`$stage|$zone|$actor|$intent|$payload\`"
}

line_count() {
    local count

    count="$(wc -l <"$1")"
    count="${count//[[:space:]]/}"
    printf '%s\n' "$count"
}

require_max_lines() {
    local rel="$1"
    local cap="$2"
    local count
    count="$(line_count "$ROOT_DIR/$rel")"
    [[ "$count" -le "$cap" ]] ||
        fail "$rel has $count lines; cap is $cap"
}

require_entrypoint_only_main() {
    local stage="$1"
    local rel="src/self_hosted/$stage/main.pgy"
    local path="$ROOT_DIR/$rel"
    local main_funcs
    local other_funcs

    require_max_lines "$rel" 80

    main_funcs="$(grep -Ec '^[[:space:]]*func[[:space:]]+Main[[:space:]]*\(' "$path" || true)"
    [[ "$main_funcs" -eq 1 ]] ||
        fail "$rel must contain exactly one Main entrypoint"

    other_funcs="$(
        grep -En '^[[:space:]]*func[[:space:]]+' "$path" |
            grep -Ev '^[0-9]+:[[:space:]]*func[[:space:]]+Main[[:space:]]*\(' || true
    )"
    [[ -z "$other_funcs" ]] ||
        fail "$rel must not define helper functions: $other_funcs"

    awk '
        /^[[:space:]]*\/\// { next }
        /^[[:space:]]*$/ { next }
        /(^|[[:space:]])(if|while|for)[[:space:]]/ ||
        /ArrayPush[[:space:]]*\(/ ||
        /JsonField/ ||
        /Substring[[:space:]]*\(/ ||
        /String(IndexOf|Length)[[:space:]]*\(/ ||
        /SEMANTIC ERROR/ ||
        /[{]["]/ {
            print FNR ":" $0
        }
    ' "$path" | while IFS= read -r line; do
        fail "$rel reintroduced semantic work into entrypoint: $line"
    done
}

require_stage_owner_line_cap() {
    local stage="$1"
    local file
    while IFS= read -r file; do
        rel="${file#"$ROOT_DIR/"}"
        require_max_lines "$rel" 600
        require_text "src/self_hosted/OWNERS.md" "$rel"
    done < <(find_stage_owner_sources "$stage")
}

require_file "src/self_hosted/OWNERS.md"
require_text "src/self_hosted/OWNERS.md" 'main.pgy` files are entrypoints only'
require_file "src/self_hosted/lib/json.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lib/json.pgy"
require_max_lines "src/self_hosted/lib/json.pgy" 600
require_file "src/self_hosted/lib/json_emit.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lib/json_emit.pgy"
require_max_lines "src/self_hosted/lib/json_emit.pgy" 600
require_file "src/self_hosted/lib/json_scan.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lib/json_scan.pgy"
require_max_lines "src/self_hosted/lib/json_scan.pgy" 600
require_file "src/self_hosted/lib/json_fact_table.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lib/json_fact_table.pgy"
require_max_lines "src/self_hosted/lib/json_fact_table.pgy" 600
require_text "src/self_hosted/lib/json_fact_table.pgy" 'import "json.pgy";'
require_text "src/self_hosted/lib/json_fact_table.pgy" "struct JsonObjectFactTable"
require_text "src/self_hosted/lib/json_fact_table.pgy" "struct JsonArrayObjectFactTable"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonObjectFactTableSchema"
require_text "src/self_hosted/lib/json_fact_table.pgy" "pgy.selfhost.json-object-fact-table.v1"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonDocumentObjectFactTable"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonObjectFactArrayBounds"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonObjectFactArrayObjectTable"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonArrayObjectFactAt"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonArrayObjectFactFieldCount"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonObjectFactStringFieldEquals"
require_text "src/self_hosted/lib/json_fact_table.pgy" "func JsonDocumentFactStringFieldEquals"
reject_text "src/self_hosted/lib/json_fact_table.pgy" "let keys: Array<String>;"
reject_text "src/self_hosted/lib/json_fact_table.pgy" "let value_starts: Array<Int>;"
reject_text "src/self_hosted/lib/json.pgy" 'import "json_emit.pgy";'
require_text "src/self_hosted/lib/json.pgy" 'import "json_scan.pgy";'
require_text "src/self_hosted/lib/json_scan.pgy" "func JsonCharAt"
reject_text "src/self_hosted/lib/json_scan.pgy" "func CharAt"
require_text "src/self_hosted/lib/json_scan.pgy" "func FindFrom(hay: String, needle: String, start: Int) -> Option<Int>"
reject_text "src/self_hosted/lib/json_scan.pgy" "func FindFrom(hay: String, needle: String, start: Int) -> Int"
require_text "src/self_hosted/lib/json.pgy" "func ReadJsonString"
require_text "src/self_hosted/lib/json.pgy" "func JsonFieldString"
require_text "src/self_hosted/lib/json.pgy" "func JsonFieldNumber"
require_text "src/self_hosted/lib/json.pgy" "func JsonFieldKey"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayEnd(json: String, open: Int, limit: Int) -> Option<Int>"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectEnd(json: String, open: Int, limit: Int) -> Option<Int>"
require_text "src/self_hosted/lib/json.pgy" "func JsonDocumentObjectEnd(json: String) -> Option<Int>"
require_text "src/self_hosted/lib/json.pgy" "func JsonValueEnd(json: String, value_start: Int, end: Int) -> Option<Int>"
reject_text "src/self_hosted/lib/json.pgy" "return -1"
reject_text "src/self_hosted/lib/json.pgy" "func JsonDocumentStringFieldEquals"
reject_text "src/self_hosted/lib/json.pgy" "func JsonDocumentHasField"
require_text "src/self_hosted/lib/json.pgy" "func JsonFieldArrayBounds"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayObjectCount"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayObjectBoundsAt"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectFieldValueBounds"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectStringField"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectNumberField"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectNumberFieldOpt"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayObjectFieldCount"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayObjectStringFieldEqualsCount"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayObjectBoolFieldEqualsCount"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayStringAt"
require_text "src/self_hosted/lib/json.pgy" "func JsonArrayStringCount"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectArrayStringAt"
require_text "src/self_hosted/lib/json.pgy" "func JsonDocumentNumberField"
reject_text "src/self_hosted/lib/json.pgy" "func JsonFirstArrayString"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEscapeString"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonStringLiteral"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEmitObject"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEmitArray"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEmitFieldRaw"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEmitFieldString"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEmitFieldNumber"
require_text "src/self_hosted/lib/json_emit.pgy" "func JsonEmitFieldBool"
for json_emit_consumer in \
    src/self_hosted/tools/air_graph_id_uniqueness/main.pgy \
    src/self_hosted/tools/air_graph_json_validator/report_owner.pgy \
    src/self_hosted/tools/air_graph_node_count_integrity/main.pgy \
    src/self_hosted/tools/air_graph_reachability/main.pgy \
    src/self_hosted/tools/air_graph_ref_integrity/main.pgy \
    src/self_hosted/tools/air_graph_ref_live/main.pgy \
    src/self_hosted/tools/ast_read_surface_checker/main.pgy \
    src/self_hosted/tools/backend_output_comparator/main.pgy \
    src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy \
    src/self_hosted/tools/doc_link_checker/main.pgy \
    src/self_hosted/tools/examples_inventory_checker/main.pgy \
    src/self_hosted/tools/module_manifest_resolver/main.pgy \
    src/self_hosted/tools/production_c_size_checker/main.pgy \
    src/self_hosted/tools/production_header_size_checker/main.pgy \
    src/self_hosted/tools/stable_subset_section_checker/main.pgy \
    src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy; do
    require_text "$json_emit_consumer" 'import "../../lib/json_emit.pgy";'
done

for stage in lexer parser semantic codegen; do
    require_dir "src/self_hosted/$stage"
    require_file "src/self_hosted/$stage/main.pgy"
    require_file "src/self_hosted/$stage/README.md"
    require_file "src/self_hosted/$stage/intent.md"
    require_dir "src/self_hosted/$stage/fixture"
    require_dir "src/self_hosted/$stage/expected"
    require_file "tests/self_hosted/parity/${stage}_parity.sh"

    for anchor in '## Intent' '## Compiler World Binding' '## Input Contract' '## Output Contract' '## Oracle'; do
        require_text "src/self_hosted/$stage/intent.md" "$anchor"
    done

    require_text "tests/self_hosted/parity/${stage}_parity.sh" "set -euo pipefail"
    require_text "Makefile" "self-host-${stage}-parity-test-smoke"
    require_text "Makefile" "tests/self_hosted/parity/${stage}_parity.sh"
    require_entrypoint_only_main "$stage"
    require_stage_owner_line_cap "$stage"
done

require_dir "src/self_hosted/mir_lower"
require_file "src/self_hosted/mir_lower/main.pgy"
require_file "src/self_hosted/mir_lower/README.md"
require_file "src/self_hosted/mir_lower/intent.md"
require_dir "src/self_hosted/mir_lower/fixture"
require_file "tests/self_hosted/parity/mir_json_parity.sh"
for anchor in '## Intent' '## Compiler World Binding' '## Input Contract' '## Output Contract' '## Oracle'; do
    require_text "src/self_hosted/mir_lower/intent.md" "$anchor"
done
require_text "tests/self_hosted/parity/mir_json_parity.sh" "set -euo pipefail"
require_text "tests/self_hosted/parity/mir_json_parity.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/mir_json_parity.sh" "compare_mir_run_output_with_owner"
require_text "tests/self_hosted/parity/mir_json_parity.sh" "run_output"
require_text "tests/self_hosted/parity/mir_json_parity.sh" "read_mir_fixture_manifest"
require_text "tests/self_hosted/parity/mir_json_parity.sh" '"$B/mir_lower.exe" --fixture-manifest'
require_text "tests/self_hosted/parity/mir_json_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/mir_json_parity.sh" '"mir-json-parity-paths"'
require_text "tests/self_hosted/parity/mir_json_parity.sh" 'MIR_LOWER_SRC="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/mir_json_parity.sh" 'CODEGEN_SRC="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/mir_json_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"'
reject_text "tests/self_hosted/parity/mir_json_parity.sh" "MIR_FIXTURES=("
reject_text "tests/self_hosted/parity/mir_json_parity.sh" "CODEGEN_FIXTURES=("
reject_text "tests/self_hosted/parity/mir_json_parity.sh" "EXAMPLE_FIXTURES=("
reject_text "tests/self_hosted/parity/mir_json_parity.sh" 'MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"'
reject_text "tests/self_hosted/parity/mir_json_parity.sh" 'CODEGEN_SRC="$ROOT_DIR/src/self_hosted/codegen/main.pgy"'
reject_text "tests/self_hosted/parity/mir_json_parity.sh" "diff <("
require_text "Makefile" "self-host-mir-json-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/mir_json_parity.sh"
require_entrypoint_only_main "mir_lower"
require_stage_owner_line_cap "mir_lower"

require_stage_world_binding "lexer" "TokenStreamZone" "LexerStage" "LexSource" "FrontendPipeline" "LexerTokenPayloadContractReady"
require_stage_world_binding "parser" "AstTreeZone" "ParserStage" "ParseTokens" "FrontendPipeline" "ParserAstTreePayloadContractReady"
require_stage_world_binding "semantic" "SemanticVerdictZone" "SemanticStage" "CheckProgramSemantics" "MiddleEndPipeline" "SemanticVerdictPayloadContractReady"
require_stage_world_binding "mir_lower" "MirFactGraphZone" "MirLowerStage" "LowerProgramFacts" "MiddleEndPipeline" "MirFactGraphPayloadContractReady"
require_stage_world_binding "codegen" "EmissionZone" "ProgramEmitter" "EmitProgramArtifact" "BackendPipeline" "TypedAstArenaPayloadContractReady"

require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" "func MirParityFixtureCount"
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" "func MirParityFixtureAt"
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" "func EmitMirParityFixtureManifest"
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" "MirParityFixtureCount()"
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" "func MirParityFixtureCount() -> Int"
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" "return 86;"
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" '"src/self_hosted/mir_lower/fixture/let_log.pgy"'
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" '"src/self_hosted/codegen/fixture/write_file.pgy"'
require_text "src/self_hosted/mir_lower/fixture_manifest_owner.pgy" '"examples/binary_search.pgy"'
require_text "src/self_hosted/mir_lower/run_owner.pgy" 'args[0] == "--fixture-manifest"'
require_text "src/self_hosted/mir_lower/run_owner.pgy" "EmitMirParityFixtureManifest()"
mir_clean_reject_count="$(grep -Ec '^base="unsupported_' "$PARITY_DIR/mir_json_parity.sh" || true)"
[[ "$mir_clean_reject_count" -eq 0 ]] ||
    fail "mir_json_parity clean reject count drifted: $mir_clean_reject_count != 0"
require_text "src/self_hosted/PROGRESS.md" "86 PASS / 0 gap plus 0 clean"
require_text "docs/self_hosted/07_hard_self_host_scorecard.md" "86 PASS / 0 gap plus 0 clean rejects"
require_text "tests/self_hosted/parity/mir_json_parity.sh" '"kind":"role","name":"IntMath","for_type":"Int"'
require_text "tests/self_hosted/parity/mir_json_parity.sh" "Role: IntMath for Int"
reject_text "tests/self_hosted/parity/mir_json_parity.sh" "unsupported MIR role declaration in self-host subset"
reject_text "tests/self_hosted/parity/mir_json_parity.sh" '"kind":"unsupported","ast_type":"AST_ROLE_DECL"'
reject_text "src/self_hosted/PROGRESS.md" "77 PASS / 0 gap plus 2 clean rejects"
reject_text "docs/self_hosted/07_hard_self_host_scorecard.md" "77 PASS / 0 gap plus 2 clean rejects"
reject_text "docs/self_hosted/07_hard_self_host_scorecard.md" "61-fixture"
reject_text "src/self_hosted/codegen/README.md" "round-trip self-compilation (the codegen tool compiling a Pergyra tool)"
reject_text "src/lexer/lexer.h" "TOKEN_COLON_ASSIGN"
reject_text "src/lexer/lexer.c" "TOKEN_COLON_ASSIGN"
reject_text "src/lexer/lexer_token_debug.c" "COLON_ASSIGN"
reject_text "src/parser/parser_statement_dispatch.c" "TOKEN_COLON_ASSIGN"
reject_text "src/self_hosted/lexer/scan_owner.pgy" "walrus"
reject_text "src/self_hosted/parser/stmt_owner.pgy" "Walrus"
reject_text "tests/self_hosted/parity/parser_parity.sh" "walrus_"
reject_text "src/self_hosted/PROGRESS.md" "walrus surface"

require_owner_surface lexer \
    "run_owner.pgy"
require_file "src/self_hosted/lexer/char_owner.pgy"
require_file "src/self_hosted/lexer/source_input_owner.pgy"
require_file "src/self_hosted/lexer/token_owner.pgy"
require_file "src/self_hosted/lexer/fixture_manifest_owner.pgy"
require_text "src/self_hosted/lexer/run_owner.pgy" 'import "scan_owner.pgy";'
require_text "src/self_hosted/lexer/run_owner.pgy" 'import "source_input_owner.pgy";'
require_text "src/self_hosted/lexer/run_owner.pgy" 'import "fixture_manifest_owner.pgy";'
require_text "src/self_hosted/lexer/scan_owner.pgy" 'import "char_owner.pgy";'
require_text "src/self_hosted/lexer/scan_owner.pgy" 'import "token_owner.pgy";'
require_text "src/self_hosted/lexer/token_owner.pgy" "func LexerTokenPayloadContractReady"
require_text "src/self_hosted/lexer/token_owner.pgy" "func LexerTokenPayloadSchema"
require_text "src/self_hosted/lexer/token_owner.pgy" "pgy.selfhost.lexer-token-stream.v1"
require_text "src/self_hosted/lexer/token_owner.pgy" "LexerTokenPayloadFixtureCount() != 8"
require_text "src/self_hosted/lexer/fixture_manifest_owner.pgy" "func LexerFixtureManifestCount() -> Int"
require_text "src/self_hosted/lexer/fixture_manifest_owner.pgy" "func EmitLexerFixtureManifest"
require_text "src/self_hosted/lexer/fixture_manifest_owner.pgy" "LexerFixtureManifestCount()"
require_text "src/self_hosted/lexer/main.pgy" "RunLexerFromArgs(Args())"
require_text "src/self_hosted/lexer/run_owner.pgy" '"--fixture-manifest"'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../lexer/token_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "LexerTokenPayloadContractReady()"
reject_text "src/self_hosted/lexer/main.pgy" 'import "char_owner.pgy";'
reject_text "src/self_hosted/lexer/main.pgy" 'import "token_owner.pgy";'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/lexer_parity.sh" "normalize_text_artifact"
require_text "tests/self_hosted/parity/lexer_parity.sh" "compile_backend_output_comparator"
require_text "tests/self_hosted/parity/lexer_parity.sh" "read_lexer_fixture_manifest"
require_text "tests/self_hosted/parity/lexer_parity.sh" '"$PERGYRA_TOOL_BUILD_DIR/main.exe" --fixture-manifest'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'compare_lexer_output_with_owner "c" "$label" "$expected_file" "$c_out" 2'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'compare_lexer_output_with_owner "llvm" "$label" "$expected_file" "$llvm_out" 2'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'compare_lexer_output_with_owner "live-tokens" "$label" "$expected_file" "$live_out" 0'
reject_text "tests/self_hosted/parity/lexer_parity.sh" "examples/hello.pgy:hello_tokens.txt"
reject_text "tests/self_hosted/parity/lexer_parity.sh" "string_escape_sequences_tokens.txt"
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lexer/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/"'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'EXPECTED_OUT="$(tr -d'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'PERGYRA_OUT="$(cd'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'LLVM_LEX_OUT="$(cd'
reject_text "tests/self_hosted/parity/lexer_parity.sh" "diff <(printf"
require_owner_surface parser \
    "run_owner.pgy"
require_file "src/self_hosted/parser/source_path_owner.pgy"
require_file "src/self_hosted/parser/program_parse_owner.pgy"
require_file "src/self_hosted/parser/fixture_manifest_owner.pgy"
require_text "src/self_hosted/parser/run_owner.pgy" 'import "source_path_owner.pgy";'
require_text "src/self_hosted/parser/run_owner.pgy" 'import "program_parse_owner.pgy";'
require_text "src/self_hosted/parser/run_owner.pgy" 'import "fixture_manifest_owner.pgy";'
require_text "src/self_hosted/parser/run_owner.pgy" 'args[0] == "--fixture-manifest"'
require_text "src/self_hosted/parser/run_owner.pgy" "EmitParserFixtureManifest()"
require_text "src/self_hosted/parser/run_owner.pgy" "ParserDefaultSourcePath(args)"
require_text "src/self_hosted/parser/run_owner.pgy" "ParseRootProgram(source_path)"
reject_text "src/self_hosted/parser/main.pgy" 'import "source_path_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "program_parse_owner.pgy";'
require_text "src/self_hosted/parser/main.pgy" "RunParserFromArgs(Args())"
require_text "src/self_hosted/parser/fixture_manifest_owner.pgy" "func ParserFixtureManifestRows"
require_text "src/self_hosted/parser/fixture_manifest_owner.pgy" "func EmitParserFixtureManifest"
require_text "src/self_hosted/parser/fixture_manifest_owner.pgy" "DirWalk(ParserFixtureDir())"
require_text "src/self_hosted/parser/fixture_manifest_owner.pgy" "ParserFixtureExpectedPath(base)"
require_text "src/self_hosted/parser/fixture_manifest_owner.pgy" "ArrayLength(rows) != 188"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func ParserAstTreePayloadContractReady"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func ParserAstTreePayloadSchema"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "pgy.selfhost.parser-ast-tree.v1"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "ParserAstTreePayloadFixtureCount() != 187"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../parser/tree_text_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "ParserAstTreePayloadContractReady()"
require_text "src/self_hosted/parser/cursor_owner.pgy" 'import "error_owner.pgy";'
require_text "src/self_hosted/parser/cursor_owner.pgy" "func ParserCharAt"
reject_text "src/self_hosted/parser/cursor_owner.pgy" "func CharAt"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func ExpectOpt"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func ConsumeStmtTerminatorOpt"
reject_text "src/self_hosted/parser/cursor_owner.pgy" "return -1"
reject_text "src/self_hosted/parser/main.pgy" 'import "error_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "cursor_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "tree_text_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "type_name_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "../lib/path.pgy";'
require_text "src/self_hosted/parser/expr_owner.pgy" 'import "expr_string_owner.pgy";'
require_text "src/self_hosted/parser/expr_owner.pgy" 'import "expr_postfix_owner.pgy";'
require_text "src/self_hosted/parser/expr_owner.pgy" 'import "expr_primary_owner.pgy";'
require_text "src/self_hosted/parser/expr_owner.pgy" 'import "expr_precedence_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "expr_string_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "expr_primary_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "expr_postfix_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "expr_precedence_owner.pgy";'
require_text "src/self_hosted/parser/expr_postfix_owner.pgy" "func ApplyPostfixExpr"
reject_text "src/self_hosted/parser/expr_primary_owner.pgy" "Postfix loop:"
require_text "src/self_hosted/parser/stmt_owner.pgy" 'import "stmt_if_owner.pgy";'
require_text "src/self_hosted/parser/stmt_owner.pgy" 'import "error_owner.pgy";'
require_text "src/self_hosted/parser/stmt_owner.pgy" 'import "stmt_loop_owner.pgy";'
require_text "src/self_hosted/parser/stmt_owner.pgy" 'import "stmt_parallel_owner.pgy";'
require_text "src/self_hosted/parser/stmt_owner.pgy" 'import "stmt_match_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "stmt_if_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "stmt_loop_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "stmt_parallel_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "stmt_match_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "stmt_owner.pgy";'
require_text "src/self_hosted/parser/stmt_loop_owner.pgy" "func ParseForStmt"
reject_text "src/self_hosted/parser/stmt_owner.pgy" "func ParseForStmt"
require_text "src/self_hosted/parser/source_path_owner.pgy" "func ParserImportGraphSeen"
require_text "src/self_hosted/parser/program_parse_owner.pgy" 'import "decl_dispatch_owner.pgy";'
require_text "src/self_hosted/parser/program_parse_owner.pgy" 'import "../lib/path.pgy";'
require_text "src/self_hosted/lib/path.pgy" "func PathCharAt"
reject_text "src/self_hosted/lib/path.pgy" "func CharAt"
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'import "stmt_owner.pgy";'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'import "expr_owner.pgy";'
require_text "src/self_hosted/parser/decl_ability_owner.pgy" 'import "function_decl_owner.pgy";'
require_text "src/self_hosted/parser/decl_role_owner.pgy" 'import "function_decl_owner.pgy";'
require_text "src/self_hosted/parser/decl_nominal_owner.pgy" 'import "function_decl_owner.pgy";'
require_text "src/self_hosted/parser/decl_intent_owner.pgy" 'import "expr_owner.pgy";'
require_text "src/self_hosted/parser/decl_zone_owner.pgy" 'import "expr_owner.pgy";'
require_text "src/self_hosted/parser/decl_zone_owner.pgy" 'import "function_decl_owner.pgy";'
require_text "src/self_hosted/parser/decl_dispatch_owner.pgy" 'import "source_path_owner.pgy";'
require_text "src/self_hosted/parser/decl_dispatch_owner.pgy" 'import "function_decl_owner.pgy";'
require_text "src/self_hosted/parser/decl_dispatch_owner.pgy" 'import "decl_ability_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "function_decl_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_type_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_ability_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_event_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_enum_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_zone_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_effect_relation_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_role_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_intent_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_nominal_owner.pgy";'
reject_text "src/self_hosted/parser/main.pgy" 'import "decl_dispatch_owner.pgy";'
require_text "src/self_hosted/parser/decl_dispatch_owner.pgy" "ParserImportGraphSeen(import_paths, imp_path)"
require_text "tests/self_hosted/parity/parser_parity.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/parser_parity.sh" "compare_parser_ast_with_owner"
require_text "tests/self_hosted/parity/parser_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/parser_parity.sh" '"parser-parity-paths"'
require_text "tests/self_hosted/parity/parser_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/parser_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/parser_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/parser_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/parser_parity.sh" 'EXPECTED_FILE="$ROOT_DIR/${harness_paths[3]}"'
require_text "tests/self_hosted/parity/parser_parity.sh" "read_parser_fixture_manifest"
require_text "tests/self_hosted/parity/parser_parity.sh" '"$manifest_bin" --fixture-manifest'
require_text "tests/self_hosted/parity/parser_parity.sh" "ast_text"
reject_text "tests/self_hosted/parity/parser_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/parser/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/src/self_hosted/parser/fixture"'
reject_text "tests/self_hosted/parity/parser_parity.sh" 'EXPECTED_FILE="$ROOT_DIR/src/self_hosted/parser/expected/clean.txt"'
reject_text "tests/self_hosted/parity/parser_parity.sh" "examples/hello.pgy:hello"
reject_text "tests/self_hosted/parity/parser_parity.sh" "src/self_hosted/parser/fixture/import_dedup_graph.pgy:import_dedup_graph"
reject_text "tests/self_hosted/parity/parser_parity.sh" "diff <("
reject_text "tests/self_hosted/parity/parser_parity.sh" "BYTE-DRIFT"
require_owner_surface semantic \
    "semantic_run_owner.pgy"
require_file "src/self_hosted/semantic/text_scan_owner.pgy"
require_file "src/self_hosted/semantic/diagnostic_code_owner.pgy"
require_file "src/self_hosted/semantic/source_bundle_owner.pgy"
require_file "src/self_hosted/semantic/diagnostic_owner.pgy"
require_file "src/self_hosted/semantic/env_owner.pgy"
require_file "src/self_hosted/semantic/expr_type_owner.pgy"
require_file "src/self_hosted/semantic/expr_validation_owner.pgy"
require_file "src/self_hosted/semantic/call_check_owner.pgy"
require_file "src/self_hosted/semantic/body_check_owner.pgy"
require_file "src/self_hosted/semantic/program_check_owner.pgy"
reject_text "src/self_hosted/semantic/main.pgy" 'import "source_bundle_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "diagnostic_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "env_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "expr_type_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "expr_validation_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "call_check_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "body_check_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "program_check_owner.pgy";'
require_text "src/self_hosted/semantic/semantic_run_owner.pgy" 'import "source_bundle_owner.pgy";'
require_text "src/self_hosted/semantic/semantic_run_owner.pgy" 'import "diagnostic_owner.pgy";'
require_text "src/self_hosted/semantic/semantic_run_owner.pgy" 'import "program_check_owner.pgy";'
require_text "src/self_hosted/semantic/source_bundle_owner.pgy" 'import "../lib/path.pgy";'
require_text "src/self_hosted/semantic/source_bundle_owner.pgy" 'import "text_scan_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "../lib/path.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "text_scan_owner.pgy";'
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" 'import "../lib/diagnostic.pgy";'
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" 'import "diagnostic_code_owner.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "../lib/diagnostic.pgy";'
reject_text "src/self_hosted/semantic/main.pgy" 'import "diagnostic_code_owner.pgy";'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'import "text_scan_owner.pgy";'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'import "expr_type_owner.pgy";'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'import "body_check_owner.pgy";'
require_text "src/self_hosted/semantic/body_check_owner.pgy" 'import "call_check_owner.pgy";'
require_text "src/self_hosted/semantic/body_check_owner.pgy" 'import "expr_validation_owner.pgy";'
require_text "src/self_hosted/semantic/body_check_owner.pgy" 'import "expr_type_owner.pgy";'
require_text "src/self_hosted/semantic/call_check_owner.pgy" 'import "expr_validation_owner.pgy";'
require_text "src/self_hosted/semantic/call_check_owner.pgy" 'import "expr_type_owner.pgy";'
require_text "src/self_hosted/semantic/expr_validation_owner.pgy" 'import "expr_type_owner.pgy";'
require_text "src/self_hosted/semantic/expr_type_owner.pgy" 'import "env_owner.pgy";'
require_text "src/self_hosted/semantic/expr_validation_owner.pgy" "func CheckUndefinedIdentifiers"
reject_text "src/self_hosted/semantic/expr_type_owner.pgy" "func CheckUndefinedIdentifiers"
require_text "src/self_hosted/semantic/diagnostic_code_owner.pgy" "func SemanticDiagnosticCodeKnown"
require_text "src/self_hosted/semantic/diagnostic_code_owner.pgy" "func SemanticDiagnosticCodeCount"
require_text "src/self_hosted/semantic/diagnostic_code_owner.pgy" "func SemanticDiagnosticOracleCode"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SemanticDiagnosticCodeKnown(code)"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "func SemanticVerdictPayloadContractReady"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "func SemanticVerdictPayloadSchema"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "pgy.selfhost.semantic.v1"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SemanticVerdictPayloadFixtureCount() != 108"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "func SemanticVerdictPayloadFixtureManifestRows"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "func EmitSemanticVerdictPayloadFixtureManifest"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "DirWalk(SemanticVerdictPayloadFixtureDir())"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SemanticVerdictPayloadExpectedStatus(base)"
require_text "src/self_hosted/semantic/semantic_run_owner.pgy" '"--fixture-manifest"'
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SemanticDiagnosticCodeCount() != 17"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../semantic/diagnostic_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "SemanticVerdictPayloadContractReady()"
require_text "tests/self_hosted/parity/semantic_parity.sh" "check_semantic_diagnostic_code_surface"
require_text "tests/self_hosted/parity/semantic_parity.sh" "semantic_oracle_code_for"
require_text "tests/self_hosted/parity/semantic_parity.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/semantic_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/semantic_parity.sh" '"semantic-parity-paths"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'EXPECTED_DIR="$ROOT_DIR/${harness_paths[3]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'DIAGNOSTIC_CODE_OWNER="$ROOT_DIR/${harness_paths[4]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'DIAGNOSTIC_RENDERER_OWNER="$ROOT_DIR/${harness_paths[5]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" 'SEMANTIC_SOURCE_DIR="$ROOT_DIR/${harness_paths[6]}"'
require_text "tests/self_hosted/parity/semantic_parity.sh" "compare_semantic_verdict_with_owner"
require_text "tests/self_hosted/parity/semantic_parity.sh" "read_semantic_fixture_manifest"
require_text "tests/self_hosted/parity/semantic_parity.sh" '"$manifest_bin" --fixture-manifest'
require_text "tests/self_hosted/parity/semantic_parity.sh" "diagnostics"
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/src/self_hosted/semantic/fixture"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" '"src/self_hosted/semantic/fixture/${base}.pgy"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" '"valid_int_return:ok"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" '"bad_logical_right:error"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" "diff <("
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'cp "$SEMANTIC_SOURCE_DIR/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"'
reject_text "tests/self_hosted/parity/semantic_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"'
require_text "tests/self_hosted/parity/regen_expected.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/regen_expected.sh" '"semantic-parity-paths"'
require_text "tests/self_hosted/parity/regen_expected.sh" 'TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/regen_expected.sh" 'TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/regen_expected.sh" 'FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/regen_expected.sh" 'FIXTURE_DIR_REL="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/regen_expected.sh" 'EXPECTED_DIR="$ROOT_DIR/${harness_paths[3]}"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'TOOL_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'FIXTURE_DIR="$ROOT_DIR/src/self_hosted/semantic/fixture"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'TOOL="$BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'cp "$TOOL_SOURCE" "$TOOL"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"'
reject_text "tests/self_hosted/parity/regen_expected.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"'
require_owner_surface codegen \
    "input/ast_input_owner.pgy" \
    "input/ast_text_inventory_owner.pgy" \
    "input/ast_text_row_fact_owner.pgy" \
    "input/ast_text_statement_owner.pgy" \
    "input/ast_usage_owner.pgy" \
    "run/codegen_run_owner.pgy" \
    "text/text_owner.pgy" \
    "type_facts/type_env.pgy" \
    "text/expr_scan.pgy" \
    "abi_layout/abi_layout_owner.pgy" \
    "runtime_abi/collection_runtime_owner.pgy" \
    "runtime_abi/host_io_runtime_owner.pgy" \
    "runtime_abi/math_runtime_owner.pgy" \
    "runtime_abi/option_result_runtime_owner.pgy" \
    "runtime_abi/string_runtime_owner.pgy" \
    "emission/struct_value_emit.pgy" \
    "emission/stmt_emit.pgy" \
    "emission/function_emit.pgy" \
    "emission/program_emit.pgy"
require_text "src/self_hosted/codegen/main.pgy" 'import "../compiler/symbol_table_owner.pgy";'
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" 'import "../../compiler/target_capability_owner.pgy";'
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" "CompilerTargetCapabilityEnvelopeReady()"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetCpuCProjection"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetCpuLlvmProjection"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetSelfHostedProjection"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetIntentGraphFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetEffectSetFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetAuthorityEvidenceFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetCoordinationFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetSlotOwnershipFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetLayoutShapeFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetLossBudgetFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetMaterializationReasonFact"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetUnsupportedShapeFallbackReason"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetForbiddenLossBudgetFallbackReason"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetRetainedEffectFallbackReason"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetMissingAuthorityEvidenceFallbackReason"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "func CompilerTargetHostOnlySlotBoundaryFallbackReason"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "CompilerTargetProjectionAt(0) == CompilerTargetCpuCProjection()"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "CompilerTargetFactAt(0) == CompilerTargetIntentGraphFact()"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" "CompilerTargetFallbackReasonAt(0) == CompilerTargetUnsupportedShapeFallbackReason()"
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetProjectionAt(0) == "cpu-c"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetProjectionAt(1) == "cpu-llvm"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetProjectionAt(2) == "self-hosted"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(0) == "intent_graph"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(1) == "effect_set"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(2) == "authority_evidence"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(3) == "coordination"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(4) == "slot_ownership"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(5) == "layout_shape"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(6) == "loss_budget"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFactAt(7) == "materialization_reason"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFallbackReasonAt(0) == "unsupported_shape"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFallbackReasonAt(1) == "forbidden_loss_budget"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFallbackReasonAt(2) == "retained_effect"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFallbackReasonAt(3) == "missing_authority_evidence"'
reject_text "src/self_hosted/compiler/target_capability_owner.pgy" 'CompilerTargetFallbackReasonAt(4) == "host_only_slot_boundary"'
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolCQualifiedName"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolRequireTable"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "CompilerSymbolRequireTable();"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolSourceOwnerRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolSourceNameRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolNamespacePathRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolCSymbolRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolLlvmSymbolRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolSelfHostedSymbolRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolCollisionPolicyRow"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "CompilerSymbolTableRowAt(0) == CompilerSymbolSourceOwnerRow()"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "CompilerSymbolProjectionAt(0) == CompilerSymbolCSymbolRow()"
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(0) == "source_owner"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(1) == "source_name"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(2) == "namespace_path"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(3) == "c_symbol"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(4) == "llvm_symbol"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(5) == "self_hosted_symbol"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolTableRowAt(6) == "collision_policy"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolProjectionAt(0) == "c_symbol"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolProjectionAt(1) == "llvm_symbol"'
reject_text "src/self_hosted/compiler/symbol_table_owner.pgy" 'CompilerSymbolProjectionAt(2) == "self_hosted_symbol"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CompilerSymbolCQualifiedName"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "SymbolMangle"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'Concat(owner, Concat("_",'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'Concat(owner_name, Concat("_",'
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCParamType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCReturnType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCLocalType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCFieldType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" 'import "../../compiler/abi_layout_row_owner.pgy";'
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "CompilerAbiLayoutRowsReady()"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "CompilerAbiLayoutRowIndex(type_name)"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "CompilerAbiLayoutRowCValueTypeAt(UnwrapOption(row))"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "CompilerAbiLayoutFieldAllowed(type_name)"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" 'LookupKindType(struct_env, type_name, "enum") == "payload_free"'
reject_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "CollectionRuntimeKindFromTypeName"
reject_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" 'import "../runtime_abi/collection_runtime_owner.pgy";'
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutConcreteRowCount"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutRowIndex"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutRowCValueTypeAt"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutFieldAllowed"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutRowMaterializationAt"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutDeclNameFact"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutMaterializationPolicyFact"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutOptionStringTypeName"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutOptionStringCValueType"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutOptionStringExplicitTypeName"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutFieldAllowedMaterialization"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "func CompilerAbiLayoutRuntimeValueOnlyMaterialization"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "CompilerAbiLayoutRowFactAt(0) == CompilerAbiLayoutDeclNameFact()"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "CompilerAbiLayoutRowTypeNameAt(9) == CompilerAbiLayoutOptionStringTypeName()"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "CompilerAbiLayoutRowCValueTypeAt(9) == CompilerAbiLayoutOptionStringCValueType()"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "CompilerAbiLayoutRowMaterializationAt(9) == CompilerAbiLayoutRuntimeValueOnlyMaterialization()"
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowFactAt(0) == "decl_name"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowFactAt(7) == "materialization_policy"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowTypeNameAt(0) == "Int"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowTypeNameAt(9) == "Option<String>"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowCValueTypeAt(0) == "long long"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowCValueTypeAt(9) == "pgy_option_string"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowMaterializationAt(0) == "field_allowed"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'CompilerAbiLayoutRowMaterializationAt(9) == "runtime_value_only"'
reject_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'StringIndexOf(type_name, "Int")'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIndentOf"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "struct CodegenAstTextNode"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let kind: Int"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let payload: String"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let name: String"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let type_name: String"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let mode: Int"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextKindOf"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextPayloadFor"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'import "ast_text_row_fact_owner.pgy";'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'node.name == "Main"'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return node.name"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return node.type_name"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'if kind == 5'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'if kind == 7'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'if kind == 8'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'if kind == 9'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let field_indent: Int = -1"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "field_indent = indent + 2"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "kind = 5"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextNominalName"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'node.text == "Function: Main"'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'StringIndexOf(node.text, " for ")'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'StringIndexOf(node.text, ": ")'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'StringIndexOf(node.text, " { ")'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return Substring(node.text, 6, fp - 6)"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return Substring(node.text, fp + 5"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return Substring(node.text, 6, brace - 6)"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "Substring(node.text, brace + 3"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'StartsWith(node.text, "inout ")'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'StartsWith(node.text, "own ")'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'StartsWith(node.text, "ref ")'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return node.text"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextEnumBracePos"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextPayloadAfter"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextNominalPrefixLen"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let prefix_len: Int = CodegenAstTextNominalPrefixLen"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let parent: Int"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextParentIndex"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return -1"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsParameters"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsProgramRoot"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsReturns"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsFieldsHeader"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsFunction"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsMainFunction"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsNominalDecl"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsRoleDecl"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsEventDecl"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsZeroArtifactDecl"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let aux_payload: String"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextAuxPayloadFor"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextFunctionName"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextReturnType"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextEnumName"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextProvenance"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextEnumVariantCount"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextEnumVariantNameAt"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return node.aux_payload"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextNominalName"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextRoleName"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextRoleForType"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "if kind == 12"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsParameterRow"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextParamMode"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextParamName"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextParamType"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" 'import "../text/text_owner.pgy";'
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" "struct CodegenAstTextRowFactInput"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" "func CodegenAstTextParamModeForPayload"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" "func CodegenAstTextParamPayloadForMode"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" "func CodegenAstTextNameFactFor(input: CodegenAstTextRowFactInput)"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" "func CodegenAstTextTypeNameFactFor(input: CodegenAstTextRowFactInput)"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" "if kind == 20"
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" 'FindTextFrom(payload, " : ", 0)'
require_text "src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy" 'FindTextFrom(payload, " = ", type_start)'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "return node.mode"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let row_input: CodegenAstTextRowFactInput"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextNameFactFor(row_input)"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextTypeNameFactFor(row_input)"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "StartsWith(node.payload, \"inout \")"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "StartsWith(node.payload, \"own \")"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "StartsWith(node.payload, \"ref \")"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextNameFactFor(kind, payload)"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextTypeNameFactFor(kind, payload)"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextRoleForPos"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextParamPayload(node"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextParamPayloadForMode"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextFieldName"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextFieldType"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextNodeInventory"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextNodeInventory(tree_text: String"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let param_indent: Int = -1"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "param_indent = indent + 2"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "kind = 12"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'import "../typed_ast_node_skeleton.pgy";'
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenTypedAstBridgeReady"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "TypedAstArenaPayloadContractReady()"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextIsProgramRoot(nodes[0])"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextExpectNode"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "let expected_kind: Int = CodegenAstTextKindOf(expected)"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "nodes[cur[0]].kind != expected_kind"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" 'nodes[0].text != "Program:"'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "nodes[cur[0]].text != expected"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "func TypedAstArenaPayloadContractReady"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "func TypedAstArenaPayloadSchema"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "pgy.selfhost.typed-ast-arena.v1"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "struct AstArena"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "struct AstNode"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "func ChildAt"
require_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "func AtomText"
reject_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "ArrayGet("
reject_text "src/self_hosted/codegen/typed_ast_node_skeleton.pgy" "SKELETON"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../codegen/typed_ast_node_skeleton.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "func CompilerEmissionFactReady"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "codegen|EmissionZone|ProgramEmitter|EmitProgramArtifact"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "TypedAstArenaPayloadContractReady()"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'import "ast_text_inventory_owner.pgy";'
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsLetStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsAssignStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsLogStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsBareReturnStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsValueReturnStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsDeferStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsArrayPopStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsArraySetStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsArrayPushStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsExitStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsBreakStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsContinueStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsForStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsWhileStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsIfStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsElseStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsBareCallStmt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 13"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 14"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 15"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 16"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 17"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 18"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 19"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 20"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 21"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 22"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 23"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 24"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 25"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 26"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 27"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 28"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.kind == 29"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLetName"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLetTypeName"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLetInitializer"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.name"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.type_name"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextAssignTarget"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextAssignValue"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLogInner"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextReturnValue"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextArrayPopTarget"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextArraySetTarget"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextArraySetIndex"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextArraySetValue"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextArrayPushTarget"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextArrayPushValue"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextExitValue"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextForLoopVar"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextForIsRange"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextForRangeStart"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextForRangeEnd"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextForEachCollection"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextWhileCondition"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIfCondition"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextHasElseAt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextIsElseIfAt"
require_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextBareCallExpr"
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "Log(")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "Let: ")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "Assign: ")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "Return:")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "ArrayPop(")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "ArraySet(")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "ArrayPush(")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "Exit(")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "For: ")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "While: ")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'StartsWith(node.text, "If: ")'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'node.text == "Return"'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'node.text == "Defer:"'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'node.text == "Break"'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'node.text == "Continue"'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'node.text == "Else:"'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "IsSingleCall(node.text)"
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "return node.text"
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'Substring(node.text, 4'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'Substring(node.text, 9'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'Substring(node.text, 10'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'Substring(node.text, 5'
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" 'CodegenAstTextPayloadAfter('
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLetNameEnd"
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLetAfterName"
reject_text "src/self_hosted/codegen/input/ast_text_statement_owner.pgy" "func CodegenAstTextLetInitializerPos"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "if IsSingleCall(text)"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIsLetStmt"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextLetName"
require_text "src/self_hosted/codegen/main.pgy" 'import "input/ast_text_statement_owner.pgy";'
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextInventory(ast: String, inout indents:"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextNodeInventory(ast: String"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "CodegenAstTextPayloadAfter(node, \"Function: \""
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextProjectLegacy"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextExpect(texts:"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "struct CodegenRuntimeUsageFacts"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "func CodegenRuntimeUsageFactsFromNodes"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "func CodegenAstTextContains"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "func CodegenAstTextKindPresent"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "StringIndexOf(nodes[i].payload"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "StringIndexOf(nodes[i].aux_payload"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "CodegenAstTextKindPresent(nodes, count, 13)"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "CodegenAstTextKindPresent(nodes, count, 17)"
require_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "CodegenAstTextContains(nodes, count, \"Args(\")"
reject_text "src/self_hosted/codegen/input/ast_usage_owner.pgy" "StringIndexOf(nodes[i].text"
require_text "src/parser/ast_print.c" "PARAM_MODE_MUT_REF"
require_text "src/parser/ast_print.c" 'printf("inout ")'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'param_mode_prefix = "inout "'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'param_mode_prefix = "ref "'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'param_mode_prefix = "own "'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "GenerateC(tree_text: String)"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" 'import "../runtime_abi/host_io_runtime_owner.pgy";'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "RejectUnsupportedCodegenBuiltins(tree_text)"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextNodeInventory(tree_text, nodes, node_count_box)"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenTypedAstBridgeReady(nodes, count)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "GenerateC(ast: String)"
reject_text "src/self_hosted/codegen/text/text_owner.pgy" "RejectUnsupportedCodegenBuiltins(ast: String)"
reject_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" "let ast: String"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let usage: CodegenRuntimeUsageFacts = CodegenRuntimeUsageFactsFromNodes(nodes, count)"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let uses_args: Bool = usage.uses_args"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextIsEventDecl(nodes[main_scan])"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextIsMainFunction(nodes[scan])"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextIsNominalDecl(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextIsRoleDecl(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextIsZeroArtifactDecl(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextNominalName(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextRoleName(nodes[cur[0]])"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" 'StartsWith(main_line, "Event:")'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let f_indent: Int = 2"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "if first_fn >= 0"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "f_indent = nodes[first_fn].indent"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let record_array_block: String = \"\""
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "pgy_CodegenAstTextNode_array_get"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "NominalDeclName(cur_line)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "RoleDeclName(cur_line)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextInventory(ast, indents, texts)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "StringIndexOf(ast,"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "texts["
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "indents["
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func BuildFunctionEnv(nodes: Array<CodegenAstTextNode>, count: Int)"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectRoleOperators(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectStructs(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectEnums(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectProtos(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func EmitFunction(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextExpectNode(nodes, count, cur, \"Parameters:\")"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextFunctionName(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextParamName(nodes[cur[0]], owner_name)"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextParamType(nodes[cur[0]], owner_name)"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextReturnType(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsReturns(nodes[cur[0]])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsParameters(nodes[j])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsFieldsHeader(nodes[j])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextEnumName(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextEnumVariantCount(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextEnumVariantNameAt(nodes[i], value)"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"=enum:payload_free|"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" 'Concat(ename, Concat(".", Concat(part, Concat("=e:"'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'Concat(env_box[0], Concat(part, Concat("=e:"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextNominalName(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextRoleName(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextRoleForType(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextFieldName(nodes[j])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextFieldType(nodes[j])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"=pm:"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"_pgy_inout_"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsZeroArtifactDecl(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsNominalDecl(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsRoleDecl(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsEnumDecl(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextIsFunction(nodes[i])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "!CodegenAstTextIsFunction(nodes[j])"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "EmitStmtList(nodes, count, stmt_indent, cur, \"    \", env_box, body_ret, copyout)"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func IsZeroArtifactDeclLine"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func IsNominalDeclLine"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func IsRoleDeclLine"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func NominalDeclPrefixLen"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func RoleDeclName"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func RoleDeclForType"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func NominalDeclName"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func ParamLineMode"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func ParamLinePayload"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "let ret_line: String = nodes[j].text"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'nodes[j].text == "Fields:"'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'nodes[j].text == "Parameters:"'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'StartsWith(nodes[j].text, "Returns:")'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "let line: String = nodes[i].text"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'StringIndexOf(line, " { ")'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'if StartsWith(line, "Function:")'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func BuildFunctionEnv(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectRoleOperators(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectStructs(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectEnums(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectProtos(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func EmitFunction(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "texts["
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "indents["
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" ".text"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitStmtList(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextExpectNode(nodes, count, cur"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitLet(node: CodegenAstTextNode"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitTryLet(node: CodegenAstTextNode"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitAssign(node: CodegenAstTextNode"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsLetStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsAssignStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextLetName(node)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextLetTypeName(node)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextLetInitializer(node)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextAssignTarget(node)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextAssignValue(node)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsLogStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextLogInner(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsBareReturnStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsValueReturnStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextReturnValue(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsDeferStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsArrayPopStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextArrayPopTarget(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsArraySetStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextArraySetTarget(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextArraySetIndex(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextArraySetValue(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsArrayPushStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextArrayPushTarget(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextArrayPushValue(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsExitStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextExitValue(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsBreakStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsContinueStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsForStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextForLoopVar(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextForIsRange(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextForRangeStart(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextForRangeEnd(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextForEachCollection(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsWhileStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextWhileCondition(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsIfStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIfCondition(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextHasElseAt(nodes, count, cur[0], stmt_indent)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsElseIfAt(nodes, count, cur[0])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextIsBareCallStmt(nodes[idx])"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextBareCallExpr(nodes[idx])"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextExpect(texts"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitStmtList(indents:"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "texts["
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "indents["
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" ".text"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitLet(line: String"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitTryLet(line: String"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitAssign(line: String"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "Let: ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "Assign: ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "Log(")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "Return:")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "ArrayPop(")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "ArraySet(")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "ArrayPush(")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "Exit(")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 't == "Return"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 't == "Defer:"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 't == "Break"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 't == "Continue"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "For: ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "While: ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(t, "If: ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'nodes[cur[0]].text == "Else:"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StartsWith(nodes[cur[0]].text, "If: ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StringIndexOf(spec, " in ")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'StringIndexOf(range, "..")'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'Substring(t, 5'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'Substring(t, 7'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'Substring(t, 4'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'IsSingleCall(t)'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'RewriteExpr(t, env_box[0])'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'Substring(line, 5'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'Substring(line, 8'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'let a_inner: String = Substring(t, 9'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'let p_inner: String = Substring(t, 10'
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "NextNewline(ast, pos)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "StringTrim(raw_line)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "IndentOf(raw_line)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" ".text"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func IndentOf"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func ExpectText"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitCollectionElementValue"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "EmitStructValue(value_expr, elem_type, env)"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "func RewriteInoutCallArgs"
require_text "src/self_hosted/codegen/type_facts/type_env.pgy" "func ResolveCallSymbol"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func ParamTypeCsvAppend"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func FunctionCallProjectionRecord"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"=cf:"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"=pt:"'
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "ResolveCallSymbol(env, ident)"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'LookupKindType(env, call_symbol, "pm")'
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'LookupKindType(env, call_symbol, "pt")'
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "func RewriteCallArgForExpectedType"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "func RewriteStructLiteralCallArg"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'LookupKindType(env, enum_key, "e")'
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "CompilerSymbolCEnumVariantName(expected_type, p)"
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'LookupKindType(env, ident, "pm")'
require_text "src/self_hosted/codegen/type_facts/type_env.pgy" "func ParamModeCsvCount"
require_text "src/self_hosted/codegen/type_facts/type_env.pgy" "func ParamModeCsvAt"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "ParamModeCsvCount(modes)"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "ParamModeCsvAt(modes, arg_index)"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'rendered = Concat("&", rendered)'
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "CompilerSymbolCQualifiedName(owner, member)"
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'out = Concat(out, "_")'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "func ModeCsvCount"
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "mode = CsvAt(modes"
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "pgy_CodegenAstTextNode_array"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "func CollectionRuntimeCGetFn"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "func CollectionRuntimeCLenFn"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "func CollectionRuntimeCPushFn"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "func CollectionRuntimeKindCode"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "func CollectionRuntimeElementTypeFromKindCode"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "func CollectionRuntimeCIntMapFn"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "Array<Int: Int>"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "ArrayCodegenAstTextNode"
require_text "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy" "Array<String: String>"
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCAbsFn"
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCSqrtFn"
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCPowFn"
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCFloorFn"
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCCeilFn"
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCRandomFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCFileExistsFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCSecureFileOpenFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCArgsFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCExitFn"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "HostIORuntimeCSecureFileOpenFn()"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "O_NOFOLLOW"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "pgy_secure_fopen(path, mode)"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" 'pgy_secure_fopen(path, \"wb\")'
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "FILE *f = fopen(path, mode);"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" 'FILE *f = fopen(path, \"wb\");'
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCConcatFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCStringLengthFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCStringJoinFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCLogFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCToFloatFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCOptionSomeFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCOptionSomeFnForPayloadKind"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCOptionNoneFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCOptionNoneFnForPayloadKind"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeOptionPayloadKindForType"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeOptionEnvKindForPayloadKind"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeOptionValueTypeForPayloadKind"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCResultOkFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCResultUnwrapFn"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "AbiLayoutCParamType"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "AbiLayoutCReturnType"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "AbiLayoutCFieldType"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "AbiLayoutCLocalType"
require_text "src/self_hosted/codegen/text/text_owner.pgy" "func FindMatchingBracket(s: String, open_idx: Int) -> Option<Int>"
require_text "src/self_hosted/codegen/text/text_owner.pgy" "func FindMatchingParen(s: String, open_idx: Int) -> Option<Int>"
require_text "src/self_hosted/codegen/text/text_owner.pgy" "func FindTopLevelPlus(s: String) -> Option<Int>"
require_text "src/self_hosted/codegen/text/text_owner.pgy" "func FindTopLevelComma(s: String) -> Option<Int>"
require_text "src/self_hosted/codegen/text/text_owner.pgy" "func CodegenCharAt"
reject_text "src/self_hosted/codegen/text/text_owner.pgy" "func CharAt"
reject_text "src/self_hosted/codegen/text/text_owner.pgy" "return -1"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "CollectionRuntimeCLenFn"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "CollectionRuntimeCGetFn"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "func FindTopLevelOp2(s: String, op: String) -> Option<Int>"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "return None"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "return Some(i)"
reject_text "src/self_hosted/codegen/text/expr_scan.pgy" "return -1"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CollectionRuntimeCPushFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CollectionRuntimeCSetFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "CollectionRuntimeCIntSortFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "StringRuntimeCStringLengthFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "StringRuntimeCConcatFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "StringRuntimeCLogFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "MathRuntimeCAbsFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "MathRuntimeCSqrtFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "MathRuntimeCPowFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "MathRuntimeCFloorFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "MathRuntimeCCeilFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "HostIORuntimeCFileExistsFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "StringRuntimeCToFloatFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "HostIORuntimeCExitFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "OptionResultRuntimeCOptionSomeFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "OptionResultRuntimeOptionPayloadKindFromExprKind(ExprKind(some_inner, env))"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "OptionResultRuntimeCOptionSomeFnForPayloadKind(some_payload_kind)"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "OptionResultRuntimeCResultOkFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "OptionResultRuntimeCResultIsOkFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "OptionResultRuntimeOptionPayloadKindForType(type_name)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "OptionResultRuntimeOptionEnvKindForPayloadKind(option_payload_kind)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "OptionResultRuntimeCOptionNoneFnForPayloadKind(return_option_payload_kind)"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "OptionResultRuntimeOptionValueTypeForPayloadKind(operand_payload_kind)"
require_text "src/self_hosted/codegen/type_facts/type_env.pgy" 'return "OptionString";'
require_file "src/self_hosted/codegen/fixture/option_string_core.pgy"
require_file "src/self_hosted/codegen/expected/option_string_core_stdout.txt"
require_file "src/self_hosted/codegen/fixture/option_try.pgy"
require_file "src/self_hosted/codegen/expected/option_try_stdout.txt"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CParamType"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CRetType"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("long long "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("const char* "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("pgy_result_int "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("pgy_option_int "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'let arr_c: String = "pgy_ai"'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"sqrt("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pow("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"floor("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"ceil("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"atof("'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"exit("'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'let elem_c: String = "long long"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'for (long long '
reject_text "src/self_hosted/codegen/text/expr_scan.pgy" '"pgy_ai_len"'
reject_text "src/self_hosted/codegen/text/expr_scan.pgy" '"pgy_as_len"'
reject_text "src/self_hosted/codegen/text/expr_scan.pgy" '"pgy_ai_get("'
reject_text "src/self_hosted/codegen/text/expr_scan.pgy" '"pgy_as_get("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_ai_sort("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_ai_reverse("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_ai_map("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_ai_filter("'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_ai_new"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_as_new"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_ai_push"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_as_push"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_ai_set"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_as_set"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_ai_pop"'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_as_pop"'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_concat"'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_concat("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_strlen("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_strcontains("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_strindexof("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_split("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_toint("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_strtrim("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_strreplace("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_toupper("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_tolower("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_print("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_charcode("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_charatn("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_subcontains_with_len("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_subequals_with_len("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_subindexof_with_len("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_substartswith_with_len("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_substr("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_strjoin("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_tostr("'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_log("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_option_none()"'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_option_some("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_option_is_some("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_option_unwrap("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_result_ok("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_result_err("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_result_is_ok("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_result_is_err("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_result_unwrap_or("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_result_unwrap("'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_result_is_ok("'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" '"pgy_result_unwrap("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_abs("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_min("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_max("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_seedrandom("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_random("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_fexists("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_writefile("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_fopen("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_fwrite("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_fclose("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_fread("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_readfile("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_dirwalk("'
reject_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" '"pgy_args("'
require_text "src/self_hosted/codegen/emission/struct_value_emit.pgy" "func EmitStructValue"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitStructValue"
require_owner_surface mir_lower \
    "run_owner.pgy"
for mir_owner in \
    "error_owner.pgy" \
    "mir_fact_graph_contract_owner.pgy" \
    "mir_json_input_owner.pgy" \
    "fixture_manifest_owner.pgy" \
    "json_fact_read.pgy" \
    "stmt_render.pgy" \
    "routine_inventory_owner.pgy" \
    "routine_lower.pgy" \
    "decl_lower.pgy" \
    "program_lower.pgy"; do
    require_text "src/self_hosted/mir_lower/run_owner.pgy" "import \"$mir_owner\";"
done
require_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'import "json_fact_read.pgy";'
require_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'MirDocumentSchemaEquals(json, "pgy.mir.v1")'
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "func MirFactGraphPayloadContractReady"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "func MirFactGraphPayloadSchema"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "pgy.mir.v1"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirFactGraphPayloadFixtureCount() != 86"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirParityFixtureCount() != MirFactGraphPayloadFixtureCount()"
require_text "src/self_hosted/mir_lower/main.pgy" "RunMirLowerFromArgs(Args())"
require_text "src/self_hosted/mir_lower/run_owner.pgy" "func RunMirLowerFromArgs"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirDocumentSchemaEquals"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonDocumentFactStringFieldEquals(json, \"schema\", expected)"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirDocumentSchemaEquals(json, MirFactGraphPayloadSchema())"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../mir_lower/mir_fact_graph_contract_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "MirFactGraphPayloadContractReady()"
reject_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'import "../lib/json.pgy";'
reject_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'StringIndexOf(json, "\"schema\":\"pgy.mir.v1\"")'
reject_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'JsonDocumentStringFieldEquals(json, "schema", "pgy.mir.v1")'
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func FindRoutine(json: String, from: Int) -> Option<Int>"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineObjectEnd(json: String, rpos: Int) -> Option<Int>"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineNameEnd(json: String, rpos: Int) -> Option<Int>"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineName"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineParamCount"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineParamNameAt"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineParamTypeAt"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineReturnType"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineBlocksBounds"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func RoutineBlocksStart(json: String, routine_name_end: Int, span_end: Int) -> Option<Int>"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "MirRoutineObjectBoundsAt(json, row, bounds)"
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "return -1"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "let object_end_opt: Option<Int> = MirObjectEnd(json, routine_name_end"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "MirObjectArrayBounds(json, routine_name_end, span_end, \"blocks\", bounds)"
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "let next_rpos: Int = FindRoutine(json, routine_name_end)"
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" 'FindFrom(json, "\"name\":'
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" 'FindFrom(json, "\"blocks\":'
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "func FindRoutine"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "RoutineParamCount(json, rpos, header_end)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "RoutineReturnType(json, rpos, header_end)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "func BlockInstructionBoundsAt"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "func BlockInstructionKind"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "func BlockInstructionOfKindBounds"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectArrayObjectBoundsAt(json, bs, be, \"instructions\", row, bounds)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectStringFact(json, inst_start, inst_end, \"kind\")"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectNumberFact(json, bs, be, field)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectArrayObjectBoundsAt(json, routine_start, routine_end, \"blocks\", ToInt(id), o)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectNumberFact(json, o[0], o[1], \"id\") == id"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "ReadSucc(json, bs, be, \"succ_true\")"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'let key: String = Concat("\"id\":'
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'FindFrom(json, ",\"reachable\""'
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "BlockBounds(json, bp"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "FindFrom(json,"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "FindFrom(json, kw"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'ReadSucc(json, bs, be, "\"succ_true\":")'
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'ReadSucc(json, bs, be, "\"succ_false\":")'
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'FindFrom(json, "\"params\":['
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'FindFrom(json, "\"return\":'
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "ReadJsonString(json, nq"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "ReadJsonString(json, pnq"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "ReadJsonString(json, rnq"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "ReadJsonString(json, kq"
require_text "src/self_hosted/mir_lower/program_lower.pgy" "let rpos_opt: Option<Int> = FindRoutine(json, 0)"
require_text "src/self_hosted/mir_lower/program_lower.pgy" "let nend_opt: Option<Int> = RoutineNameEnd(json, rpos)"
require_text "src/self_hosted/mir_lower/program_lower.pgy" "let nend: Int = UnwrapOption(nend_opt)"
require_text "src/self_hosted/mir_lower/program_lower.pgy" "FindRoutine(json, span_end)"
reject_text "src/self_hosted/mir_lower/program_lower.pgy" "FindRoutine(json, nend)"
reject_text "src/self_hosted/mir_lower/program_lower.pgy" "ReadJsonString(json,"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "let routine_name_end_opt: Option<Int> = RoutineNameEnd(json, rpos)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "let bp: Option<Int> = RoutineBlocksStart(json, routine_name_end, span_end)"
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" 'Substring(json, e[0]'
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" ',"kind":"function"'
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" ',"kind":"method"'

require_text "tests/self_hosted/parity/selfcheck_sources.sh" 'source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"semantic-parity-paths"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"self-host-completeness-semantic-targets"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" 'TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" 'SELF_TARGET_ROWS=()'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" 'SEMANTIC_TARGET_MANIFEST'
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" 'SELF_SOURCES=('
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" 'TOOL_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"'
require_text "tests/self_hosted/parity/completeness_ledger.sh" '"codegen-parity-paths"'
require_text "tests/self_hosted/parity/completeness_ledger.sh" '"lexer-parity-paths"'
require_text "tests/self_hosted/parity/completeness_ledger.sh" '"parser-parity-paths"'
require_text "tests/self_hosted/parity/completeness_ledger.sh" '"semantic-parity-paths"'
require_text "tests/self_hosted/parity/completeness_ledger.sh" "LEXER_PATH_MANIFEST"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "PARSER_PATH_MANIFEST"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "SEMANTIC_PATH_MANIFEST"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "CODEGEN_PATH_MANIFEST"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "lexer_tool_source_path"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "parser_tool_source_path"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "semantic_tool_source_path"
require_text "tests/self_hosted/parity/completeness_ledger.sh" "codegen_tool_source_path"
reject_text "tests/self_hosted/parity/completeness_ledger.sh" 'copy_lib'
reject_text "tests/self_hosted/parity/completeness_ledger.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/completeness_ledger.sh" 'cp "$ROOT_DIR/$copy_dir/"*.pgy'
reject_text "tests/self_hosted/parity/completeness_ledger.sh" '$BUILD_DIR/lexer/main.pgy'
reject_text "tests/self_hosted/parity/completeness_ledger.sh" '$BUILD_DIR/parser/main.pgy'
reject_text "tests/self_hosted/parity/completeness_ledger.sh" '$BUILD_DIR/semantic/main.pgy'
reject_text "tests/self_hosted/parity/completeness_ledger.sh" 'CODEGEN_BIN="$(compile_tool codegen "$ROOT_DIR/src/self_hosted/codegen/main.pgy" codegen "")"'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" "JsonStringLiteral(path)"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" "DiagnosticCatalogJsonForOwners"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" "InputErrorJsonForOwners"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/run_owner.pgy" 'import "scan_owner.pgy";'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/run_owner.pgy" 'import "report_owner.pgy";'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/run_owner.pgy" "func RunDiagnosticCatalogCheckFromArgs"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/run_owner.pgy" "DiagnosticCatalogHeaderPathFromArgs(args)"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/run_owner.pgy" "DiagnosticCatalogDocsPathFromArgs(args)"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "RunDiagnosticCatalogCheckFromArgs(Args())"
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "pgy_selfhost_compare_expected_text_artifact_with_owner"
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" '"diagnostic-catalog-paths"'
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" '"$HEADER_REL" "$DOCS_REL"'
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" '"run_output"'
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'C_ORACLE="$ROOT_DIR/tests/diagnostic_registry_smoke.sh"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'PERGYRA_TOOL_SOURCE_DIR="$ROOT_DIR/src/self_hosted/tools/diagnostic_catalog_checker"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'PERGYRA_TOOL_SOURCE_DIR='
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE_DIR"/*.pgy "$PERGYRA_TOOL_BUILD_DIR"/'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../lib"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'EXPECTED_MISSING_JSON="$(cat "$EXPECTED_MISSING_JSON_FILE")"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" 'EXPECTED_INPUT_ERROR_JSON="$(cat "$EXPECTED_INPUT_ERROR_JSON_FILE")"'
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "SHELL_CODES="
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "SHELL_DOCUMENTED="
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "SHELL_DUP_TOTAL="
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "SHELL_DUP_UNIQUE="
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "SHELL_DUPLICATES"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "SHELL_ORPHANS"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.codes parity FAIL"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.documented parity FAIL"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.duplicates parity FAIL"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.orphans parity FAIL"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "shell grep ground truth"
reject_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "shell duplicates ground truth"
require_text "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy" "JsonEmitArray("
reject_text "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" 'import "../../lib/json_fact_table.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "func AirGraphSummaryIntField"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "func AirGraphScalarFieldValues"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "func AirGraphCollectScalarFieldValues"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "func RequiredGraphFeatureKeys"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "func CountMissingGraphFeatureKeys"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "func BuildMissingGraphFeatureFindingFacts"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonDocumentFactStringFieldEquals(content, \"schema\", \"pgy.air.graph.v1\")"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonDocumentObjectFactTable(content)"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectFactObjectTable(root, \"summary\")"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectFactNumberFieldOpt(summary, field)"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectFactHasField(root, key)"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "AirGraphScalarFieldValues(content, key)"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "let value_end_opt: Option<Int> = JsonValueEnd(content,"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "let item_end_opt: Option<Int> = JsonValueEnd(content,"
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" '"execution_lane"'
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" '"boundary_capture"'
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" 'import "../../compiler/air_evidence_owner.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" 'import "scan_owner.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" 'import "report_owner.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "CompilerAirEvidenceEnvelopeReady()"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "func AirGraphFixturePathFromArgs"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "func AirGraphCapabilityFixturePathFromArgs"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "func RunAirGraphJsonValidationFromArgs"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "RunAirGraphJsonValidationWithPaths("
require_text "src/self_hosted/tools/air_graph_json_validator/main.pgy" "RunAirGraphJsonValidationFromArgs(Args())"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceIntentGraphFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceEffectSetFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceAuthorityEvidenceFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceCoordinationFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceSlotOwnershipFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceMaterializationReasonFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "func CompilerAirEvidenceLossBudgetFact"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "CompilerAirEvidenceFactAt(0) == CompilerAirEvidenceIntentGraphFact()"
require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "CompilerAirEvidenceFactAt(6) == CompilerAirEvidenceLossBudgetFact()"
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(0) == "intent_graph"'
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(1) == "effect_set"'
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(2) == "authority_evidence"'
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(3) == "coordination"'
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(4) == "slot_ownership"'
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(5) == "materialization_reason"'
reject_text "src/self_hosted/compiler/air_evidence_owner.pgy" 'CompilerAirEvidenceFactAt(6) == "loss_budget"'
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "RequiredGraphFeatureKeys()"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "CountMissingGraphFeatureKeys(content, feature_keys)"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "AIR_EVIDENCE_OWNER"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'AIR_EVIDENCE_OWNER="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" '"air-graph-json-validator-paths"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'FIXTURE_REL="${harness_paths[3]}"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'CAP_FIXTURE_REL="${harness_paths[4]}"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" '"$CLEAN_BIN" "$FIXTURE_REL" "$CAP_FIXTURE_REL"'
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "compare_clean_json_with_owner"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "compare_air_json_file_with_owner"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "pgy_selfhost_backend_output_comparator_bin"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "pgy_selfhost_path_relative_to_root"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "air_json"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "SHELL_INTENTS="
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "SHELL_BOUNDARIES="
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "SHELL_EVIDENCE="
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "SHELL_DRIFTS="
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "SHELL_EFFECT_SITES="
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "SHELL_ENV_EFFECT_SITES="
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "counts.intents parity FAIL"
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "counts.env_effect_sites parity FAIL"
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "shell grep ground truth"
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'PERGYRA_TOOL_SOURCE_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'PERGYRA_TOOL_SOURCE_DIR='
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE_DIR"/*.pgy "$PERGYRA_TOOL_BUILD_DIR"/'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../lib"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../compiler"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'cp "$AIR_EVIDENCE_OWNER" "$PERGYRA_TOOL_BUILD_DIR/../../compiler/air_evidence_owner.pgy"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/expected/clean.json"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" 'CAP_FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/cap_env.json"'
reject_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "diff -q"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" 'StringContains(content, "\"schema\":\"pgy.air.graph.v1\"")'
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonDocumentStringFieldEquals(content, \"schema\", \"pgy.air.graph.v1\")"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "let doc_end: Int = JsonDocumentObjectEnd(content)"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectFieldValueBounds(content, 0, UnwrapOption(doc_end_opt), \"summary\", summary_bounds)"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectFactValueBounds(root, \"summary\", summary_bounds)"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "StringContains(content, key)"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonFieldKey(\"execution_lane\")"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonFieldKey(\"boundary_capture\")"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "let value_end: Int = JsonValueEnd(content,"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectNumberField(content, summary_bounds[0], summary_bounds[1], field)"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonObjectNumberFieldOpt(content, summary_bounds[0], summary_bounds[1], field)"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "StringLength(num_str) == 0"
for air_graph_report in \
    src/self_hosted/tools/air_graph_id_uniqueness/main.pgy \
    src/self_hosted/tools/air_graph_node_count_integrity/main.pgy \
    src/self_hosted/tools/air_graph_reachability/main.pgy \
    src/self_hosted/tools/air_graph_ref_integrity/main.pgy \
    src/self_hosted/tools/air_graph_ref_live/main.pgy; do
    require_text "$air_graph_report" 'import "../../lib/json.pgy";'
    require_text "$air_graph_report" "JsonEmitObject(report_fields)"
    require_text "$air_graph_report" "JsonEmitArray("
    reject_text "$air_graph_report" 'let json_parts: Array<String>'
done
for air_graph_fact_consumer in \
    src/self_hosted/tools/air_graph_id_uniqueness/main.pgy \
    src/self_hosted/tools/air_graph_node_count_integrity/main.pgy \
    src/self_hosted/tools/air_graph_reachability/main.pgy \
    src/self_hosted/tools/air_graph_ref_integrity/main.pgy \
    src/self_hosted/tools/air_graph_ref_live/main.pgy; do
    require_text "$air_graph_fact_consumer" 'import "../air_graph_json_validator/scan_owner.pgy";'
    require_text "$air_graph_fact_consumer" "func FixturePathFromArgs"
    require_text "$air_graph_fact_consumer" "let args: Array<String> = Args();"
    require_text "$air_graph_fact_consumer" "AirGraphScalarFieldValues(content,"
    reject_text "$air_graph_fact_consumer" "func ExtractIds"
    reject_text "$air_graph_fact_consumer" "func ExtractByKey"
    reject_text "$air_graph_fact_consumer" "func CountKey"
    reject_text "$air_graph_fact_consumer" "func FirstTokenAfter"
    reject_text "$air_graph_fact_consumer" "StringIndexOf(rest"
done
require_text "src/self_hosted/tools/air_graph_node_count_integrity/main.pgy" 'import "../air_graph_json_validator/scan_owner.pgy";'
require_text "src/self_hosted/tools/air_graph_node_count_integrity/main.pgy" "AirGraphSummaryIntField(content, \"intent_count\", intents_box)"
require_text "src/self_hosted/tools/air_graph_node_count_integrity/main.pgy" "AirGraphSummaryIntField(content, \"boundary_count\", boundaries_box)"
require_text "src/self_hosted/tools/air_graph_node_count_integrity/main.pgy" "AirGraphSummaryIntField(content, \"evidence_count\", evidence_box)"
reject_text "src/self_hosted/tools/air_graph_node_count_integrity/main.pgy" "func ExtractIntField"
require_text "src/self_hosted/tools/air_graph_ref_live/main.pgy" 'import "../air_graph_json_validator/scan_owner.pgy";'
require_text "src/self_hosted/tools/air_graph_ref_live/main.pgy" "AirGraphSummaryIntField(content, \"intent_count\", intent_count_box)"
require_text "src/self_hosted/tools/air_graph_ref_live/main.pgy" "AirGraphSummaryIntField(content, \"boundary_count\", boundary_count_box)"
reject_text "src/self_hosted/tools/air_graph_ref_live/main.pgy" "func ExtractIntField"
for air_graph_parity in \
    tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh \
    tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh \
    tests/self_hosted/parity/air_graph_reachability_parity.sh \
    tests/self_hosted/parity/air_graph_ref_integrity_parity.sh \
    tests/self_hosted/parity/air_graph_ref_live_parity.sh; do
    require_text "$air_graph_parity" "pgy_selfhost_read_test_harness_manifest"
    require_text "$air_graph_parity" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
    require_text "$air_graph_parity" 'AIR_GRAPH_SCAN_OWNER="$ROOT_DIR/${harness_paths[1]}"'
    require_text "$air_graph_parity" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[2]}"'
    require_text "$air_graph_parity" 'FIXTURE_REL="${harness_paths[3]}"'
    require_text "$air_graph_parity" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
    require_text "$air_graph_parity" '"$CLEAN_BIN" "$FIXTURE_REL"'
    require_text "$air_graph_parity" "pgy_selfhost_compare_expected_text_artifact_with_owner"
    require_text "$air_graph_parity" '"air_json"'
    require_text "$air_graph_parity" 'assert_llvm_leg "self-host-parity:air-'
    require_text "$air_graph_parity" '"$FIXTURE_REL"'
    reject_text "$air_graph_parity" 'EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"'
    reject_text "$air_graph_parity" "clean JSON parity FAIL"
    reject_text "$air_graph_parity" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
    reject_text "$air_graph_parity" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
    reject_text "$air_graph_parity" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
    reject_text "$air_graph_parity" 'cp "$AIR_GRAPH_SCAN_OWNER" "$AIR_SCAN_BUILD_DIR/scan_owner.pgy"'
    reject_text "$air_graph_parity" 'TOOL_DIR="$ROOT_DIR/src/self_hosted/tools/air_graph'
    reject_text "$air_graph_parity" 'PERGYRA_TOOL_SOURCE="$TOOL_DIR/main.pgy"'
    reject_text "$air_graph_parity" 'AIR_GRAPH_SCAN_OWNER="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy"'
    reject_text "$air_graph_parity" '--run'
done
require_text "tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh" '"air-graph-id-uniqueness-paths"'
require_text "tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh" "expected-json clean"
require_text "tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh" 'DUP_FIXTURE_REL="${harness_paths[4]}"'
reject_text "tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh" "SHELL_DUPS="
reject_text "tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh" "grep -oE '\"id\":[^,}]*'"
require_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" '"air-graph-node-count-paths"'
require_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" "SHELL_IDS="
reject_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" "SHELL_DECLARED="
reject_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" "counts.ids parity FAIL"
reject_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" "counts.declared parity FAIL"
reject_text "tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh" "grep -oE '\"id\":'"
require_text "tests/self_hosted/parity/air_graph_reachability_parity.sh" '"air-graph-reachability-paths"'
require_text "tests/self_hosted/parity/air_graph_reachability_parity.sh" "expected-json clean"
require_text "tests/self_hosted/parity/air_graph_reachability_parity.sh" 'ORPHAN_FIXTURE_REL="${harness_paths[4]}"'
reject_text "tests/self_hosted/parity/air_graph_reachability_parity.sh" "SHELL_NODES="
reject_text "tests/self_hosted/parity/air_graph_reachability_parity.sh" "grep -oE '\"id\":[0-9]+'"
require_text "tests/self_hosted/parity/air_graph_ref_integrity_parity.sh" '"air-graph-ref-integrity-paths"'
require_text "tests/self_hosted/parity/air_graph_ref_integrity_parity.sh" "expected-json clean"
require_text "tests/self_hosted/parity/air_graph_ref_integrity_parity.sh" 'DANGLING_FIXTURE_REL="${harness_paths[4]}"'
reject_text "tests/self_hosted/parity/air_graph_ref_integrity_parity.sh" "SHELL_DANGLING="
reject_text "tests/self_hosted/parity/air_graph_ref_integrity_parity.sh" "comm -23"
require_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" '"air-graph-ref-live-paths"'
require_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "SHELL_INTENTS="
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "SHELL_BOUNDARIES="
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "SHELL_BOUNDARY_DANGLING="
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "SHELL_INTENT_DANGLING="
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "SHELL_DANGLING="
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "count_dangling_refs()"
reject_text "tests/self_hosted/parity/air_graph_ref_live_parity.sh" "counts.dangling parity FAIL"
require_make_target_recipe_line \
    "self-host-air-graph-consumer-parity-test-smoke" \
    'PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_json_validator_parity.sh'
require_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" 'let json_parts: Array<String>'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" '"ast-read-surface-paths"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'RATCHET_REL="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" '"$CLEAN_BIN"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/ast_read_surface_checker/main.pgy"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/ast_read_surface_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'RATCHET_REL="tests/ast_read_surface_ratchet.txt"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'bash "$ROOT_DIR/tests/ast_read_surface_smoke.sh"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "SHELL_ENUM="
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "SHELL_CODEGEN="
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "SHELL_COMPILER="
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "SHELL_SOURCE_DECL_CODEGEN="
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "SHELL_SOURCE_DECL_COMPILER="
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "SHELL_ROUTINE_SOURCE_DECL_CODEGEN="
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "enum parity FAIL"
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "source_ast_codegen parity FAIL"
require_text "src/self_hosted/tools/production_c_size_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/production_c_size_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/production_c_size_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/production_c_size_checker/main.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/production_header_size_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/production_header_size_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/production_header_size_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/production_header_size_checker/main.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" "JsonEmitArray(pieces)"
require_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" "let args: Array<String> = Args();"
require_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" "manifest_path = args[0];"
require_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" 'JsonEmitFieldString("manifest_owner", manifest_path)'
reject_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/stable_subset_section_checker/intent.md" "Pergyra checker owns the section count"
reject_text "src/self_hosted/tools/stable_subset_section_checker/intent.md" "shell grep is the auxiliary parity backend"
reject_text "src/self_hosted/tools/stable_subset_section_checker/intent.md" "grep -c '^## ' ground truth"
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" '"stable-subset-section-paths"'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'MANIFEST_PATH="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" '"$CLEAN_BIN" "$MANIFEST_PATH"'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'assert_llvm_leg "self-host-parity:stable-subset-section" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$MANIFEST_PATH"'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" "SHELL_SECTIONS="
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" "grep -c '^## '"
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/stable_subset_section_checker/main.pgy"'
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/stable_subset_section_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'MANIFEST_PATH="docs/107_beta_stable_subset.md"'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../compiler/artifact_zone_owner.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../compiler/test_harness_owner.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../compiler/subprocess_runner_owner.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerRunOutputArtifactKind()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerArtifactKindKnown(artifact_kind)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "artifact_kind = args[4];"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'JsonEmitFieldString("artifact_kind", artifact_kind)'
reject_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'JsonEmitFieldString("artifact_kind", CompilerRunOutputArtifactKind())'
reject_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerArtifactKindAt(6)"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerEmittedCArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerDiagnosticsArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerAirJsonArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerMirJsonArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerAstTextArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerAbiLayoutArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerRuntimeMaterializationArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerEmittedLlvmArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerEmittedSelfHostedArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspDiagnosticsArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspTransportFrameArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspTransportStreamArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspRequestDispatchArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspResponseEmissionArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspSessionReplayArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "func CompilerLspDocumentStoreArtifactKind"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindCount() == 19"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(0) == CompilerDiagnosticsArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(1) == CompilerAirJsonArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(2) == CompilerMirJsonArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(3) == CompilerAbiLayoutArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(4) == CompilerRuntimeMaterializationArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(5) == CompilerEmittedCArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(6) == CompilerEmittedLlvmArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(7) == CompilerEmittedSelfHostedArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(8) == CompilerRunOutputArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(9) == CompilerAstTextArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(10) == CompilerLspDiagnosticsArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(11) == CompilerLspTransportFrameArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(12) == CompilerLspTransportStreamArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(13) == CompilerLspRequestDispatchArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(14) == CompilerLspResponseEmissionArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(15) == CompilerLspSessionReplayArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(16) == CompilerLspDocumentStoreArtifactKind()"
require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "CompilerArtifactKindAt(17) == CompilerLspSessionStateArtifactKind()"
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(0) == "diagnostics"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(1) == "air_json"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(2) == "mir_json"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(3) == "abi_layout"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(4) == "runtime_materialization"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(6) == "emitted_llvm"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(9) == "ast_text"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(10) == "lsp_diagnostics"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(11) == "lsp_transport_frame"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(12) == "lsp_transport_stream"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(13) == "lsp_request_dispatch"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(14) == "lsp_response_emission"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(15) == "lsp_session_replay"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(16) == "lsp_document_store"'
reject_text "src/self_hosted/compiler/artifact_zone_owner.pgy" 'CompilerArtifactKindAt(17) == "lsp_session_state"'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "let args: Array<String> = Args();"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "expected_path = args[0];"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "actual_path = args[1];"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessProjectionOrExit(ToInt(args[2]))"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessProjectionOrExit(ToInt(args[3]))"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessComparableArtifactPathAt(0)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessComparableArtifactPathAt(1)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessFindingCap()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessUseCaseAt(0)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessUseCaseKnown(CompilerSubprocessOracleCompareUseCase())"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessSchema()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessOracleCompareTimeoutMs()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessOracleCompareEnvAllowlist()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessOracleComparePlanJson("
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'JsonEmitFieldRaw("subprocess_plan", subprocess_plan)'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'let json_parts: Array<String>'
reject_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'src/self_hosted/tools/backend_output_comparator/fixture/'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessProjectionIndexKnown"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessProjectionOrExit"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessComparableArtifactPathAt"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessFindingCap"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessSourcePathRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedDiagnosticRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedAirJsonRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedMirJsonRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedAbiLayoutRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedStdoutRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedExitRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessProjectionRow"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessCOracleProjection"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessLlvmOracleProjection"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessSelfHostedProjection"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessExpectedComparableArtifactPath"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessActualComparableArtifactPath"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendOutputComparatorSuiteName"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendOutputComparatorToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendOutputComparatorExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendOutputComparatorPathAt"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendOutputComparatorReady"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_tool_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_air_graph_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_driver_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_codegen_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_parser_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_semantic_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_mir_json_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_codegen_bootstrap_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" 'import "test_harness_lsp_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphScanOwnerPath"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphIdUniquenessSuiteName"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphIdUniquenessPathAt"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphNodeCountSuiteName"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphNodeCountPathAt"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphReachabilitySuiteName"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphReachabilityPathAt"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphRefIntegritySuiteName"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphRefIntegrityPathAt"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphRefLiveSuiteName"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphRefLivePathAt"
require_text "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" "func CompilerHarnessAirGraphConsumersReady"
require_text "src/self_hosted/compiler/test_harness_driver_paths_owner.pgy" "func CompilerHarnessDriverRung0SuiteName"
require_text "src/self_hosted/compiler/test_harness_driver_paths_owner.pgy" "func CompilerHarnessDriverRung1SuiteName"
require_text "src/self_hosted/compiler/test_harness_driver_paths_owner.pgy" "func CompilerHarnessDriverRung0PathAt"
require_text "src/self_hosted/compiler/test_harness_driver_paths_owner.pgy" "func CompilerHarnessDriverRung1PathAt"
require_text "src/self_hosted/compiler/test_harness_driver_paths_owner.pgy" "func CompilerHarnessDriverPathsReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLinterParitySuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLinterToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLinterExpectedDiagnosticsPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLinterFixturePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLinterPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLinterParityReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessModuleManifestResolverSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessModuleManifestResolverToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessModuleManifestResolverExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessModuleManifestResolverInputManifestPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessModuleManifestResolverPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessModuleManifestResolverReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStableSubsetSectionSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStableSubsetSectionToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStableSubsetSectionExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStableSubsetSectionInputManifestPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStableSubsetSectionPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStableSubsetSectionReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessRuntimeBoundarySuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessRuntimeBoundaryToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessRuntimeBoundaryExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessRuntimeBoundaryPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessRuntimeBoundaryReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDocLinkCheckerSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDocLinkCheckerToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDocLinkCheckerExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDocLinkCheckerIndexPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDocLinkCheckerPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDocLinkCheckerReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessExamplesInventorySuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessExamplesInventoryToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessExamplesInventoryExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessExamplesInventoryPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessExamplesInventoryReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLexerParitySuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLexerToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLexerComparatorSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLexerFixtureDirPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLexerPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessLexerParityReady"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenParitySuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenParserSourcePath"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenComparatorSourcePath"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenFixtureDirPath"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenExpectedDirPath"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenPathAt"
require_text "src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" "func CompilerHarnessCodegenParityReady"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserParitySuiteName"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserComparatorSourcePath"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserFixtureDirPath"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserExpectedCleanPath"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserPathAt"
require_text "src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" "func CompilerHarnessParserParityReady"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticParitySuiteName"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticComparatorSourcePath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticFixtureDirPath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticExpectedDirPath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticDiagnosticCodeOwnerPath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticDiagnosticRendererPath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticSourceDirPath"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticPathAt"
require_text "src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" "func CompilerHarnessSemanticParityReady"
require_text "src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" "func CompilerHarnessMirJsonParitySuiteName"
require_text "src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" "func CompilerHarnessMirJsonMirLowerSourcePath"
require_text "src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" "func CompilerHarnessMirJsonCodegenSourcePath"
require_text "src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" "func CompilerHarnessMirJsonComparatorSourcePath"
require_text "src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" "func CompilerHarnessMirJsonPathAt"
require_text "src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" "func CompilerHarnessMirJsonParityReady"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapPathSuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapComponentSuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapToolSuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapSampleSuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapMirFixtureSuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessFuzzBackendGeneratorSuiteName"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapPathAt"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessFuzzBackendGeneratorPathAt"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapComponentRowAt"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapToolRowAt"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapSampleAt"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapMirFixtureAt"
require_text "src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy" "func CompilerHarnessCodegenBootstrapRowsReady"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspDiagnosticsSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspDiagnosticsPathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspTransportFrameSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspTransportFramePathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspTransportStreamSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspTransportStreamPathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspRequestDispatchSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspRequestDispatchPathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspResponseEmissionSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspResponseEmissionPathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspSessionReplaySuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspSessionReplayPathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspDocumentStoreSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspDocumentStorePathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspSessionStateSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspSessionStatePathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspHoverContentSuiteName"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspHoverContentPathAt"
require_text "src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy" "func CompilerHarnessLspPathsReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorAirEvidenceOwnerPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorFixturePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorCapabilityFixturePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorLiveSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorCapabilityLiveSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAirGraphJsonValidatorReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogMissingCodeJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogMissingInputJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogCodeOwnerPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogDocsOwnerPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogCOraclePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessDiagnosticCatalogReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAstReadSurfaceSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAstReadSurfaceToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAstReadSurfaceExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAstReadSurfaceRatchetPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAstReadSurfacePathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessAstReadSurfaceReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventorySuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryCDispatchPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryCUnaryDispatchPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryLlvmDispatchPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryPathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessStdlibDispatchInventoryReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionCSizeSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionCSizeToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionCSizeExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionCSizePathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionCSizeReady"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionHeaderSizeSuiteName"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionHeaderSizeToolSourcePath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionHeaderSizeExpectedJsonPath"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionHeaderSizePathAt"
require_text "src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" "func CompilerHarnessProductionHeaderSizeReady"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriSmokeSuiteName"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriExtendedSuiteName"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriSmokeCaseCount"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriSmokeCaseAt"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriExtendedCaseCount"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriExtendedCaseAt"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriSmokeCasesReady"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriExtendedCasesReady"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessBackendTriSuiteReady"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessRowAt(0) == CompilerHarnessSourcePathRow()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessRowAt(7) == CompilerHarnessProjectionRow()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessProjectionAt(0) == CompilerHarnessCOracleProjection()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessProjectionAt(2) == CompilerHarnessSelfHostedProjection()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessComparableArtifactPathAt(0) == CompilerHarnessExpectedComparableArtifactPath()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessComparableArtifactPathAt(1) == CompilerHarnessActualComparableArtifactPath()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessLinterParityReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessModuleManifestResolverReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessStableSubsetSectionReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessRuntimeBoundaryReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessDocLinkCheckerReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessExamplesInventoryReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessLexerParityReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessBackendOutputComparatorReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessAirGraphJsonValidatorReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessDiagnosticCatalogReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessAstReadSurfaceReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessStdlibDispatchInventoryReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessProductionCSizeReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessProductionHeaderSizeReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessAirGraphConsumersReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessLspPathsReady()"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "CompilerHarnessBackendTriSuiteReady()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_tool_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_air_graph_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_driver_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_codegen_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_parser_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_semantic_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_mir_json_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_codegen_bootstrap_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" 'import "test_harness_lsp_paths_owner.pgy";'
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitBackendTriSmokeCases"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLinterParityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLinterPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLinterParitySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitModuleManifestResolverPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessModuleManifestResolverPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessModuleManifestResolverSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitStableSubsetSectionPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessStableSubsetSectionPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessStableSubsetSectionSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitRuntimeBoundaryPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessRuntimeBoundaryPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessRuntimeBoundarySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitDocLinkCheckerPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDocLinkCheckerPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDocLinkCheckerSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitExamplesInventoryPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessExamplesInventoryPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessExamplesInventorySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLexerParityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLexerPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLexerParitySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitCodegenParityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessCodegenPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessCodegenParitySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitParserParityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessParserPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessParserParitySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitSemanticParityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessSemanticPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessSemanticParitySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitBackendOutputComparatorPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessBackendOutputComparatorPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessBackendOutputComparatorSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAirGraphJsonValidatorPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphJsonValidatorPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphJsonValidatorSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitDiagnosticCatalogPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDiagnosticCatalogPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDiagnosticCatalogSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAstReadSurfacePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAstReadSurfacePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAstReadSurfaceSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitStdlibDispatchInventoryPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessStdlibDispatchInventoryPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessStdlibDispatchInventorySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitFuzzBackendGeneratorPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessFuzzBackendGeneratorPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessFuzzBackendGeneratorSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspDiagnosticsPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspDiagnosticsPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspDiagnosticsSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspTransportFramePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspTransportFramePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspTransportFrameSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspTransportStreamPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspTransportStreamPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspTransportStreamSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspRequestDispatchPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspRequestDispatchPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspRequestDispatchSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspResponseEmissionPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspResponseEmissionPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspResponseEmissionSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspSessionReplayPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspSessionReplayPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspSessionReplaySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspDocumentStorePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspDocumentStorePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspDocumentStoreSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspSessionStatePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspSessionStatePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspSessionStateSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitLspHoverContentPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspHoverContentPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessLspHoverContentSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitProductionCSizePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessProductionCSizePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessProductionCSizeSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitProductionHeaderSizePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessProductionHeaderSizePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessProductionHeaderSizeSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitDriverRung0Paths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitDriverRung1Paths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDriverRung0PathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDriverRung1PathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDriverRung0SuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessDriverRung1SuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAirGraphIdUniquenessPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphIdUniquenessPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphIdUniquenessSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAirGraphNodeCountPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphNodeCountPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphNodeCountSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAirGraphReachabilityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphReachabilityPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphReachabilitySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAirGraphRefIntegrityPaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphRefIntegrityPathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphRefIntegritySuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "EmitAirGraphRefLivePaths"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphRefLivePathAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessAirGraphRefLiveSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessBackendTriSmokeCaseAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessBackendTriExtendedCaseAt(i)"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessBackendTriSmokeSuiteName()"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" "CompilerHarnessBackendTriExtendedSuiteName()"
require_text "tests/self_hosted/parity/selfcheck_sources.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" "CompilerCompletenessSemanticTargetsSuiteName"
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(0) == "source_path"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(1) == "expected_diagnostic"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(2) == "expected_air_json"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(3) == "expected_mir_json"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(4) == "expected_abi_layout"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(5) == "expected_stdout"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(6) == "expected_exit"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessRowAt(7) == "projection"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessProjectionAt(0) == "c_oracle"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessProjectionAt(1) == "llvm_oracle"'
reject_text "src/self_hosted/compiler/test_harness_owner.pgy" 'CompilerHarnessProjectionAt(2) == "self_hosted"'
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareTimeoutMs"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareEnvAllowlist"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessPlanSchema"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessPlanJson"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleComparePlanJson"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessPlanJsonReady"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "JsonEmitFieldString(CompilerSubprocessExecutablePathFact(), executable_path)"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "JsonEmitFieldString(CompilerSubprocessArgvFact(), argv_csv)"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "JsonEmitFieldString(CompilerSubprocessCwdFact(), cwd)"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareTimeoutMsValue"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "return ToString(CompilerSubprocessOracleCompareTimeoutMsValue())"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareEnvAllowlistCount"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessEnvPathName"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessEnvPgyBinName"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessEnvBackendRunTimeoutName"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessEnvSelfHostBuildDirName"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareEnvAllowlistAt"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareEnvAllowlistKnown"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessOracleCompareEnvAllowlistCount() == 4"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessOracleCompareEnvAllowlistAt(0) == CompilerSubprocessEnvPathName()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessOracleCompareEnvAllowlistKnown(CompilerSubprocessEnvBackendRunTimeoutName())"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "StringJoin(names, \",\")"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessExecutablePathFact"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessArgvFact"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessCwdFact"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessEnvAllowlistFact"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessTimeoutMsFact"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessFactKnown"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessFixtureBuildUseCase"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessArtifactProbeUseCase"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessUseCaseKnown"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(0) == CompilerSubprocessExecutablePathFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(1) == CompilerSubprocessArgvFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(2) == CompilerSubprocessCwdFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(3) == CompilerSubprocessEnvAllowlistFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(4) == CompilerSubprocessTimeoutMsFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(5) == CompilerSubprocessStreamFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessFactAt(6) == CompilerSubprocessExitFact()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessUseCaseAt(0) == CompilerSubprocessOracleCompareUseCase()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessUseCaseAt(1) == CompilerSubprocessFixtureBuildUseCase()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "CompilerSubprocessUseCaseAt(2) == CompilerSubprocessArtifactProbeUseCase()"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleComparePlanReady"
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessFactAt(0) == "executable_path"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessFactAt(1) == "argv"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessFactAt(2) == "cwd"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessFactAt(3) == "env_allowlist"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessFactAt(4) == "timeout_ms"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 5 { return "stdout_stderr"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 6 { return "exit_code"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 0 { return "oracle_compare"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 1 { return "fixture_build"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 2 { return "artifact_probe"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessUseCaseAt(1) == "fixture_build"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessUseCaseAt(2) == "artifact_probe"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'return "30000";'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'return "PATH,PGY_BIN,PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS,PGY_SELFHOST_BUILD_DIR";'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 0 { return "PATH"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 1 { return "PGY_BIN"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 2 { return "PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'if index == 3 { return "PGY_SELFHOST_BUILD_DIR"; }'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessOracleCompareEnvAllowlistAt(0) == "PATH"'
reject_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" 'CompilerSubprocessOracleCompareEnvAllowlistKnown("PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS")'
require_text "src/self_hosted/tools/backend_output_comparator/expected/clean.json" '"subprocess_timeout_ms":"30000"'
require_text "src/self_hosted/tools/backend_output_comparator/expected/clean.json" '"subprocess_env_allowlist":"PATH,PGY_BIN,PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS,PGY_SELFHOST_BUILD_DIR"'
require_text "src/self_hosted/tools/backend_output_comparator/expected/clean.json" '"subprocess_plan":{"schema":"pgy.selfhost.subprocess-plan.v1"'
require_text "src/self_hosted/tools/backend_output_comparator/expected/clean.json" '"executable_path":"backend_output_comparator"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" '"backend-output-comparator-paths"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'PERGYRA_TOOL_INPUT="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'FIXTURE_EXPECTED_REL="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'FIXTURE_ACTUAL_REL="${harness_paths[3]}"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" '"$ARG_BIN" "$ARG_EXPECTED_PATH" "$ARG_ACTUAL_PATH" 0 2'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" '"actual_projection":"self_hosted"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/artifact_zone_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_tool_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_driver_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_parser_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/completeness_ledger_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/subprocess_runner_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/expected/clean.json"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'FIXTURE_EXPECTED="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/fixture/expected.txt"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'FIXTURE_ACTUAL="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/fixture/actual.txt"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "compile_harness_manifest_once"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "pgy_selfhost_compile_test_harness_manifest"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "pgy_selfhost_test_harness_manifest_bin"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "read_harness_manifest_suite"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "compile_tri_comparator"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" '"backend-output-comparator-paths"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'comparator_source="$ROOT_DIR/${comparator_paths[0]}"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'TRI_COMPARE_BIN="$(pgy_selfhost_backend_output_comparator_bin "$WORK_DIR")"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "append_cases_from_harness_manifest"
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'append_cases_from_harness_manifest "backend-tri-smoke"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'append_cases_from_harness_manifest "backend-tri-extended"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/artifact_zone_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/subprocess_runner_owner.pgy"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'HARNESS_MANIFEST_SOURCE="$ROOT_DIR/src/self_hosted/compiler/test_harness_manifest.pgy"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'tri_tool="$tri_root/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" '"tests/cases/backend_compare/basic"'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" '"tests/cases/backend_compare/zone_host_method_abi_combo"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'timeout "$RUN_TIMEOUT_SECONDS"s "$bin" "$@"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'run_windows_fallback "$bin" "$out" "$err" "$@"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" '$(pgy_quote_ps "$bin_native")${ps_args}'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'run_native_bin "$tri_bin" "$tri_stdout" "$tri_stderr" "$expected_arg" "$actual_arg" 0 1'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" '"schema":"pgy.selfhost.backend-output-comparator.v1"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" '"ok":true'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "files_equal"
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" "show_diff"
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'diff -u'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_backend_output_comparator_source"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '"backend-output-comparator-paths"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'local comparator_source="${3:-}"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'comparator_source="$(pgy_selfhost_backend_output_comparator_source "$label" "$build_dir")"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "TestHarness backend-output comparator source must be repo-relative"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'local comparator_source="${3:-$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy}"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_compile_test_harness_manifest"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "test_harness_manifest_\$\$.exe"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "assert_llvm_leg_with_artifact_owner"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "comparator artifact path escapes repo root"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "comparator artifact path must be repo-relative"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "backend_output_comparator_\$\$.exe"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_compare_expected_text_artifact_with_owner"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'assert_llvm_leg_with_artifact_owner "$label" "$build_dir" "$c_out" "$llvm_out"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'local run_args=("$@")'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '"$c_bin" "${run_args[@]}"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '"$llvm_bin" "${run_args[@]}"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '"$comparator_bin" "$c_rel" "$llvm_rel" 0 1'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "artifact-equal"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" ".tmp/self_hosted/shared"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_comparator_needs_rebuild"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'diff <(printf'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '$(cat "$c_out")'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'c_out="$(cd "$ROOT_DIR"'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'llvm_out="$(cd "$ROOT_DIR"'
require_text "tests/self_hosted/parity/linter_parity.sh" "pgy_selfhost_compare_expected_text_artifact_with_owner"
require_text "tests/self_hosted/parity/linter_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/linter_parity.sh" '"linter-parity-paths"'
require_text "tests/self_hosted/parity/linter_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/linter_parity.sh" 'source_arg="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/linter_parity.sh" 'FIXTURE_REL="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/linter_parity.sh" '"diagnostics"'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/linter/main.pgy"'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/linter/expected/diagnostics.json"'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/linter/fixture.pgy"'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'EXPECTED_JSON="$(tr -d'
reject_text "tests/self_hosted/parity/linter_parity.sh" 'if [[ "$LLVM_JSON" != "$C_JSON" ]]'
require_text "src/self_hosted/tools/linter/main.pgy" "let args: Array<String> = Args();"
require_text "src/self_hosted/tools/linter/main.pgy" "target_path = args[0];"
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" '"ast-read-surface-paths"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'RATCHET_REL="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" '"$CLEAN_BIN"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/ast_read_surface_checker/main.pgy"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/ast_read_surface_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/ast_read_surface_checker_parity.sh" 'RATCHET_REL="tests/ast_read_surface_ratchet.txt"'
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "JsonEmitArray(findings)"
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "let args: Array<String> = Args();"
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "index_path = args[0];"
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "JsonEmitFieldString(\"index_owner\", index_path)"
reject_text "src/self_hosted/tools/doc_link_checker/main.pgy" 'let json_parts: Array<String>'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" '"doc-link-checker-paths"'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'INDEX_PATH="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" '"$CLEAN_BIN" "$INDEX_PATH"'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" "expected-json clean"
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'assert_llvm_leg "self-host-parity:doc-link-checker" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$INDEX_PATH"'
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" "SHELL_TOTAL="
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" "grep -oE '\\]\\('"
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/doc_link_checker/main.pgy"'
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/doc_link_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'INDEX_PATH="docs/INDEX.md"'
require_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" 'let json_parts: Array<String>'
require_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" '"examples-inventory-paths"'
require_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" '"$CLEAN_BIN"'
reject_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/examples_inventory_checker/main.pgy"'
reject_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/examples_inventory_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/examples_inventory_checker/expected/clean.json"'
require_text "tests/self_hosted/parity/lexer_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/lexer_parity.sh" '"lexer-parity-paths"'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/lexer/main.pgy"'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/src/self_hosted/lexer/fixture"'
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" '"production-c-size-paths"'
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" '"$CLEAN_BIN"'
require_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/production_c_size_checker/main.pgy"'
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/production_c_size_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "SHELL_C="
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "SHELL_STATS="
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "SHELL_VIOLATIONS"
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "SHELL_MAX"
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "c_files parity FAIL"
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "violations parity FAIL"
reject_text "tests/self_hosted/parity/production_c_size_checker_parity.sh" "max_lines parity FAIL"
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" '"production-header-size-paths"'
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" '"$CLEAN_BIN"'
require_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/production_header_size_checker/main.pgy"'
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/production_header_size_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "SHELL_HEADERS="
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "SHELL_STATS="
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "SHELL_VIOLATIONS"
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "SHELL_MAX"
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "headers parity FAIL"
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "violations parity FAIL"
reject_text "tests/self_hosted/parity/production_header_size_checker_parity.sh" "max_lines parity FAIL"
require_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" 'let json_parts: Array<String>'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" '"stdlib-dispatch-inventory-paths"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'PERGYRA_TOOL_INPUT="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'C_DISPATCH="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'C_UNARY_DISPATCH="${harness_paths[3]}"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'LLVM_DISPATCH="${harness_paths[4]}"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" '"$CLEAN_BIN"'
require_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/stdlib_dispatch_inventory_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'C_DISPATCH="src/codegen/transpiler_expr_stdlib_scalar_builtin.c"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'C_UNARY_DISPATCH="src/codegen/transpiler_expr_stdlib_scalar_unary.c"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" 'LLVM_DISPATCH="src/codegen/llvm_expr_stdlib_scalar_io_calls.c"'
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "SHELL_C_MAIN="
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "SHELL_C_MATH="
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "SHELL_C="
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "SHELL_LLVM="
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "SHELL_DRIFT_TOLERANCE"
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "SHELL_RAW_DRIFT"
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "c_entries parity FAIL"
reject_text "tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh" "llvm_entries parity FAIL"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" 'import "../../lib/json_fact_table.pgy";'
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonDocumentObjectFactTable(content)"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonObjectFactArrayObjectTable(root_facts, \"modules\")"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectFactTableReady(modules_facts)"
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" "nested-modules fixture"
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" '"module-manifest-resolver-paths"'
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'MANIFEST_PATH="${harness_paths[2]}"'
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" '"$CLEAN_BIN" "$MANIFEST_PATH"'
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'assert_llvm_leg "self-host-parity:module-manifest-resolver" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$MANIFEST_PATH"'
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" "SHELL_MODULES="
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" "grep -c '\"name\":'"
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/module_manifest_resolver/main.pgy"'
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/module_manifest_resolver/expected/clean.json"'
reject_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" 'MANIFEST_PATH="docs/language_module_manifest.json"'
for report_output_parity in \
    tests/self_hosted/parity/ast_read_surface_checker_parity.sh \
    tests/self_hosted/parity/doc_link_checker_parity.sh \
    tests/self_hosted/parity/examples_inventory_checker_parity.sh \
    tests/self_hosted/parity/module_manifest_resolver_parity.sh \
    tests/self_hosted/parity/production_c_size_checker_parity.sh \
    tests/self_hosted/parity/production_header_size_checker_parity.sh \
    tests/self_hosted/parity/runtime_boundary_checker_parity.sh \
    tests/self_hosted/parity/stable_subset_section_checker_parity.sh \
    tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh; do
    require_text "$report_output_parity" "pgy_selfhost_compare_expected_text_artifact_with_owner"
    require_text "$report_output_parity" '"run_output"'
    reject_text "$report_output_parity" 'EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"'
    reject_text "$report_output_parity" "clean JSON parity FAIL"
done
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "let doc_end_opt: Option<Int> = JsonDocumentObjectEnd(content)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonFieldArrayBounds(content, 0, UnwrapOption(doc_end_opt), \"modules\", module_bounds)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectCount(content, modules_open, modules_end)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectBoundsAt("
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonObjectHasField(content, object_bounds[0], object_bounds[1], \"layer\")"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectBoolFieldEqualsCount(content, modules_open, modules_end, \"beta_blocker\", true)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectStringFieldEqualsCount(content, modules_open, modules_end, \"status\", \"stable-subset\")"
require_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" "func RuntimeBoundaryRequiredTermCount"
require_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" "func RuntimeBoundaryRequiredPathAt"
require_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" "func RuntimeBoundaryRequiredTermAt"
require_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" "func EmitRuntimeBoundaryTermManifest"
require_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" 'args[0] == "--terms"'
reject_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" 'ArrayPush(paths, "src/self_hosted/runtime/README.md")'
reject_text "src/self_hosted/tools/runtime_boundary_checker/main.pgy" 'ArrayPush(terms, "The native runtime kernel remains C")'
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" '"runtime-boundary-paths"'
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" '"$CLEAN_BIN" --terms'
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" "TERMS_FILE="
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" "required_count="
require_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" "strip_pair="
reject_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/runtime_boundary_checker/main.pgy"'
reject_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"'
reject_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/runtime_boundary_checker/expected/clean.json"'
reject_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'required_terms=('
reject_text "tests/self_hosted/parity/runtime_boundary_checker_parity.sh" 'Portable runtime policy can move to Pergyra'
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "let args: Array<String> = Args();"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "manifest_path = args[0];"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectFactCount(modules_facts)"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectFactFieldCount(modules_facts, \"layer\")"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectFactBoolFieldEqualsCount(modules_facts, \"beta_blocker\", true)"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectFactStringFieldEqualsCount(modules_facts, \"status\", \"stable-subset\")"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" 'let json_parts: Array<String>'
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" 'StringContains(content, "\"modules\":")'
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "TextScan.CountOccurrences"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonFieldArrayBounds(content, 0, JsonDocumentObjectEnd(content)"
require_text "Makefile" "self-host-semantic-selfcheck-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/selfcheck_sources.sh"
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" "lexer_selfcheck_unit"
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" "grep -h -v '^import '"
reject_text "src/self_hosted/lexer/main.pgy" "fixture/source.txt"
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" "SELF_SOURCES"
reject_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" "return 148;"
reject_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" "return 154;"
completeness_min_count="$(grep -F "return 155;" "$ROOT_DIR/src/self_hosted/compiler/completeness_ledger_owner.pgy" |
    wc -l |
    tr -d ' ')"
[[ "$completeness_min_count" -ge 8 ]] ||
    fail "self-host completeness minima drifted below the 155-source closed slice"

require_text "src/self_hosted/mir_lower/json_fact_read.pgy" 'import "../lib/json.pgy";'
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" 'import "../lib/json_fact_table.pgy";'
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirFactObjectStart"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonCharAt"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func CharAt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectStringFact"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectStringFactOpt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectNumberFact"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectNumberFactOpt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectEnd(json: String, start: Int, limit: Int) -> Option<Int>"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectFieldValueBounds"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirRoutineArrayBounds"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirRoutineArrayObjectFactTable"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirRoutineObjectBoundsAt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectArrayStringFactAt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectArrayStringFactCount"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectArrayBounds"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirObjectArrayObjectBoundsAt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirDeclArrayObjectFactTable"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirDeclObjectBoundsAt"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonObjectFactArrayObjectTable(root, \"decls\")"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonObjectFactArrayObjectTable(root, \"routines\")"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonArrayObjectFactAt(MirDeclArrayObjectFactTable(json), row)"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonArrayObjectFactAt(MirRoutineArrayObjectFactTable(json), row)"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func SourceLocalType"
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "MirObjectArrayObjectBoundsAt(json, start, end, \"source_locals\", row, bounds)"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func ReadJsonString"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "ReadJsonString(json,"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" 'FindFrom(json, "\"source_locals\":['
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func JsonFieldString"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func JsonFieldNumber"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func JsonFirstArrayString"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "let doc_end_opt: Option<Int> = JsonDocumentObjectEnd(json)"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "JsonFieldArrayBounds(json, 0, UnwrapOption(doc_end_opt)"
require_text "src/self_hosted/lib/json.pgy" "func JsonObjectStringFieldOpt"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirDeclArrayBounds(json, decls)"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirRoutineObjectBoundsAt(json, 0, routine)"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirObjectArrayBounds(json, routine[0], routine[1], \"source_locals\", source_locals)"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirObjectArrayObjectBoundsAt(json, routine[0], routine[1], \"body\", 0, inst)"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirObjectStringFactOpt(json, routine[0], routine[1], \"name\")"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirObjectStringFactOpt(json, inst[0], inst[1], \"source_type\")"
reject_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "JsonObjectStringField(json,"
reject_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "JsonFieldArrayBounds(json,"
reject_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "JsonArrayObjectBoundsAt(json,"
reject_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "JsonObjectFieldValueBounds(json,"
reject_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "JsonDocumentStringFieldEquals(json,"
reject_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" 'import "../lib/json.pgy";'
require_text "src/self_hosted/mir_lower/decl_lower.pgy" "MirDeclObjectBoundsAt(json, row, decl_bounds)"
require_text "src/self_hosted/mir_lower/decl_lower.pgy" "MirObjectArrayObjectBoundsAt(json, decl_start, decl_end, \"fields\", row, field_bounds)"
require_text "src/self_hosted/mir_lower/decl_lower.pgy" "MirObjectStringFact(json, decl_bounds[0], decl_bounds[1], \"kind\")"
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" "func DeclObjectEnd"
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" "func VariantObjectEnd"
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" 'FindFrom(json, "\"decls\":['
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" 'FindFrom(json, "\"fields\":['
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" 'FindFrom(json, "\"methods\":['
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" 'FindFrom(json, "\"variants\":['
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" "JsonFieldString(json,"
reject_text "src/self_hosted/mir_lower/decl_lower.pgy" "ReadJsonString(json,"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "MirObjectStringFact(json, rpos"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "MirObjectFieldValueBounds(json, rpos"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func FindRoutineByOwnerName(json: String, owner: String, name: String) -> Option<Int>"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "return Some(rpos)"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "return None"
require_text "src/self_hosted/mir_lower/decl_lower.pgy" "UnwrapOption(routine_pos)"
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "JsonFieldString(json,"
reject_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "ReadJsonString(json,"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectStringFact(json, kp, inst_end, \"source_type\")"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectArrayStringFactCount(json, kp, inst_end, \"match_patterns\")"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectArrayStringFactAt(json, kp, inst_end, \"match_patterns\", 0)"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" "MirObjectArrayStringFactAt(json, kp, inst_end, \"match_bindings\", 0)"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "JsonFieldString(json,"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "JsonFirstArrayString(json,"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" 'FindFrom(json, "\"match_patterns\":['
require_text "src/self_hosted/mir_lower/stmt_render.pgy" "MirObjectArrayStringFactAt(json, inst_start, inst_end, \"destructure_bindings\", index)"
require_text "src/self_hosted/mir_lower/stmt_render.pgy" "MirObjectArrayStringFactAt(json, inst_start, inst_end, \"defer_body\", emitted)"
reject_text "src/self_hosted/mir_lower/stmt_render.pgy" 'FindFrom(json, "\"destructure_bindings\":['
reject_text "src/self_hosted/mir_lower/stmt_render.pgy" 'FindFrom(json, "\"defer_body\":['
reject_text "src/self_hosted/mir_lower/stmt_render.pgy" "ReadJsonString(json,"

semantic_fixture_count="$(find "$SELF_HOST_DIR/semantic/fixture" -maxdepth 1 -type f -name '*.pgy' | wc -l | tr -d ' ')"
semantic_expected_count="$(find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.diag' | wc -l | tr -d ' ')"
[[ "$semantic_fixture_count" -eq 108 ]] ||
    fail "semantic fixture count drifted: $semantic_fixture_count != 108"
[[ "$semantic_expected_count" -eq 108 ]] ||
    fail "semantic expected count drifted: $semantic_expected_count != 108"
require_text "src/self_hosted/PROGRESS.md" "across 108 fixtures"
require_file "src/self_hosted/semantic/fixture/valid_scalar_math_builtins.pgy"
require_file "src/self_hosted/semantic/fixture/valid_string_plus.pgy"
require_file "src/self_hosted/semantic/fixture/valid_string_scalar_plus.pgy"
require_file "src/self_hosted/semantic/fixture/valid_bool_arith.pgy"
require_file "src/self_hosted/semantic/fixture/valid_seedrandom_builtin.pgy"
require_file "src/self_hosted/semantic/fixture/valid_writefile_builtin.pgy"
require_file "src/self_hosted/semantic/fixture/valid_let_mut_reassign.pgy"
require_file "src/self_hosted/semantic/fixture/valid_generated_source_string_literal.pgy"
require_file "src/self_hosted/semantic/fixture/valid_scalar_utility_int.pgy"
require_file "src/self_hosted/semantic/fixture/valid_option_unwrap_payload.pgy"
require_file "src/self_hosted/semantic/fixture/valid_option_try_payload.pgy"
require_file "src/self_hosted/semantic/fixture/bad_option_payload_return.pgy"
require_file "src/self_hosted/semantic/fixture/bad_option_payload_let.pgy"
require_file "src/self_hosted/semantic/fixture/bad_issome_non_option.pgy"
require_file "src/self_hosted/semantic/fixture/bad_unwrap_non_option.pgy"
require_file "src/self_hosted/semantic/fixture/valid_option_none_call.pgy"
require_file "src/self_hosted/semantic/fixture/bad_issome_none_literal.pgy"
require_file "src/self_hosted/semantic/fixture/bad_issome_none_call.pgy"
require_file "src/self_hosted/semantic/fixture/bad_unwrap_none_literal.pgy"
require_file "src/self_hosted/semantic/fixture/bad_unwrap_none_call.pgy"
require_text "src/self_hosted/semantic/expr_type_owner.pgy" "func OptionCallReturnType"
require_text "src/self_hosted/semantic/expr_type_owner.pgy" "func TryOperandBounds"
require_text "src/self_hosted/semantic/expr_type_owner.pgy" "TryOperandBounds(text, try_bounds)"
require_text "src/self_hosted/semantic/expr_validation_owner.pgy" "func CheckTryOperand"
require_text "src/self_hosted/semantic/expr_validation_owner.pgy" "TryOperandBounds(text, bounds)"
require_text "src/self_hosted/semantic/expr_type_owner.pgy" 'split_op == "+" && lt == "String" && rt == "String"'
require_text "src/self_hosted/semantic/expr_type_owner.pgy" 'lt == "Int" || lt == "Long" || lt == "Float" || lt == "Bool"'
require_text "src/self_hosted/semantic/expr_type_owner.pgy" 'return Concat(Concat("Option<", value_type), ">")'
require_text "src/self_hosted/semantic/expr_type_owner.pgy" "return Substring(option_type, 7, StringLength(option_type) - 8)"
require_text "src/self_hosted/semantic/expr_type_owner.pgy" 'if callee == "None"'
require_text "src/self_hosted/semantic/call_check_owner.pgy" "func CheckOptionBuiltinArgs"
require_text "src/self_hosted/semantic/call_check_owner.pgy" "builtin_arg_type_mismatch"
require_text "src/self_hosted/semantic/call_check_owner.pgy" "option_concrete_type_required"
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "None")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Sqrt")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Random")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "SeedRandom")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "ReadStdin")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "WriteFile")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Abs")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Min")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Max")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Clamp")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Sin")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Atan2")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Log2")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Join")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "StringSplit")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "CharAt")'
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Print")'
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func SkipLineComment"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func SkipBlockComment"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func FindMatchingBraceWithin"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func ExpectLiteral(content: String, start: Int, tok: String) -> Option<Int>"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "return None"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "return Some(i + tl)"
reject_text "src/self_hosted/semantic/text_scan_owner.pgy" "return -1"
require_file "src/self_hosted/semantic/fixture/valid_comment_brace_scope.pgy"

while IFS= read -r fixture; do
    base="$(basename "$fixture" .pgy)"
    require_file "src/self_hosted/semantic/expected/${base}.diag"
done < <(find "$SELF_HOST_DIR/semantic/fixture" -maxdepth 1 -type f -name '*.pgy' | sort)

while IFS= read -r expected; do
    base="$(basename "$expected" .diag)"
    require_file "src/self_hosted/semantic/fixture/${base}.pgy"
done < <(find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.diag' | sort)

if find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.json' -print -quit | grep -q .; then
    fail "semantic verdict fixtures must stay diagnostic blocks (*.diag), not JSON"
fi

codegen_fixture_count="$(find "$SELF_HOST_DIR/codegen/fixture" -maxdepth 1 -type f -name '*.pgy' | wc -l | tr -d ' ')"
codegen_expected_count="$(find "$SELF_HOST_DIR/codegen/expected" -maxdepth 1 -type f -name '*_stdout.txt' | wc -l | tr -d ' ')"
[[ "$codegen_fixture_count" -eq 66 ]] ||
    fail "codegen fixture count drifted: $codegen_fixture_count != 66"
[[ "$codegen_expected_count" -eq 66 ]] ||
    fail "codegen expected count drifted: $codegen_expected_count != 66"
require_file "src/self_hosted/codegen/fixture/hello.pgy"
require_file "src/self_hosted/codegen/fixture/seed_random.pgy"
require_file "src/self_hosted/codegen/fixture/array_index_assign.pgy"
require_file "src/self_hosted/codegen/fixture/string_array_index_return.pgy"
require_text "src/self_hosted/codegen/README.md" "Golden/platform contract"
require_text "src/self_hosted/codegen/README.md" "PGY_SELFHOST_CODEGEN_BACKENDS=c"
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" "func CodegenParityFixtureManifestRows"
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" "func EmitCodegenParityFixtureManifest"
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" "DirWalk(CodegenParityFixtureDir())"
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" "CodegenParityExpectedPath(base)"
require_text "src/self_hosted/codegen/run/codegen_run_owner.pgy" '"--fixture-manifest"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture()'
require_text "tests/self_hosted/parity/codegen_parity.sh" "read_codegen_fixture_manifest"
require_text "tests/self_hosted/parity/codegen_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/codegen_parity.sh" '"codegen-parity-paths"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/${harness_paths[3]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'EXPECTED_DIR="$ROOT_DIR/${harness_paths[4]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" "compile_parser_ast_producer"
require_text "tests/self_hosted/parity/codegen_parity.sh" '"$PARSER_BIN" "$src_rel"'
require_text "tests/self_hosted/parity/codegen_parity.sh" '"$manifest_bin" --fixture-manifest'
reject_text "tests/self_hosted/parity/codegen_parity.sh" "    hello"
reject_text "tests/self_hosted/parity/codegen_parity.sh" "    seed_random"
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'TOOL_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'PARSER_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'FIXTURE_DIR="$ROOT_DIR/src/self_hosted/codegen/fixture"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'EXPECTED_DIR="$ROOT_DIR/src/self_hosted/codegen/expected"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'pgy_binary_is_runnable_here "$bin"'
require_text "tests/pgy_binary_path_helpers.sh" "pgy_reject_wsl_windows_pgy_parity_mix()"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:codegen" "$PGY"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'pgy_reject_wsl_windows_pgy_parity_mix "self-host-bootstrap" "$PGY"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"codegen-bootstrap-paths"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"codegen-bootstrap-components"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"codegen-bootstrap-tools"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"codegen-bootstrap-samples"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"codegen-bootstrap-mir-fixtures"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'CODEGEN_FIXTURE_DIR="$ROOT_DIR/${harness_paths[3]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'MIR_LOWER_SOURCE="$ROOT_DIR/${harness_paths[4]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'MIR_FIXTURE_DIR="$ROOT_DIR/${harness_paths[5]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'FUZZ_SOURCE="$ROOT_DIR/${harness_paths[6]}"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "compile_parser_ast_producer"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "emit_self_parser_ast"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'compare_artifact_with_owner "fixpoint_gen2_gen3" "$B/gen2.c" "$B/gen3.c" "emitted_c"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'compare_artifact_with_owner "fuzz_generator_manifest"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'SAMPLE="hello func_recursive struct_param array_push str_indexof else_if_chain string_equality io_probe"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'MIR_BOOTSTRAP_FIXTURES="let_log forloop role_operator_dispatch"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"emitted_c"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" '"emitted_self_hosted"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'pgy --ast'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" "compare_emitted_c_with_owner"
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" "files_equal_text"
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" "show_file_delta"
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'diff -u'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'TOOL_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'PARSER_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'MIR_LOWER_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'FUZZ_SOURCE="$ROOT_DIR/src/self_hosted/fuzz/backend_parity_generator/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'csrc="$ROOT_DIR/src/self_hosted/$comp/main.pgy"'
reject_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'tsrc="$ROOT_DIR/src/self_hosted/tools/$name/main.pgy"'
require_text "Makefile" "self-host-driver-rung0-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/driver_rung0_parity.sh"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "func DriverParityFixtureCount"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "func DriverParityFixtureAt"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "func EmitDriverParityFixtureManifest"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "DriverParityFixtureCount()"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" 'DriverParityFixtureAt(i)'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" '"examples/hello.pgy"'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" '"src/self_hosted/codegen/fixture/func_call.pgy"'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" '"src/self_hosted/codegen/fixture/struct_param.pgy"'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" 'args[0] == "--fixture-manifest"'
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" 'args[0] == "--fixture-manifest"'
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "EmitDriverParityFixtureManifest()"
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" '"driver-rung0-paths"'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'DRIVER_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'CODEGEN_SOURCE="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" "read_driver_fixture_manifest"
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" '"$PARSER_BIN" "$fixture_rel"'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" '"$manifest_bin" --fixture-manifest'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" '"ast_text"'
require_text "tests/self_hosted/parity/driver_rung0_parity.sh" '"emitted_c"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" '    "examples/hello.pgy"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" '    "src/self_hosted/codegen/fixture/func_call.pgy"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" '    "src/self_hosted/codegen/fixture/struct_param.pgy"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'DRIVER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/driver_rung0_main.pgy"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'PARSER_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'CODEGEN_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'pgy --ast'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/driver_rung0_parity.sh" 'diff -u'
require_text "Makefile" "self-host-driver-rung1-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/driver_rung1_parity.sh"
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" "driver-rung1"
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" '"driver-rung1-paths"'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'DRIVER_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'CODEGEN_SOURCE="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" "read_driver_fixture_manifest"
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" '"$PARSER_BIN" "$fixture_rel"'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" '"$manifest_bin" --fixture-manifest'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" "-o"
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" '"ast_text"'
require_text "tests/self_hosted/parity/driver_rung1_parity.sh" '"emitted_c"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" '    "examples/hello.pgy"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" '    "src/self_hosted/codegen/fixture/func_call.pgy"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" '    "src/self_hosted/codegen/fixture/struct_param.pgy"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'DRIVER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/driver_rung1_main.pgy"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'PARSER_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'CODEGEN_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'pgy --ast'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/driver_rung1_parity.sh" 'diff -u'
require_file "src/self_hosted/lsp/main.pgy"
require_file "src/self_hosted/lsp/README.md"
require_file "src/self_hosted/lsp/intent.md"
require_file "src/self_hosted/lsp/diagnostics_owner.pgy"
require_file "src/self_hosted/lsp/document_store_owner.pgy"
require_file "src/self_hosted/lsp/feature_owner.pgy"
require_file "src/self_hosted/lsp/request_owner.pgy"
require_file "src/self_hosted/lsp/response_owner.pgy"
require_file "src/self_hosted/lsp/session_owner.pgy"
require_file "src/self_hosted/lsp/session_state_owner.pgy"
require_file "src/self_hosted/lsp/hover_content_owner.pgy"
require_file "src/self_hosted/lsp/squiggle_owner.pgy"
require_file "src/self_hosted/lsp/transport_owner.pgy"
require_max_lines "src/self_hosted/lsp/diagnostics_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/document_store_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/feature_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/request_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/response_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/session_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/session_state_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/hover_content_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/squiggle_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/transport_owner.pgy" 600
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/main.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/diagnostics_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/document_store_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/feature_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/request_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/response_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/session_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/session_state_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/hover_content_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/squiggle_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/lsp/transport_owner.pgy"
require_text "src/self_hosted/lsp/main.pgy" 'import "diagnostics_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "document_store_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "request_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "response_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "session_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "session_state_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "hover_content_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" 'import "transport_owner.pgy";'
require_text "src/self_hosted/lsp/main.pgy" "LspTransportProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspTransportFrameProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspTransportStreamProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspTransportStreamProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspRequestDispatchProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspRequestDispatchProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspResponseProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspResponseProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspSessionReplayProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspSessionReplayProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspDocumentStoreProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspDocumentStoreProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspSessionStateProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspSessionStateProbeFromArgs(args)"
require_text "src/self_hosted/lsp/main.pgy" "LspHoverContentProbeRequested(args)"
require_text "src/self_hosted/lsp/main.pgy" "RunLspHoverContentProbeFromArgs(args)"
require_text "src/self_hosted/lsp/feature_owner.pgy" 'import "../lib/json_emit.pgy";'
require_text "src/self_hosted/lsp/feature_owner.pgy" "func LspFeatureContractReady"
require_text "src/self_hosted/lsp/feature_owner.pgy" "func LspFeatureResultForMethod"
require_text "src/self_hosted/lsp/feature_owner.pgy" "func LspFeatureTextDocumentMethod"
require_text "src/self_hosted/lsp/feature_owner.pgy" "textDocument/hover"
require_text "src/self_hosted/lsp/feature_owner.pgy" "textDocument/completion"
require_text "src/self_hosted/lsp/response_owner.pgy" 'import "feature_owner.pgy";'
require_text "src/self_hosted/lsp/response_owner.pgy" "LspFeatureResultForMethod(method)"
require_text "src/self_hosted/lsp/response_owner.pgy" "LspFeatureTextDocumentMethod(method)"
require_text "src/self_hosted/lsp/document_store_owner.pgy" 'import "../lib/json_fact_table.pgy";'
require_text "src/self_hosted/lsp/document_store_owner.pgy" 'import "transport_owner.pgy";'
require_text "src/self_hosted/lsp/document_store_owner.pgy" "func LspDocumentStoreContractReady"
require_text "src/self_hosted/lsp/document_store_owner.pgy" "func LspDocumentStoreJson"
require_text "src/self_hosted/lsp/document_store_owner.pgy" "JsonObjectFactObjectTable"
require_text "src/self_hosted/lsp/document_store_owner.pgy" "JsonObjectFactArrayObjectTable"
require_text "src/self_hosted/lsp/document_store_owner.pgy" "LspTransportCompleteFrameLength(tail)"
require_text "src/self_hosted/lsp/request_owner.pgy" 'import "../lib/json_fact_table.pgy";'
require_text "src/self_hosted/lsp/request_owner.pgy" 'import "transport_owner.pgy";'
require_text "src/self_hosted/lsp/request_owner.pgy" "func LspRequestDispatchContractReady"
require_text "src/self_hosted/lsp/request_owner.pgy" "func LspRequestDispatchStreamJson"
require_text "src/self_hosted/lsp/request_owner.pgy" "JsonDocumentObjectFactTable(body)"
require_text "src/self_hosted/lsp/request_owner.pgy" "LspTransportCompleteFrameLength(tail)"
require_text "src/self_hosted/lsp/response_owner.pgy" 'import "../lib/json_fact_table.pgy";'
require_text "src/self_hosted/lsp/response_owner.pgy" 'import "request_owner.pgy";'
require_text "src/self_hosted/lsp/response_owner.pgy" 'import "transport_owner.pgy";'
require_text "src/self_hosted/lsp/response_owner.pgy" "func LspResponseEmissionContractReady"
require_text "src/self_hosted/lsp/response_owner.pgy" "func LspResponseEmissionStreamJson"
require_text "src/self_hosted/lsp/response_owner.pgy" "JsonDocumentObjectFactTable(body)"
require_text "src/self_hosted/lsp/response_owner.pgy" "LspRequestResponseRequired(method"
require_text "src/self_hosted/lsp/response_owner.pgy" "LspTransportCompleteFrameLength(tail)"
require_text "src/self_hosted/lsp/session_owner.pgy" 'import "response_owner.pgy";'
require_text "src/self_hosted/lsp/session_owner.pgy" 'import "transport_owner.pgy";'
require_text "src/self_hosted/lsp/session_owner.pgy" "func LspSessionReplayContractReady"
require_text "src/self_hosted/lsp/session_owner.pgy" "func LspSessionReplayJson"
require_text "src/self_hosted/lsp/session_owner.pgy" "LspTransportCompleteFrameLength(tail)"
require_text "src/self_hosted/lsp/session_owner.pgy" "LspResponseFrameForRequestBody(body)"
require_text "src/self_hosted/lsp/session_state_owner.pgy" 'import "document_store_owner.pgy";'
require_text "src/self_hosted/lsp/session_state_owner.pgy" 'import "session_owner.pgy";'
require_text "src/self_hosted/lsp/session_state_owner.pgy" "func LspSessionStateContractReady"
require_text "src/self_hosted/lsp/session_state_owner.pgy" "func LspSessionStateJson"
require_text "src/self_hosted/lsp/session_state_owner.pgy" "LspSessionReplayJson(buffer)"
require_text "src/self_hosted/lsp/session_state_owner.pgy" "LspDocumentStoreJson(buffer)"
require_text "src/self_hosted/lsp/hover_content_owner.pgy" 'import "document_store_owner.pgy";'
require_text "src/self_hosted/lsp/hover_content_owner.pgy" 'import "transport_owner.pgy";'
require_text "src/self_hosted/lsp/hover_content_owner.pgy" "func LspHoverContentContractReady"
require_text "src/self_hosted/lsp/hover_content_owner.pgy" "func LspHoverContentJson"
require_text "src/self_hosted/lsp/hover_content_owner.pgy" "func LspHoverTextForWord"
require_text "src/self_hosted/lsp/hover_content_owner.pgy" "LspDocumentStoreApplyText(body)"
require_text "src/self_hosted/lsp/hover_content_owner.pgy" "LspTransportCompleteFrameLength(tail)"
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'import "squiggle_owner.pgy";'
require_text "src/self_hosted/lsp/transport_owner.pgy" 'import "../lib/json_emit.pgy";'
require_text "src/self_hosted/lsp/transport_owner.pgy" "func LspTransportFrameContractReady"
require_text "src/self_hosted/lsp/transport_owner.pgy" "func LspTransportStreamContractReady"
require_text "src/self_hosted/lsp/transport_owner.pgy" "func LspTransportStreamJson"
require_text "src/self_hosted/lsp/transport_owner.pgy" "func RunLspTransportFrameProbeFromArgs"
require_text "src/self_hosted/lsp/transport_owner.pgy" "func RunLspTransportStreamProbeFromArgs"
require_text "src/self_hosted/lsp/transport_owner.pgy" "ReadStdin(max_bytes)"
require_text "src/self_hosted/lsp/transport_owner.pgy" '"Content-Length:"'
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'import "../lib/json_emit.pgy";'
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'import "../semantic/diagnostic_code_owner.pgy";'
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'import "../semantic/source_bundle_owner.pgy";'
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'import "../semantic/diagnostic_owner.pgy";'
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'import "../semantic/program_check_owner.pgy";'
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" "func LspDiagnosticPayloadContractReady"
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" "func LspUriForPath"
require_text "src/self_hosted/lsp/diagnostics_owner.pgy" 'if c == "\\"'
require_text "src/self_hosted/lsp/squiggle_owner.pgy" "func LspSquigglePolicyContractReady"
require_text "src/self_hosted/lsp/squiggle_owner.pgy" "func LspSquiggleClassForDiagnostic"
require_text "src/self_hosted/lsp/squiggle_owner.pgy" "func LspSquigglePolicySnapshotJson"
require_text "src/self_hosted/lsp/squiggle_owner.pgy" '"pgy.selfhost.lsp-squiggle-policy.v1"'
require_text "src/self_hosted/lsp/squiggle_owner.pgy" "PGY_AIR_MEANING_ERASABLE"
require_text "src/self_hosted/lsp/squiggle_owner.pgy" "pin_escape"
require_file "src/self_hosted/lsp/fixture/valid_int_return.pgy"
require_file "src/self_hosted/lsp/fixture/bad_logical_right.pgy"
require_file "src/self_hosted/lsp/fixture/bad_undefined_return.pgy"
require_file "src/self_hosted/lsp/fixture/bad_return_type.pgy"
require_file "src/self_hosted/lsp/fixture/bad_while_condition.pgy"
require_file "src/self_hosted/lsp/fixture/bad_not_operand.pgy"
require_file "src/self_hosted/lsp/expected/valid_int_return.json"
require_file "src/self_hosted/lsp/expected/bad_logical_right.json"
require_file "src/self_hosted/lsp/expected/bad_undefined_return.json"
require_file "src/self_hosted/lsp/expected/bad_return_type.json"
require_file "src/self_hosted/lsp/expected/bad_while_condition.json"
require_file "src/self_hosted/lsp/expected/bad_not_operand.json"
require_file "src/self_hosted/lsp/expected/squiggle_policy.json"
require_file "src/self_hosted/lsp/expected/transport_frame.json"
require_file "src/self_hosted/lsp/expected/transport_frame_incomplete.json"
require_file "src/self_hosted/lsp/expected/transport_stream.json"
require_file "src/self_hosted/lsp/expected/transport_stream_partial.json"
require_file "src/self_hosted/lsp/expected/request_dispatch.json"
require_file "src/self_hosted/lsp/expected/request_dispatch_missing_method.json"
require_file "src/self_hosted/lsp/expected/response_emission.json"
require_file "src/self_hosted/lsp/expected/response_emission_feature.json"
require_file "src/self_hosted/lsp/expected/response_emission_unsupported.json"
require_file "src/self_hosted/lsp/expected/session_replay.json"
require_file "src/self_hosted/lsp/expected/session_replay_feature.json"
require_file "src/self_hosted/lsp/expected/session_replay_unsupported.json"
require_file "src/self_hosted/lsp/expected/session_state.json"
require_file "src/self_hosted/lsp/expected/hover_content.json"
require_file "src/self_hosted/lsp/expected/document_store.json"
require_file "src/self_hosted/lsp/expected/document_store_multi_uri.json"
require_text "src/self_hosted/lsp/expected/valid_int_return.json" '"diagnostics":[]'
require_text "src/self_hosted/lsp/expected/bad_logical_right.json" '"code":"logical_operand_not_bool"'
require_text "src/self_hosted/lsp/expected/bad_logical_right.json" '"oracleCode":"PGY_SEM_BINOP_TYPE_MISMATCH"'
require_text "src/self_hosted/lsp/expected/bad_undefined_return.json" '"code":"undefined_symbol"'
require_text "src/self_hosted/lsp/expected/bad_undefined_return.json" '"oracleCode":"PGY_SEM_UNDEFINED_SYMBOL"'
require_text "src/self_hosted/lsp/expected/bad_return_type.json" '"code":"return_type_mismatch"'
require_text "src/self_hosted/lsp/expected/bad_return_type.json" '"oracleCode":"PGY_SEM_TYPE_MISMATCH"'
require_text "src/self_hosted/lsp/expected/bad_while_condition.json" '"code":"condition_not_bool"'
require_text "src/self_hosted/lsp/expected/bad_while_condition.json" '"oracleCode":"PGY_SEM_TYPE_MISMATCH"'
require_text "src/self_hosted/lsp/expected/bad_not_operand.json" '"code":"not_operand_not_bool"'
require_text "src/self_hosted/lsp/expected/bad_not_operand.json" '"oracleCode":"PGY_SEM_UNOP_TYPE_MISMATCH"'
require_text "src/self_hosted/lsp/expected/squiggle_policy.json" '"class":"red"'
require_text "src/self_hosted/lsp/expected/squiggle_policy.json" '"class":"amber"'
require_text "src/self_hosted/lsp/expected/squiggle_policy.json" '"class":"blue"'
require_text "src/self_hosted/lsp/expected/squiggle_policy.json" '"class":"violet"'
require_text "src/self_hosted/lsp/expected/transport_frame.json" '"schema":"pgy.selfhost.lsp-transport-frame.v1"'
require_text "src/self_hosted/lsp/expected/transport_frame.json" '"ok":true'
require_text "src/self_hosted/lsp/expected/transport_frame_incomplete.json" '"reason":"body_incomplete"'
require_text "src/self_hosted/lsp/expected/transport_stream.json" '"schema":"pgy.selfhost.lsp-transport-stream.v1"'
require_text "src/self_hosted/lsp/expected/transport_stream.json" '"frameCount":2'
require_text "src/self_hosted/lsp/expected/transport_stream_partial.json" '"partialReason":"body_incomplete"'
require_text "src/self_hosted/lsp/expected/request_dispatch.json" '"schema":"pgy.selfhost.lsp-request-dispatch-stream.v1"'
require_text "src/self_hosted/lsp/expected/request_dispatch.json" '"method":"initialize"'
require_text "src/self_hosted/lsp/expected/request_dispatch_missing_method.json" '"reason":"missing_method"'
require_text "src/self_hosted/lsp/expected/response_emission.json" '"schema":"pgy.selfhost.lsp-response-emission-stream.v1"'
require_text "src/self_hosted/lsp/expected/response_emission.json" '"method":"initialize"'
require_text "src/self_hosted/lsp/expected/response_emission_feature.json" '"method":"textDocument/hover"'
require_text "src/self_hosted/lsp/expected/response_emission_feature.json" '\"result\":null'
require_text "src/self_hosted/lsp/expected/response_emission_feature.json" '\"items\":[]'
require_text "src/self_hosted/lsp/expected/response_emission_unsupported.json" '"reason":"unsupported_response"'
require_text "src/self_hosted/lsp/expected/response_emission_unsupported.json" '"method":"textDocument/semanticTokens/full"'
require_text "src/self_hosted/lsp/expected/session_replay.json" '"schema":"pgy.selfhost.lsp-session-replay.v1"'
require_text "src/self_hosted/lsp/expected/session_replay.json" '"wire":"Content-Length: 490'
require_text "src/self_hosted/lsp/expected/session_replay_feature.json" '\"result\":null'
require_text "src/self_hosted/lsp/expected/session_replay_feature.json" '\"items\":[]'
require_text "src/self_hosted/lsp/expected/session_replay_unsupported.json" '"errors":1'
require_text "src/self_hosted/lsp/expected/session_state.json" '"schema":"pgy.selfhost.lsp-session-state.v1"'
require_text "src/self_hosted/lsp/expected/session_state.json" '"documentCount":1'
require_text "src/self_hosted/lsp/expected/session_state.json" '\"result\":null'
require_text "src/self_hosted/lsp/expected/hover_content.json" '"schema":"pgy.selfhost.lsp-hover-content.v1"'
require_text "src/self_hosted/lsp/expected/hover_content.json" '"hoverCount":2'
require_text "src/self_hosted/lsp/expected/hover_content.json" '"**func** - Function declaration"'
require_text "src/self_hosted/lsp/expected/hover_content.json" '"reason":"no_hover"'
require_text "src/self_hosted/lsp/expected/document_store.json" '"schema":"pgy.selfhost.lsp-document-store.v1"'
require_text "src/self_hosted/lsp/expected/document_store.json" '"finalText":"Log(2);"'
require_text "src/self_hosted/lsp/expected/document_store.json" '"documentCount":1'
require_text "src/self_hosted/lsp/expected/document_store_multi_uri.json" '"documentCount":2'
require_text "src/self_hosted/lsp/expected/document_store_multi_uri.json" '"uri":"file:///a.pgy"'
require_text "src/self_hosted/lsp/expected/document_store_multi_uri.json" '"uri":"file:///b.pgy"'
require_text "Makefile" "self-host-lsp-transport-frame-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_transport_frame_parity.sh"
require_text "tests/self_hosted/parity/lsp_transport_frame_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_transport_frame_parity.sh" '"lsp_transport_frame"'
require_text "tests/self_hosted/parity/lsp_transport_frame_parity.sh" '"lsp-transport-frame-paths"'
require_text "Makefile" "self-host-lsp-transport-stream-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_transport_stream_parity.sh"
require_text "tests/self_hosted/parity/lsp_transport_stream_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_transport_stream_parity.sh" '"lsp_transport_stream"'
require_text "tests/self_hosted/parity/lsp_transport_stream_parity.sh" '"lsp-transport-stream-paths"'
require_text "Makefile" "self-host-lsp-request-dispatch-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_request_dispatch_parity.sh"
require_text "tests/self_hosted/parity/lsp_request_dispatch_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_request_dispatch_parity.sh" '"lsp_request_dispatch"'
require_text "tests/self_hosted/parity/lsp_request_dispatch_parity.sh" '"lsp-request-dispatch-paths"'
require_text "Makefile" "self-host-lsp-response-emission-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_response_emission_parity.sh"
require_text "tests/self_hosted/parity/lsp_response_emission_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_response_emission_parity.sh" '"lsp_response_emission"'
require_text "tests/self_hosted/parity/lsp_response_emission_parity.sh" '"lsp-response-emission-paths"'
require_text "Makefile" "self-host-lsp-session-replay-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_session_replay_parity.sh"
require_text "tests/self_hosted/parity/lsp_session_replay_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_session_replay_parity.sh" '"lsp_session_replay"'
require_text "tests/self_hosted/parity/lsp_session_replay_parity.sh" '"lsp-session-replay-paths"'
require_text "Makefile" "self-host-lsp-document-store-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_document_store_parity.sh"
require_text "tests/self_hosted/parity/lsp_document_store_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_document_store_parity.sh" '"lsp_document_store"'
require_text "tests/self_hosted/parity/lsp_document_store_parity.sh" '"lsp-document-store-paths"'
require_text "Makefile" "self-host-lsp-session-state-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_session_state_parity.sh"
require_text "tests/self_hosted/parity/lsp_session_state_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_session_state_parity.sh" '"lsp_session_state"'
require_text "tests/self_hosted/parity/lsp_session_state_parity.sh" '"lsp-session-state-paths"'
require_text "Makefile" "self-host-lsp-hover-content-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/lsp_hover_content_parity.sh"
require_text "tests/self_hosted/parity/lsp_hover_content_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_hover_content_parity.sh" '"lsp_hover_content"'
require_text "tests/self_hosted/parity/lsp_hover_content_parity.sh" '"lsp-hover-content-paths"'
require_text "Makefile" "self-host-lsp-diagnostics-parity-test-smoke"
require_text "Makefile" 'self-host-lsp-diagnostics-parity-test-smoke: $(PGY) $(PGY_LSP)'
require_text "Makefile" 'self-host-preparation-parity-test-smoke: $(PGY) $(PGY_LSP)'
require_text "Makefile" 'PGY_LSP_BIN="$(abspath $(PGY_LSP))"'
require_text "src/lsp/pgy_lsp.c" "--dump-diagnostics"
require_text "src/lsp/pgy_lsp_diagnostics.c" "lsp_build_diagnostics_params"
require_text "src/lsp/pgy_lsp_internal.h" "lsp_build_diagnostics_params"
require_text "Makefile" "tests/self_hosted/parity/lsp_diagnostics_parity.sh"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "lsp-diagnostics"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" '"lsp-diagnostics-paths"'
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" '"lsp_diagnostics"'
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "PGY_LSP_BIN"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "--dump-diagnostics"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "PGY_SEM_BINOP_TYPE_MISMATCH"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "semantic:binop:operand_types"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "PGY_SEM_UNDEFINED_SYMBOL"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "semantic:symbol:undefined"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "lsp_canonical_event_artifact"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "lsp-diagnostics:normalized"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "event=logical_operand_not_bool"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "event=undefined_symbol"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "pgy_reject_wsl_windows_pgy_parity_mix"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" "--squiggle-policy"
require_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" '"class":"violet"'
reject_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/lsp_diagnostics_parity.sh" 'diff -u'
for lsp_script in \
    tests/self_hosted/parity/lsp_diagnostics_parity.sh \
    tests/self_hosted/parity/lsp_transport_frame_parity.sh \
    tests/self_hosted/parity/lsp_transport_stream_parity.sh \
    tests/self_hosted/parity/lsp_request_dispatch_parity.sh \
    tests/self_hosted/parity/lsp_response_emission_parity.sh \
    tests/self_hosted/parity/lsp_session_replay_parity.sh \
    tests/self_hosted/parity/lsp_document_store_parity.sh \
    tests/self_hosted/parity/lsp_session_state_parity.sh \
    tests/self_hosted/parity/lsp_hover_content_parity.sh; do
    require_text "$lsp_script" "pgy_selfhost_read_test_harness_manifest"
    reject_text "$lsp_script" 'LSP_SOURCE="$ROOT_DIR/src/self_hosted/lsp/main.pgy"'
    reject_text "$lsp_script" '$ROOT_DIR/src/self_hosted/lsp/expected/'
done
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:fuzz-generator" "$PGY"'
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" '"fuzz-backend-generator-paths"'
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" "pgy_selfhost_compare_expected_text_artifact_file_with_owner"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" '"emitted_self_hosted"'
reject_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'TOOL_SOURCE="$ROOT_DIR/src/self_hosted/fuzz/backend_parity_generator/main.pgy"'
reject_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'diff -u'
reject_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'show_diff'
reject_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'files_equal'
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" '"lexer-parity-paths"'
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'SCALE_LIMIT="${PGY_SCALE_PROBE_LIMIT:-20}"'
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" "--full"
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" "--limit=*"
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" "artifact_files_equal"
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" "run_output"
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
reject_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/lexer/main.pgy"'
reject_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/lexer_scale_probe.sh" 'cp "$ROOT_DIR/src/self_hosted/lexer/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/"'
require_text "tests/self_hosted/parity/parser_scale_probe.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/parser_scale_probe.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/parser_scale_probe.sh" '"parser-parity-paths"'
require_text "tests/self_hosted/parity/parser_scale_probe.sh" 'SCALE_LIMIT="${PGY_SCALE_PROBE_LIMIT:-20}"'
require_text "tests/self_hosted/parity/parser_scale_probe.sh" "--full"
require_text "tests/self_hosted/parity/parser_scale_probe.sh" "--limit=*"
require_text "tests/self_hosted/parity/parser_scale_probe.sh" "artifact_files_equal"
require_text "tests/self_hosted/parity/parser_scale_probe.sh" "ast_text"
require_text "tests/self_hosted/parity/parser_scale_probe.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/parser_scale_probe.sh" 'PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"'
require_text "tests/self_hosted/parity/parser_scale_probe.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"'
reject_text "tests/self_hosted/parity/parser_scale_probe.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/parser_scale_probe.sh" 'PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"'
reject_text "tests/self_hosted/parity/parser_scale_probe.sh" 'PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"'
reject_text "tests/self_hosted/parity/parser_scale_probe.sh" 'cp "$ROOT_DIR/src/self_hosted/parser/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/"'
reject_text "tests/self_hosted/parity/parser_scale_probe.sh" 'LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"'
reject_text "tests/self_hosted/parity/parser_scale_probe.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"'
require_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" '"mir-json-parity-paths"'
require_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" 'COVERAGE_LIMIT="${PGY_MIR_COVERAGE_LIMIT:-0}"'
require_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" "--limit=*"
require_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" 'MIR_LOWER_SRC="$ROOT_DIR/${harness_paths[0]}"'
require_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" 'CODEGEN_SRC="$ROOT_DIR/${harness_paths[1]}"'
reject_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" 'MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"'
reject_text "tests/self_hosted/parity/mir_json_coverage_probe.sh" 'CODEGEN_SRC="$ROOT_DIR/src/self_hosted/codegen/main.pgy"'
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" '| Artifact Zone evidence | `src/self_hosted/compiler/artifact_zone_owner.pgy`, `ArtifactZone` | `self-host-component-contract-test-smoke`, parity artifact gates |'
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Consumer parity scripts must not recompute artifact equality in shell"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "expected-JSON bootstrap comparison plus mismatch and missing-input fixtures"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "backend_output_comparator_parity.sh now consumes its source, expected JSON, and comparable artifact paths from TestHarness"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "lexer_parity.sh now consumes its lexer source, backend comparator source, and lexer fixture directory from TestHarness"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "When a surface is ready only for the current self-host C subset"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" 'one `READY` row with the subset scope'
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" '`ACTIVE` row with the global scope'
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" '| Target capability envelope (self-host C subset) |'
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" '| Target capability envelope (native/global consumers) |'
require_text "Makefile" "clean-scratch:"
require_text "Makefile" "'\$(PROJECT_ROOT)'/.tmp"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "compile_c_artifact_with_bounded_log"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "PGY_SELFHOST_CC_LOG_LIMIT_BYTES"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" 'removes the ignored `.tmp` scratch zone'
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "single evidence log into a multi-hundred-megabyte artifact"
reject_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "It remains active until all parity artifacts are written and compared from this owner."
reject_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "AIR evidence, Artifact Zone, and TestHarness"
reject_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" 'Native C/LLVM target-specific consumers still need to read the same envelope before this surface can leave `ACTIVE`'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "expected-json clean"
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "Shell text equivalence is the parity backend"
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "Shell drift detector"
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "shell drift detector disagrees"
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'EXPECTED_CONTENT_RAW="$(<"$FIXTURE_EXPECTED")"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'ACTUAL_CONTENT_RAW="$(<"$FIXTURE_ACTUAL")"'
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "EXPECTED_CONTENT_NORM"
reject_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" "ACTUAL_CONTENT_NORM"
reject_text "tests/self_hosted/parity/codegen_parity.sh" "MINGW BYPASS"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe" "${run_args[@]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$run_raw" "$run_err" "$self_exe" "${run_args[@]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" "compile_backend_output_comparator"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'compare_run_output_with_owner "$backend" "$base" "$expected_file" "$run_norm" 2'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_generated_secure_open_probe "$backend" "$base" "$self_exe"'
require_text "tests/self_hosted/parity/codegen_parity.sh" "generated_secure_open_probe_supported"
require_text "tests/self_hosted/parity/codegen_parity.sh" "generated C followed a symlink write target"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'compare_run_output_with_owner "c-oracle" "$base" "$expected_file" "$oracle_norm" 0'
require_text "tests/self_hosted/parity/codegen_parity.sh" '"$expected_rel" "$actual_rel" 0 "$actual_projection"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" '--ast "$(pgy_path_for_compiler "$PGY" "$src")"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'self_out="$(cat "$run_norm")"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'expected_norm="$(tr -d'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'oracle_out="$(tr -d'

while IFS= read -r fixture; do
    base="$(basename "$fixture" .pgy)"
    require_file "src/self_hosted/codegen/expected/${base}_stdout.txt"
done < <(find "$SELF_HOST_DIR/codegen/fixture" -maxdepth 1 -type f -name '*.pgy' | sort)

while IFS= read -r expected; do
    name="$(basename "$expected")"
    base="${name%_stdout.txt}"
    [[ "$name" != "$base" ]] || fail "codegen expected must end with _stdout.txt: $name"
    require_file "src/self_hosted/codegen/fixture/${base}.pgy"
done < <(find "$SELF_HOST_DIR/codegen/expected" -maxdepth 1 -type f -name '*_stdout.txt' | sort)

echo "[self-host-component-contract] compiler-stage contracts ok"

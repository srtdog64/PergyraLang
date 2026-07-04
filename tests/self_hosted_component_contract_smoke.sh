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
reject_text "src/self_hosted/lib/json_fact_table.pgy" "let keys: Array<String>;"
reject_text "src/self_hosted/lib/json_fact_table.pgy" "let value_starts: Array<Int>;"
reject_text "src/self_hosted/lib/json.pgy" 'import "json_emit.pgy";'
require_text "src/self_hosted/lib/json.pgy" 'import "json_scan.pgy";'
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
require_text "src/self_hosted/lib/json.pgy" "func JsonDocumentStringFieldEquals"
require_text "src/self_hosted/lib/json.pgy" "func JsonDocumentHasField"
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
require_text "Makefile" "self-host-mir-json-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/mir_json_parity.sh"
require_entrypoint_only_main "mir_lower"
require_stage_owner_line_cap "mir_lower"

require_stage_world_binding "lexer" "TokenStreamZone" "LexerStage" "LexSource" "FrontendPipeline" "LexerTokenPayloadContractReady"
require_stage_world_binding "parser" "AstTreeZone" "ParserStage" "ParseTokens" "FrontendPipeline" "ParserAstTreePayloadContractReady"
require_stage_world_binding "semantic" "SemanticVerdictZone" "SemanticStage" "CheckProgramSemantics" "MiddleEndPipeline" "SemanticVerdictPayloadContractReady"
require_stage_world_binding "mir_lower" "MirFactGraphZone" "MirLowerStage" "LowerProgramFacts" "MiddleEndPipeline" "MirFactGraphPayloadContractReady"
require_stage_world_binding "codegen" "EmissionZone" "ProgramEmitter" "EmitProgramArtifact" "BackendPipeline" "TypedAstArenaPayloadContractReady"

mir_positive_count="$(
    {
        extract_shell_array_items "$PARITY_DIR/mir_json_parity.sh" MIR_FIXTURES
        extract_shell_array_items "$PARITY_DIR/mir_json_parity.sh" CODEGEN_FIXTURES
        extract_shell_array_items "$PARITY_DIR/mir_json_parity.sh" EXAMPLE_FIXTURES
    } | wc -l | tr -d ' '
)"
[[ "$mir_positive_count" -eq 86 ]] ||
    fail "mir_json_parity positive fixture count drifted: $mir_positive_count != 86"
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
    "scan_owner.pgy" \
    "source_input_owner.pgy"
require_file "src/self_hosted/lexer/char_owner.pgy"
require_file "src/self_hosted/lexer/token_owner.pgy"
require_text "src/self_hosted/lexer/scan_owner.pgy" 'import "char_owner.pgy";'
require_text "src/self_hosted/lexer/scan_owner.pgy" 'import "token_owner.pgy";'
require_text "src/self_hosted/lexer/token_owner.pgy" "func LexerTokenPayloadContractReady"
require_text "src/self_hosted/lexer/token_owner.pgy" "func LexerTokenPayloadSchema"
require_text "src/self_hosted/lexer/token_owner.pgy" "pgy.selfhost.lexer-token-stream.v1"
require_text "src/self_hosted/lexer/token_owner.pgy" "LexerTokenPayloadFixtureCount() != 7"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../lexer/token_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "LexerTokenPayloadContractReady()"
reject_text "src/self_hosted/lexer/main.pgy" 'import "char_owner.pgy";'
reject_text "src/self_hosted/lexer/main.pgy" 'import "token_owner.pgy";'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
require_text "tests/self_hosted/parity/lexer_parity.sh" "normalize_text_artifact"
require_text "tests/self_hosted/parity/lexer_parity.sh" "compile_backend_output_comparator"
require_text "tests/self_hosted/parity/lexer_parity.sh" 'compare_lexer_output_with_owner "c" "$label" "$expected_file" "$c_out" 2'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'compare_lexer_output_with_owner "llvm" "$label" "$expected_file" "$llvm_out" 2'
require_text "tests/self_hosted/parity/lexer_parity.sh" 'compare_lexer_output_with_owner "live-tokens" "$label" "$expected_file" "$live_out" 0'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'EXPECTED_OUT="$(tr -d'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'PERGYRA_OUT="$(cd'
reject_text "tests/self_hosted/parity/lexer_parity.sh" 'LLVM_LEX_OUT="$(cd'
reject_text "tests/self_hosted/parity/lexer_parity.sh" "diff <(printf"
require_owner_surface parser \
    "source_path_owner.pgy" \
    "program_parse_owner.pgy"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func ParserAstTreePayloadContractReady"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func ParserAstTreePayloadSchema"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "pgy.selfhost.parser-ast-tree.v1"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "ParserAstTreePayloadFixtureCount() != 186"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../parser/tree_text_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "ParserAstTreePayloadContractReady()"
require_text "src/self_hosted/parser/cursor_owner.pgy" 'import "error_owner.pgy";'
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
require_text "tests/self_hosted/parity/parser_parity.sh" "import_dedup_graph"
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
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SemanticDiagnosticCodeCount() != 17"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../semantic/diagnostic_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "SemanticVerdictPayloadContractReady()"
require_text "tests/self_hosted/parity/semantic_parity.sh" "check_semantic_diagnostic_code_surface"
require_text "tests/self_hosted/parity/semantic_parity.sh" "semantic_oracle_code_for"
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
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolCQualifiedName"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "func CompilerSymbolRequireTable"
require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "CompilerSymbolRequireTable();"
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
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'if index == 9 { return "Option<String>"; }'
require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" 'if index == 9 { return "pgy_option_string"; }'
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
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let f_indent: Int = nodes[first_fn].indent"
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
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'LookupKindType(env, ident, "pm")'
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
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCArgsFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCExitFn"
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
require_text "tests/self_hosted/parity/codegen_parity.sh" "option_string_core"
require_text "tests/self_hosted/parity/codegen_parity.sh" "option_try"
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
    "error_owner.pgy" \
    "mir_fact_graph_contract_owner.pgy" \
    "mir_json_input_owner.pgy" \
    "json_fact_read.pgy" \
    "stmt_render.pgy" \
    "routine_inventory_owner.pgy" \
    "routine_lower.pgy" \
    "decl_lower.pgy" \
    "program_lower.pgy"
require_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'import "../lib/json.pgy";'
require_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'JsonDocumentStringFieldEquals(json, "schema", "pgy.mir.v1")'
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "func MirFactGraphPayloadContractReady"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "func MirFactGraphPayloadSchema"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "pgy.mir.v1"
require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirFactGraphPayloadFixtureCount() != 86"
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" 'import "../mir_lower/mir_fact_graph_contract_owner.pgy";'
require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "MirFactGraphPayloadContractReady()"
reject_text "src/self_hosted/mir_lower/mir_json_input_owner.pgy" 'StringIndexOf(json, "\"schema\":\"pgy.mir.v1\"")'
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

require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lexer/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lexer/scan_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/path_manifest_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/stage_intents.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/target_capability_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/air_evidence_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/artifact_zone_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/test_harness_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/subprocess_runner_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/abi_layout_row_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/symbol_table_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/stage_artifact_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/world.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/emission/expr_rewrite.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/emission/function_emit.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/emission/program_emit.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/emission/stmt_emit.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/emission/struct_value_emit.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/run/codegen_run_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/input/ast_usage_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/typed_ast_node_skeleton.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/input/ast_text_inventory_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/input/ast_text_statement_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/text/text_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/type_facts/type_env.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/json_fact_read.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/mir_json_input_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/routine_inventory_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/decl_lower.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/stmt_render.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/routine_lower.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/program_lower.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/sea/execution_lane.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/path.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/json.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/json_emit.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/json_scan.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/json_fact_table.pgy"'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" "JsonStringLiteral(path)"
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
require_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" "JsonDocumentStringFieldEquals(content, \"schema\", \"pgy.air.graph.v1\")"
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
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "CompilerAirEvidenceEnvelopeReady()"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "RequiredGraphFeatureKeys()"
require_text "src/self_hosted/tools/air_graph_json_validator/run_owner.pgy" "CountMissingGraphFeatureKeys(content, feature_keys)"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "AIR_EVIDENCE_OWNER"
require_text "tests/self_hosted/parity/air_graph_json_validator_parity.sh" "compiler/air_evidence_owner.pgy"
reject_text "src/self_hosted/tools/air_graph_json_validator/scan_owner.pgy" 'StringContains(content, "\"schema\":\"pgy.air.graph.v1\"")'
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
    require_text "$air_graph_parity" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"'
    require_text "$air_graph_parity" 'cp "$AIR_GRAPH_SCAN_OWNER" "$AIR_SCAN_BUILD_DIR/scan_owner.pgy"'
done
require_make_target_recipe_line \
    "self-host-air-graph-consumer-parity-test-smoke" \
    'PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_json_validator_parity.sh'
require_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/ast_read_surface_checker/main.pgy" 'let json_parts: Array<String>'
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
reject_text "src/self_hosted/tools/stable_subset_section_checker/main.pgy" 'let json_parts: Array<String>'
require_text "tests/self_hosted/parity/stable_subset_section_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../compiler/artifact_zone_owner.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../compiler/test_harness_owner.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'import "../../compiler/subprocess_runner_owner.pgy";'
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerArtifactKindAt(6)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "let args: Array<String> = Args();"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "expected_path = args[0];"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "actual_path = args[1];"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessProjectionOrExit(ToInt(args[2]))"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessProjectionOrExit(ToInt(args[3]))"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessComparableArtifactPathAt(0)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessComparableArtifactPathAt(1)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerHarnessFindingCap()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessUseCaseAt(0)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessSchema()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessOracleCompareTimeoutMs()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "CompilerSubprocessOracleCompareEnvAllowlist()"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/backend_output_comparator/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'let json_parts: Array<String>'
reject_text "src/self_hosted/tools/backend_output_comparator/main.pgy" 'src/self_hosted/tools/backend_output_comparator/fixture/'
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessProjectionIndexKnown"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessProjectionOrExit"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessComparableArtifactPathAt"
require_text "src/self_hosted/compiler/test_harness_owner.pgy" "func CompilerHarnessFindingCap"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareTimeoutMs"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleCompareEnvAllowlist"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "func CompilerSubprocessOracleComparePlanReady"
require_text "src/self_hosted/tools/backend_output_comparator/expected/clean.json" '"subprocess_timeout_ms":"30000"'
require_text "src/self_hosted/tools/backend_output_comparator/expected/clean.json" '"subprocess_env_allowlist":"PATH,PGY_BIN,PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS,PGY_SELFHOST_BUILD_DIR"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/artifact_zone_owner.pgy"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_owner.pgy"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/subprocess_runner_owner.pgy"'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" '"$ARG_BIN" "$ARG_EXPECTED_PATH" "$ARG_ACTUAL_PATH" 0 2'
require_text "tests/self_hosted/parity/backend_output_comparator_parity.sh" '"actual_projection":"self_hosted"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/artifact_zone_owner.pgy"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_owner.pgy"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/compiler/subprocess_runner_owner.pgy"'
require_text "tests/self_hosted/parity/backend_output_tri_compare_parity.sh" 'run_native_bin "$tri_bin" "$tri_stdout" "$tri_stderr" "$expected_arg" "$actual_arg" 0 1'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_compile_backend_output_comparator"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "assert_llvm_leg_with_artifact_owner"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "comparator artifact path escapes repo root"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "comparator artifact path must be repo-relative"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "backend_output_comparator_\$\$.exe"
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'assert_llvm_leg_with_artifact_owner "$label" "$build_dir" "$c_out" "$llvm_out"'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '"$comparator_bin" "$c_rel" "$llvm_rel" 0 1'
require_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "artifact-equal"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" ".tmp/self_hosted/shared"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" "pgy_selfhost_comparator_needs_rebuild"
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'diff <(printf'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'cmp -s'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" '$(cat "$c_out")'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'c_out="$(cd "$ROOT_DIR"'
reject_text "tests/self_hosted/parity/llvm_leg_helpers.sh" 'llvm_out="$(cd "$ROOT_DIR"'
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/doc_link_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/doc_link_checker/main.pgy" 'let json_parts: Array<String>'
require_text "tests/self_hosted/parity/doc_link_checker_parity.sh" 'cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy'
require_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/examples_inventory_checker/main.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" 'import "../../lib/json_fact_table.pgy";'
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonDocumentObjectFactTable(content)"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonObjectFactArrayObjectTable(root_facts, \"modules\")"
require_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectFactTableReady(modules_facts)"
require_text "tests/self_hosted/parity/module_manifest_resolver_parity.sh" "nested-modules fixture"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "let doc_end_opt: Option<Int> = JsonDocumentObjectEnd(content)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonFieldArrayBounds(content, 0, UnwrapOption(doc_end_opt), \"modules\", module_bounds)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectCount(content, modules_open, modules_end)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectBoundsAt("
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonObjectHasField(content, object_bounds[0], object_bounds[1], \"layer\")"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectBoolFieldEqualsCount(content, modules_open, modules_end, \"beta_blocker\", true)"
reject_text "src/self_hosted/tools/module_manifest_resolver/main.pgy" "JsonArrayObjectStringFieldEqualsCount(content, modules_open, modules_end, \"status\", \"stable-subset\")"
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
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/source_bundle_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/body_check_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/call_check_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/expr_type_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/expr_validation_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/program_check_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/semantic_run_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/expr_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/function_decl_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/decl_dispatch_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/program_parse_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/stmt_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/parser/tree_text_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/mir_lower/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/fuzz/backend_parity_generator/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy"'
require_text "Makefile" "self-host-semantic-selfcheck-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/selfcheck_sources.sh"
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" "lexer_selfcheck_unit"
reject_text "tests/self_hosted/parity/selfcheck_sources.sh" "grep -h -v '^import '"
reject_text "src/self_hosted/lexer/main.pgy" "fixture/source.txt"
selfcheck_items="$(extract_shell_array_items "$PARITY_DIR/selfcheck_sources.sh" SELF_SOURCES)"
selfcheck_count="$(printf '%s\n' "$selfcheck_items" | sed '/^$/d' | wc -l | tr -d ' ')"
[[ "$selfcheck_count" -eq 110 ]] ||
    fail "real-source selfcheck count drifted: $selfcheck_count != 110"

require_text "src/self_hosted/mir_lower/json_fact_read.pgy" 'import "../lib/json.pgy";'
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" 'import "../lib/json_fact_table.pgy";'
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func MirFactObjectStart"
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

semantic_items="$(extract_shell_array_items "$PARITY_DIR/semantic_parity.sh" SOURCE_PAIRS | sed 's/:.*//')"
[[ -n "$semantic_items" ]] || fail "semantic parity SOURCE_PAIRS is empty"
semantic_count="$(printf '%s\n' "$semantic_items" | sed '/^$/d' | wc -l | tr -d ' ')"
[[ "$semantic_count" -eq 108 ]] ||
    fail "semantic parity fixture count drifted: $semantic_count != 108"
require_text "src/self_hosted/PROGRESS.md" "across 108 fixtures"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_scalar_math_builtins"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_string_plus"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_string_scalar_plus"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_bool_arith"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_seedrandom_builtin"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_writefile_builtin"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_let_mut_reassign"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_generated_source_string_literal"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_scalar_utility_int"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_option_unwrap_payload"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_option_try_payload"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_option_payload_return"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_option_payload_let"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_issome_non_option"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_unwrap_non_option"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_option_none_call"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_issome_none_literal"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_issome_none_call"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_unwrap_none_literal"
require_text "tests/self_hosted/parity/semantic_parity.sh" "bad_unwrap_none_call"
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
require_text "src/self_hosted/semantic/program_check_owner.pgy" 'ArrayPush(func_names, "Print")'
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func SkipLineComment"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func SkipBlockComment"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func FindMatchingBraceWithin"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "func ExpectLiteral(content: String, start: Int, tok: String) -> Option<Int>"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "return None"
require_text "src/self_hosted/semantic/text_scan_owner.pgy" "return Some(i + tl)"
reject_text "src/self_hosted/semantic/text_scan_owner.pgy" "return -1"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_comment_brace_scope"

while IFS= read -r fixture; do
    base="$(basename "$fixture" .pgy)"
    contains_line "$semantic_items" "$base" ||
        fail "semantic fixture not listed in SOURCE_PAIRS: $base"
    require_file "src/self_hosted/semantic/expected/${base}.diag"
done < <(find "$SELF_HOST_DIR/semantic/fixture" -maxdepth 1 -type f -name '*.pgy' | sort)

while IFS= read -r expected; do
    base="$(basename "$expected" .diag)"
    contains_line "$semantic_items" "$base" ||
        fail "semantic expected not listed in SOURCE_PAIRS: $base"
    require_file "src/self_hosted/semantic/fixture/${base}.pgy"
done < <(find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.diag' | sort)

if find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.json' -print -quit | grep -q .; then
    fail "semantic verdict fixtures must stay diagnostic blocks (*.diag), not JSON"
fi

codegen_items="$(extract_shell_array_items "$PARITY_DIR/codegen_parity.sh" FIXTURES)"
[[ -n "$codegen_items" ]] || fail "codegen parity FIXTURES is empty"
contains_line "$codegen_items" "hello" ||
    fail "codegen no-argument golden fixture must stay listed: hello"
contains_line "$codegen_items" "seed_random" ||
    fail "codegen SeedRandom replay fixture must stay listed: seed_random"
contains_line "$codegen_items" "array_index_assign" ||
    fail "codegen indexed array assignment fixture must stay listed: array_index_assign"
contains_line "$codegen_items" "string_array_index_return" ||
    fail "codegen Array<String> index return fixture must stay listed: string_array_index_return"
require_text "src/self_hosted/codegen/README.md" "Golden/platform contract"
require_text "src/self_hosted/codegen/README.md" "PGY_SELFHOST_CODEGEN_BACKENDS=c"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture()'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'pgy_binary_is_runnable_here "$bin"'
require_text "tests/pgy_binary_path_helpers.sh" "pgy_reject_wsl_windows_pgy_parity_mix()"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:codegen" "$PGY"'
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" 'pgy_reject_wsl_windows_pgy_parity_mix "self-host-bootstrap" "$PGY"'
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" 'pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:fuzz-generator" "$PGY"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" "MINGW BYPASS"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe" "${run_args[@]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$run_raw" "$run_err" "$self_exe" "${run_args[@]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'COMPARATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"'
require_text "tests/self_hosted/parity/codegen_parity.sh" "compile_backend_output_comparator"
require_text "tests/self_hosted/parity/codegen_parity.sh" 'compare_run_output_with_owner "$backend" "$base" "$expected_file" "$run_norm" 2'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'compare_run_output_with_owner "c-oracle" "$base" "$expected_file" "$oracle_norm" 0'
require_text "tests/self_hosted/parity/codegen_parity.sh" '"$expected_rel" "$actual_rel" 0 "$actual_projection"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'self_out="$(cat "$run_norm")"'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'expected_norm="$(tr -d'
reject_text "tests/self_hosted/parity/codegen_parity.sh" 'oracle_out="$(tr -d'

while IFS= read -r fixture; do
    base="$(basename "$fixture" .pgy)"
    contains_line "$codegen_items" "$base" ||
        fail "codegen fixture not listed in FIXTURES: $base"
    require_file "src/self_hosted/codegen/expected/${base}_stdout.txt"
done < <(find "$SELF_HOST_DIR/codegen/fixture" -maxdepth 1 -type f -name '*.pgy' | sort)

while IFS= read -r expected; do
    name="$(basename "$expected")"
    base="${name%_stdout.txt}"
    [[ "$name" != "$base" ]] || fail "codegen expected must end with _stdout.txt: $name"
    contains_line "$codegen_items" "$base" ||
        fail "codegen expected not listed in FIXTURES: $base"
    require_file "src/self_hosted/codegen/fixture/${base}.pgy"
done < <(find "$SELF_HOST_DIR/codegen/expected" -maxdepth 1 -type f -name '*_stdout.txt' | sort)

echo "[self-host-component-contract] compiler-stage contracts ok"

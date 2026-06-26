#!/usr/bin/env bash
# Gates compiler-stage self-host substitute contracts.
#
# Heavy parity scripts prove behavior. This smoke proves the self-hosted
# compiler-stage surface itself is wired correctly: each active stage has an
# intent-verification pair, a Makefile target, and fixture/expected files that
# are actually listed by its parity harness.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
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

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

reject_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel must not contain retired term: $term"
    fi
}

contains_line() {
    local haystack="$1"
    local needle="$2"
    printf '%s\n' "$haystack" | grep -Fxq -- "$needle"
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

line_count() {
    wc -l < "$1" | tr -d ' '
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
require_text "src/self_hosted/lib/json.pgy" "func ReadJsonString"
require_text "src/self_hosted/lib/json.pgy" "func JsonFieldString"
require_text "src/self_hosted/lib/json.pgy" "func JsonFieldNumber"
require_text "src/self_hosted/lib/json.pgy" "func JsonFirstArrayString"
require_text "src/self_hosted/lib/json.pgy" "func JsonEscapeString"
require_text "src/self_hosted/lib/json.pgy" "func JsonStringLiteral"
require_text "src/self_hosted/lib/json.pgy" "func JsonEmitObject"
require_text "src/self_hosted/lib/json.pgy" "func JsonEmitArray"
require_text "src/self_hosted/lib/json.pgy" "func JsonEmitFieldRaw"
require_text "src/self_hosted/lib/json.pgy" "func JsonEmitFieldString"
require_text "src/self_hosted/lib/json.pgy" "func JsonEmitFieldNumber"
require_text "src/self_hosted/lib/json.pgy" "func JsonEmitFieldBool"

for stage in lexer parser semantic codegen; do
    require_dir "src/self_hosted/$stage"
    require_file "src/self_hosted/$stage/main.pgy"
    require_file "src/self_hosted/$stage/README.md"
    require_file "src/self_hosted/$stage/intent.md"
    require_dir "src/self_hosted/$stage/fixture"
    require_dir "src/self_hosted/$stage/expected"
    require_file "tests/self_hosted/parity/${stage}_parity.sh"

    for anchor in '## Intent' '## Input Contract' '## Output Contract' '## Oracle'; do
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
for anchor in '## Intent' '## Input Contract' '## Output Contract' '## Oracle'; do
    require_text "src/self_hosted/mir_lower/intent.md" "$anchor"
done
require_text "tests/self_hosted/parity/mir_json_parity.sh" "set -euo pipefail"
require_text "Makefile" "self-host-mir-json-parity-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/mir_json_parity.sh"
require_entrypoint_only_main "mir_lower"
require_stage_owner_line_cap "mir_lower"

mir_positive_count="$(
    {
        extract_shell_array_items "$PARITY_DIR/mir_json_parity.sh" MIR_FIXTURES
        extract_shell_array_items "$PARITY_DIR/mir_json_parity.sh" CODEGEN_FIXTURES
        extract_shell_array_items "$PARITY_DIR/mir_json_parity.sh" EXAMPLE_FIXTURES
    } | wc -l | tr -d ' '
)"
[[ "$mir_positive_count" -eq 85 ]] ||
    fail "mir_json_parity positive fixture count drifted: $mir_positive_count != 85"
mir_clean_reject_count="$(grep -Ec '^base="unsupported_' "$PARITY_DIR/mir_json_parity.sh" || true)"
[[ "$mir_clean_reject_count" -eq 0 ]] ||
    fail "mir_json_parity clean reject count drifted: $mir_clean_reject_count != 0"
require_text "src/self_hosted/PROGRESS.md" "85 PASS / 0 gap plus 0 clean"
require_text "docs/self_hosted/07_hard_self_host_scorecard.md" "85 PASS / 0 gap plus 0 clean rejects"
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
reject_text "src/self_hosted/lexer/main.pgy" 'import "char_owner.pgy";'
reject_text "src/self_hosted/lexer/main.pgy" 'import "token_owner.pgy";'
require_owner_surface parser \
    "source_path_owner.pgy" \
    "program_parse_owner.pgy"
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
require_text "tests/self_hosted/parity/semantic_parity.sh" "check_semantic_diagnostic_code_surface"
require_text "tests/self_hosted/parity/semantic_parity.sh" "semantic_oracle_code_for"
require_owner_surface codegen \
    "input/ast_input_owner.pgy" \
    "input/ast_text_inventory_owner.pgy" \
    "run/codegen_run_owner.pgy" \
    "text/text_owner.pgy" \
    "type_facts/type_env.pgy" \
    "text/expr_scan.pgy" \
    "symbol_facts/symbol_mangle_owner.pgy" \
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
require_text "src/self_hosted/codegen/symbol_facts/symbol_mangle_owner.pgy" "func SymbolMangleCQualifiedName"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "SymbolMangleCQualifiedName"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'Concat(owner, Concat("_",'
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" 'Concat(owner_name, Concat("_",'
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCParamType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCReturnType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCLocalType"
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "func AbiLayoutCFieldType"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextIndentOf"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "struct CodegenAstTextNode"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextNodeInventory"
require_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextExpectNode"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextInventory(ast: String, inout indents:"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextProjectLegacy"
reject_text "src/self_hosted/codegen/input/ast_text_inventory_owner.pgy" "func CodegenAstTextExpect(texts:"
require_text "src/parser/ast_print.c" "PARAM_MODE_MUT_REF"
require_text "src/parser/ast_print.c" 'printf("inout ")'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'param_mode_prefix = "inout "'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'param_mode_prefix = "ref "'
require_text "src/self_hosted/parser/function_decl_owner.pgy" 'param_mode_prefix = "own "'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextNodeInventory(ast, nodes, node_count_box)"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let main_line: String = nodes[main_scan].text"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let f_indent: Int = nodes[first_fn].indent"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let cur_line: String = nodes[cur[0]].text"
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "let record_array_block: String = \"\""
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "pgy_CodegenAstTextNode_array_get"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "CodegenAstTextInventory(ast, indents, texts)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "texts["
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "indents["
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func BuildFunctionEnv(nodes: Array<CodegenAstTextNode>, count: Int)"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectRoleOperators(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectStructs(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectEnums(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectProtos(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func EmitFunction(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "CodegenAstTextExpectNode(nodes, count, cur, \"Parameters:\")"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "func ParamLineMode"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"=pm:"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" '"_pgy_inout_"'
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "EmitStmtList(nodes, count, stmt_indent, cur, \"    \", env_box, body_ret, copyout)"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func BuildFunctionEnv(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectRoleOperators(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectStructs(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectEnums(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CollectProtos(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func EmitFunction(indents:"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "texts["
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "indents["
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitStmtList(nodes: Array<CodegenAstTextNode>, count: Int"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextExpectNode(nodes, count, cur"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CodegenAstTextExpect(texts"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitStmtList(indents:"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "texts["
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "indents["
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "NextNewline(ast, pos)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "StringTrim(raw_line)"
reject_text "src/self_hosted/codegen/emission/program_emit.pgy" "IndentOf(raw_line)"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func IndentOf"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func ExpectText"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "func EmitCollectionElementValue"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "EmitStructValue(value_expr, elem_type, env)"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "func RewriteInoutCallArgs"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'LookupKindType(env, ident, "pm")'
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" 'rendered = Concat("&", rendered)'
require_text "src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" "pgy_CodegenAstTextNode_array"
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
require_text "src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy" "func MathRuntimeCRandomFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCFileExistsFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "func HostIORuntimeCArgsFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCConcatFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCStringLengthFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCStringJoinFn"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" "func StringRuntimeCLogFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCOptionSomeFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCOptionNoneFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCResultOkFn"
require_text "src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy" "func OptionResultRuntimeCResultUnwrapFn"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "AbiLayoutCParamType"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "AbiLayoutCReturnType"
require_text "src/self_hosted/codegen/emission/function_emit.pgy" "AbiLayoutCFieldType"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "AbiLayoutCLocalType"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "CollectionRuntimeCLenFn"
require_text "src/self_hosted/codegen/text/expr_scan.pgy" "CollectionRuntimeCGetFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CollectionRuntimeCPushFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "CollectionRuntimeCSetFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "CollectionRuntimeCIntSortFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "StringRuntimeCStringLengthFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "StringRuntimeCConcatFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "StringRuntimeCLogFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "MathRuntimeCAbsFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "HostIORuntimeCFileExistsFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "OptionResultRuntimeCOptionSomeFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "OptionResultRuntimeCResultOkFn"
require_text "src/self_hosted/codegen/emission/stmt_emit.pgy" "OptionResultRuntimeCResultIsOkFn"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CParamType"
reject_text "src/self_hosted/codegen/emission/function_emit.pgy" "func CRetType"
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("long long "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("const char* "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("pgy_result_int "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'return Concat("pgy_option_int "'
reject_text "src/self_hosted/codegen/emission/stmt_emit.pgy" 'let arr_c: String = "pgy_ai"'
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
    "mir_json_input_owner.pgy" \
    "json_fact_read.pgy" \
    "stmt_render.pgy" \
    "routine_inventory_owner.pgy" \
    "routine_lower.pgy" \
    "decl_lower.pgy" \
    "program_lower.pgy"
require_text "src/self_hosted/mir_lower/routine_inventory_owner.pgy" "func FindRoutine"
reject_text "src/self_hosted/mir_lower/routine_lower.pgy" "func FindRoutine"

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
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/compiler/world.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/main.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/input/ast_text_inventory_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/symbol_facts/symbol_mangle_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/codegen/text/text_owner.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/path.pgy"'
require_text "tests/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lib/json.pgy"'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/diagnostic_catalog_checker/report_owner.pgy" "JsonStringLiteral(path)"
require_text "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/air_graph_json_validator/report_owner.pgy" "JsonStringLiteral(fixture_path)"
require_text "src/self_hosted/tools/production_c_size_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/production_c_size_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/production_c_size_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/production_c_size_checker/main.pgy" 'let json_parts: Array<String>'
require_text "src/self_hosted/tools/production_header_size_checker/main.pgy" 'import "../../lib/json.pgy";'
require_text "src/self_hosted/tools/production_header_size_checker/main.pgy" "JsonEmitObject(report_fields)"
require_text "src/self_hosted/tools/production_header_size_checker/main.pgy" "JsonEmitArray(findings)"
reject_text "src/self_hosted/tools/production_header_size_checker/main.pgy" 'let json_parts: Array<String>'
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
[[ "$selfcheck_count" -eq 87 ]] ||
    fail "real-source selfcheck count drifted: $selfcheck_count != 87"

require_text "src/self_hosted/mir_lower/json_fact_read.pgy" 'import "../lib/json.pgy";'
require_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func SourceLocalType"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func ReadJsonString"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func JsonFieldString"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func JsonFieldNumber"
reject_text "src/self_hosted/mir_lower/json_fact_read.pgy" "func JsonFirstArrayString"

semantic_items="$(extract_shell_array_items "$PARITY_DIR/semantic_parity.sh" SOURCE_PAIRS | sed 's/:.*//')"
[[ -n "$semantic_items" ]] || fail "semantic parity SOURCE_PAIRS is empty"
semantic_count="$(printf '%s\n' "$semantic_items" | sed '/^$/d' | wc -l | tr -d ' ')"
[[ "$semantic_count" -eq 93 ]] ||
    fail "semantic parity fixture count drifted: $semantic_count != 93"
require_text "src/self_hosted/PROGRESS.md" "across 93 fixtures"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_scalar_math_builtins"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_seedrandom_builtin"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_writefile_builtin"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_let_mut_reassign"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_generated_source_string_literal"
require_text "tests/self_hosted/parity/semantic_parity.sh" "valid_scalar_utility_int"
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
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe" "${run_args[@]}"'
require_text "tests/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$run_raw" "$run_err" "$self_exe" "${run_args[@]}"'

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

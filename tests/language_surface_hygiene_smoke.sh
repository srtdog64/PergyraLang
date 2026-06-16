#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[language-surface-hygiene] $*" >&2
    exit 1
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

for rel in \
    "docs/134_language_surface_hygiene.md" \
    "docs/mut_borrow_parameters.md" \
    "docs/42_keyword_orthogonality.md" \
    "docs/99_language_module_taxonomy.md" \
    "docs/67_layered_stdlib_and_domain_kits.md" \
    "src/self_hosted/tools/linter/main.pgy" \
    "src/parser/ast_types.h" \
    "src/parser/parser_decl.c" \
    "src/parser/parser_async.c" \
    "src/semantic/type_checker_helpers_late.c"; do
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
done

require_text "docs/134_language_surface_hygiene.md" "not to reduce Pergyra's domain vocabulary."
require_text "docs/134_language_surface_hygiene.md" "Keyword count is not the"
require_text "docs/134_language_surface_hygiene.md" "debt; duplicated truth paths are the debt."
require_text "docs/134_language_surface_hygiene.md" "only spelling for value-result mutable parameters"
require_text "docs/134_language_surface_hygiene.md" "rejected because it implies a live borrow"
require_text "docs/134_language_surface_hygiene.md" "Treating"
require_text "docs/134_language_surface_hygiene.md" "source_ast"
require_text "docs/134_language_surface_hygiene.md" "semantic inventory truth is not allowed"
require_text "docs/134_language_surface_hygiene.md" "Authority has one approval source of truth."
require_text "docs/134_language_surface_hygiene.md" "MPaC, Message-Passing and Contracted Concurrency"
require_text "docs/134_language_surface_hygiene.md" "pgy.kit.mpac"
require_text "docs/134_language_surface_hygiene.md" "Allowed debt must be named. Unnamed fallback is not allowed."

require_text "docs/mut_borrow_parameters.md" "function is explicit and value-result, spelled"
require_text "docs/mut_borrow_parameters.md" "spelled"
require_text "docs/mut_borrow_parameters.md" "The implementation mode"
require_text "docs/mut_borrow_parameters.md" "copy-in/copy-out"
require_text "docs/mut_borrow_parameters.md" "is rejected by the parser"
require_text "docs/mut_borrow_parameters.md" "inout"
require_text "docs/mut_borrow_parameters.md" "only spelling for"

require_text "docs/42_keyword_orthogonality.md" "The goal is not to reduce the number of keywords mechanically."
require_text "docs/42_keyword_orthogonality.md" "Intent Is Not A Universal Owner"

require_text "docs/99_language_module_taxonomy.md" "pgy.kit.mpac"
require_text "docs/99_language_module_taxonomy.md" "Message-Passing and Contracted Concurrency"
require_text "docs/67_layered_stdlib_and_domain_kits.md" "MPaC decision"
require_text "docs/67_layered_stdlib_and_domain_kits.md" "pgy.kit.mpac"

require_text "src/self_hosted/tools/linter/main.pgy" "func ScanStructure(content: String, inout diags: Array<String>) -> Void"
require_text "src/parser/ast_types.h" "inout value-result mutable parameter"
require_text "src/parser/parser_decl.c" "strcmp(parser->current_token.text, \"inout\")"
require_text "src/parser/parser_decl.c" "'&mut' is not a binding mode in this language."
require_text "src/parser/parser_async.c" "strcmp(parser->current_token.text, \"inout\")"
require_text "src/parser/parser_async.c" "'&mut' is not a binding mode in this language."
require_text "src/semantic/type_checker_helpers_late.c" "multiple inout parameters"

echo "[language-surface-hygiene] surface naming and domain-kit placement ok"

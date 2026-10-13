#!/usr/bin/env bash
# Proves the 147-row language-word registry, full self-host metadata projection,
# reserved lexer compatibility view, editor scope projection, and generated
# implementation inventory remain one source-of-truth chain.
# SoT fallback IDs covered here or by the companion enforcement refs in the
# owner registry: native_keyword_table, token_debug_keyword_switch,
# selfhost_handwritten_keyword_map, parser_unregistered_contextual_selector,
# selfhost_raw_keyword_selector, lsp_hardcoded_completion_words,
# selfhost_empty_completion_provider, lsp_unregistered_hover_word,
# selfhost_handwritten_hover_table, textmate_only_language_word,
# textmate_scope_as_authority, docs_keyword_list_as_authority,
# second_tmLanguage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/lexer/language_keyword_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy"
PROJECTION_PARTS=(
    language_word_identity_projection_owner.pgy
    language_word_row_projection_owner.pgy
    language_keyword_compatibility_projection_owner.pgy
)
GRAMMAR="$ROOT_DIR/editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json"
INVENTORY="$ROOT_DIR/docs/semantics/language_word_implementation_inventory.generated.md"
FIXTURE_SOURCE="$ROOT_DIR/src/self_hosted/lexer/fixture/all_keywords.pgy"
FIXTURE_EXPECTED="$ROOT_DIR/src/self_hosted/lexer/fixture/all_keywords_tokens.txt"
NATIVE_PROBE="$ROOT_DIR/tests/language_keyword_registry_probe.c"
BUILD_DIR="$ROOT_DIR/.tmp/language_keyword_registry"

fail() {
    echo "[language-keyword-registry] $*" >&2
    exit 1
}

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi
[[ -n "$PYTHON_BIN" ]] || fail "python3/python is required"

for path in \
    "$REGISTRY" \
    "$PROJECTION" \
    "$GRAMMAR" \
    "$INVENTORY" \
    "$FIXTURE_SOURCE" \
    "$FIXTURE_EXPECTED" \
    "$NATIVE_PROBE"; do
    [[ -f "$path" ]] || fail "missing ${path#"$ROOT_DIR/"}"
done
for part in "${PROJECTION_PARTS[@]}"; do
    part_path="$ROOT_DIR/src/self_hosted/lexer/$part"
    [[ -f "$part_path" ]] || fail "missing ${part_path#"$ROOT_DIR/"}"
    case "$part" in
        language_word_identity_projection_owner.pgy) part_cap=330 ;;
        language_word_row_projection_owner.pgy) part_cap=200 ;;
        language_keyword_compatibility_projection_owner.pgy) part_cap=250 ;;
        *) fail "projection cap is not owned for $part" ;;
    esac
    part_lines="$("$PYTHON_BIN" "$ROOT_DIR/scripts/source_size_count.py" "$part_path")" || fail "source-size measurement failed: $part"
    [[ "$part_lines" -le "$part_cap" ]] ||
        fail "generated projection exceeds $part_cap lines: $part"
done
projection_lines="$("$PYTHON_BIN" "$ROOT_DIR/scripts/source_size_count.py" "$PROJECTION")" || fail "source-size measurement failed: $PROJECTION"
[[ "$projection_lines" -le 80 ]] ||
    fail "generated projection hub exceeds 80 lines"

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B \
    "$ROOT_DIR/scripts/render_language_keyword_registry.py" \
    "$REGISTRY" "$PROJECTION" --check \
    --textmate-grammar "$GRAMMAR" \
    --implementation-inventory "$INVENTORY"

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B - \
    "$ROOT_DIR" "$FIXTURE_SOURCE" "$FIXTURE_EXPECTED" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
fixture_source = Path(sys.argv[2])
fixture_expected = Path(sys.argv[3])
sys.path.insert(0, str(root / "scripts"))
import render_language_keyword_registry as registry

all_rows = registry.load_rows(root / "src/lexer/language_keyword_registry.def")
rows = registry.reserved_rows(all_rows)
expected_source = "".join(f"{row.spelling}\n" for row in rows)
if fixture_source.read_text(encoding="utf-8") != expected_source:
    raise SystemExit("all_keywords.pgy is not the exhaustive ordered registry projection")

source_label = fixture_source.relative_to(root).as_posix()
lines = [f"=== tokens: {source_label} ==="]
for index, row in enumerate(rows, start=1):
    lines.append(
        f'{index:4}  Token{{type: {row.debug_identity}, text: "{row.spelling}", '
        f"line: {index}, col: 1}}"
    )
eof_index = len(rows) + 1
lines.append(
    f'{eof_index:4}  Token{{type: EOF, text: "", line: {eof_index}, col: 1}}'
)
lines.append(f"  {eof_index} tokens total")
expected_tokens = "\n".join(lines) + "\n"
if fixture_expected.read_text(encoding="utf-8") != expected_tokens:
    raise SystemExit("all_keywords_tokens.txt drifted from canonical debug identities")

reserved = {row.spelling for row in rows}
non_reserved_rows = [
    row for row in all_rows if row.keyword_class != registry.RESERVED
]
non_reserved = {row.spelling for row in non_reserved_rows}
if reserved & non_reserved:
    raise SystemExit("reserved/contextual spelling collision")

# Only explicit parser grammar-selector owners are scanned. This deliberately
# excludes arbitrary string literals, diagnostics, fixtures, AST data, and
# recursive repository grep so prose/value strings cannot become authority.
# Stable `PGY_LANGUAGE_WORD_*` identity selectors are equivalent typed edges.
helper_selector = re.compile(
    r"parser_(?:decl_(?:match|check)_contextual_keyword|"
    r"match_identifier_keyword(?:_on_line)?|intent_match_keyword|"
    r"check_contextual_keyword)\s*\(\s*parser\s*,\s*\"([a-z]+)\"",
    re.DOTALL,
)
direct_selector = re.compile(
    r"strcmp\s*\(\s*(?:parser->(?:current_token|previous_token)|mode)"
    r"\.text\s*,\s*\"([a-z]+)\"\s*\)"
)
parser_selectors = set()
identity_spellings = {
    "PGY_LANGUAGE_WORD_" + row.debug_identity: row.spelling
    for row in all_rows
}
identity_selector = re.compile(r"\bPGY_LANGUAGE_WORD_[A-Z0-9_]+\b")
for parser_owner in sorted((root / "src/parser").glob("parser*.c")):
    owner_source = parser_owner.read_text(encoding="utf-8")
    parser_selectors.update(helper_selector.findall(owner_source))
    parser_selectors.update(direct_selector.findall(owner_source))
    parser_selectors.update(
        identity_spellings[identity]
        for identity in identity_selector.findall(owner_source)
        if identity in identity_spellings
    )

registry_spellings = reserved | non_reserved
missing_registry_rows = parser_selectors - registry_spellings
if missing_registry_rows:
    raise SystemExit(
        "parser grammar selector missing from registry: "
        f"{sorted(missing_registry_rows)}"
    )
# Retired spellings are read only to name their replacement in a parse error.
# They live in one table in src/parser/parser.c and must never return as
# registry rows.
retired_table = re.findall(
    r'\[PGY_RETIRED_SPELLING_[A-Z0-9_]+\]\s*=\s*"([a-z]+)"',
    (root / "src/parser/parser.c").read_text(encoding="utf-8"),
)
if set(retired_table) & registry_spellings:
    raise SystemExit(
        "retired spelling returned as a registry row: "
        f"{sorted(set(retired_table) & registry_spellings)}"
    )
parser_non_reserved = parser_selectors - reserved
# A contextual word the registry declares unimplemented (support 0) needs no
# selector; every other contextual/soft row must be read by a native parser.
unimplemented = {
    row.spelling for row in non_reserved_rows if row.implementation_support == "0"
}
selector_required = non_reserved - unimplemented
if parser_non_reserved - non_reserved or selector_required - parser_non_reserved:
    raise SystemExit(
        "registry contextual/soft rows and parser selectors disagree: "
        f"parser-only={sorted(parser_non_reserved - non_reserved)}, "
        f"registry-only={sorted(selector_required - parser_non_reserved)}"
    )
PY

mkdir -p "$BUILD_DIR"
CC_BIN="${CC:-cc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR/src" \
    "$NATIVE_PROBE" "$ROOT_DIR/src/lexer/lexer_keywords.c" \
    -o "$BUILD_DIR/language_keyword_registry_probe.exe"
"$BUILD_DIR/language_keyword_registry_probe.exe" ||
    fail "native lookup/debug registry probe failed"

echo "[language-keyword-registry] ok (registry render check, fixture projections, parser selector consistency, native probe)"

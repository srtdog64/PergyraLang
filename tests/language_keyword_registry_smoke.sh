#!/usr/bin/env bash
# Proves the 145-row grammar registry and its reserved 71-word lexer projection
# remain one source-of-truth chain, including parser-selector negative ratchets.
# SoT fallback IDs covered here or by the companion enforcement refs in the
# owner registry: native_keyword_table, token_debug_keyword_switch,
# selfhost_handwritten_keyword_map, parser_unregistered_contextual_selector,
# lsp_hardcoded_completion_words, lsp_unregistered_hover_word,
# selfhost_handwritten_hover_table, textmate_only_language_word,
# docs_keyword_list_as_authority, second_tmLanguage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/lexer/language_keyword_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy"
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
    "$FIXTURE_SOURCE" \
    "$FIXTURE_EXPECTED" \
    "$NATIVE_PROBE"; do
    [[ -f "$path" ]] || fail "missing ${path#"$ROOT_DIR/"}"
done

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B \
    "$ROOT_DIR/scripts/render_language_keyword_registry.py" \
    "$REGISTRY" "$PROJECTION" --check

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

required_repairs = {
    "collapse": "COLLAPSE",
    "impl": "IMPL",
    "innate": "INNATE",
    "local": "LOCAL",
    "nondeterministic": "NONDETERMINISTIC",
    "override": "OVERRIDE",
    "own": "OWN",
    "ref": "REF",
    "type": "TYPE",
}
actual = {row.spelling: row.debug_identity for row in rows}
if any(actual.get(word) != identity for word, identity in required_repairs.items()):
    raise SystemExit("the nine repaired self-host keyword identities regressed")

reserved = {row.spelling for row in rows}
non_reserved_rows = [
    row for row in all_rows if row.keyword_class != registry.RESERVED
]
non_reserved = {row.spelling for row in non_reserved_rows}
if reserved & non_reserved:
    raise SystemExit("reserved/contextual spelling collision")

native_only = {
    "activate", "all", "backoff", "capacity", "current", "deactivate",
    "detach", "forbids", "full", "give", "invariant", "involves", "is",
    "layer", "lifecycle", "maintain", "max", "min", "none", "objects",
    "pin", "pool", "pre", "priority", "product", "projection", "relations",
    "rollback", "subjects", "sum", "timeout", "tobjects", "unlink",
}
actual_native_only = {
    row.spelling
    for row in non_reserved_rows
    if "PGY_KEYWORD_SUPPORT_SELF_HOST" not in row.implementation_support
}
if actual_native_only != native_only:
    raise SystemExit(
        "contextual/soft implementation support drift: "
        f"expected native-only={sorted(native_only)}, "
        f"actual={sorted(actual_native_only)}"
    )

completion = {
    "action", "authority", "authorized", "causes", "requires", "transfer",
    "using", "who", "within",
}
hover = {
    "action", "authority", "authorized", "by", "causes", "effects",
    "requires", "transfer", "using", "who", "within",
}
highlight = {
    "action", "authorized", "by", "causes", "concurrent", "exclusive",
    "failure", "guard", "invariant", "involves", "mut", "on", "post",
    "pre", "projection", "requires", "state", "step", "success", "who",
    "within",
}
tooling_contract = {
    "PGY_KEYWORD_TOOLING_COMPLETION": completion,
    "PGY_KEYWORD_TOOLING_HOVER": hover,
    "PGY_KEYWORD_TOOLING_HIGHLIGHT": highlight,
}
for flag, expected in tooling_contract.items():
    actual = {
        row.spelling for row in non_reserved_rows if flag in row.tooling_flags
    }
    if actual != expected:
        raise SystemExit(
            f"{flag} contextual/soft set drift: "
            f"expected={sorted(expected)}, actual={sorted(actual)}"
        )

# Only explicit parser grammar-selector owners are scanned. This deliberately
# excludes arbitrary string literals, diagnostics, fixtures, AST data, and
# recursive repository grep so prose/value strings cannot become authority.
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
for parser_owner in sorted((root / "src/parser").glob("parser*.c")):
    owner_source = parser_owner.read_text(encoding="utf-8")
    parser_selectors.update(helper_selector.findall(owner_source))
    parser_selectors.update(direct_selector.findall(owner_source))

registry_spellings = reserved | non_reserved
missing_registry_rows = parser_selectors - registry_spellings
if missing_registry_rows:
    raise SystemExit(
        "parser grammar selector missing from registry: "
        f"{sorted(missing_registry_rows)}"
    )
parser_non_reserved = parser_selectors - reserved
if parser_non_reserved != non_reserved:
    raise SystemExit(
        "registry contextual/soft rows and parser selectors disagree: "
        f"parser-only={sorted(parser_non_reserved - non_reserved)}, "
        f"registry-only={sorted(non_reserved - parser_non_reserved)}"
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

grep -Fq '#include "language_keyword_registry.def"' \
    "$ROOT_DIR/src/lexer/lexer_keywords.c" ||
    fail "native lookup does not consume the registry"
grep -Fq 'lexer_keyword_debug_name(type)' \
    "$ROOT_DIR/src/lexer/lexer_token_debug.c" ||
    fail "native debug rendering does not consume the registry projection"
grep -Fq 'import "language_keyword_registry_projection_owner.pgy";' \
    "$ROOT_DIR/src/self_hosted/lexer/token_owner.pgy" ||
    fail "self-host token owner does not import the generated projection"
grep -Fq 'return LanguageKeywordDebugIdentity(text);' \
    "$ROOT_DIR/src/self_hosted/lexer/token_owner.pgy" ||
    fail "KeywordType does not delegate to the generated projection"
if grep -Fq 'if text ==' "$ROOT_DIR/src/self_hosted/lexer/token_owner.pgy"; then
    fail "token_owner.pgy recreated a hand-written keyword table"
fi
grep -Fq 'return 9;' \
    "$ROOT_DIR/src/self_hosted/lexer/fixture_manifest_owner.pgy" ||
    fail "lexer fixture manifest count is not 9"
grep -Fq 'src/self_hosted/lexer/fixture/all_keywords.pgy' \
    "$ROOT_DIR/src/self_hosted/lexer/fixture_manifest_owner.pgy" ||
    fail "lexer fixture manifest is missing all_keywords.pgy"
grep -Fq 'all_keywords_tokens.txt' \
    "$ROOT_DIR/src/self_hosted/lexer/fixture_manifest_owner.pgy" ||
    fail "lexer fixture manifest is missing all_keywords_tokens.txt"

echo "[language-keyword-registry] ok (145 rows; 71 reserved lexer rows; 74 parser selectors; 9 fixtures)"

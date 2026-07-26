#!/usr/bin/env bash
# Proves the 71-word native registry and generated self-host lexer projection
# remain one source-of-truth chain, including the committed exhaustive fixture.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/lexer/language_keyword_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy"
FIXTURE_SOURCE="$ROOT_DIR/src/self_hosted/lexer/fixture/all_keywords.pgy"
FIXTURE_EXPECTED="$ROOT_DIR/src/self_hosted/lexer/fixture/all_keywords_tokens.txt"

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
    "$FIXTURE_EXPECTED"; do
    [[ -f "$path" ]] || fail "missing ${path#"$ROOT_DIR/"}"
done

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B \
    "$ROOT_DIR/scripts/render_language_keyword_registry.py" \
    "$REGISTRY" "$PROJECTION" --check

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B - \
    "$ROOT_DIR" "$FIXTURE_SOURCE" "$FIXTURE_EXPECTED" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
fixture_source = Path(sys.argv[2])
fixture_expected = Path(sys.argv[3])
sys.path.insert(0, str(root / "scripts"))
import render_language_keyword_registry as registry

rows = registry.load_rows(root / "src/lexer/language_keyword_registry.def")
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
PY

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

echo "[language-keyword-registry] ok (71 rows; native + generated self-host projection; 9 fixtures)"

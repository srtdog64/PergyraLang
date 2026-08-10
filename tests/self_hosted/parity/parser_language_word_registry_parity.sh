#!/usr/bin/env bash
# Focused parser gate for registry-owned language-word selectors.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/parser_tool_build_leg.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parser_language_word_registry}"
POSITIVE_REL="src/self_hosted/parser/fixture/language_word_roles.pgy"
NEGATIVE_REL="src/self_hosted/parser/reject_fixture/systemic_slot.pgy"
SELF_BIN="$BUILD_DIR/parser_c.exe"
mkdir -p "$BUILD_DIR"

python3 - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
parser_dir = root / "src/self_hosted/parser"
owners = sorted(parser_dir.glob("*.pgy"))
cursor = parser_dir / "cursor_owner.pgy"
nominal = parser_dir / "decl_nominal_owner.pgy"

raw_word_call = re.compile(
    r'MatchKeyword\s*\([^)]*?,\s*"[a-z][a-z0-9_-]*"\s*\)',
    re.DOTALL,
)
violations = []
for owner in owners:
    text = owner.read_text(encoding="utf-8")
    if raw_word_call.search(text):
        violations.append(owner.relative_to(root).as_posix())
if violations:
    raise SystemExit(
        "raw lowercase MatchKeyword selector remains: " + ", ".join(violations)
    )

# A typed selector must not be followed by a cursor advance that independently
# re-encodes the selected word's byte length. This bounded line-window catches
# the local `if MatchLanguageWord(...) { i = i + N; }` family, including probe
# cursors such as `after_ws` and `next_i`, without confusing punctuation scans
# elsewhere in the parser.
match_call = re.compile(
    r"MatchLanguageWord\(content,\s*([A-Za-z_][A-Za-z0-9_]*),\s*"
    r"LanguageWordId\.([A-Za-z0-9_]+)\)"
)
numeric_advance = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\+\s*([0-9]+)\b"
)
owned_advance = re.compile(
    r"LanguageWordEnd\(\s*([A-Za-z_][A-Za-z0-9_]*),\s*"
    r"LanguageWordId\.([A-Za-z0-9_]+)\s*\)"
)
manual_advances = []
for owner in owners:
    recent_matches = []
    for line_index, line in enumerate(
        owner.read_text(encoding="utf-8").splitlines()
    ):
        recent_matches = [
            match for match in recent_matches
            if line_index - match[0] <= 6
        ]
        for match in match_call.finditer(line):
            recent_matches.append(
                (line_index, match.group(1), match.group(2))
            )
        for advance in owned_advance.finditer(line):
            recent_matches = [
                match for match in recent_matches
                if (match[1], match[2])
                != (advance.group(1), advance.group(2))
            ]
        for advance in numeric_advance.finditer(line):
            base = advance.group(1)
            amount = int(advance.group(2))
            for _, cursor_name, variant in reversed(recent_matches):
                spelling_variant = (
                    variant[4:] if variant.startswith("Word") else variant
                )
                if cursor_name == base and amount == len(spelling_variant):
                    manual_advances.append(
                        f"{owner.relative_to(root).as_posix()}:"
                        f"{line_index + 1}:{base}+{advance.group(2)} after {variant}"
                    )
                    break
if manual_advances:
    raise SystemExit(
        "manual language-word cursor advance remains: "
        + ", ".join(manual_advances)
    )

entry_consumers = {
    "decl_ability_owner.pgy": (
        "LanguageWordEnd(start, LanguageWordId.WordAbility)"
    ),
    "decl_type_owner.pgy": "LanguageWordEnd(start, LanguageWordId.WordType)",
    "decl_event_owner.pgy": "LanguageWordEnd(start, LanguageWordId.WordEvent)",
    "decl_enum_owner.pgy": "LanguageWordEnd(start, LanguageWordId.WordEnum)",
    "decl_effect_relation_owner.pgy": (
        "LanguageWordEnd(i, LanguageWordId.WordEffect)",
        "LanguageWordEnd(i, LanguageWordId.WordRelation)",
    ),
    "decl_intent_owner.pgy": "LanguageWordEnd(i, LanguageWordId.WordIntent)",
    "decl_role_owner.pgy": "LanguageWordEnd(i, LanguageWordId.WordRole)",
    "decl_zone_owner.pgy": "LanguageWordEnd(i, LanguageWordId.WordZone)",
}
for owner_name, required in entry_consumers.items():
    owner_text = (parser_dir / owner_name).read_text(encoding="utf-8")
    required_values = required if isinstance(required, tuple) else (required,)
    for value in required_values:
        if value not in owner_text:
            raise SystemExit(
                f"declaration entry keyword length is not owner-derived: "
                f"{owner_name}:{value}"
            )

length_literal = re.compile(
    r"\b(?:decl|ref_pub)_kw_len\s*(?::[^=]+)?=\s*[1-9][0-9]*\b"
)
for owner in owners:
    if length_literal.search(owner.read_text(encoding="utf-8")):
        raise SystemExit(
            "keyword length variable reintroduced a numeric literal: "
            + owner.relative_to(root).as_posix()
        )

cursor_text = cursor.read_text(encoding="utf-8")
if cursor_text.count("func MatchLanguageWord(") != 1:
    raise SystemExit("cursor_owner must own exactly one MatchLanguageWord adapter")
if 'import "../lexer/language_keyword_registry_projection_owner.pgy";' not in cursor_text:
    raise SystemExit("cursor_owner does not import the generated language-word projection")
if "LanguageWordSpelling(id)" not in cursor_text:
    raise SystemExit("MatchLanguageWord does not project spelling from LanguageWordId")
if "LanguageWordLength(id)" not in cursor_text:
    raise SystemExit("LanguageWordEnd does not project length from LanguageWordId")

all_owner_text = "\n".join(path.read_text(encoding="utf-8") for path in owners)
for variant in ("Impl", "Ref", "Own", "Type"):
    selector = f"LanguageWordId.Word{variant}"
    if selector not in all_owner_text:
        raise SystemExit(f"typed parser role is missing: {selector}")
if "LanguageWordId.WordSystemic" in all_owner_text:
    raise SystemExit("unregistered systemic selector reappeared")
if '"systemic"' in nominal.read_text(encoding="utf-8"):
    raise SystemExit("decl_nominal_owner reintroduced the systemic branch")

positive = (parser_dir / "fixture/language_word_roles.pgy").read_text(encoding="utf-8")
for spelling in ("action", "type", "impl", "ref", "own"):
    if not re.search(rf"\b{spelling}\b", positive):
        raise SystemExit(f"positive parser role fixture is missing: {spelling}")
negative = (parser_dir / "reject_fixture/systemic_slot.pgy").read_text(encoding="utf-8")
if "world W {" not in negative or "systemic slot actors: Int;" not in negative:
    raise SystemExit("systemic-slot rejection fixture drifted")
PY

if [[ "${PGY_SELFHOST_PARSER_LANGUAGE_WORD_STATIC_ONLY:-0}" == "1" ]]; then
    echo "[self-host-parity:parser-language-word] static owner gate ok"
    exit 0
fi

if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:parser-language-word] missing compiler binary: $PGY" >&2
    exit 1
fi
PGY_EXEC="$(pgy_path_for_bash_tool "$PGY")"

pgy_selfhost_compile_parser_tool \
    "self-host-parity:parser-language-word" \
    "$ROOT_DIR/src/self_hosted/parser/main.pgy" c \
    "$SELF_BIN" "$BUILD_DIR/parser_c.compile.log"

(cd "$ROOT_DIR" && "$PGY_EXEC" --native-pipeline --ast \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$POSITIVE_REL")") \
    >"$BUILD_DIR/positive.native.ast" 2>"$BUILD_DIR/positive.native.err"
(cd "$ROOT_DIR" && "$SELF_BIN" "$POSITIVE_REL") \
    >"$BUILD_DIR/positive.self.ast" 2>"$BUILD_DIR/positive.self.err"
tr -d '\r' <"$BUILD_DIR/positive.native.ast" >"$BUILD_DIR/positive.native.norm"
tr -d '\r' <"$BUILD_DIR/positive.self.ast" >"$BUILD_DIR/positive.self.norm"
if ! cmp -s "$BUILD_DIR/positive.native.norm" "$BUILD_DIR/positive.self.norm"; then
    echo "[self-host-parity:parser-language-word] action/impl/ref/own/type AST parity failed" >&2
    diff -u "$BUILD_DIR/positive.native.norm" "$BUILD_DIR/positive.self.norm" >&2 || true
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$PGY_EXEC" --native-pipeline --ast \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$NEGATIVE_REL")") \
    >"$BUILD_DIR/systemic.native.out" 2>"$BUILD_DIR/systemic.native.err"
native_rc=$?
(cd "$ROOT_DIR" && "$SELF_BIN" "$NEGATIVE_REL") \
    >"$BUILD_DIR/systemic.self.out" 2>"$BUILD_DIR/systemic.self.err"
self_rc=$?
set -e

if [[ "$native_rc" -ne 1 || "$self_rc" -ne 1 ]]; then
    echo "[self-host-parity:parser-language-word] systemic slot rejection boundary drifted (native=$native_rc self=$self_rc)" >&2
    cat "$BUILD_DIR/systemic.native.out" "$BUILD_DIR/systemic.native.err" \
        "$BUILD_DIR/systemic.self.out" "$BUILD_DIR/systemic.self.err" >&2
    exit 1
fi
if grep -Fq "Program:" "$BUILD_DIR/systemic.native.out" \
    "$BUILD_DIR/systemic.self.out"; then
    echo "[self-host-parity:parser-language-word] systemic rejection emitted an AST" >&2
    exit 1
fi

echo "[self-host-parity:parser-language-word] typed selectors and parser boundaries ok"

#!/usr/bin/env bash
# A failed compile on the default route (self-hosted front end) names a
# diagnostic code and the source position it is about, in text and in JSON,
# for `--backend=c` and `--emit-c`. Before this gate the self-host semantic
# printed `Span: none` for every code, and most parse refusals printed
# nothing: the launcher's `self-host driver failed (exit 1)` was all a user
# saw, and JSON mode had no receipt at all.
# - Imported-file rows: the refusal lives in lib.pgy, so the span must name
#   lib.pgy and its line, not the entry file.
# - A builtin call refused for arity or argument type names the builtin's
#   whole registry signature on the default route; native names it for the
#   TextBuilder family.
# - Broken-input rows: each must exit non-zero with one positioned diagnostic.
# - Binding names follow the registry NAME context on both front ends.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-route-diagnostic-position"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
FIXTURES="tests/self_hosted/parity/fixture/diagnostic_position"
WORK_REL=".tmp/self_hosted/default_route_diagnostic_position"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$PYTHON_BIN" >/dev/null || fail "missing $PYTHON_BIN"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

# refuse LOG SOURCE LEG: run one default-route leg that must fail.
refuse() {
    local log="$1" source="$2" leg="$3" flags=()
    case "$leg" in
        c-text) flags=(--backend=c -o "$WORK_REL/refused.bin") ;;
        c-json) flags=(--backend=c --error-format=json -o "$WORK_REL/refused.bin") ;;
        emit-text) flags=(--emit-c -o "$WORK_REL/refused.c") ;;
        emit-json) flags=(--emit-c --error-format=json -o "$WORK_REL/refused.c") ;;
        *) fail "unknown leg $leg" ;;
    esac
    rm -f "$WORK_DIR/refused.bin" "$WORK_DIR/refused.c"
    if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$source" "${flags[@]}") >"$log" 2>&1; then
        fail "$leg accepted $source"
    fi
    [[ ! -e "$WORK_DIR/refused.bin" && ! -e "$WORK_DIR/refused.c" ]] ||
        fail "$leg published an artifact for $source"
}

# check_text LOG FILE LINE CODE: one diagnostic block names CODE and a span
# FILE:LINE:COLUMN with a real column.
check_text() {
    local log="$1" file="$2" line="$3" code="$4"
    tr -d '\r' <"$log" | grep -Fxq "Code: $code" ||
        { cat "$log" >&2; fail "$log lost code $code"; }
    # Both lines must sit in one block, which starts at a Diagnostic: line.
    tr -d '\r' <"$log" | PGY_DIAG_CODE_LINE="Code: $code" \
        PGY_DIAG_SPAN_RE="^Span: ${file//./\\.}:$line:[1-9][0-9]*$" awk '
        /^Diagnostic:/ { if (c && s) ok = 1; c = 0; s = 0 }
        $0 == ENVIRON["PGY_DIAG_CODE_LINE"] { c = 1 }
        $0 ~ ENVIRON["PGY_DIAG_SPAN_RE"] { s = 1 }
        END { if (c && s) ok = 1; exit !ok }' ||
        { cat "$log" >&2; fail "$log does not place $code at $file:$line in one diagnostic"; }
}

# check_json LOG FILE LINE CODE: one receipt entry has a code and a location
# naming FILE:LINE with a real column, and carries CODE in its message.
check_json() {
    "$PYTHON_BIN" - "$1" "$2" "$3" "$4" <<'PY' || fail "$1 lacks a positioned JSON diagnostic"
import json
import sys

log, file, line, code = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
entries = []
for raw in open(log, encoding="utf-8", errors="replace"):
    raw = raw.strip()
    if raw.startswith("["):
        try:
            entries.extend(json.loads(raw))
        except json.JSONDecodeError:
            pass
for entry in entries:
    location = entry.get("location") or {}
    if (entry.get("code") and location.get("file") == file and
            location.get("line") == line and location.get("column", 0) >= 1 and
            f"Code: {code}" in entry.get("message", "")):
        sys.exit(0)
sys.stderr.write(open(log, encoding="utf-8", errors="replace").read())
sys.exit(1)
PY
}

# expect_refusal NAME SOURCE FILE LINE CODE [EXTRA_TEXT]
expect_refusal() {
    local name="$1" source="$2" file="$3" line="$4" code="$5" extra="${6:-}"
    local leg log
    for leg in c-text c-json emit-text emit-json; do
        log="$WORK_DIR/$name-$leg.log"
        refuse "$log" "$source" "$leg"
        case "$leg" in
            *-text) check_text "$log" "$file" "$line" "$code" ;;
            *-json) check_json "$log" "$file" "$line" "$code" ;;
        esac
        [[ -z "$extra" ]] || grep -Fq -- "$extra" "$log" ||
            { cat "$log" >&2; fail "$name $leg lost: $extra"; }
    done
}

# name|expected line in lib.pgy|code|required text
IMPORTED_CASES=(
    "undefined_function|3|undefined_function|- func: Frobnicate"
    "call_arg_type|6|call_arg_type_mismatch|- func: Twice"
    "call_arity|7|call_arity_mismatch|- func: Twice"
    "undefined_symbol|3|undefined_symbol|- name: nope"
    "builtin_arity_new|3|call_arity_mismatch|signature: TextBuilderNew(Int) -> TextBuilder"
    "builtin_arity_finish|4|call_arity_mismatch|signature: TextBuilderFinish(TextBuilder, Allocator) -> String"
    "builtin_arg_type|3|call_arg_type_mismatch|signature: StringLength(String) -> Int"
)
for row in "${IMPORTED_CASES[@]}"; do
    IFS='|' read -r name line code extra <<<"$row"
    case_rel="$WORK_REL/imported-$name"
    mkdir -p "$ROOT_DIR/$case_rel"
    cp "$ROOT_DIR/$FIXTURES/main.pgy" "$ROOT_DIR/$case_rel/main.pgy"
    cp "$ROOT_DIR/$FIXTURES/lib_$name.pgy" "$ROOT_DIR/$case_rel/lib.pgy"
    expect_refusal "imported-$name" "$case_rel/main.pgy" lib.pgy "$line" "$code" "$extra"
done

# name|expected line|code
BROKEN_CASES=(
    "spawn_local|1|binding_name_reserved"
    "async_local|2|binding_name_reserved"
    "stray_brace|1|statement_head_unexpected_token"
    "stray_token|3|statement_head_unexpected_token"
    "braced_match_arm|5|statement_head_unexpected_token"
    "unclosed_body|1|block_unclosed"
    "unclosed_if|1|block_unclosed"
    "unclosed_paren|2|expected_token"
    "unterminated_string|2|string_unterminated"
    "unclosed_generic|2|type_name_invalid"
    "missing_return_type|1|type_name_invalid"
    "missing_let_name|2|declaration_name_missing"
    "missing_func_name|1|declaration_name_missing"
    "bad_type_name|2|let_type_mismatch"
    "inout_field|9|inout_argument_not_variable"
    "inout_element|8|inout_argument_not_variable"
    "member_call_arg|9|member_call_arg_type_mismatch"
    "enum_unclosed|1|block_unclosed"
    "zone_unclosed|1|block_unclosed"
    "class_unclosed|1|block_unclosed"
    "enum_missing_name|1|declaration_name_missing"
    "struct_field_colon|1|expected_token"
    "export_statement|1|expected_token"
    "import_unquoted|1|expected_token"
    "import_missing|1|import_source_missing"
    "script_with_main|4|script_statement_with_main"
    "long_literal|2|long_literal_out_of_range"
    "expression_statement|3|statement_kind_unsupported"
    "member_expression_statement|5|statement_kind_unsupported"
    "match_unclosed|3|block_unclosed"
)
for row in "${BROKEN_CASES[@]}"; do
    IFS='|' read -r name line code <<<"$row"
    expect_refusal "broken-$name" "$FIXTURES/broken_$name.pgy" \
        "broken_$name.pgy" "$line" "$code"
done

# Reserved binding names: native refuses the same spellings, and the words
# the registry admits as names compile on both front ends.
for name in spawn_local async_local; do
    if (cd "$ROOT_DIR" && "$PGY" "$FIXTURES/broken_$name.pgy" --native-pipeline \
        --backend=c -o "$WORK_REL/native-$name.bin") \
        >"$WORK_DIR/native-$name.log" 2>&1; then
        fail "native accepted the reserved binding in $name"
    fi
done
for leg in native default; do
    flags=(--backend=c)
    [[ "$leg" == native ]] && flags+=(--native-pipeline)
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" \
        "$FIXTURES/control_binding_words.pgy" "${flags[@]}" \
        -o "$WORK_REL/control-$leg.exe") >"$WORK_DIR/control-$leg.log" 2>&1 ||
        { cat "$WORK_DIR/control-$leg.log" >&2; fail "$leg refused admitted binding words"; }
    [[ "$("$WORK_DIR/control-$leg.exe" | tr -d '\r')" == "3" ]] ||
        fail "$leg control printed the wrong value"
done

# Native TextBuilder arity names the signature the self-host rows name.
case_rel="$WORK_REL/imported-builtin_arity_new"
if (cd "$ROOT_DIR" && "$PGY" "$case_rel/main.pgy" --native-pipeline \
    --backend=c -o "$WORK_REL/native-arity.bin") >"$WORK_DIR/native-arity.log" 2>&1; then
    fail "native accepted TextBuilderNew()"
fi
grep -Fq 'Declared signature: TextBuilderNew(Int) -> TextBuilder' \
    "$WORK_DIR/native-arity.log" ||
    { cat "$WORK_DIR/native-arity.log" >&2; fail "native TextBuilderNew arity lost its signature"; }

# Ownership ratchets: one span policy, one binding-name authority, and a
# location join that fails closed instead of printing `Span: none`.
grep -Fq 'source-location rows do not join the admitted tree' \
    "$ROOT_DIR/src/self_hosted/parser/program_parse_owner.pgy" &&
    ! grep -Fq 'locations = AstSourceLocationFactsUnknown()' \
        "$ROOT_DIR/src/self_hosted/parser/program_parse_owner.pgy" ||
    fail "a location join that does not match the tree regained a silent fallback"
grep -Fq 'func SemanticDiagnosticSpanIsProgramWide(' \
    "$ROOT_DIR/src/self_hosted/semantic/diagnostic_code_owner.pgy" ||
    fail "the program-wide span policy left diagnostic_code_owner"
[[ "$(grep -RFl --include='*.pgy' 'func SemanticDiagnosticSpanIsProgramWide(' \
    "$ROOT_DIR/src/self_hosted" | wc -l)" -eq 1 ]] ||
    fail "the program-wide span policy has more than one owner"
! grep -Eq 'case TOKEN_(SLOT|ZONE|WORLD|PARTY):' "$ROOT_DIR/src/parser/parser_name_tokens.c" ||
    fail "native binding names regained a hand-written token list"
grep -Fq 'PGY_KEYWORD_CONTEXT_NAME' "$ROOT_DIR/src/parser/parser_name_tokens.c" ||
    fail "native binding names no longer read the registry NAME context"
grep -Fq 'row.context_mask / 256' "$ROOT_DIR/src/self_hosted/parser/cursor_owner.pgy" ||
    fail "self-host binding names no longer read the registry NAME context"

echo "[$LABEL] ${#IMPORTED_CASES[@]} imported-file spans, ${#BROKEN_CASES[@]} broken inputs x 4 default legs, reserved binding names, builtin signatures: PASS"

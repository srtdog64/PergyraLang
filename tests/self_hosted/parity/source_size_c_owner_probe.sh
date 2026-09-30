#!/usr/bin/env bash
# Narrow actual SourceSize owner versus independently fixed C/header goldens.
# This is not a whole-tool/driver build or complete C preprocessor lexer proof.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="source-size-c-owner-probe"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
FIXTURE="tests/self_hosted/fixtures/source_size_c_owner_probe.pgy"
OWNER="src/self_hosted/lib/source_size.pgy"
HOST="scripts/source_size_count.py"
BUILD_TIMEOUT="${PGY_SELFHOST_PROBE_BUILD_TIMEOUT:-300}s"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
for tool in "$CC" timeout mktemp sha256sum python; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/source_size_c_owner.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
cd "$ROOT_DIR"
sha256sum "$FIXTURE" "$OWNER" "$HOST" "$PGY" > "$WORK_DIR/source-before.sha256"
if ! timeout "$BUILD_TIMEOUT" "$PGY" "$FIXTURE" --native-pipeline --emit-c \
    -o "$WORK_REL/probe.c" > "$WORK_DIR/emit.out" 2> "$WORK_DIR/emit.err"; then
    cat "$WORK_DIR/emit.out" "$WORK_DIR/emit.err" >&2
    fail "native bootstrap probe emission failed"
fi
export PGY_SELFHOST_CC_PROFILE=test
pgy_selfhost_select_emitted_c_compile_profile
if ! timeout "$BUILD_TIMEOUT" "$CC" -x c -std=gnu11 \
    "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" -Isrc -Isrc/runtime -pthread \
    "$WORK_DIR/probe.c" -lm -o "$WORK_DIR/probe.exe" \
    > "$WORK_DIR/compile.out" 2> "$WORK_DIR/compile.err"; then
    cat "$WORK_DIR/compile.err" >&2
    fail "probe C compilation failed"
fi

# Input files are generated test data, not handwritten production projections.
# ReadFile only receives project-relative paths; no I/O authority bypass.
python - "$WORK_DIR" "$WORK_REL" <<'PY'
import pathlib
import sys
work, relative = pathlib.Path(sys.argv[1]), sys.argv[2]
cases = (
    ("empty.c", b"", 0),
    ("newline.h", b"\n", 1),
    ("whitespace.c", b" \t\n\v\f\n", 2),
    ("blank-block.h", b"/* comment\n\n \t\nend */\n", 2),
    ("comments.c", b"// comment\n/* comment */\n", 0),
    ("inline.h", b"/* left */ int n; // right\n", 1),
    ("strings.c", b'char *s = "// /* */";\n// comment\n', 1),
    ("escaped.h", rb'char *s = "\"//\"";' + b"\n" + rb"char q = '\''; // tail" + b"\n", 2),
    ("continued-comment.c", b"// comment \\\nhidden(); \\\nstill_hidden();\nreal();\n", 1),
    ("continued-comment-crlf.h", b"// comment \\\r\nhidden();\r\nreal();\r\n", 1),
    ("continued-comment-blank.c", b"// comment \\\n\nreal();\n", 2),
    ("continued-string.h", b'char *s = "a\\\n// string payload";\n', 2),
    ("crlf.c", b"// comment\r\nvalue;\r\n\r\nlast;", 3),
    ("eof-code.h", b"value;", 1),
    ("eof-comment.c", b"// comment", 0),
    ("eof-blank.h", b" \t", 1),
    ("bom-code.c", b"\xef\xbb\xbf// comment\nvalue;\n", 1),
    ("bom-only.h", b"\xef\xbb\xbf", 0),
    ("bom-newline.c", b"\xef\xbb\xbf\n", 1),
    ("quoted-splice-start.c", b'char *s = "/\\\n* payload";\n', 2),
    ("quoted-splice-end.h", b'char *s = "*\\\n/ payload";\n', 2),
    ("quoted-splice-crlf.c", b'char *s = "/\\\r\n/ payload";\r\n', 2),
    ("code600.c", b"// comment\n" * 900 + b"value; // inline\n" * 600, 600),
    ("code601.h", b"// comment\n" * 900 + b"value;\n" * 600 + b"value;", 601),
)
rows = []
for name, content, expected in cases:
    (work / name).write_bytes(content)
    rows.append(f"{name}\t{relative}/{name}\t{expected}\n")
(work / "goldens.tsv").write_bytes("".join(rows).encode("utf-8"))
unclosed = (
    ("unclosed-comment.c", b"/* open\nvalue;"),
    ("unclosed-string.h", b'char *s = "open\n'),
    ("unclosed-char.c", b"char c = '"),
)
spliced = (
    ("splice-block-start.c", b"/\\\n* comment */\n"),
    ("splice-line-start.h", b"/\\\n/ comment\n"),
    ("splice-block-end.c", b"/* comment\n*\\\n/\n"),
    ("splice-prefixed-end.h", b"/* comment *\\\n/\n"),
    ("splice-chained-start.c", b"/\\\n\\\n* comment */\n"),
    ("splice-crlf-start.h", b"/\\\r\n/ comment\r\n"),
    ("splice-crlf-end.c", b"/* comment\r\n*\\\r\n/\r\n"),
    ("splice-chained-end.h", b"/* comment\n*\\\n\\\n/\n"),
)
negative_rows = []
for name, content in unclosed + spliced:
    (work / name).write_bytes(content)
    if (name, content) in unclosed:
        host_diagnostic = "unterminated block comment or string literal"
        pgy_diagnostic = "[source-size] unterminated C comment or literal:"
    else:
        host_diagnostic = "unsupported C comment delimiter formed by line splice"
        pgy_diagnostic = "[source-size] " + host_diagnostic + ":"
    negative_rows.append(f"{name}\t{relative}/{name}\t{host_diagnostic}\t{pgy_diagnostic}\n")
(work / "negatives.tsv").write_bytes("".join(negative_rows).encode("utf-8"))
PY

checks=0
while IFS=$'\t' read -r name input expected; do
    if ! timeout "$STEP_TIMEOUT" python "$HOST" "$input" \
        > "$WORK_DIR/$name.host.out" 2> "$WORK_DIR/$name.host.err"; then
        fail "host rejected valid golden $name"
    fi
    [[ "$(tr -d '\r\n' < "$WORK_DIR/$name.host.out")" == "$expected" ]] ||
        fail "host golden mismatch $name: expected $expected"
    if ! timeout "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" "$input" \
        > "$WORK_DIR/$name.pgy.out" 2> "$WORK_DIR/$name.pgy.err"; then
        fail "Pergyra owner rejected valid golden $name"
    fi
    [[ "$(tr -d '\r\n' < "$WORK_DIR/$name.pgy.out")" == "$expected" ]] ||
        fail "Pergyra owner golden mismatch $name: expected $expected"
    checks=$((checks + 1))
done < "$WORK_DIR/goldens.tsv"

while IFS=$'\t' read -r name input host_diagnostic pgy_diagnostic; do
    host_status=0
    timeout "$STEP_TIMEOUT" python "$HOST" "$input" \
        > "$WORK_DIR/$name.host.out" 2> "$WORK_DIR/$name.host.err" || host_status=$?
    [[ "$host_status" == 1 ]] || fail "host malformed golden status $name: $host_status"
    grep -Fq "$host_diagnostic" "$WORK_DIR/$name.host.err" ||
        fail "host malformed golden diagnostic missing: $name"
    pgy_status=0
    timeout "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" "$input" \
        > "$WORK_DIR/$name.pgy.out" 2> "$WORK_DIR/$name.pgy.err" || pgy_status=$?
    [[ "$pgy_status" == 1 ]] || fail "Pergyra malformed golden status $name: $pgy_status"
    grep -Fq "$pgy_diagnostic" \
        "$WORK_DIR/$name.pgy.out" "$WORK_DIR/$name.pgy.err" ||
        fail "Pergyra malformed golden diagnostic missing: $name"
    checks=$((checks + 1))
done < "$WORK_DIR/negatives.tsv"

timeout "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" "$WORK_REL/code600.c" --cap600 \
    > "$WORK_DIR/cap600.out" 2> "$WORK_DIR/cap600.err" || fail "600 code plus comments refused"
[[ "$(tr -d '\r\n' < "$WORK_DIR/cap600.out")" == 600 ]] || fail "cap600 positive output drifted"
cap_status=0
timeout "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" "$WORK_REL/code601.h" --cap600 \
    > "$WORK_DIR/cap601.out" 2> "$WORK_DIR/cap601.err" || cap_status=$?
[[ "$cap_status" == 1 ]] || fail "601 code refusal status: $cap_status"
grep -Fq '[source-size-probe] cap600 rejected 601' "$WORK_DIR/cap601.out" || fail "cap601 refusal diagnostic missing"
sha256sum "$FIXTURE" "$OWNER" "$HOST" "$PGY" > "$WORK_DIR/source-after.sha256"
cmp -s "$WORK_DIR/source-before.sha256" "$WORK_DIR/source-after.sha256" ||
    fail "probe/owners/launcher changed during the gate"
sha256sum "$WORK_DIR/probe.c" "$WORK_DIR/probe.exe" "$WORK_DIR/goldens.tsv" \
    "$WORK_DIR/negatives.tsv" > "$WORK_DIR/artifacts.sha256"
echo "[$LABEL] actual owner and host: $checks goldens + 2 cap branches PASS"

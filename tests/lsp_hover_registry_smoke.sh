#!/usr/bin/env bash
# Proves LSP hover prose has one presentation owner while lowercase exposure
# remains an exact projection of the language keyword registry HOVER flags.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PRESENTATION="$ROOT_DIR/src/lsp/lsp_hover_content.def"
LANGUAGE_REGISTRY="$ROOT_DIR/src/lexer/language_keyword_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lsp/hover_content_projection_owner.pgy"
RENDERER="$ROOT_DIR/scripts/render_lsp_hover_content.py"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_lsp_hover_registry.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[lsp-hover-registry] python3/python is required" >&2
    exit 1
fi

for path in "$PRESENTATION" "$LANGUAGE_REGISTRY" "$PROJECTION" "$RENDERER"; do
    if [[ ! -f "$path" ]]; then
        echo "[lsp-hover-registry] missing ${path#"$ROOT_DIR/"}" >&2
        exit 1
    fi
done

PGY_LSP="${PGY_LSP_BIN:-$ROOT_DIR/bin/pgy-lsp}"
if [[ "$PGY_LSP" != *.exe && -x "${PGY_LSP}.exe" ]]; then
    PGY_LSP="${PGY_LSP}.exe"
fi
if [[ ! -x "$PGY_LSP" ]]; then
    if [[ -n "${PGY_LSP_BIN:-}" ]]; then
        echo "[lsp-hover-registry] missing explicit LSP binary: $PGY_LSP" >&2
        exit 1
    fi
    PGY_LSP=""
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
SELF_HOST_BIN=""
if [[ -x "$PGY" ]]; then
    SELF_HOST_BIN="$WORK_DIR/lsp_hover_projection_selfhost.exe"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/lsp/main.pgy")" \
        --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$SELF_HOST_BIN")" \
        >"$WORK_DIR/selfhost.compile.log" 2>&1); then
        echo "[lsp-hover-registry] self-host hover projection failed to build" >&2
        cat "$WORK_DIR/selfhost.compile.log" >&2
        exit 1
    fi
elif [[ -n "${PGY_BIN:-}" ]]; then
    echo "[lsp-hover-registry] missing explicit compiler binary: $PGY" >&2
    exit 1
fi

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B "$RENDERER" \
    "$PRESENTATION" "$LANGUAGE_REGISTRY" "$PROJECTION" --check

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B - \
    "$ROOT_DIR" "$PGY_LSP" "$SELF_HOST_BIN" <<'PY'
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys


root = Path(sys.argv[1])
lsp_bin = sys.argv[2]
self_host_bin = sys.argv[3]
sys.path.insert(0, str(root / "scripts"))
import render_lsp_hover_content as hover


def fail(message: str) -> None:
    raise SystemExit(f"[lsp-hover-registry] {message}")


presentation = root / "src/lsp/lsp_hover_content.def"
language_registry = root / "src/lexer/language_keyword_registry.def"
projection = root / "src/self_hosted/lsp/hover_content_projection_owner.pgy"
c_owner = root / "src/lsp/pgy_lsp_hover.c"
self_host_owner = root / "src/self_hosted/lsp/hover_content_owner.pgy"

rows = hover.load_hover_rows(presentation)
language_rows = [row for row in rows if row.language_word]
builtin_rows = [row for row in rows if not row.language_word]
exposed = hover.load_hover_exposure(language_registry)
hover.require_language_exposure(rows, exposed)

if len(language_rows) != 25 or len(builtin_rows) != 7 or len(rows) != 32:
    fail(
        f"row count drift: language={len(language_rows)} "
        f"builtin={len(builtin_rows)} total={len(rows)}"
    )
expected_builtins = {"Err", "Log", "LogBanner", "LogBlock", "LogRaw", "Ok", "Unwrap"}
if {row.word for row in builtin_rows} != expected_builtins:
    fail("builtin hover set drifted")
if {row.word for row in language_rows} != exposed:
    fail("lowercase presentation rows and registry HOVER flags differ")

for label, falsified_rows in (
    ("lost hover", [row for row in rows if row.word != "func"]),
    (
        "unregistered hover",
        rows + [hover.HoverRow("unregistered_hover", "**unregistered**", True)],
    ),
):
    try:
        hover.require_language_exposure(falsified_rows, exposed)
    except SystemExit:
        pass
    else:
        fail(f"renderer accepted {label} falsifier")

expected_projection = hover.render(rows)
if projection.read_text(encoding="utf-8") != expected_projection:
    fail("generated self-host projection differs from presentation owner")

c_source = c_owner.read_text(encoding="utf-8")
self_host_source = self_host_owner.read_text(encoding="utf-8")
required_c_terms = (
    '#include "lsp_hover_content.def"',
    '#include "../lexer/lexer_keywords.h"',
    "PGY_KEYWORD_TOOLING_HOVER",
    "lsp_language_word_hover_enabled(word)",
    "json_escape_copy(escaped_hover, sizeof(escaped_hover), hover_text)",
)
for term in required_c_terms:
    if term not in c_source:
        fail(f"C hover projection is missing: {term}")
if '"func"' in c_source or "**func** - Function declaration" in c_source:
    fail("C hover owner reintroduced hand-written presentation rows")

required_self_host_terms = (
    'import "hover_content_projection_owner.pgy";',
    "return LspHoverPresentationTextForWord(word);",
    "LspHoverPresentationProjectionReady()",
)
for term in required_self_host_terms:
    if term not in self_host_source:
        fail(f"self-host hover consumer is missing: {term}")
if "if word ==" in self_host_source or "**func** - Function declaration" in self_host_source:
    fail("self-host hover consumer reintroduced a presentation table")

representative = "**func** - Function declaration"
allowed_prose_owners = {presentation.resolve(), projection.resolve()}
for base in (root / "src/lsp", root / "src/self_hosted/lsp"):
    for path in base.rglob("*"):
        if not path.is_file() or path.suffix not in {".c", ".h", ".pgy", ".def"}:
            continue
        if representative in path.read_text(encoding="utf-8", errors="replace"):
            if path.resolve() not in allowed_prose_owners:
                fail(f"independent hover prose table reappeared in {path.relative_to(root)}")

if "world" in exposed or "not_a_hover_word" in exposed:
    fail("negative runtime witnesses unexpectedly became HOVER-exposed")


def lsp_messages(payload: bytes):
    messages = []
    cursor = 0
    while cursor < len(payload):
        header_end = payload.find(b"\r\n\r\n", cursor)
        if header_end < 0:
            fail("runtime LSP output has an incomplete header")
        headers = {}
        for line in payload[cursor:header_end].decode("ascii").split("\r\n"):
            name, separator, value = line.partition(":")
            if not separator:
                fail(f"runtime LSP output has malformed header {line!r}")
            headers[name.lower()] = value.strip()
        if "content-length" not in headers:
            fail("runtime LSP output is missing Content-Length")
        start = header_end + 4
        end = start + int(headers["content-length"])
        if end > len(payload):
            fail("runtime LSP output has an incomplete body")
        messages.append(json.loads(payload[start:end].decode("utf-8")))
        cursor = end
    return messages


runtime_checked = False
self_host_runtime_checked = False
if lsp_bin:
    source = "// func world not_a_hover_word Err subject\n"
    uri = "file:///tmp/pgy_lsp_hover_registry.pgy"
    requests = [
        {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {"processId": None, "rootUri": None, "capabilities": {}},
        },
        {"jsonrpc": "2.0", "method": "initialized", "params": {}},
        {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": uri,
                    "languageId": "pergyra",
                    "version": 1,
                    "text": source,
                }
            },
        },
    ]
    for request_id, word in (
        (10, "func"),
        (11, "world"),
        (12, "not_a_hover_word"),
        (13, "Err"),
        (14, "subject"),
    ):
        requests.append(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": "textDocument/hover",
                "params": {
                    "textDocument": {"uri": uri},
                    "position": {
                        "line": 0,
                        "character": source.index(word) + 1,
                    },
                },
            }
        )
    requests.extend(
        [
            {"jsonrpc": "2.0", "id": 2, "method": "shutdown", "params": None},
            {"jsonrpc": "2.0", "method": "exit", "params": None},
        ]
    )
    wire = bytearray()
    for request in requests:
        body = json.dumps(request, separators=(",", ":")).encode("utf-8")
        wire.extend(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii"))
        wire.extend(body)
    proc = subprocess.run(
        [lsp_bin],
        input=bytes(wire),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=10,
    )
    if proc.returncode != 0 or proc.stderr:
        fail(
            "runtime LSP probe failed: "
            + proc.stderr.decode("utf-8", errors="replace")
        )
    responses = {
        message.get("id"): message
        for message in lsp_messages(proc.stdout)
        if isinstance(message, dict) and "id" in message
    }
    if responses.get(10, {}).get("result") is None:
        fail("runtime lost registered func hover")
    if responses.get(13, {}).get("result") is None:
        fail("runtime lost registered Err builtin hover")
    if responses.get(11, {}).get("result") is not None:
        fail("runtime exposed registry word without HOVER flag: world")
    if responses.get(12, {}).get("result") is not None:
        fail("runtime exposed unregistered hover word")
    subject_result = responses.get(14, {}).get("result")
    if not isinstance(subject_result, dict):
        fail("runtime lost registered subject hover")
    c_subject = subject_result.get("contents", {}).get("value")
    expected_subject = next(row.markdown for row in language_rows if row.word == "subject")
    if c_subject != expected_subject or "\n- subject values" not in c_subject:
        fail("C runtime subject hover did not decode canonical markdown newlines")
    runtime_checked = True

if self_host_bin:
    source = "// subject world not_a_hover_word Err\n"
    uri = "file:///tmp/pgy_selfhost_hover_registry.pgy"
    open_body = {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {
            "textDocument": {
                "uri": uri,
                "languageId": "pergyra",
                "version": 1,
                "text": source,
            }
        },
    }
    bodies = [open_body]
    for request_id, word in (
        (20, "subject"),
        (21, "world"),
        (22, "not_a_hover_word"),
        (23, "Err"),
    ):
        bodies.append(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": "textDocument/hover",
                "params": {
                    "textDocument": {"uri": uri},
                    "position": {
                        "line": 0,
                        "character": source.index(word) + 1,
                    },
                },
            }
        )
    wire = bytearray()
    for body_value in bodies:
        body = json.dumps(body_value, separators=(",", ":")).encode("utf-8")
        wire.extend(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii"))
        wire.extend(body)
    proc = subprocess.run(
        [self_host_bin, "--hover-content-probe", "8192"],
        input=bytes(wire),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=10,
    )
    if proc.returncode != 0 or proc.stderr:
        fail(
            "self-host runtime probe failed: "
            + proc.stderr.decode("utf-8", errors="replace")
        )
    try:
        artifact = json.loads(proc.stdout.decode("utf-8").strip())
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"self-host runtime emitted invalid JSON: {exc}")
    events = {event.get("id"): event for event in artifact.get("events", [])}
    self_subject = (
        events.get("20", {}).get("result", {}).get("contents", {}).get("value")
    )
    expected_subject = next(row.markdown for row in language_rows if row.word == "subject")
    if self_subject != expected_subject or "\n- subject values" not in self_subject:
        fail("self-host runtime subject hover did not decode canonical markdown newlines")
    if lsp_bin and self_subject != c_subject:
        fail("C and self-host runtimes decoded different subject markdown")
    if events.get("21", {}).get("result") is not None:
        fail("self-host runtime exposed word without HOVER flag: world")
    if events.get("22", {}).get("result") is not None:
        fail("self-host runtime exposed unregistered hover word")
    if events.get("23", {}).get("result") is None:
        fail("self-host runtime lost registered Err builtin hover")
    self_host_runtime_checked = True

print(
    "[lsp-hover-registry] ok "
    f"(language={len(language_rows)} builtin={len(builtin_rows)} "
    f"c-runtime={'yes' if runtime_checked else 'skipped'} "
    f"selfhost-runtime={'yes' if self_host_runtime_checked else 'skipped'})"
)
PY

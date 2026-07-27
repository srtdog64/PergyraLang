#!/usr/bin/env bash
# Proves LSP completion is a bounded projection of LanguageKeywordRegistry.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PROTOCOL="$ROOT_DIR/src/lsp/pgy_lsp_protocol.c"
INTERNAL="$ROOT_DIR/src/lsp/pgy_lsp_internal.h"
CALLSITE="$ROOT_DIR/src/lsp/pgy_lsp.c"
SELF_HOST_OWNER="$ROOT_DIR/src/self_hosted/lsp/completion_owner.pgy"
SELF_HOST_FEATURE="$ROOT_DIR/src/self_hosted/lsp/feature_owner.pgy"
SELF_HOST_RESPONSE="$ROOT_DIR/src/self_hosted/lsp/response_owner.pgy"
SELF_HOST_MAIN="$ROOT_DIR/src/self_hosted/lsp/main.pgy"
CC_BIN="${PGY_CC:-${CC:-cc}}"
TMP_DIR="$(mktemp -d)"
PROBE="$TMP_DIR/lsp-completion-registry-probe"

trap 'rm -rf "$TMP_DIR"' EXIT

fail() {
    echo "[lsp-completion-registry] $*" >&2
    exit 1
}

grep -Fq 'lexer_keyword_registry_count()' "$PROTOCOL" ||
    fail "completion owner does not enumerate the registry"
grep -Fq 'lexer_keyword_registry_row(row_index)' "$PROTOCOL" ||
    fail "completion owner does not read registry rows"
grep -Fq 'PGY_KEYWORD_TOOLING_COMPLETION' "$PROTOCOL" ||
    fail "completion owner does not select the registry completion flag"
grep -Fq 'lsp_completion_items_json()' "$CALLSITE" ||
    fail "completion request does not consume the registry projection"
grep -Fq 'lsp_build_completion_items_json' "$INTERNAL" ||
    fail "completion builder contract is not declared"

for path in "$SELF_HOST_OWNER" "$SELF_HOST_FEATURE" \
    "$SELF_HOST_RESPONSE" "$SELF_HOST_MAIN"; do
    [[ -f "$path" ]] || fail "missing ${path#"$ROOT_DIR/"}"
done
for term in 'LanguageWordRegistryCount()' \
    'LanguageWordRegistryProjectionReady()' \
    'LanguageWordSpellingAt(index)' \
    'LanguageWordCompletionEnabledAt(index)' \
    'LanguageWordCompletionOwnedAt(index)' \
    'LanguageWordClassNameAt(index)' \
    'LanguageWordAxisNameAt(index)' \
    'LspCompletionSpellingStrictlyAfter(previous, spelling)' \
    'LspCompletionOwnedItemCount()' \
    'JsonEmitFieldString("label", spelling)' \
    'JsonEmitFieldNumber("kind", "14")' \
    'JsonEmitFieldString("detail", detail)'; do
    grep -Fq -- "$term" "$SELF_HOST_OWNER" ||
        fail "self-host completion owner is missing $term"
done
grep -Fq 'import "completion_owner.pgy";' "$SELF_HOST_FEATURE" ||
    fail "self-host feature dispatch does not import completion owner"
grep -Fq 'return LspCompletionItemsJson();' "$SELF_HOST_FEATURE" ||
    fail "self-host completion response does not consume registry projection"
grep -Fq '\"completionProvider\":{\"resolveProvider\":false}' \
    "$SELF_HOST_RESPONSE" ||
    fail "self-host server does not advertise completionProvider"
if grep -Fq 'semanticTokensProvider' "$SELF_HOST_RESPONSE"; then
    fail "self-host initialize unexpectedly advertises semanticTokens"
fi
if grep -Fq '"items", "[]"' "$SELF_HOST_FEATURE" \
    || grep -Fq '"items":[]' "$SELF_HOST_FEATURE"; then
    fail "advertised self-host completion restored the empty fallback"
fi
if grep -R -Fq --include='*.pgy' '\"items\":[]' \
    "$ROOT_DIR/src/self_hosted/lsp" \
    || grep -R -Fq --include='*.pgy' '"items", "[]"' \
        "$ROOT_DIR/src/self_hosted/lsp" \
    || grep -R -Fq --include='*.pgy' 'JsonEmitFieldRaw("items"' \
        "$ROOT_DIR/src/self_hosted/lsp"; then
    fail "self-host LSP source reintroduced an independent empty items fallback"
fi
if grep -Eq '\\"label\\":\\"[A-Za-z_]' "$SELF_HOST_OWNER"; then
    fail "self-host completion owner reintroduced word-specific labels"
fi

if grep -Eq '\\"label\\":\\"[A-Za-z_]' "$PROTOCOL"; then
    fail "word-specific completion JSON labels reappeared"
fi
if grep -Eq 'const[[:space:]]+char[[:space:]]*\*[[:space:]]*lsp_completion_items[[:space:]]*=' "$PROTOCOL"; then
    fail "the removed hardcoded completion array reappeared"
fi

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/lsp_completion_registry_probe.c" \
    "$ROOT_DIR/src/lsp/pgy_lsp_protocol.c" \
    "$ROOT_DIR/src/lexer/lexer_keywords.c" \
    "$ROOT_DIR/src/common/numeric_parse.c" \
    -o "$PROBE"

if [[ -x "$PROBE.exe" ]]; then
    PROBE="$PROBE.exe"
fi
"$PROBE"

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required for runtime parity"
    fi
fi

PGY_LSP="${PGY_LSP_BIN:-$ROOT_DIR/bin/pgy-lsp}"
if [[ "$PGY_LSP" != *.exe && -x "${PGY_LSP}.exe" ]]; then
    PGY_LSP="${PGY_LSP}.exe"
fi
[[ -x "$PGY_LSP" ]] || fail "missing native LSP binary: $PGY_LSP"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
SELF_HOST_BIN="$TMP_DIR/lsp-completion-selfhost.exe"
if ! (cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$SELF_HOST_MAIN")" \
    --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$SELF_HOST_BIN")" \
    >"$TMP_DIR/selfhost.compile.log" 2>&1); then
    cat "$TMP_DIR/selfhost.compile.log" >&2
    fail "self-host completion projection failed to build"
fi

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B - \
    "$PGY_LSP" "$SELF_HOST_BIN" <<'PY'
from __future__ import annotations

import json
import subprocess
import sys


native_bin, self_host_bin = sys.argv[1:]


def fail(message: str) -> None:
    raise SystemExit(f"[lsp-completion-registry] {message}")


def frame(body: dict[str, object]) -> bytes:
    payload = json.dumps(body, separators=(",", ":")).encode("utf-8")
    return f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii") + payload


def parse_wire(payload: bytes) -> list[dict[str, object]]:
    messages: list[dict[str, object]] = []
    cursor = 0
    while cursor < len(payload):
        header_end = payload.find(b"\r\n\r\n", cursor)
        if header_end < 0:
            fail("native output has an incomplete header")
        headers: dict[str, str] = {}
        for line in payload[cursor:header_end].decode("ascii").split("\r\n"):
            name, separator, value = line.partition(":")
            if not separator:
                fail(f"native output has malformed header {line!r}")
            headers[name.lower()] = value.strip()
        if "content-length" not in headers:
            fail("native output is missing Content-Length")
        start = header_end + 4
        end = start + int(headers["content-length"])
        if end > len(payload):
            fail("native output has an incomplete body")
        messages.append(json.loads(payload[start:end].decode("utf-8")))
        cursor = end
    return messages


initialize = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {"processId": None, "rootUri": None, "capabilities": {}},
}
completion = {
    "jsonrpc": "2.0",
    "id": 4,
    "method": "textDocument/completion",
    "params": {},
}
native_wire = b"".join(
    (
        frame(initialize),
        frame(completion),
        frame({"jsonrpc": "2.0", "id": 2, "method": "shutdown"}),
        frame({"jsonrpc": "2.0", "method": "exit"}),
    )
)
native = subprocess.run(
    [native_bin],
    input=native_wire,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    timeout=15,
)
if native.returncode != 0 or native.stderr:
    fail("native runtime failed: " + native.stderr.decode("utf-8", errors="replace"))
native_responses = {
    message.get("id"): message for message in parse_wire(native.stdout)
}
native_items = native_responses.get(4, {}).get("result")

self_wire = frame(initialize) + frame(completion)
self_host = subprocess.run(
    [self_host_bin, "--response-probe", "16384"],
    input=self_wire,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    timeout=15,
)
if self_host.returncode != 0 or self_host.stderr:
    fail(
        "self-host runtime failed: "
        + self_host.stderr.decode("utf-8", errors="replace")
    )
try:
    artifact = json.loads(self_host.stdout.decode("utf-8").strip())
except (UnicodeDecodeError, json.JSONDecodeError) as exc:
    fail(f"self-host runtime emitted invalid JSON: {exc}")
plans = {
    plan.get("method"): plan for plan in artifact.get("responses", [])
}
initialize_body = json.loads(plans.get("initialize", {}).get("body", "null"))
self_body = json.loads(
    plans.get("textDocument/completion", {}).get("body", "null")
)
self_items = self_body.get("result") if isinstance(self_body, dict) else None
capabilities = (
    initialize_body.get("result", {}).get("capabilities", {})
    if isinstance(initialize_body, dict)
    else {}
)
if capabilities.get("completionProvider") != {"resolveProvider": False}:
    fail("self-host runtime does not advertise the owned completion provider")
if "semanticTokensProvider" in capabilities:
    fail("self-host runtime unexpectedly advertises semanticTokens")
if not isinstance(native_items, list) or not isinstance(self_items, list):
    fail("native or self-host completion result is not an item array")
if native_items != self_items:
    fail("native and self-host completion structures differ")
if not self_items:
    fail("registry-owned completion projection is unexpectedly empty")
labels = [item.get("label") for item in self_items if isinstance(item, dict)]
if len(labels) != len(self_items) or labels != sorted(labels) \
        or len(set(labels)) != len(labels):
    fail("self-host completion labels are unsorted or duplicated")
for item in self_items:
    if not isinstance(item, dict) or list(item) != ["label", "kind", "detail"]:
        fail("self-host completion item field shape/order drifted")
    if item["kind"] != 14 or not isinstance(item["label"], str):
        fail("self-host completion label/kind contract drifted")
    detail = item.get("detail")
    if not isinstance(detail, str) or not detail.startswith("Pergyra ") \
            or not detail.endswith(" keyword"):
        fail("self-host completion detail contract drifted")
if "domain" in labels or "sync" in labels:
    fail("self-host completion exposed a registry row without completion flag")

print(
    "[lsp-completion-registry] self-host parity ok "
    f"({len(self_items)} registry-owned items)"
)
PY

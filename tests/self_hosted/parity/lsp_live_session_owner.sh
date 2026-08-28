#!/usr/bin/env bash
# Focused G-LSP-STREAM falsifier: one Pergyra process owns fragmented framing,
# typed document revisions, semantic hover, and shutdown/exit lifecycle.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:lsp-live-session] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_lsp_live.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
pgy_require_runnable_binary_here "lsp-live-session" "$PGY"

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="$(command -v python3)"
else
    PYTHON_BIN="$(command -v python)"
fi
[[ -n "$PYTHON_BIN" ]] || fail "python is required"

SOURCE="$ROOT_DIR/src/self_hosted/lsp/main.pgy"
OUT_BASE="$WORK_DIR/pgy-self-lsp-live"
if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$OUT_BASE")" \
    >"$WORK_DIR/compile.log" 2>&1); then
    cat "$WORK_DIR/compile.log" >&2
    fail "live owner failed to compile"
fi
LSP_BIN="$(pgy_select_optional_exe_binary "$OUT_BASE")"
[[ -x "$LSP_BIN" ]] || fail "missing compiled live owner: $LSP_BIN"
RUNTIME_BIN="$LSP_BIN"
if [[ -n "${PGY_LSP_BIN:-}" ]]; then
    RUNTIME_BIN="$(pgy_select_optional_exe_binary "$PGY_LSP_BIN")"
    [[ -x "$RUNTIME_BIN" ]] || fail "missing public LSP binary: $RUNTIME_BIN"
    # Focused source runs use the just-compiled sibling. Publication-boundary
    # runs may pass the installed sibling explicitly without changing the
    # protocol harness or weakening the missing-sibling negative below.
    export PGY_SELF_LSP_BIN="${PGY_SELF_LSP_BIN:-$LSP_BIN}"
fi

"$PYTHON_BIN" - "$RUNTIME_BIN" <<'PY'
import json
import queue
import subprocess
import sys
import threading
import time


binary = sys.argv[1]


def fail(message: str) -> None:
    raise SystemExit(f"[self-host-parity:lsp-live-session] {message}")


def frame(message: dict[str, object]) -> bytes:
    body = json.dumps(message, separators=(",", ":")).encode("utf-8")
    return f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body


class LiveProcess:
    def __init__(self) -> None:
        self.process = subprocess.Popen(
            [binary],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.messages: queue.Queue[object] = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self) -> None:
        try:
            while True:
                header = bytearray()
                while not header.endswith(b"\r\n\r\n"):
                    byte = self.process.stdout.read(1)
                    if not byte:
                        self.messages.put(None)
                        return
                    header.extend(byte)
                headers: dict[str, str] = {}
                for line in header[:-4].decode("ascii").split("\r\n"):
                    name, separator, value = line.partition(":")
                    if not separator:
                        fail(f"malformed response header: {line!r}")
                    headers[name.lower()] = value.strip()
                if "content-length" not in headers:
                    fail("response frame lost Content-Length")
                remaining = int(headers["content-length"])
                body = bytearray()
                while len(body) < remaining:
                    chunk = self.process.stdout.read(remaining - len(body))
                    if not chunk:
                        fail("response body ended early")
                    body.extend(chunk)
                self.messages.put(json.loads(body.decode("utf-8")))
        except BaseException as exc:
            self.messages.put(exc)

    def send(self, payload: bytes) -> None:
        self.process.stdin.write(payload)
        self.process.stdin.flush()

    def next(self, timeout: float = 8.0) -> dict[str, object]:
        try:
            item = self.messages.get(timeout=timeout)
        except queue.Empty:
            fail("timed out waiting for a live response")
        if isinstance(item, BaseException):
            raise item
        if item is None:
            fail("live process closed stdout before the expected response")
        return item

    def until_id(self, request_id: int) -> dict[str, object]:
        while True:
            message = self.next()
            if message.get("id") == request_id:
                return message

    def until_method(self, method: str) -> dict[str, object]:
        while True:
            message = self.next()
            if message.get("method") == method:
                return message


initialize = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {"capabilities": {}},
}
uri = "file:///tmp/pgy_live_owner.pgy"
source_v1 = "func Main() -> Void {}"
source_v2 = "subject Hero {}\nfunc Main() -> Void {}"
did_open = {
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": uri,
            "languageId": "pergyra",
            "version": 1,
            "text": source_v1,
        }
    },
}
hover_v1 = {
    "jsonrpc": "2.0",
    "id": 10,
    "method": "textDocument/hover",
    "params": {
        "textDocument": {"uri": uri},
        "position": {"line": 0, "character": 1},
    },
}
did_change = {
    "jsonrpc": "2.0",
    "method": "textDocument/didChange",
    "params": {
        "textDocument": {"uri": uri, "version": 2},
        "contentChanges": [{"text": source_v2}],
    },
}
hover_v2 = {
    "jsonrpc": "2.0",
    "id": 11,
    "method": "textDocument/hover",
    "params": {
        "textDocument": {"uri": uri},
        "position": {"line": 0, "character": 1},
    },
}

live = LiveProcess()
live.send(frame(initialize))
if live.until_id(1).get("result", {}).get("serverInfo", {}).get("name") != "pgy-lsp":
    fail("initialize response lost the owned server identity")
live.send(frame(did_open))
live.until_method("textDocument/publishDiagnostics")
live.send(frame(hover_v1))
if live.until_id(10).get("result", {}).get("contents", {}).get("value") != (
    "**func** - Function declaration"
):
    fail("hover did not read didOpen revision 1")

change_wire = frame(did_change)
split = len(change_wire) - max(1, len(change_wire) // 4)
live.send(change_wire[:split])
try:
    unexpected = live.messages.get(timeout=0.35)
except queue.Empty:
    unexpected = "no-frame"
if unexpected != "no-frame":
    fail("partial didChange body produced output before completion")
live.send(change_wire[split:])
live.until_method("textDocument/publishDiagnostics")
live.send(frame(hover_v2))
subject = live.until_id(11).get("result", {}).get("contents", {}).get("value")
if not isinstance(subject, str) or not subject.startswith("**subject**"):
    fail("hover did not observe the split didChange revision 2")

live.send(frame({"jsonrpc": "2.0", "id": 2, "method": "shutdown"}))
if live.until_id(2).get("result", "missing") is not None:
    fail("shutdown response is not null")
live.send(frame({"jsonrpc": "2.0", "method": "exit"}))
live.process.stdin.close()
if live.process.wait(timeout=8.0) != 0:
    fail(live.process.stderr.read().decode("utf-8", errors="replace"))
if live.process.stderr.read():
    fail("successful live session emitted stderr")


def expect_rejected_change(version: int, text: str, label: str) -> None:
    process = LiveProcess()
    process.send(frame(initialize))
    process.until_id(1)
    process.send(frame(did_open))
    process.until_method("textDocument/publishDiagnostics")
    rejected = {
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {"uri": uri, "version": version},
            "contentChanges": [{"text": text}],
        },
    }
    process.send(frame(rejected))
    process.process.stdin.close()
    rc = process.process.wait(timeout=8.0)
    if rc == 0:
        fail(f"{label} didChange did not fail closed")


expect_rejected_change(0, "stale", "stale-version")
expect_rejected_change(1, "conflict", "same-version")

incomplete = LiveProcess()
incomplete.send(b"Content-Length: 10\r\n\r\n{}")
incomplete.process.stdin.close()
if incomplete.process.wait(timeout=8.0) == 0:
    fail("incomplete body at EOF did not fail closed")
if incomplete.process.stdout.read():
    fail("incomplete body emitted a partial response")


def expect_rejected_transport(payload: bytes, label: str) -> None:
    process = LiveProcess()
    process.send(payload)
    process.process.stdin.close()
    if process.process.wait(timeout=8.0) == 0:
        fail(f"{label} Content-Length did not fail closed")
    if process.process.stdout.read():
        fail(f"{label} Content-Length emitted a partial response")


expect_rejected_transport(
    b"Content-Length: 262145\r\n\r\n", "over-limit"
)
expect_rejected_transport(
    b"Content-Length: 999999999999999999999999999999\r\n\r\n",
    "integer-overflow",
)

print("[self-host-parity:lsp-live-session] live fragmented session owner ok")
PY

if [[ -n "${PGY_LSP_BIN:-}" ]]; then
    MISSING_PUBLIC="$WORK_DIR/missing-public-lsp.exe"
    cp "$RUNTIME_BIN" "$MISSING_PUBLIC"
    set +e
    env -u PGY_SELF_LSP_BIN "$MISSING_PUBLIC" \
        </dev/null >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
    missing_rc=$?
    set -e
    [[ "$missing_rc" -ne 0 ]] ||
        fail "missing installed sibling silently entered the native loop"
    [[ ! -s "$WORK_DIR/missing.out" ]] ||
        fail "missing installed sibling emitted a partial protocol response"
    grep -Fq "self-host live session is unavailable" "$WORK_DIR/missing.err" ||
        fail "missing installed sibling lost its owned boundary diagnostic"
fi

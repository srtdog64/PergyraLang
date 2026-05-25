#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY_BIN_WAS_EXPLICIT=0
PGY_LSP_BIN_WAS_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
    PGY_BIN_WAS_EXPLICIT=1
else
    PGY="$ROOT_DIR/bin/pgy"
fi
if [[ -n "${PGY_LSP_BIN:-}" ]]; then
    PGY_LSP="$PGY_LSP_BIN"
    PGY_LSP_BIN_WAS_EXPLICIT=1
else
    PGY_LSP="$ROOT_DIR/bin/pgy-lsp"
fi
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ "$PGY_LSP" != *.exe && -x "${PGY_LSP}.exe" ]]; then
    PGY_LSP="${PGY_LSP}.exe"
fi
PGY="$(pgy_path_for_bash_tool "$PGY")"
PGY_LSP="$(pgy_path_for_bash_tool "$PGY_LSP")"

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_tooling_conformance.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[tooling-conformance] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
    echo "[tooling-conformance] SKIP executable probe; missing compiler binary: $PGY"
    exit 0
fi
if [[ ! -x "$PGY_LSP" ]]; then
    if [[ "$PGY_LSP_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[tooling-conformance] missing explicit LSP binary: $PGY_LSP" >&2
        exit 1
    fi
    echo "[tooling-conformance] SKIP executable probe; missing LSP binary: $PGY_LSP"
    exit 0
fi

if "$PGY" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    :
else
    rc=$?
    if [[ "$rc" -eq 126 || "$rc" -eq 127 ]]; then
        if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
            echo "[tooling-conformance] shell cannot launch explicit compiler binary: $PGY" >&2
            exit "$rc"
        fi
        echo "[tooling-conformance] SKIP executable probe; shell cannot launch compiler binary: $PGY"
        exit 0
    fi
    cat "$WORK_DIR/pgy-help.err" >&2
    exit "$rc"
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        PYTHON_BIN=""
    fi
fi

PGY_BIN="$PGY" PGY_CC="${PGY_CC:-cc}" bash "$ROOT_DIR/tests/fmt_smoke.sh" >/dev/null

DEBUG_SOURCE="$WORK_DIR/debug_case.pgy"
DEBUG_SOURCE_ARG="$(pgy_path_for_compiler "$PGY" "$DEBUG_SOURCE")"
cat > "$DEBUG_SOURCE" <<'EOF'
func Main() -> Void
{
    Log(1);
}
EOF

printf 'q\n' | "$PGY" debug "$DEBUG_SOURCE_ARG" >"$WORK_DIR/debug.out" 2>"$WORK_DIR/debug.err"
grep -Fq "Pergyra Debugger v0.1" "$WORK_DIR/debug.out"
grep -Fq "Commands: n(ext), c(ontinue), b <line>, l(ist), q(uit)" "$WORK_DIR/debug.out"
grep -Fq "(pgy-debug:" "$WORK_DIR/debug.out"
grep -Fq "0 error(s), 0 warning(s)" "$WORK_DIR/debug.err"
if grep -Fq "pgy debug:" "$WORK_DIR/debug.err"; then
    echo "debugger emitted a command failure during conformance smoke:" >&2
    cat "$WORK_DIR/debug.err" >&2
    exit 1
fi

if [[ -z "$PYTHON_BIN" ]]; then
    lsp_body='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}}'
    shutdown_body='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'
    exit_body='{"jsonrpc":"2.0","method":"exit","params":null}'
    {
        printf 'Content-Length: %s\r\n\r\n%s' "${#lsp_body}" "$lsp_body"
        printf 'Content-Length: %s\r\n\r\n%s' "${#shutdown_body}" "$shutdown_body"
        printf 'Content-Length: %s\r\n\r\n%s' "${#exit_body}" "$exit_body"
    } | "$PGY_LSP" >"$WORK_DIR/lsp.out" 2>"$WORK_DIR/lsp.err"
    if [[ -s "$WORK_DIR/lsp.err" ]]; then
        echo "LSP emitted stderr during shell fallback conformance smoke:" >&2
        cat "$WORK_DIR/lsp.err" >&2
        exit 1
    fi
    grep -Fq '"serverInfo":{"name":"pgy-lsp","version":"0.1"}' "$WORK_DIR/lsp.out"
    grep -Fq '"experimental":{"airSchema":"pgy.air.graph.v1"' "$WORK_DIR/lsp.out"
    grep -Fq '"observabilitySchema":"pgy.intent.observability.v1"' "$WORK_DIR/lsp.out"
    echo "[tooling-conformance] python not found; shell LSP initialize contract ok"
    echo "tooling-conformance-smoke: PASS"
    exit 0
fi

"$PYTHON_BIN" - "$PGY_LSP" "$WORK_DIR/lsp.out" <<'PY'
import json
import subprocess
import sys

lsp_bin = sys.argv[1]
out_path = sys.argv[2]

source = "func Main() {\n    Log(1);\n}\n"
invalid_source = "func Main() -> Void\n{\n    let x: Int = ;\n}\n"
uri = "file:///tmp/pgy_tooling_conformance.pgy"

messages = [
    {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "processId": None,
            "rootUri": None,
            "capabilities": {},
        },
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
    {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "textDocument/hover",
        "params": {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": 1},
        },
    },
    {
        "jsonrpc": "2.0",
        "id": 3,
        "method": "textDocument/completion",
        "params": {
            "textDocument": {"uri": uri},
            "position": {"line": 2, "character": 4},
        },
    },
    {
        "jsonrpc": "2.0",
        "id": 4,
        "method": "textDocument/documentSymbol",
        "params": {"textDocument": {"uri": uri}},
    },
    {
        "jsonrpc": "2.0",
        "id": 5,
        "method": "textDocument/definition",
        "params": {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": 5},
        },
    },
    {
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {"uri": uri, "version": 2},
            "contentChanges": [{"text": invalid_source}],
        },
    },
    {"jsonrpc": "2.0", "id": 6, "method": "shutdown", "params": None},
    {"jsonrpc": "2.0", "method": "exit", "params": None},
]

payload = bytearray()
for msg in messages:
    body = json.dumps(msg, separators=(",", ":")).encode("utf-8")
    payload.extend(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii"))
    payload.extend(body)

proc = subprocess.run(
    [lsp_bin],
    input=bytes(payload),
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    timeout=10,
)

stdout = proc.stdout.decode("utf-8", errors="replace")
stderr = proc.stderr.decode("utf-8", errors="replace")
with open(out_path, "w", encoding="utf-8") as f:
    f.write(stdout)

if proc.returncode != 0:
    sys.stderr.write(stderr)
    raise SystemExit(proc.returncode)
if stderr:
    sys.stderr.write("LSP emitted stderr during conformance smoke:\n")
    sys.stderr.write(stderr)
    raise SystemExit(1)

required = [
    '"serverInfo":{"name":"pgy-lsp","version":"0.1"}',
    '"experimental":{"airSchema":"pgy.air.graph.v1"',
    '"observabilitySchema":"pgy.intent.observability.v1"',
    '"traceSchema":"pgy.intent.trace.v1"',
    '"observabilitySurfaces":["last","history","active","recent"]',
    '"textDocumentSync":1',
    '"hoverProvider":true',
    '"completionProvider":{"resolveProvider":false}',
    '"Function declaration"',
    '"id":4,"result":[{"name":"Main","kind":12',
    '"id":5,"result":{"uri":"file:///tmp/pgy_tooling_conformance.pgy"',
    '"label":"subject"',
    '"method":"textDocument/publishDiagnostics"',
    '"code":"PGY_PARSE_SYNTAX"',
    '"data":{"layer":"syntax"',
]
missing = [needle for needle in required if needle not in stdout]
if missing:
    sys.stderr.write("LSP conformance output missing:\n")
    for needle in missing:
        sys.stderr.write(f"  {needle}\n")
    sys.stderr.write(stdout)
    raise SystemExit(1)
PY

grep -Fq '"serverInfo":{"name":"pgy-lsp","version":"0.1"}' "$WORK_DIR/lsp.out"
grep -Fq '"observabilitySchema":"pgy.intent.observability.v1"' "$WORK_DIR/lsp.out"
grep -Fq '"label":"subject"' "$WORK_DIR/lsp.out"

echo "tooling-conformance-smoke: PASS"

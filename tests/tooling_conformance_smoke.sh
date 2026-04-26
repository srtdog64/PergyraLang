#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_LSP="${PGY_LSP_BIN:-$ROOT_DIR/bin/pgy-lsp}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ "$PGY_LSP" != *.exe && -x "${PGY_LSP}.exe" ]]; then
    PGY_LSP="${PGY_LSP}.exe"
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_tooling_conformance.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi
if [[ ! -x "$PGY_LSP" ]]; then
    echo "missing LSP binary: $PGY_LSP" >&2
    exit 1
fi

PGY_BIN="$PGY" PGY_CC="${PGY_CC:-cc}" bash "$ROOT_DIR/tests/fmt_smoke.sh" >/dev/null

DEBUG_SOURCE="$WORK_DIR/debug_case.pgy"
cat > "$DEBUG_SOURCE" <<'EOF'
func Main() -> Void
{
    Log(1);
}
EOF

printf 'q\n' | "$PGY" debug "$DEBUG_SOURCE" >"$WORK_DIR/debug.out" 2>"$WORK_DIR/debug.err"
grep -Fq "Pergyra Debugger v0.1" "$WORK_DIR/debug.out"
grep -Fq "Commands: n(ext), c(ontinue), b <line>, l(ist), q(uit)" "$WORK_DIR/debug.out"
grep -Fq "(pgy-debug:" "$WORK_DIR/debug.out"
grep -Fq "0 error(s), 0 warning(s)" "$WORK_DIR/debug.err"
if grep -Fq "pgy debug:" "$WORK_DIR/debug.err"; then
    echo "debugger emitted a command failure during conformance smoke:" >&2
    cat "$WORK_DIR/debug.err" >&2
    exit 1
fi

python3 - "$PGY_LSP" "$WORK_DIR/lsp.out" <<'PY'
import json
import subprocess
import sys

lsp_bin = sys.argv[1]
out_path = sys.argv[2]

source = "func Main() -> Void\n{\n    Log(1);\n}\n"
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
    {"jsonrpc": "2.0", "id": 4, "method": "shutdown", "params": None},
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
    '"textDocumentSync":1',
    '"hoverProvider":true',
    '"completionProvider":{"resolveProvider":false}',
    '"Function declaration"',
    '"label":"subject"',
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
grep -Fq '"label":"subject"' "$WORK_DIR/lsp.out"

echo "tooling-conformance-smoke: PASS"

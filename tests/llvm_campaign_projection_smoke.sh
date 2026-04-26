#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
elif [[ -x "$TMP_PGY" && ( ! -x "$DEFAULT_PGY" || "$TMP_PGY" -nt "$DEFAULT_PGY" ) ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "[llvm-campaign-projection] missing python" >&2
        exit 1
    fi
fi

if [[ ! -x "$PGY" ]]; then
    echo "[llvm-campaign-projection] missing compiler binary: $PGY" >&2
    exit 1
fi

output="$("$PGY" "$ROOT_DIR/examples/campaign_graph_fsm/main.pgy" \
    --run --backend=llvm -o "${TMP_BASE%/}/pgy-campaign-projection-llvm" 2>&1)"

"$PYTHON_BIN" - "$ROOT_DIR" "$output" <<'PY'
import difflib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
actual = sys.argv[2]
expected_path = root / "examples" / "campaign_graph_fsm" / "expected_stdout.txt"
expected = expected_path.read_text(encoding="utf-8")

def normalize(text: str) -> list[str]:
    lines = []
    seen = False
    for raw in text.replace("\r", "").splitlines():
        if raw in {
            "0 error(s), 0 warning(s)",
            "--- output ---",
            "--- end ---",
        }:
            continue
        if raw.startswith("pgy: compiled"):
            continue
        if not seen and raw == "":
            continue
        seen = True
        lines.append(raw)
    return lines

expected_lines = normalize(expected)
actual_lines = normalize(actual)
if expected_lines != actual_lines:
    diff = difflib.unified_diff(
        expected_lines,
        actual_lines,
        fromfile="campaign_graph_fsm.expected",
        tofile="campaign_graph_fsm.llvm",
        lineterm="",
    )
    print("[llvm-campaign-projection] stdout mismatch", file=sys.stderr)
    for line in diff:
        print(line, file=sys.stderr)
    raise SystemExit(1)

print("[llvm-campaign-projection] campaign_graph_fsm LLVM projection parity ok")
PY

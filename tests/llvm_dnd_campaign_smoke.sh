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
        echo "[llvm-dnd-campaign] missing python" >&2
        exit 1
    fi
fi

if [[ ! -x "$PGY" ]]; then
    echo "[llvm-dnd-campaign] missing compiler binary: $PGY" >&2
    exit 1
fi

tmp_dir="$(mktemp -d "${TMP_BASE%/}/pgy-dnd-campaign.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

c_output="$("$PGY" "$ROOT_DIR/examples/dnd_tavern_campaign/main.pgy" \
    --run --backend=c -o "$tmp_dir/dnd-c" 2>&1)"
llvm_output="$("$PGY" "$ROOT_DIR/examples/dnd_tavern_campaign/main.pgy" \
    --run --backend=llvm -o "$tmp_dir/dnd-llvm" 2>&1)"

"$PYTHON_BIN" - "$c_output" "$llvm_output" <<'PY'
import difflib
import sys

c_output = sys.argv[1]
llvm_output = sys.argv[2]

def normalize(text: str) -> list[str]:
    lines: list[str] = []
    for raw in text.replace("\r", "").splitlines():
        if raw.startswith("pgy: compiled") or raw.startswith("pgy: wrote"):
            continue
        lines.append(raw)
    return lines

c_lines = normalize(c_output)
llvm_lines = normalize(llvm_output)
if c_lines != llvm_lines:
    print("[llvm-dnd-campaign] C/LLVM stdout mismatch", file=sys.stderr)
    for line in difflib.unified_diff(
        c_lines,
        llvm_lines,
        fromfile="dnd_tavern_campaign.c",
        tofile="dnd_tavern_campaign.llvm",
        lineterm="",
    ):
        print(line, file=sys.stderr)
    raise SystemExit(1)

choice_count = sum(1 for line in llvm_lines if line.startswith("[Choice] "))
if choice_count != 5:
    raise SystemExit(
        f"[llvm-dnd-campaign] expected exactly 5 choice lines, got {choice_count}"
    )
if sum(1 for line in llvm_lines if line == "== EPILOGUE ==") != 1:
    raise SystemExit("[llvm-dnd-campaign] expected exactly one epilogue")
if "ready=true/true" not in llvm_lines:
    raise SystemExit("[llvm-dnd-campaign] missing final ready=true/true projection state")

print("[llvm-dnd-campaign] dnd_tavern_campaign C/LLVM parity ok")
PY

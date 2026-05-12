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

if [[ ! -x "$PGY" ]]; then
    echo "[llvm-dnd-campaign] SKIP executable probe; missing compiler binary: $PGY"
    exit 0
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

tmp_dir="$(mktemp -d "${TMP_BASE%/}/pgy-dnd-campaign.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

c_output="$("$PGY" "$ROOT_DIR/examples/dnd_tavern_campaign/main.pgy" \
    --run --backend=c -o "$tmp_dir/dnd-c" 2>&1)"
llvm_output="$("$PGY" "$ROOT_DIR/examples/dnd_tavern_campaign/main.pgy" \
    --run --backend=llvm -o "$tmp_dir/dnd-llvm" 2>&1)"

if [[ -z "$PYTHON_BIN" ]]; then
    normalize_output() {
        tr -d '\r' | sed -E \
            -e '/^pgy: compiled/d' \
            -e '/^pgy: wrote/d'
    }
    printf '%s\n' "$c_output" | normalize_output > "$tmp_dir/c.out"
    printf '%s\n' "$llvm_output" | normalize_output > "$tmp_dir/llvm.out"
    if ! diff -u "$tmp_dir/c.out" "$tmp_dir/llvm.out"; then
        echo "[llvm-dnd-campaign] C/LLVM stdout mismatch" >&2
        exit 1
    fi
    choice_count="$(grep -c '^\[Choice\] ' "$tmp_dir/llvm.out" || true)"
    if [[ "$choice_count" -ne 5 ]]; then
        echo "[llvm-dnd-campaign] expected exactly 5 choice lines, got $choice_count" >&2
        exit 1
    fi
    if [[ "$(grep -c '^== EPILOGUE ==$' "$tmp_dir/llvm.out" || true)" -ne 1 ]]; then
        echo "[llvm-dnd-campaign] expected exactly one epilogue" >&2
        exit 1
    fi
    if ! grep -Fq "ready=true/true" "$tmp_dir/llvm.out"; then
        echo "[llvm-dnd-campaign] missing final ready=true/true projection state" >&2
        exit 1
    fi
    echo "[llvm-dnd-campaign] dnd_tavern_campaign C/LLVM parity ok"
    exit 0
fi

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

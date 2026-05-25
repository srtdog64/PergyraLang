#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"
PGY_BIN_WAS_EXPLICIT=0

if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
    PGY_BIN_WAS_EXPLICIT=1
elif [[ -x "$TMP_PGY" && ( ! -x "$DEFAULT_PGY" || "$TMP_PGY" -nt "$DEFAULT_PGY" ) ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[llvm-dnd-campaign] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
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

c_output="$("$PGY" "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/dnd_tavern_campaign/main.pgy")" \
    --run --backend=c -o "$(pgy_path_for_compiler "$PGY" "$tmp_dir/dnd-c")" 2>&1)"
llvm_output="$("$PGY" "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/dnd_tavern_campaign/main.pgy")" \
    --run --backend=llvm -o "$(pgy_path_for_compiler "$PGY" "$tmp_dir/dnd-llvm")" 2>&1)"
printf '%s\n' "$c_output" > "$tmp_dir/c.raw.out"
printf '%s\n' "$llvm_output" > "$tmp_dir/llvm.raw.out"

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

"$PYTHON_BIN" - "$tmp_dir/c.raw.out" "$tmp_dir/llvm.raw.out" <<'PY'
import difflib
import pathlib
import sys

c_output = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
llvm_output = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace")

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

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
        echo "[llvm-campaign-projection] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
    echo "[llvm-campaign-projection] SKIP executable probe; missing compiler binary: $PGY"
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

output="$("$PGY" "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/campaign_graph_fsm/main.pgy")" \
    --run --backend=llvm -o "$(pgy_path_for_compiler "$PGY" "${TMP_BASE%/}/pgy-campaign-projection-llvm")" 2>&1)"

if [[ -z "$PYTHON_BIN" ]]; then
    tmp_dir="$(mktemp -d "${TMP_BASE%/}/pgy-campaign-projection.XXXXXX")"
    trap 'rm -rf "$tmp_dir"' EXIT
    normalize_output() {
        tr -d '\r' | sed -E \
            -e '/^0 error\(s\), 0 warning\(s\)$/d' \
            -e '/^--- output ---$/d' \
            -e '/^--- end ---$/d' \
            -e '/^pgy: compiled/d' | awk 'seen || length($0) > 0 { print; seen = 1 }'
    }
    normalize_output < "$ROOT_DIR/examples/campaign_graph_fsm/expected_stdout.txt" \
        > "$tmp_dir/expected.txt"
    printf '%s\n' "$output" | normalize_output > "$tmp_dir/actual.txt"
    if ! diff -u "$tmp_dir/expected.txt" "$tmp_dir/actual.txt"; then
        echo "[llvm-campaign-projection] stdout mismatch" >&2
        exit 1
    fi
    echo "[llvm-campaign-projection] campaign_graph_fsm LLVM projection parity ok"
    exit 0
fi

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

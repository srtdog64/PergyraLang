#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
WORK_ROOT="$ROOT_DIR/.tmp"
mkdir -p "$WORK_ROOT"
WORK_DIR="$(mktemp -d "$WORK_ROOT/pgy_air_backend_nonimpact.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_PGY="${TMPDIR:-/tmp}/pgy-$(basename "$ROOT_DIR")-bin/pgy"
EXPLICIT_PGY=0
if pgy_binary_expects_windows_paths "${DEFAULT_PGY}.exe"; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if pgy_binary_expects_windows_paths "${TMP_PGY}.exe"; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    EXPLICIT_PGY=1
    PGY_BIN="$PGY_BIN"
elif [[ -x "$TMP_PGY" ]]; then
    PGY_BIN="$TMP_PGY"
else
    PGY_BIN="$DEFAULT_PGY"
fi

if [[ ! -x "$PGY_BIN" ]]; then
    if [[ "$EXPLICIT_PGY" -eq 1 ]]; then
        echo "air-backend-nonimpact: missing compiler binary: $PGY_BIN" >&2
        exit 1
    fi
    echo "air-backend-nonimpact: SKIP default compiler executable probe; backend non-impact remains gated when PGY_BIN is provided"
    exit 0
fi

if pgy_binary_expects_windows_paths "$PGY_BIN"; then
    for dir in \
        "/c/Program Files/LLVM/bin" \
        "/c/ProgramData/mingw64/mingw64/bin" \
        "/c/msys64/mingw64/bin"; do
        if [[ -d "$dir" ]]; then
            PATH="$dir:$PATH"
        fi
    done
    export PATH
fi

pgy_path_arg() {
    pgy_path_for_compiler "$PGY_BIN" "$1"
}

require_normal_backend_air_mir_gate() {
    local source_rel="tests/cases/backend_compare/intent_zone_binding/main.pgy"
    local out="$WORK_DIR/air_mir_gate.c"
    local log="$WORK_DIR/air_mir_gate.log"
    local source_arg
    local out_arg

    source_arg="$(pgy_path_arg "$ROOT_DIR/$source_rel")"
    out_arg="$(pgy_path_arg "$out")"

    if pgy_binary_expects_windows_paths "$PGY_BIN" \
        && command -v powershell.exe >/dev/null 2>&1; then
        local ps1="$WORK_DIR/air-mir-gate.ps1"
        local win_pgy win_source win_out win_log win_ps1
        win_pgy="$(pgy_path_arg "$PGY_BIN")"
        win_source="$source_arg"
        win_out="$out_arg"
        win_log="$(pgy_path_arg "$log")"
        win_ps1="$(pgy_path_arg "$ps1")"
cat >"$ps1" <<EOF
\$ErrorActionPreference = 'Continue'
\$env:PATH = 'C:\Program Files\LLVM\bin;C:\ProgramData\mingw64\mingw64\bin;C:\msys64\mingw64\bin;' + \$env:PATH
\$env:PGY_DEBUG_PIPELINE_STAGE = '1'
& '$win_pgy' '$win_source' --emit-c -o '$win_out' 2>&1 | ForEach-Object { \$_.ToString() } | Set-Content -LiteralPath '$win_log' -Encoding utf8
exit \$LASTEXITCODE
EOF
        if ! powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$win_ps1"; then
            if [[ "$EXPLICIT_PGY" -eq 0 ]]; then
                return 1
            fi
            echo "air-backend-nonimpact: normal backend AIR/MIR gate probe failed" >&2
            cat "$log" >&2
            return 1
        fi
    else
        if ! (cd "$ROOT_DIR" && PGY_DEBUG_PIPELINE_STAGE=1 "$PGY_BIN" "$source_arg" --emit-c -o "$out_arg") \
            >"$log" 2>&1; then
            if [[ "$EXPLICIT_PGY" -eq 0 ]]; then
                return 1
            fi
            echo "air-backend-nonimpact: normal backend AIR/MIR gate probe failed" >&2
            cat "$log" >&2
            return 1
        fi
    fi

    if ! grep -Fq "[driver stage] air_mir_evidence" "$log"; then
        echo "air-backend-nonimpact: normal backend path did not report AIR MIR evidence stage" >&2
        cat "$log" >&2
        return 1
    fi
    if ! awk '
        /\[driver stage\] air_mir_evidence/ { saw_air = NR }
        /\[driver stage\] backend_c/ { saw_backend = NR }
        END { exit !(saw_air > 0 && saw_backend > saw_air) }
    ' "$log"; then
        echo "air-backend-nonimpact: backend stage did not run after AIR MIR evidence gate" >&2
        cat "$log" >&2
        return 1
    fi
}

if ! require_normal_backend_air_mir_gate; then
    echo "air-backend-nonimpact: compiler executable probe failed: $PGY_BIN" >&2
    exit 1
fi

DEFAULT_SOURCES=(
    "tests/cases/backend_compare/intent_zone_binding/main.pgy"
    "tests/cases/backend_compare/intent_cross_world_transfer/main.pgy"
    "tests/cases/backend_compare/handoff_projection_frontier/main.pgy"
    "tests/cases/backend_compare/handoff_world_state_frontier/main.pgy"
    "tests/cases/backend_compare/world_zone_projection_visibility/main.pgy"
    "tests/cases/backend_compare/world_embedded_action_frontier/main.pgy"
    "tests/cases/backend_compare/relation_effect_propagation/main.pgy"
    "tests/cases/backend_compare/authority_failure_surface/main.pgy"
)

if [[ "${PGY_AIR_NONIMPACT_SOURCE:-}" == "all" ]]; then
    mapfile -t SOURCES < <(
        cd "$ROOT_DIR"
        find tests/cases/backend_compare -mindepth 2 -maxdepth 2 -name main.pgy | LC_ALL=C sort
    )
elif [[ -n "${PGY_AIR_NONIMPACT_SOURCE:-}" ]]; then
    read -r -a SOURCES <<<"$PGY_AIR_NONIMPACT_SOURCE"
else
    SOURCES=("${DEFAULT_SOURCES[@]}")
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

generated_files_equal() {
    local left="$1"
    local right="$2"
    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import pathlib, re, sys

def normalize(path):
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"ast=0x[0-9A-Fa-f]+", "ast=<ptr>", text)
    return text

left = normalize(sys.argv[1])
right = normalize(sys.argv[2])
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi
    sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$left" >"$left.norm"
    sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$right" >"$right.norm"
    cmp -s "$left.norm" "$right.norm"
}

show_generated_diff() {
    local left="$1"
    local right="$2"
    local left_norm="$WORK_DIR/$(basename "$left").norm"
    local right_norm="$WORK_DIR/$(basename "$right").norm"
    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" "$left_norm" "$right_norm" <<'PY'
import pathlib, re, sys

def normalize(path):
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    return re.sub(r"ast=0x[0-9A-Fa-f]+", "ast=<ptr>", text)

pathlib.Path(sys.argv[3]).write_text(normalize(sys.argv[1]), encoding="utf-8")
pathlib.Path(sys.argv[4]).write_text(normalize(sys.argv[2]), encoding="utf-8")
PY
    else
        sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$left" >"$left_norm"
        sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$right" >"$right_norm"
    fi
    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index -- "$left_norm" "$right_norm" || true
    elif command -v diff >/dev/null 2>&1; then
        diff -u "$left_norm" "$right_norm" || true
    else
        echo "--- $left_norm"
        cat "$left_norm"
        echo "--- $right_norm"
        cat "$right_norm"
    fi
}

sanitize_case_name() {
    local source_rel="$1"
    printf '%s\n' "$source_rel" | sed -E 's#[^A-Za-z0-9_]+#_#g; s#_main_pgy$##'
}

run_emit_pair() {
    local name="$1"
    local flag="$2"
    local ext="$3"
    local source_rel="$4"
    local case_name
    local relaxed_out
    local strict_out
    local relaxed_log
    local strict_log
    local source_arg
    local relaxed_out_arg
    local strict_out_arg

    case_name="$(sanitize_case_name "$source_rel")"
    relaxed_out="$WORK_DIR/${case_name}_${name}_relaxed.${ext}"
    strict_out="$WORK_DIR/${case_name}_${name}_strict.${ext}"
    relaxed_log="$WORK_DIR/${case_name}_${name}_relaxed.log"
    strict_log="$WORK_DIR/${case_name}_${name}_strict.log"
    source_arg="$(pgy_path_arg "$ROOT_DIR/$source_rel")"
    relaxed_out_arg="$(pgy_path_arg "$relaxed_out")"
    strict_out_arg="$(pgy_path_arg "$strict_out")"

    if pgy_binary_expects_windows_paths "$PGY_BIN" \
        && command -v powershell.exe >/dev/null 2>&1; then
        local relaxed_ps1="$WORK_DIR/${case_name}_${name}_relaxed.ps1"
        local strict_ps1="$WORK_DIR/${case_name}_${name}_strict.ps1"
        local win_pgy win_source win_relaxed_out win_strict_out
        local win_relaxed_log win_strict_log win_relaxed_ps1 win_strict_ps1
        win_pgy="$(pgy_path_arg "$PGY_BIN")"
        win_source="$source_arg"
        win_relaxed_out="$relaxed_out_arg"
        win_strict_out="$strict_out_arg"
        win_relaxed_log="$(pgy_path_arg "$relaxed_log")"
        win_strict_log="$(pgy_path_arg "$strict_log")"
        win_relaxed_ps1="$(pgy_path_arg "$relaxed_ps1")"
        win_strict_ps1="$(pgy_path_arg "$strict_ps1")"
cat >"$relaxed_ps1" <<EOF
\$ErrorActionPreference = 'Continue'
\$env:PATH = 'C:\Program Files\LLVM\bin;C:\ProgramData\mingw64\mingw64\bin;C:\msys64\mingw64\bin;' + \$env:PATH
\$env:PGY_AIR_STRICT_EVIDENCE = '0'
& '$win_pgy' '$win_source' '$flag' -o '$win_relaxed_out' 2>&1 | ForEach-Object { \$_.ToString() } | Set-Content -LiteralPath '$win_relaxed_log' -Encoding utf8
exit \$LASTEXITCODE
EOF
        if ! powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$win_relaxed_ps1"; then
            echo "air-backend-nonimpact: relaxed AIR emit failed for $name" >&2
            cat "$relaxed_log" >&2
            return 1
        fi
cat >"$strict_ps1" <<EOF
\$ErrorActionPreference = 'Continue'
\$env:PATH = 'C:\Program Files\LLVM\bin;C:\ProgramData\mingw64\mingw64\bin;C:\msys64\mingw64\bin;' + \$env:PATH
Remove-Item Env:\PGY_AIR_STRICT_EVIDENCE -ErrorAction SilentlyContinue
& '$win_pgy' '$win_source' '$flag' -o '$win_strict_out' 2>&1 | ForEach-Object { \$_.ToString() } | Set-Content -LiteralPath '$win_strict_log' -Encoding utf8
exit \$LASTEXITCODE
EOF
        if ! powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$win_strict_ps1"; then
            echo "air-backend-nonimpact: default strict AIR emit failed for $name" >&2
            cat "$strict_log" >&2
            return 1
        fi
    else
        if ! (cd "$ROOT_DIR" && PGY_AIR_STRICT_EVIDENCE=0 "$PGY_BIN" "$source_arg" "$flag" -o "$relaxed_out_arg") \
            >"$relaxed_log" 2>&1; then
            echo "air-backend-nonimpact: relaxed AIR emit failed for $name" >&2
            cat "$relaxed_log" >&2
            return 1
        fi

        if ! (cd "$ROOT_DIR" && unset PGY_AIR_STRICT_EVIDENCE && "$PGY_BIN" "$source_arg" "$flag" -o "$strict_out_arg") \
            >"$strict_log" 2>&1; then
            echo "air-backend-nonimpact: default strict AIR emit failed for $name" >&2
            cat "$strict_log" >&2
            return 1
        fi
    fi

    if [[ ! -f "$relaxed_out" ]]; then
        echo "air-backend-nonimpact: relaxed AIR emit failed for $name" >&2
        cat "$relaxed_log" >&2
        return 1
    fi
    if [[ ! -f "$strict_out" ]]; then
        echo "air-backend-nonimpact: default strict AIR emit failed for $name" >&2
        cat "$strict_log" >&2
        return 1
    fi

    if ! generated_files_equal "$relaxed_out" "$strict_out"; then
        echo "air-backend-nonimpact: generated $name output changed under default strict AIR" >&2
        show_generated_diff "$relaxed_out" "$strict_out" >&2
        return 1
    fi

    echo "air-backend-nonimpact: PASS $name ($source_rel)"
}

for source_rel in "${SOURCES[@]}"; do
    source="$ROOT_DIR/$source_rel"
    if [[ ! -f "$source" ]]; then
        echo "air-backend-nonimpact: missing source fixture: $source_rel" >&2
        exit 1
    fi

    run_emit_pair "c" "--emit-c" "c" "$source_rel"

    if [[ "${PGY_AIR_NONIMPACT_BACKENDS:-c llvm}" == *"llvm"* ]]; then
        if "$PGY_BIN" --help 2>&1 | grep -q -- "--emit-llvm"; then
            run_emit_pair "llvm" "--emit-llvm" "ll" "$source_rel"
        else
            echo "air-backend-nonimpact: SKIP llvm (compiler built without LLVM support)"
        fi
    fi
done

echo "air-backend-nonimpact: ok"

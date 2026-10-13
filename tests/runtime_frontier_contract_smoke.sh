#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate: the native runtime-frontier compiler contract.
# Delegating would turn a self-host coverage gap into a frontier regression.
# This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"

fail() {
    echo "[runtime-frontier-contract] $*" >&2
    exit 1
}

normalized_file_for() {
    local path="$1"
    local key
    local normalized_path

    key="$(printf '%s' "$path" | sed 's#[^A-Za-z0-9_]#_#g')"
    normalized_path="$tmp_dir/norm_${key}"
    if [[ ! -f "$normalized_path" ]]; then
        tr '\n\r\t' '   ' < "$path" |
            sed 's/[[:space:]][[:space:]]*/ /g' > "$normalized_path"
    fi
    printf '%s\n' "$normalized_path"
}

require_term() {
    local label="$1"
    local path="$2"
    local term="$3"
    local normalized_term
    local normalized_path

    if grep -Fq -- "$term" "$path"; then
        return 0
    fi

    normalized_term="$(
        printf '%s' "$term" |
            tr '\n\r\t' '   ' |
            sed 's/[[:space:]][[:space:]]*/ /g'
    )"
    normalized_path="$(normalized_file_for "$path")"
    if ! grep -Fq -- "$normalized_term" "$normalized_path"; then
        fail "$label missing frontier contract term: $term"
    fi
}

require_terms() {
    local label="$1"
    local path="$2"
    shift 2
    local term
    for term in "$@"; do
        require_term "$label" "$path" "$term"
    done
}

require_generated_frontier_limit() {
    local pgy_bin="${PGY_BIN:-$DEFAULT_PGY}"
    local explicit_pgy=0
    local source="$ROOT_DIR/tests/cases/backend_compare/world_embedded_action_frontier/main.pgy"
    local out_c="$tmp_dir/world_embedded_action_frontier.c"
    local log="$tmp_dir/world_embedded_action_frontier.log"
    local source_arg="$source"
    local out_c_arg="$out_c"
    local emitted=0

    if [[ -n "${PGY_BIN:-}" ]]; then
        explicit_pgy=1
    elif [[ ! -x "$pgy_bin" ]] \
        && pgy_binary_expects_windows_paths "${pgy_bin}.exe"; then
        pgy_bin="${pgy_bin}.exe"
    fi

    if [[ -z "$pgy_bin" || ! -x "$pgy_bin" ]]; then
        if [[ "$explicit_pgy" -eq 1 ]]; then
            fail "PGY_BIN must point to an executable pgy for generated frontier check"
        fi
        echo "[runtime-frontier-contract] SKIP generated frontier fixture; no default pgy executable" >&2
        return 0
    fi
    if pgy_binary_expects_windows_paths "$pgy_bin" \
        && command -v powershell.exe >/dev/null 2>&1; then
        local ps1="$tmp_dir/run-frontier-fixture.ps1"
        local win_pgy win_source win_out win_log win_ps1
        win_pgy="$(pgy_path_for_compiler "$pgy_bin" "$pgy_bin")"
        win_source="$(pgy_path_for_compiler "$pgy_bin" "$source")"
        win_out="$(pgy_path_for_compiler "$pgy_bin" "$out_c")"
        win_log="$(pgy_path_for_compiler "$pgy_bin" "$log")"
        win_ps1="$(pgy_path_for_compiler "$pgy_bin" "$ps1")"
cat >"$ps1" <<EOF
\$ErrorActionPreference = 'Continue'
\$env:PATH = '$PGY_WINDOWS_PS_PATH_PREFIX' + \$env:PATH
& '$win_pgy' '$win_source' --emit-c -o '$win_out' 2>&1 | ForEach-Object { \$_.ToString() } | Set-Content -LiteralPath '$win_log' -Encoding utf8
exit \$LASTEXITCODE
EOF
        if ! powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$win_ps1"; then
            if [[ -f "$log" ]]; then
                sed 's/^/[runtime-frontier-contract] pgy: /' "$log" >&2 || true
            fi
            if [[ "$explicit_pgy" -eq 1 ]]; then
                fail "failed to emit generated frontier fixture with $pgy_bin"
            fi
            fail "failed to emit generated frontier fixture with default $pgy_bin"
        fi
        emitted=1
    elif pgy_binary_expects_windows_paths "$pgy_bin"; then
        source_arg="$(pgy_path_for_compiler "$pgy_bin" "$source")"
        out_c_arg="$(pgy_path_for_compiler "$pgy_bin" "$out_c")"
    fi
    if [[ "$emitted" -eq 0 ]] && ! "$pgy_bin" "$source_arg" --emit-c -o "$out_c_arg" >"$log" 2>&1; then
        if [[ -f "$log" ]]; then
            sed 's/^/[runtime-frontier-contract] pgy: /' "$log" >&2 || true
        fi
        if [[ "$explicit_pgy" -eq 1 ]]; then
            fail "failed to emit generated frontier fixture with $pgy_bin"
        fi
        fail "failed to emit generated frontier fixture with default $pgy_bin"
    fi

    require_terms "generated embedded world frontier C" "$out_c" \
        "_pgy_world_frontier_pass_limit = 7;" \
        "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit)"

    if grep -Fq "_pgy_world_frontier_pass_limit = 5;" "$out_c"; then
        fail "generated embedded world frontier still uses outer-only pass limit"
    fi
}

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

require_generated_frontier_limit

echo "[runtime-frontier-contract] generated embedded world frontier pass limit ok"

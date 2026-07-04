#!/usr/bin/env bash
#
# LSP-0 parity: the Pergyra LSP diagnostics owner projects semantic verdicts
# into a publishDiagnostics-shaped JSON payload. This rung is payload-only:
# JSON-RPC stdin framing and C LSP session parity remain later rungs.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

PGY_LSP="${PGY_LSP_BIN:-$ROOT_DIR/bin/pgy-lsp}"
if [[ "$PGY_LSP" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY_LSP}.exe"; then
    PGY_LSP="${PGY_LSP}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:lsp-diagnostics] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:lsp-diagnostics] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:lsp-diagnostics" "$PGY"
if [[ -x "$PGY_LSP" ]]; then
    pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:lsp-diagnostics:c-lsp-oracle" "$PGY_LSP"
fi

LSP_SOURCE="$ROOT_DIR/src/self_hosted/lsp/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp_diagnostics}"
FIXTURES=(
    "valid_int_return"
    "bad_logical_right"
)

mkdir -p "$BUILD_DIR"

compile_lsp_backend() {
    local backend="$1"
    local out_bin="$2"
    local compile_log="$BUILD_DIR/main_${backend}.compile.log"

    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$LSP_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" \
        >"$compile_log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$compile_log"; then
            return 2
        fi
        echo "[self-host-parity:lsp-diagnostics] backend=$backend compile failed" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

lsp_runtime_arg_for_binary() {
    local bin="$1"
    local rel="$2"

    if pgy_binary_expects_windows_paths "$bin"; then
        printf '%s\n' "${rel//\//\\}"
        return 0
    fi
    printf '%s\n' "$rel"
}

capture_lsp_output() {
    local backend="$1"
    local bin="$2"
    local fixture_base="$3"
    local fixture_rel="src/self_hosted/lsp/fixture/${fixture_base}.pgy"
    local out_file="$BUILD_DIR/${fixture_base}_${backend}.json"
    local err_file="$BUILD_DIR/${fixture_base}_${backend}.err"
    local arg
    local rc

    arg="$(lsp_runtime_arg_for_binary "$bin" "$fixture_rel")"
    set +e
    (cd "$ROOT_DIR" && "$bin" "$arg" >"$out_file.raw" 2>"$err_file")
    rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"

    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:lsp-diagnostics] backend=$backend fixture=$fixture_base failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
    if ! grep -Fq '"schema":"pgy.selfhost.lsp-diagnostics.v1"' "$out_file"; then
        echo "[self-host-parity:lsp-diagnostics] backend=$backend fixture=$fixture_base schema missing" >&2
        cat "$out_file" >&2
        exit 1
    fi
    if grep -Eq '"uri":"[^"]*\\' "$out_file"; then
        echo "[self-host-parity:lsp-diagnostics] backend=$backend fixture=$fixture_base leaked backslash path" >&2
        cat "$out_file" >&2
        exit 1
    fi

    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "lsp-diagnostics:$backend:$fixture_base" \
        "$BUILD_DIR" \
        "$ROOT_DIR/src/self_hosted/lsp/expected/${fixture_base}.json" \
        "$out_file" \
        "lsp_diagnostics"
}

capture_policy_output() {
    local backend="$1"
    local bin="$2"
    local out_file="$BUILD_DIR/squiggle_policy_${backend}.json"
    local err_file="$BUILD_DIR/squiggle_policy_${backend}.err"
    local rc

    set +e
    (cd "$ROOT_DIR" && "$bin" --squiggle-policy >"$out_file.raw" 2>"$err_file")
    rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"

    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:lsp-diagnostics] backend=$backend squiggle policy failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
    for cls in '"class":"red"' '"class":"amber"' '"class":"blue"' '"class":"violet"'; do
        if ! grep -Fq "$cls" "$out_file"; then
            echo "[self-host-parity:lsp-diagnostics] backend=$backend squiggle policy lost $cls" >&2
            cat "$out_file" >&2
            exit 1
        fi
    done

    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "lsp-diagnostics:$backend:squiggle_policy" \
        "$BUILD_DIR" \
        "$ROOT_DIR/src/self_hosted/lsp/expected/squiggle_policy.json" \
        "$out_file" \
        "lsp_diagnostics"
}

c_lsp_runtime_arg_for_binary() {
    local bin="$1"
    local rel="$2"

    if pgy_binary_expects_windows_paths "$bin"; then
        printf '%s\n' "${rel//\//\\}"
        return 0
    fi
    printf '%s\n' "$rel"
}

check_c_lsp_oracle() {
    local fixture_base="$1"
    local fixture_rel="src/self_hosted/lsp/fixture/${fixture_base}.pgy"
    local out_file="$BUILD_DIR/${fixture_base}_c_lsp_oracle.json"
    local err_file="$BUILD_DIR/${fixture_base}_c_lsp_oracle.err"
    local arg
    local rc

    if [[ ! -x "$PGY_LSP" ]]; then
        echo "[self-host-parity:lsp-diagnostics] missing C LSP oracle binary: $PGY_LSP" >&2
        exit 1
    fi

    arg="$(c_lsp_runtime_arg_for_binary "$PGY_LSP" "$fixture_rel")"
    set +e
    (cd "$ROOT_DIR" && "$PGY_LSP" --dump-diagnostics "$arg" >"$out_file.raw" 2>"$err_file")
    rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"

    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:lsp-diagnostics] C LSP oracle fixture=$fixture_base failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
    if ! grep -Fq '"method":"textDocument/publishDiagnostics"' "$out_file"; then
        echo "[self-host-parity:lsp-diagnostics] C LSP oracle fixture=$fixture_base missing publishDiagnostics method" >&2
        cat "$out_file" >&2
        exit 1
    fi
    if grep -Eq '"uri":"[^"]*\\' "$out_file"; then
        echo "[self-host-parity:lsp-diagnostics] C LSP oracle fixture=$fixture_base leaked backslash path" >&2
        cat "$out_file" >&2
        exit 1
    fi
    case "$fixture_base" in
        valid_int_return)
            grep -Fq '"diagnostics":[]' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle clean fixture emitted diagnostics" >&2
                cat "$out_file" >&2
                exit 1
            }
            ;;
        bad_logical_right)
            grep -Fq '"code":"PGY_SEM_BINOP_TYPE_MISMATCH"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle error fixture lost logical operand code" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"cause_ir":"semantic:binop:operand_types"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle error fixture lost binop cause" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"squiggleClass":"red"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle error fixture lost red squiggle class" >&2
                cat "$out_file" >&2
                exit 1
            }
            ;;
    esac
}

BACKENDS="${PGY_SELFHOST_LSP_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()

for backend in $BACKENDS; do
    lsp_bin="$BUILD_DIR/main_${backend}.exe"
    set +e
    compile_lsp_backend "$backend" "$lsp_bin"
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        echo "[self-host-parity:lsp-diagnostics] LLVM backend unavailable; skipping llvm-built LSP diagnostics"
        SKIPPED_BACKENDS+=("$backend")
        continue
    fi
    if [[ "$compile_rc" -ne 0 ]]; then
        exit "$compile_rc"
    fi

    for fixture_base in "${FIXTURES[@]}"; do
        require_fixture="$ROOT_DIR/src/self_hosted/lsp/fixture/${fixture_base}.pgy"
        require_expected="$ROOT_DIR/src/self_hosted/lsp/expected/${fixture_base}.json"
        [[ -f "$require_fixture" ]] || {
            echo "[self-host-parity:lsp-diagnostics] missing fixture: $require_fixture" >&2
            exit 1
        }
        [[ -f "$require_expected" ]] || {
            echo "[self-host-parity:lsp-diagnostics] missing expected: $require_expected" >&2
            exit 1
        }
        capture_lsp_output "$backend" "$lsp_bin" "$fixture_base"
    done
    require_policy="$ROOT_DIR/src/self_hosted/lsp/expected/squiggle_policy.json"
    [[ -f "$require_policy" ]] || {
        echo "[self-host-parity:lsp-diagnostics] missing expected: $require_policy" >&2
        exit 1
    }
    capture_policy_output "$backend" "$lsp_bin"
    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:lsp-diagnostics] no requested backend ran" >&2
    exit 1
fi

for fixture_base in "${FIXTURES[@]}"; do
    check_c_lsp_oracle "$fixture_base"
done

echo "[self-host-parity:lsp-diagnostics] payload parity ok (backends=${RAN_BACKENDS[*]}; skipped=${SKIPPED_BACKENDS[*]:-none})"

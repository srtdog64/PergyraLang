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

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp_diagnostics}"
HARNESS_PATHS_FILE="$BUILD_DIR/lsp_diagnostics_harness_paths.txt"

mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:lsp-diagnostics" \
    "$BUILD_DIR" \
    "lsp-diagnostics-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 14 ]]; then
    echo "[self-host-parity:lsp-diagnostics] TestHarness manifest expected 14 paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

LSP_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_SQUIGGLE_POLICY="$ROOT_DIR/${harness_paths[1]}"
FIXTURES=()
FIXTURE_RELS=()
FIXTURE_EXPECTEDS=()
for ((i = 0; i < 6; i++)); do
    fixture_rel="${harness_paths[$((2 + i))]}"
    fixture_base="${fixture_rel##*/}"
    fixture_base="${fixture_base%.pgy}"
    FIXTURES+=("$fixture_base")
    FIXTURE_RELS+=("$fixture_rel")
    FIXTURE_EXPECTEDS+=("$ROOT_DIR/${harness_paths[$((8 + i))]}")
done

for path in "$LSP_SOURCE" "$EXPECTED_SQUIGGLE_POLICY"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:lsp-diagnostics] missing TestHarness input: $path" >&2
        exit 1
    fi
done

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
    local fixture_rel="$4"
    local expected_path="$5"
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
        "$expected_path" \
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
        "$EXPECTED_SQUIGGLE_POLICY" \
        "$out_file" \
        "lsp_diagnostics"
}

lsp_json_uri() {
    local file="$1"
    sed -n 's/.*"uri":"\([^"]*\)".*/\1/p' "$file" | sed -n '1p'
}

lsp_canonical_event_artifact() {
    local label="$1"
    local json_file="$2"
    local out_file="$3"
    local uri

    if ! grep -Fq '"method":"textDocument/publishDiagnostics"' "$json_file"; then
        echo "[self-host-parity:lsp-diagnostics] $label missing publishDiagnostics method" >&2
        cat "$json_file" >&2
        exit 1
    fi

    uri="$(lsp_json_uri "$json_file")"
    if [[ -z "$uri" ]]; then
        echo "[self-host-parity:lsp-diagnostics] $label missing URI" >&2
        cat "$json_file" >&2
        exit 1
    fi

    if grep -Fq '"diagnostics":[]' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=0"
            echo "event=ok"
            echo "severity=none"
            echo "squiggle=none"
        } > "$out_file"
        return 0
    fi

    if ! grep -Fq '"severity":1' "$json_file"; then
        echo "[self-host-parity:lsp-diagnostics] $label expected severity 1" >&2
        cat "$json_file" >&2
        exit 1
    fi
    if ! grep -Fq '"squiggleClass":"red"' "$json_file"; then
        echo "[self-host-parity:lsp-diagnostics] $label expected red squiggle" >&2
        cat "$json_file" >&2
        exit 1
    fi

    if grep -Fq '"code":"logical_operand_not_bool"' "$json_file" \
        && grep -Fq '"oracleCode":"PGY_SEM_BINOP_TYPE_MISMATCH"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=logical_operand_not_bool"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"undefined_symbol"' "$json_file" \
        && grep -Fq '"oracleCode":"PGY_SEM_UNDEFINED_SYMBOL"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=undefined_symbol"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"return_type_mismatch"' "$json_file" \
        && grep -Fq '"oracleCode":"PGY_SEM_TYPE_MISMATCH"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=type_mismatch"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"condition_not_bool"' "$json_file" \
        && grep -Fq '"oracleCode":"PGY_SEM_TYPE_MISMATCH"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=condition_not_bool"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"not_operand_not_bool"' "$json_file" \
        && grep -Fq '"oracleCode":"PGY_SEM_UNOP_TYPE_MISMATCH"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=not_operand_not_bool"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"PGY_SEM_BINOP_TYPE_MISMATCH"' "$json_file" \
        && grep -Fq '"cause_ir":"semantic:binop:operand_types"' "$json_file" \
        && grep -Fq "Logical operator requires Bool operands" "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=logical_operand_not_bool"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"PGY_SEM_UNDEFINED_SYMBOL"' "$json_file" \
        && grep -Fq '"cause_ir":"semantic:symbol:undefined"' "$json_file" \
        && grep -Fq "Undefined symbol" "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=undefined_symbol"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"PGY_SEM_TYPE_MISMATCH"' "$json_file" \
        && grep -Fq '"cause_ir":"semantic:assignability_check"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=type_mismatch"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"PGY_SEM_TYPE_MISMATCH"' "$json_file" \
        && grep -Fq '"cause_ir":"semantic:condition:non_bool"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=condition_not_bool"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    if grep -Fq '"code":"PGY_SEM_UNOP_TYPE_MISMATCH"' "$json_file" \
        && grep -Fq '"cause_ir":"semantic:unary_operator:operand"' "$json_file"; then
        {
            echo "method=textDocument/publishDiagnostics"
            echo "uri=$uri"
            echo "diagnostic_count=1"
            echo "event=not_operand_not_bool"
            echo "severity=1"
            echo "squiggle=red"
        } > "$out_file"
        return 0
    fi

    echo "[self-host-parity:lsp-diagnostics] $label has no known canonical event" >&2
    cat "$json_file" >&2
    exit 1
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
    local fixture_rel="$2"
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
        bad_undefined_return)
            grep -Fq '"code":"PGY_SEM_UNDEFINED_SYMBOL"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle undefined fixture lost symbol code" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"cause_ir":"semantic:symbol:undefined"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle undefined fixture lost symbol cause" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"squiggleClass":"red"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle undefined fixture lost red squiggle class" >&2
                cat "$out_file" >&2
                exit 1
            }
            ;;
        bad_return_type)
            grep -Fq '"code":"PGY_SEM_TYPE_MISMATCH"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle return-type fixture lost type mismatch code" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"cause_ir":"semantic:assignability_check"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle return-type fixture lost assignability cause" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"squiggleClass":"red"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle return-type fixture lost red squiggle class" >&2
                cat "$out_file" >&2
                exit 1
            }
            ;;
        bad_while_condition)
            grep -Fq '"code":"PGY_SEM_TYPE_MISMATCH"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle condition fixture lost type mismatch code" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"cause_ir":"semantic:condition:non_bool"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle condition fixture lost condition cause" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"squiggleClass":"red"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle condition fixture lost red squiggle class" >&2
                cat "$out_file" >&2
                exit 1
            }
            ;;
        bad_not_operand)
            grep -Fq '"code":"PGY_SEM_UNOP_TYPE_MISMATCH"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle not-operand fixture lost unary mismatch code" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"cause_ir":"semantic:unary_operator:operand"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle not-operand fixture lost unary cause" >&2
                cat "$out_file" >&2
                exit 1
            }
            grep -Fq '"squiggleClass":"red"' "$out_file" || {
                echo "[self-host-parity:lsp-diagnostics] C LSP oracle not-operand fixture lost red squiggle class" >&2
                cat "$out_file" >&2
                exit 1
            }
            ;;
    esac

    local oracle_canon="$BUILD_DIR/${fixture_base}_c_lsp_oracle.canon"
    lsp_canonical_event_artifact "c-lsp-oracle:$fixture_base" "$out_file" "$oracle_canon"
    for backend in "${RAN_BACKENDS[@]}"; do
        local self_out="$BUILD_DIR/${fixture_base}_${backend}.json"
        local self_canon="$BUILD_DIR/${fixture_base}_${backend}.canon"
        lsp_canonical_event_artifact "self-host:$backend:$fixture_base" "$self_out" "$self_canon"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "lsp-diagnostics:normalized:$backend:$fixture_base" \
            "$BUILD_DIR" \
            "$oracle_canon" \
            "$self_canon" \
            "lsp_diagnostics"
    done
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

    for ((i = 0; i < ${#FIXTURES[@]}; i++)); do
        fixture_base="${FIXTURES[$i]}"
        require_fixture="$ROOT_DIR/${FIXTURE_RELS[$i]}"
        require_expected="${FIXTURE_EXPECTEDS[$i]}"
        [[ -f "$require_fixture" ]] || {
            echo "[self-host-parity:lsp-diagnostics] missing fixture: $require_fixture" >&2
            exit 1
        }
        [[ -f "$require_expected" ]] || {
            echo "[self-host-parity:lsp-diagnostics] missing expected: $require_expected" >&2
            exit 1
        }
        capture_lsp_output "$backend" "$lsp_bin" "$fixture_base" "${FIXTURE_RELS[$i]}" "$require_expected"
    done
    require_policy="$EXPECTED_SQUIGGLE_POLICY"
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

for ((i = 0; i < ${#FIXTURES[@]}; i++)); do
    check_c_lsp_oracle "${FIXTURES[$i]}" "${FIXTURE_RELS[$i]}"
done

echo "[self-host-parity:lsp-diagnostics] payload parity ok (backends=${RAN_BACKENDS[*]}; skipped=${SKIPPED_BACKENDS[*]:-none})"

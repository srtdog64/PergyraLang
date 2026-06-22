#!/usr/bin/env bash
# MIR-JSON lowering parity gate (self-host path (a), rung-0b, 2026-06-18).
#
# Proves that the Pergyra-origin MIR -> C lowering is run-equivalent to the C
# backend on the supported subset -- linear code, function signatures, and
# CFG-structured control flow (if/else, nested if, while, for), plus selected
# codegen fixture surfaces that already lower from MIR facts without reading
# transitional AST text:
#
#   pgy --mir-json fixture.pgy            (MIR JSON facts)
#     | mir_lower   (Pergyra: MIR-JSON facts -> reconstructed --ast tree)
#     | codegen     (Pergyra: --ast tree -> standalone C)
#     -> gcc -> run-stdout
#
# must equal
#
#   pgy fixture.pgy --backend=c -> run-stdout       (the C oracle).
#
# mir_lower and codegen are both compiled through the oracle (gen0); the test is
# purely about the lowering they perform, verified by run-stdout equality.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    if [[ -z "${PGY_BIN:-}" ]]; then
        echo "[self-host-parity:mir-json] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:mir-json] missing compiler binary: $PGY" >&2
    exit 1
fi
CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-parity:mir-json] SKIP missing C compiler on PATH: $CC"
    exit 0
fi

MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
CODEGEN_SRC="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/mir_lower/fixture"
CODEGEN_FIXTURE_DIR="$ROOT_DIR/src/self_hosted/codegen/fixture"
B="$ROOT_DIR/.tmp/self_hosted/mir_lower/parity"
mkdir -p "$B"

if grep -Fq 'JsonFieldString(json, kp, inst_end, "\"ast\":")' "$MIR_LOWER_SRC" \
    || grep -Fq "StringLength(ast)" "$MIR_LOWER_SRC"; then
    echo "[self-host-parity:mir-json] mir_lower must not read transitional ast compatibility text" >&2
    exit 1
fi

# gen0: oracle-built mir_lower + codegen tools.
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/mir_lower.exe")" >/dev/null)
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$CODEGEN_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/codegen.exe")" >/dev/null)

MIR_FIXTURES=(
    let_log
    multilet
    arith
    strlog
    funcparam
    multi_func_void
    ifelse
    nestedif
    reassign_block
    whileloop
    forloop
)

CODEGEN_FIXTURES=(
    args_probe
    array_max
    array_param
    array_push
    array_sum
    bool_logic
    concat
    else_if_chain
    file_handle
    float_math
    for_continue
    func_recursive
    io_probe
    mixed_int_str
    nested_ctrl
    str_array
    str_array_push
    str_builtins
    string_concat_op
    string_equality
    write_file
)

pass=0
for fixture_entry in "${MIR_FIXTURES[@]}" "${CODEGEN_FIXTURES[@]}"; do
    base="$fixture_entry"
    src="$FIXTURE_DIR/$base.pgy"
    if [[ ! -f "$src" ]]; then
        src="$CODEGEN_FIXTURE_DIR/$base.pgy"
    fi
    if [[ ! -f "$src" ]]; then
        echo "[self-host-parity:mir-json] missing fixture: $src" >&2
        exit 1
    fi
    mj="$B/$base.mirjson"
    reast="$B/$base.reast"
    via_c="$B/$base.c"

    # Pergyra MIR -> C path.
    (cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$src")" \
        2>/dev/null | tr -d '\r' > "$mj")
    if ! grep -q '"schema":"pgy.mir.v1"' "$mj"; then
        echo "[self-host-parity:mir-json] $base: oracle --mir-json did not emit pgy.mir.v1" >&2
        exit 1
    fi
    if ! grep -q '"expr0":' "$mj" || ! grep -q '"source_type":' "$mj" || ! grep -q '"source_locals":\[' "$mj"; then
        echo "[self-host-parity:mir-json] $base: MIR JSON is missing explicit expression/source-local facts" >&2
        exit 1
    fi
    if [[ "$base" == "forloop" ]]; then
        for required in \
            '"source_type":"AST_FOR_LOOP"' \
            '"arg0":"i"' \
            '"expr0":"0"' \
            '"expr1":"3"'; do
            if ! grep -q "$required" "$mj"; then
                echo "[self-host-parity:mir-json] forloop: missing MIR for-loop fact: $required" >&2
                exit 1
            fi
        done
    fi
    "$B/mir_lower.exe" "${mj#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$reast" || true
    if grep -q '^MIR-LOWER ERROR' "$reast"; then
        echo "[self-host-parity:mir-json] $base: mir_lower rejected the MIR-JSON:" >&2
        grep '^MIR-LOWER ERROR' "$reast" | head -1 >&2
        exit 1
    fi
    if [[ "$base" == "forloop" ]]; then
        if ! grep -q 'For: i in 0..3' "$reast"; then
            echo "[self-host-parity:mir-json] forloop: mir_lower did not reconstruct the for-loop from MIR facts" >&2
            exit 1
        fi
        if grep -q 'If: 0' "$reast"; then
            echo "[self-host-parity:mir-json] forloop: mir_lower treated the for bound expr as a branch condition" >&2
            exit 1
        fi
    fi
    "$B/codegen.exe" "${reast#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$via_c" || true
    if grep -q '^CODEGEN ERROR' "$via_c"; then
        echo "[self-host-parity:mir-json] $base: codegen rejected the reconstructed AST:" >&2
        grep '^CODEGEN ERROR' "$via_c" | head -1 >&2
        exit 1
    fi
    if ! "$CC" "$via_c" -o "$B/${base}_via_mir.exe" 2>"$B/${base}_cc.log"; then
        echo "[self-host-parity:mir-json] $base: reconstructed C failed to compile" >&2
        cat "$B/${base}_cc.log" >&2
        exit 1
    fi

    # C oracle.
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$src")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/${base}_oracle.exe")" >/dev/null 2>&1)

    via="$(cd "$ROOT_DIR" && "$B/${base}_via_mir.exe" 2>/dev/null | tr -d '\r')"
    orc="$(cd "$ROOT_DIR" && "$B/${base}_oracle.exe" 2>/dev/null | tr -d '\r')"
    if [[ "$via" != "$orc" ]]; then
        echo "[self-host-parity:mir-json] $base: MIR->C run-stdout differs from oracle" >&2
        diff <(printf '%s' "$via") <(printf '%s' "$orc") | head -10 >&2
        exit 1
    fi
    pass=$((pass + 1))
done

echo "[self-host-parity:mir-json] rung-0b MIR->C parity ok (${pass} fixtures; pgy --mir-json | mir_lower | codegen == C oracle)"

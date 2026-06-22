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
    break_after_stmt
    random_inferred_let
    match_case_int
    nested_if_in_loop
    array_destructure
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
    array_pop
    array_push
    array_sum
    bool_logic
    builtin_name_literal
    concat
    dir_walk
    else_if_chain
    exit_guard
    file_handle
    io_absolute_policy
    float_math
    for_each
    for_continue
    for_sum
    func_call
    func_recursive
    hello
    if_else
    int_arith
    int_neg
    int_subdiv
    io_probe
    log_int_direct
    log_trailing_newline
    mixed_int_str
    nested_concat
    nested_ctrl
    str_array
    str_array_concat
    str_array_push
    str_builtins
    str_builtins2
    str_case_math
    str_greet
    str_indexof
    str_reassign
    str_trim
    struct_param
    struct_point
    string_concat_op
    string_equality
    two_logs
    while_break
    while_sum
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
    if [[ "$base" == "struct_point" ]]; then
        for required in \
            '"decls":[{"kind":"struct"' \
            '"name":"Point"' \
            '"name":"x","type":"Int"' \
            '"name":"y","type":"Int"'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] struct_point: missing MIR struct declaration fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "struct_param" ]]; then
        for required in \
            '"decls":[{"kind":"struct"' \
            '"name":"Pair"' \
            '"name":"a","type":"Int"' \
            '"name":"b","type":"Int"'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] struct_param: missing MIR struct declaration fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "match_case_int" ]]; then
        for required in \
            '"source_type":"AST_MATCH_CASE"' \
            '"match_patterns":["1"]' \
            '"match_patterns":["2"]' \
            '"match_patterns":["3"]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] match_case_int: missing MIR match pattern fact: $required" >&2
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
    if [[ "$base" == "struct_point" ]] && ! grep -q 'Struct: Point' "$reast"; then
        echo "[self-host-parity:mir-json] struct_point: mir_lower did not reconstruct the struct declaration from MIR facts" >&2
        exit 1
    fi
    if [[ "$base" == "struct_param" ]] && ! grep -q 'Struct: Pair' "$reast"; then
        echo "[self-host-parity:mir-json] struct_param: mir_lower did not reconstruct the struct declaration from MIR facts" >&2
        exit 1
    fi
    if [[ "$base" == "match_case_int" ]] && ! grep -q 'If: x == 3' "$reast"; then
        echo "[self-host-parity:mir-json] match_case_int: mir_lower did not reconstruct match case conditions from MIR facts" >&2
        exit 1
    fi
    if [[ "$base" == "nested_if_in_loop" ]]; then
        if grep -q 'While: (right < size)' "$reast" || grep -q 'While: (largest == cur)' "$reast"; then
            echo "[self-host-parity:mir-json] nested_if_in_loop: mir_lower misclassified inner if branches as loops" >&2
            exit 1
        fi
        if ! grep -q 'If: (largest == cur)' "$reast"; then
            echo "[self-host-parity:mir-json] nested_if_in_loop: mir_lower did not preserve the inner break guard as if" >&2
            exit 1
        fi
    fi
    if [[ "$base" == "array_destructure" ]]; then
        if ! grep -Fq '"destructure_bindings":["id_str","name","active_str"]' "$mj"; then
            echo "[self-host-parity:mir-json] array_destructure: MIR JSON is missing destructure binding facts" >&2
            exit 1
        fi
        if ! grep -q 'Let: _pgy_destructure_.* : Array<String> = Split(csv, ",")' "$reast"; then
            echo "[self-host-parity:mir-json] array_destructure: mir_lower did not materialize a fact-owned array temp" >&2
            exit 1
        fi
        if ! grep -Fq 'Let: id_str : String = _pgy_destructure_' "$reast" \
            || ! grep -Fq '[0]' "$reast"; then
            echo "[self-host-parity:mir-json] array_destructure: mir_lower did not reconstruct first binding from facts" >&2
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

rejects=0
base="unsupported_ability_decl"
src="$FIXTURE_DIR/$base.pgy"
mj="$B/$base.mirjson"
reast="$B/$base.reast"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$src")" \
    2>/dev/null | tr -d '\r' > "$mj")
for required in \
    '"kind":"unsupported","ast_type":"AST_ABILITY_DECL"' \
    '"kind":"unsupported","ast_type":"AST_ROLE_DECL"'; do
    if ! grep -Fq "$required" "$mj"; then
        echo "[self-host-parity:mir-json] $base: missing unsupported declaration fact: $required" >&2
        exit 1
    fi
done
set +e
"$B/mir_lower.exe" "${mj#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$reast"
reject_rc=${PIPESTATUS[0]}
set -e
if [[ "$reject_rc" -eq 0 ]]; then
    echo "[self-host-parity:mir-json] $base: mir_lower must exit nonzero for unsupported declaration facts" >&2
    exit 1
fi
if ! grep -q '^MIR-LOWER ERROR: unsupported MIR declaration in self-host subset:' "$reast"; then
    echo "[self-host-parity:mir-json] $base: mir_lower must reject unsupported declaration facts" >&2
    sed -n '1,5p' "$reast" >&2
    exit 1
fi
rejects=$((rejects + 1))

base="unsupported_codegen_builtin"
src="$FIXTURE_DIR/$base.pgy"
mj="$B/$base.mirjson"
reast="$B/$base.reast"
via_c="$B/$base.c"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$src")" \
    2>/dev/null | tr -d '\r' > "$mj")
"$B/mir_lower.exe" "${mj#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$reast"
set +e
"$B/codegen.exe" "${reast#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$via_c"
reject_rc=${PIPESTATUS[0]}
set -e
if [[ "$reject_rc" -eq 0 ]]; then
    echo "[self-host-parity:mir-json] $base: codegen must exit nonzero for unsupported builtins" >&2
    exit 1
fi
if ! grep -q '^CODEGEN ERROR: unsupported builtin in self-host codegen subset: ArraySort' "$via_c"; then
    echo "[self-host-parity:mir-json] $base: codegen must reject unsupported builtin facts before C emission" >&2
    sed -n '1,5p' "$via_c" >&2
    exit 1
fi
rejects=$((rejects + 1))

echo "[self-host-parity:mir-json] rung-0b MIR->C parity ok (${pass} fixtures, ${rejects} clean reject; pgy --mir-json | mir_lower | codegen == C oracle)"

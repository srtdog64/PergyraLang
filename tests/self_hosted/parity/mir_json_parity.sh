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
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix_from_current_path)"

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

compile_c_to_exe() {
    local src="$1"
    local out="$2"
    local log="$3"

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            command -v powershell.exe >/dev/null 2>&1 || return 127

            local src_native
            local out_native
            local log_native
            src_native="$(pgy_path_for_windows_tool "$src")"
            out_native="$(pgy_path_for_windows_tool "$out")"
            log_native="$(pgy_path_for_windows_tool "$log")"

            powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
                "\$env:PATH=$(pgy_powershell_quote "$PGY_WINDOWS_PS_PATH_PREFIX") + \$env:PATH; & $(pgy_powershell_quote "$CC") $(pgy_powershell_quote "$src_native") '-o' $(pgy_powershell_quote "$out_native") 2> $(pgy_powershell_quote "$log_native"); exit \$LASTEXITCODE"
            return $?
            ;;
    esac

    "$CC" "$src" -o "$out" 2>"$log"
}

MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
CODEGEN_SRC="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/mir_lower/fixture"
CODEGEN_FIXTURE_DIR="$ROOT_DIR/src/self_hosted/codegen/fixture"
EXAMPLE_FIXTURE_DIR="$ROOT_DIR/examples"
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

# Fail loud at the source if the self-host tool rebuild did not produce runnable
# binaries (CLAUDE.md s1.1). `pgy --backend=c -o` shells out to gcc; in a shell
# where gcc is unavailable it can return success yet leave the tool missing or
# stale. Catch that here once, with the real cause, rather than N cryptic empty-
# output failures downstream.
for tool in mir_lower codegen; do
    if [[ ! -s "$B/$tool.exe" ]]; then
        echo "[self-host-parity:mir-json] self-host tool '$tool.exe' was not built." >&2
        echo "  'pgy --backend=c -o $B/$tool.exe' produced no binary -- the gcc" >&2
        echo "  subprocess it invokes is likely unavailable in this shell. Run the" >&2
        echo "  gate where gcc works (the documented PowerShell->bash.exe path)." >&2
        exit 1
    fi
done

MIR_FIXTURES=(
    let_log
    multilet
    arith
    strlog
    funcparam
    multi_func_void
    break_after_stmt
    random_inferred_let
    class_decl
    class_method
    nominal_subject
    nominal_object
    nominal_tobject
    nominal_vessel
    ability_decl
    enum_simple
    match_case_int
    nested_if_in_loop
    option_match
    array_destructure
    role_operator_dispatch
    ifelse
    nestedif
    reassign_block
    whileloop
    forloop
)

CODEGEN_FIXTURES=(
    args_probe
    array_combinators
    array_max
    array_param
    array_pop
    array_push
    array_reverse
    array_sum
    bool_logic
    builtin_name_literal
    concat
    defer_scope
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
    option_int_core
    result_int_core
    result_try
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
    struct_mixed_fields
    struct_nested_fields
    struct_param
    struct_point
    string_concat_op
    string_equality
    string_utils_core
    two_logs
    while_break
    while_sum
    write_file
)

EXAMPLE_FIXTURES=(
    binary_search
)

pass=0
for fixture_entry in "${MIR_FIXTURES[@]}" "${CODEGEN_FIXTURES[@]}" "${EXAMPLE_FIXTURES[@]}"; do
    base="$fixture_entry"
    src="$FIXTURE_DIR/$base.pgy"
    if [[ ! -f "$src" ]]; then
        src="$CODEGEN_FIXTURE_DIR/$base.pgy"
    fi
    if [[ ! -f "$src" ]]; then
        src="$EXAMPLE_FIXTURE_DIR/$base.pgy"
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
    if [[ "$base" == "class_decl" ]]; then
        for required in \
            '"decls":[{"kind":"class","nominal_kind":"class","name":"Vec2"' \
            '"fields":[{"name":"x","type":"Int"},{"name":"y","type":"Int"}]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] class_decl: missing MIR class declaration fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "class_method" ]]; then
        for required in \
            '"methods":[{"name":"Length","return":"Int"}]' \
            '"name":"Length","kind":"method","owner":"Vec2"'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] class_method: missing MIR class method fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == nominal_* ]]; then
        nominal_kind=""
        nominal_name=""
        field_name=""
        case "$base" in
            nominal_subject)
                nominal_kind="subject"
                nominal_name="Hero"
                field_name="hp"
                ;;
            nominal_object)
                nominal_kind="object"
                nominal_name="PlayerView"
                field_name="score"
                ;;
            nominal_tobject)
                nominal_kind="tobject"
                nominal_name="PlayerDto"
                field_name="score"
                ;;
            nominal_vessel)
                nominal_kind="vessel"
                nominal_name="HP"
                field_name="value"
                ;;
        esac
        for required in \
            "\"decls\":[{\"kind\":\"class\",\"nominal_kind\":\"$nominal_kind\",\"name\":\"$nominal_name\"" \
            "\"fields\":[{\"name\":\"$field_name\",\"type\":\"Int\"}]"; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] $base: missing MIR nominal declaration fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "ability_decl" ]]; then
        for required in \
            '"decls":[{"kind":"ability","name":"Arithmetic"' \
            '"methods":[{"name":"Add","return":"Int","params":[{"name":"self","type":null},{"name":"rhs","type":"Int"}]}]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] ability_decl: missing MIR ability declaration fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "enum_simple" ]]; then
        for required in \
            '"decls":[{"kind":"enum","name":"Direction"' \
            '"variants":[{"name":"North","param_count":0},{"name":"East","param_count":0},{"name":"South","param_count":0},{"name":"West","param_count":0}]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] enum_simple: missing MIR enum declaration fact: $required" >&2
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
    if [[ "$base" == "option_match" ]]; then
        for required in \
            '"source_type":"AST_MATCH_CASE"' \
            '"match_patterns":["Some(v)"],"match_variant":"Some","match_bindings":["v"]' \
            '"match_patterns":["None"],"match_variant":"None","match_bindings":[]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] option_match: missing MIR option match fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "role_operator_dispatch" ]]; then
        for required in \
            '"kind":"role","name":"IntMath","for_type":"Int"' \
            '"impls":[{"ability":{"base":"Arithmetic","actuals":[]},"method_start":0,"method_count":1}]' \
            '"methods":[{"name":"Add","return":"Int","params":[{"name":"self","type":null},{"name":"rhs","type":"Int"}]}]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] $base: missing role declaration fact: $required" >&2
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
    # Observable-failure guard (CLAUDE.md s1.1): the `|| true` above swallows the
    # tool's exit code, so a mir_lower.exe that is missing, stale, or unrunnable in
    # this environment yields an EMPTY $reast with no MIR-LOWER ERROR marker. Left
    # unchecked, that silently flows into an empty reconstructed C and surfaces much
    # later as a cryptic `undefined reference to WinMain` link error. Fail loud here
    # with the real cause instead.
    if [[ ! -s "$reast" ]]; then
        echo "[self-host-parity:mir-json] $base: mir_lower produced EMPTY output." >&2
        echo "  This is a self-host TOOL/BUILD issue, not a lowering gap: mir_lower.exe" >&2
        echo "  ($B/mir_lower.exe) is missing, stale, or not runnable in this" >&2
        echo "  environment (e.g. the 'pgy --backend=c' tool rebuild could not invoke" >&2
        echo "  gcc). Rebuild the self-host tools in a shell where gcc works." >&2
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
    if [[ "$base" == "class_decl" ]]; then
        for required in \
            'Class: Vec2' \
            'Let: v : Vec2 = Vec2(3, 7)' \
            'Log(v.x)'; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] class_decl: mir_lower did not reconstruct class fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "role_operator_dispatch" ]]; then
        for required in \
            'Role: IntMath for Int' \
            'Function: Add' \
            'self: Int' \
            'Log((a + b))'; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] $base: mir_lower did not reconstruct role fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "class_method" ]]; then
        for required in \
            'Class: Vec2' \
            'Methods:' \
            'Function: Length' \
            'Log(v.Length())'; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] class_method: mir_lower did not reconstruct class method fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == nominal_* ]]; then
        ast_label=""
        nominal_name=""
        local_name=""
        init_expr=""
        log_expr=""
        case "$base" in
            nominal_subject)
                ast_label="Subject"
                nominal_name="Hero"
                local_name="hero"
                init_expr="Hero(hp: 7)"
                log_expr="Log(hero.hp)"
                ;;
            nominal_object)
                ast_label="Object"
                nominal_name="PlayerView"
                local_name="view"
                init_expr="PlayerView(score: 11)"
                log_expr="Log(view.score)"
                ;;
            nominal_tobject)
                ast_label="TObject"
                nominal_name="PlayerDto"
                local_name="dto"
                init_expr="PlayerDto(score: 12)"
                log_expr="Log(dto.score)"
                ;;
            nominal_vessel)
                ast_label="Vessel"
                nominal_name="HP"
                local_name="hp"
                init_expr="HP(value: 13)"
                log_expr="Log(hp.value)"
                ;;
        esac
        for required in \
            "$ast_label: $nominal_name" \
            "Let: $local_name : $nominal_name = $init_expr" \
            "$log_expr"; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] $base: mir_lower did not reconstruct nominal fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "ability_decl" ]]; then
        for required in \
            '[export] Ability: Arithmetic' \
            'Function: Add' \
            'rhs: Int' \
            'Returns: Int' \
            'Log(7)'; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] ability_decl: mir_lower did not reconstruct ability fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" == "enum_simple" ]]; then
        if ! grep -Fq 'Enum: Direction { North, East, South, West }' "$reast"; then
            echo "[self-host-parity:mir-json] enum_simple: mir_lower did not reconstruct enum facts" >&2
            exit 1
        fi
        if ! grep -Fq 'Let: d : Int = Direction.South' "$reast"; then
            echo "[self-host-parity:mir-json] enum_simple: mir_lower did not preserve enum reference expression" >&2
            exit 1
        fi
    fi
    if [[ "$base" == "match_case_int" ]] && ! grep -q 'If: x == 3' "$reast"; then
        echo "[self-host-parity:mir-json] match_case_int: mir_lower did not reconstruct match case conditions from MIR facts" >&2
        exit 1
    fi
    if [[ "$base" == "option_match" ]]; then
        for required in \
            'If: IsSome(val)' \
            'Let: v : Int = UnwrapOption(val)' \
            'If: !IsSome(val)'; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] option_match: mir_lower did not reconstruct option match fact: $required" >&2
                exit 1
            fi
        done
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
    if [[ "$base" == "defer_scope" ]]; then
        if ! grep -Fq '"source_type":"AST_DEFER_STMT","defer_body":["Log(\"last\")"]' "$mj"; then
            echo "[self-host-parity:mir-json] defer_scope: missing first defer body fact" >&2
            exit 1
        fi
        if ! grep -Fq '"source_type":"AST_DEFER_STMT","defer_body":["Log(\"middle\")"]' "$mj"; then
            echo "[self-host-parity:mir-json] defer_scope: missing second defer body fact" >&2
            exit 1
        fi
        if ! grep -Fq 'Defer:' "$reast" || ! grep -Fq 'Log("middle")' "$reast"; then
            echo "[self-host-parity:mir-json] defer_scope: mir_lower did not reconstruct defer body facts" >&2
            exit 1
        fi
    fi
    "$B/codegen.exe" "${reast#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$via_c" || true
    if grep -q '^CODEGEN ERROR' "$via_c"; then
        echo "[self-host-parity:mir-json] $base: codegen rejected the reconstructed AST:" >&2
        grep '^CODEGEN ERROR' "$via_c" | head -1 >&2
        exit 1
    fi
    # Observable-failure guard (CLAUDE.md s1.1): same rationale as the $reast guard
    # above. An empty $via_c (codegen.exe missing/stale/unrunnable) would otherwise
    # be handed to gcc and fail as `undefined reference to WinMain` (no main), hiding
    # the real cause. Report the tool/build issue directly.
    if [[ ! -s "$via_c" ]]; then
        echo "[self-host-parity:mir-json] $base: codegen produced EMPTY C output." >&2
        echo "  This is a self-host TOOL/BUILD issue, not a codegen gap: codegen.exe" >&2
        echo "  ($B/codegen.exe) is missing, stale, or not runnable in this environment." >&2
        echo "  Rebuild the self-host tools in a shell where gcc works." >&2
        exit 1
    fi
    if ! compile_c_to_exe "$via_c" "$B/${base}_via_mir.exe" "$B/${base}_cc.log"; then
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

echo "[self-host-parity:mir-json] rung-0b MIR->C parity ok (${pass} fixtures, 0 clean rejects; pgy --mir-json | mir_lower | codegen == C oracle)"

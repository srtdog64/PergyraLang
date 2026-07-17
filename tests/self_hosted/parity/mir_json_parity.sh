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
source "$ROOT_DIR/tests/portable_process_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
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

B="$ROOT_DIR/.tmp/self_hosted/mir_lower/parity"
HARNESS_PATHS_FILE="$B/mir_json_harness_paths.txt"
ARTIFACT_COMPARE_BUILD_DIR="$B/artifact_owner"
MIR_FIXTURE_MANIFEST_FILE="$B/mir_fixture_manifest.txt"
MIR_RUN_COMPARATOR_BIN=""
RUN_TIMEOUT_SECONDS="${PGY_SELFHOST_MIR_RUN_TIMEOUT_SECONDS:-30}"
MIR_LOWER_SRC=""
CODEGEN_SRC=""
COMPARATOR_SOURCE=""
FIXTURES=()
mkdir -p "$B"

pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:mir-json" \
    "$B" \
    "mir-json-parity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[self-host-parity:mir-json] TestHarness manifest expected 3 MIR JSON paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

MIR_LOWER_SRC="$ROOT_DIR/${harness_paths[0]}"
CODEGEN_SRC="$ROOT_DIR/${harness_paths[1]}"
COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"

for path in "$MIR_LOWER_SRC" "$CODEGEN_SRC" "$COMPARATOR_SOURCE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:mir-json] missing TestHarness input: $path" >&2
        exit 1
    fi
done

compare_mir_run_output_with_owner() {
    local base="$1"
    local via_file="$2"
    local oracle_file="$3"
    local safe_label="${base//[^A-Za-z0-9_]/_}"
    local oracle_norm="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}_oracle.out"
    local via_norm="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}_via_mir.out"
    local cmp_out="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}.compare.out"
    local cmp_err="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}.compare.err"
    local oracle_rel
    local via_rel

    pgy_selfhost_normalize_text_artifact < "$oracle_file" > "$oracle_norm"
    pgy_selfhost_normalize_text_artifact < "$via_file" > "$via_norm"
    oracle_rel="$(pgy_selfhost_path_relative_to_root "$oracle_norm")"
    via_rel="$(pgy_selfhost_path_relative_to_root "$via_norm")"

    if ! (cd "$ROOT_DIR" && "$MIR_RUN_COMPARATOR_BIN" \
        "$oracle_rel" "$via_rel" 0 2 run_output \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[self-host-parity:mir-json] $base: MIR->C run-output artifact parity FAIL" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

run_mir_binary_to_file() {
    local base="$1"
    local kind="$2"
    local bin="$3"
    local output="$4"
    local error="$5"
    local rc

    if (cd "$ROOT_DIR" && pgy_run_with_timeout \
            "$RUN_TIMEOUT_SECONDS" "$output" "$error" "$bin"); then
        return
    else
        rc=$?
    fi
    echo "[self-host-parity:mir-json] $base: $kind execution failed (rc=$rc)" >&2
    cat "$error" >&2 || true
    exit 1
}

read_mir_fixture_manifest() {
    local manifest_err="$B/mir_fixture_manifest.err"
    local line

    FIXTURES=()
    if ! (cd "$ROOT_DIR" && "$B/mir_lower.exe" --fixture-manifest \
        >"$MIR_FIXTURE_MANIFEST_FILE" \
        2>"$manifest_err"); then
        echo "[self-host-parity:mir-json] fixture manifest emission failed" >&2
        cat "$manifest_err" >&2
        exit 1
    fi

    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        FIXTURES+=("$line")
    done <"$MIR_FIXTURE_MANIFEST_FILE"

    if [[ "${#FIXTURES[@]}" -eq 0 ]]; then
        echo "[self-host-parity:mir-json] fixture manifest is empty" >&2
        exit 1
    fi

    if [[ -n "${PGY_SELFHOST_MIR_FIXTURES:-}" ]]; then
        local selected=()
        local requested
        local requested_fixtures=()
        local fixture_entry
        IFS=', ' read -r -a requested_fixtures \
            <<< "$PGY_SELFHOST_MIR_FIXTURES"
        for requested in "${requested_fixtures[@]}"; do
            [[ -n "$requested" ]] || continue
            local matched=0
            for fixture_entry in "${FIXTURES[@]}"; do
                if [[ "$(basename "$fixture_entry" .pgy)" == "$requested" ]]; then
                    selected+=("$fixture_entry")
                    matched=1
                    break
                fi
            done
            if [[ "$matched" -ne 1 ]]; then
                echo "[self-host-parity:mir-json] unknown fixture filter: $requested" >&2
                exit 1
            fi
        done
        if [[ "${#selected[@]}" -eq 0 ]]; then
            echo "[self-host-parity:mir-json] empty fixture filter" >&2
            exit 1
        fi
        FIXTURES=("${selected[@]}")
    fi
}

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
pgy_selfhost_compile_backend_output_comparator \
    "self-host-parity:mir-json" "$ARTIFACT_COMPARE_BUILD_DIR" "$COMPARATOR_SOURCE"
MIR_RUN_COMPARATOR_BIN="$(pgy_selfhost_backend_output_comparator_bin "$ARTIFACT_COMPARE_BUILD_DIR")"

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

# The self-hosted input owner must consume parallel capture facts even while
# the MIR->AST lowering subset does not yet emit parallel bodies. This keeps
# schema acceptance from becoming an ignore-unknown compatibility fallback.
parallel_src="$ROOT_DIR/tests/cases/backend_compare/parallel_join_stencil/main.pgy"
parallel_mir="$B/parallel_capture_valid.mirjson"
parallel_bad_kind="$B/parallel_capture_bad_kind.mirjson"
parallel_bad_writer="$B/parallel_capture_bad_writer.mirjson"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$parallel_src")" \
    2>/dev/null | tr -d '\r' >"$parallel_mir")
if ! (cd "$ROOT_DIR" && "$B/mir_lower.exe" --verify-input \
        "${parallel_mir#$ROOT_DIR/}" \
        >"$B/parallel_capture_valid.out" \
        2>"$B/parallel_capture_valid.err"); then
    echo "[self-host-parity:mir-json] self-host input owner rejected valid parallel capture facts" >&2
    cat "$B/parallel_capture_valid.out" "$B/parallel_capture_valid.err" >&2
    exit 1
fi
grep -Fq 'pgy.mir.v1 input verified' "$B/parallel_capture_valid.out" || {
    echo "[self-host-parity:mir-json] verify-input success marker is missing" >&2
    exit 1
}
sed 's/"kind":"join_readonly"/"kind":"unknown"/g' \
    "$parallel_mir" >"$parallel_bad_kind"
if (cd "$ROOT_DIR" && "$B/mir_lower.exe" --verify-input \
        "${parallel_bad_kind#$ROOT_DIR/}") >/dev/null 2>&1; then
    echo "[self-host-parity:mir-json] self-host input owner admitted an unknown parallel capture kind" >&2
    exit 1
fi
sed 's/"kind":"join_index_disjoint","writer_task":0/"kind":"join_index_disjoint","writer_task":1/g' \
    "$parallel_mir" >"$parallel_bad_writer"
if (cd "$ROOT_DIR" && "$B/mir_lower.exe" --verify-input \
        "${parallel_bad_writer#$ROOT_DIR/}") >/dev/null 2>&1; then
    echo "[self-host-parity:mir-json] self-host input owner admitted a writer on a join disposition" >&2
    exit 1
fi

read_mir_fixture_manifest

pass=0
for fixture_entry in "${FIXTURES[@]}"; do
    base="$(basename "$fixture_entry" .pgy)"
    src="$ROOT_DIR/$fixture_entry"
    if [[ ! -f "$src" ]]; then
        echo "[self-host-parity:mir-json] missing fixture: $fixture_entry" >&2
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
            '"methods":[{"name":"LengthPlus","return":"Int"}]' \
            '"name":"LengthPlus","kind":"method","owner":"Vec2"'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] class_method: missing MIR class method fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" =~ ^nominal_(subject|object|tobject|vessel)$ ]]; then
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
    if [[ "$base" == "nominal_record_array" ]]; then
        for required in \
            '"decls":[{"kind":"struct","nominal_kind":"struct","name":"AstExpressionGraphRows"' \
            '"fields":[{"name":"ok","type":"Bool"},{"name":"roots","type":"Array<Int>"}]'; do
            if ! grep -Fq "$required" "$mj"; then
                echo "[self-host-parity:mir-json] nominal_record_array: missing MIR struct declaration fact: $required" >&2
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
    if [[ "$base" == "dir_walk" ]]; then
        if ! grep -Fq 'If: (ArrayLength(files) > 0)' "$reast" ||
            grep -Fq 'While: (ArrayLength(files) > 0)' "$reast"; then
            echo "[self-host-parity:mir-json] dir_walk: acyclic merge edge was classified as a loop backedge" >&2
            exit 1
        fi
    fi
    if [[ "$base" == "break_after_stmt" ]]; then
        if ! grep -Fq 'If: (i == 3)' "$reast" ||
            grep -Fq 'While: (i == 3)' "$reast"; then
            echo "[self-host-parity:mir-json] break_after_stmt: outer-cycle edge was classified as an inner loop backedge" >&2
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
            'Function: LengthPlus' \
            'Return: ((self.x + self.y) + extra)' \
            'Return: (v.LengthPlus(5) - 15)'; do
            if ! grep -Fq "$required" "$reast"; then
                echo "[self-host-parity:mir-json] class_method: mir_lower did not reconstruct class method fact: $required" >&2
                exit 1
            fi
        done
    fi
    if [[ "$base" =~ ^nominal_(subject|object|tobject|vessel)$ ]]; then
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
                init_expr="Hero { hp: 7 }"
                log_expr="Log(hero.hp)"
                ;;
            nominal_object)
                ast_label="Object"
                nominal_name="PlayerView"
                local_name="view"
                init_expr="PlayerView { score: 11 }"
                log_expr="Log(view.score)"
                ;;
            nominal_tobject)
                ast_label="TObject"
                nominal_name="PlayerDto"
                local_name="dto"
                init_expr="PlayerDto { score: 12 }"
                log_expr="Log(dto.score)"
                ;;
            nominal_vessel)
                ast_label="Vessel"
                nominal_name="HP"
                local_name="hp"
                init_expr="HP { value: 13 }"
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
    if [[ "$base" == "nominal_record_array" ]] &&
        ! grep -Fq 'Struct: AstExpressionGraphRows' "$reast"; then
        echo "[self-host-parity:mir-json] nominal_record_array: mir_lower did not reconstruct struct facts" >&2
        exit 1
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
    if [[ "$base" == "break_after_stmt" ]]; then
        if grep -q 'While: (i == 3)' "$reast" || ! grep -q 'If: (i == 3)' "$reast"; then
            echo "[self-host-parity:mir-json] break_after_stmt: inner break guard was not reconstructed as an if" >&2
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

    via="$B/${base}_via_mir.run.out"
    orc="$B/${base}_oracle.run.out"
    run_mir_binary_to_file \
        "$base" "reconstructed binary" "$B/${base}_via_mir.exe" "$via" "$B/${base}_via_mir.run.err"
    run_mir_binary_to_file \
        "$base" "oracle binary" "$B/${base}_oracle.exe" "$orc" "$B/${base}_oracle.run.err"
    compare_mir_run_output_with_owner "$base" "$via" "$orc"
    pass=$((pass + 1))
done

echo "[self-host-parity:mir-json] rung-0b MIR->C parity ok (${pass} fixtures, 0 clean rejects; pgy --mir-json | mir_lower | codegen == C oracle)"

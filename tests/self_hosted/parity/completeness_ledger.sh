#!/usr/bin/env bash
# M2 self-host completeness ledger.
#
# Counts real self-host production sources through the staged self-host tools.
# Out-of-subset codegen is a measured failure, not a skip.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    if [[ -z "${PGY_BIN:-}" ]]; then
        echo "[self-host-completeness] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-completeness] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_COMPLETENESS_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/completeness}"
SOURCE_MANIFEST="$BUILD_DIR/sources.txt"
STAGE_MANIFEST="$BUILD_DIR/stages.txt"
SEMANTIC_TARGET_MANIFEST="$BUILD_DIR/semantic_targets.txt"
BASELINE_MANIFEST="$BUILD_DIR/baseline.txt"
LEX_PARSE_BASELINE_MANIFEST="$BUILD_DIR/lex_parse_baseline.txt"
LEX_PARSE_SEMANTIC_BASELINE_MANIFEST="$BUILD_DIR/lex_parse_semantic_baseline.txt"
FULL_PIPELINE_BASELINE_MANIFEST="$BUILD_DIR/full_pipeline_baseline.txt"
LEXER_PATH_MANIFEST="$BUILD_DIR/lexer_paths.txt"
PARSER_PATH_MANIFEST="$BUILD_DIR/parser_paths.txt"
SEMANTIC_PATH_MANIFEST="$BUILD_DIR/semantic_paths.txt"
CODEGEN_PATH_MANIFEST="$BUILD_DIR/codegen_paths.txt"
CHECK_TIMEOUT_SEC="${PGY_SELFHOST_COMPLETENESS_TIMEOUT_SEC:-60}"
TIMEOUT_EXIT_CODE=124
mkdir -p "$BUILD_DIR"

read_manifest() {
    local suite="$1"
    local out_file="$2"
    pgy_selfhost_read_test_harness_manifest \
        "self-host-completeness" \
        "$BUILD_DIR/manifest" \
        "$suite" \
        "$out_file"
}

read_manifest "self-host-completeness-sources" "$SOURCE_MANIFEST"
read_manifest "self-host-completeness-stages" "$STAGE_MANIFEST"
read_manifest "self-host-completeness-semantic-targets" "$SEMANTIC_TARGET_MANIFEST"
read_manifest "self-host-completeness-baseline" "$BASELINE_MANIFEST"
read_manifest "self-host-completeness-lex-parse-baseline" "$LEX_PARSE_BASELINE_MANIFEST"
read_manifest "self-host-completeness-lex-parse-semantic-baseline" "$LEX_PARSE_SEMANTIC_BASELINE_MANIFEST"
read_manifest "self-host-completeness-full-pipeline-baseline" "$FULL_PIPELINE_BASELINE_MANIFEST"
read_manifest "lexer-parity-paths" "$LEXER_PATH_MANIFEST"
read_manifest "parser-parity-paths" "$PARSER_PATH_MANIFEST"
read_manifest "semantic-parity-paths" "$SEMANTIC_PATH_MANIFEST"
read_manifest "codegen-parity-paths" "$CODEGEN_PATH_MANIFEST"

SOURCES=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    SOURCES+=("$line")
done < <(grep -E '[.]pgy$' "$SOURCE_MANIFEST" | sort)

STAGES=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    STAGES+=("$line")
done < <(grep -E '^(lexer|parser|semantic|codegen)$' "$STAGE_MANIFEST")

if [[ "${#STAGES[@]}" -ne 4 ]]; then
    echo "[self-host-completeness] expected 4 stage rows, got ${#STAGES[@]}" >&2
    cat "$STAGE_MANIFEST" >&2
    exit 1
fi

stage_known() {
    case "$1" in
        lexer|parser|semantic|codegen)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

stage_selected() {
    local wanted="$1"
    local stage
    for stage in "${STAGES[@]}"; do
        if [[ "$stage" == "$wanted" ]]; then
            return 0
        fi
    done
    return 1
}

if [[ -n "${PGY_SELFHOST_COMPLETENESS_STAGES:-}" ]]; then
    FILTERED_STAGES=()
    IFS=', ' read -r -a FILTER_ITEMS <<< "$PGY_SELFHOST_COMPLETENESS_STAGES"
    for stage in "${FILTER_ITEMS[@]}"; do
        [[ -n "$stage" ]] || continue
        if ! stage_known "$stage"; then
            echo "[self-host-completeness] unknown stage filter: $stage" >&2
            exit 1
        fi
        FILTERED_STAGES+=("$stage")
    done
    if [[ "${#FILTERED_STAGES[@]}" -eq 0 ]]; then
        echo "[self-host-completeness] empty stage filter" >&2
        exit 1
    fi
    STAGES=("${FILTERED_STAGES[@]}")
fi

FULL_STAGE_RUN=0
if [[ "${#STAGES[@]}" -eq 4 ]] \
    && stage_selected lexer \
    && stage_selected parser \
    && stage_selected semantic \
    && stage_selected codegen; then
    FULL_STAGE_RUN=1
fi

baseline_value() {
    local key="$1"
    local value
    value="$(grep -E "^${key}=" "$BASELINE_MANIFEST" | tail -1 | cut -d= -f2-)"
    if [[ -z "$value" ]]; then
        echo "[self-host-completeness] missing baseline key: $key" >&2
        cat "$BASELINE_MANIFEST" >&2
        exit 1
    fi
    printf '%s\n' "$value"
}

SOURCE_MIN="$(baseline_value source_min)"
LEXER_PASS_MIN="$(baseline_value lexer_pass_min)"
PARSER_PASS_MIN="$(baseline_value parser_pass_min)"
SEMANTIC_PASS_MIN="$(baseline_value semantic_pass_min)"
CODEGEN_PASS_MIN="$(baseline_value codegen_pass_min)"
LEX_PARSE_PASS_MIN="$(baseline_value lex_parse_pass_min)"
LEX_PARSE_SEMANTIC_PASS_MIN="$(baseline_value lex_parse_semantic_pass_min)"
FULL_PIPELINE_PASS_MIN="$(baseline_value full_pipeline_pass_min)"

tool_source_path_from_manifest() {
    local label="$1"
    local manifest="$2"
    local source_rel
    source_rel="$(sed -n '1p' "$manifest")"
    if [[ -z "$source_rel" ]]; then
        echo "[self-host-completeness] missing $label source row from TestHarness" >&2
        cat "$manifest" >&2
        exit 1
    fi
    if [[ ! -f "$ROOT_DIR/$source_rel" ]]; then
        echo "[self-host-completeness] missing $label source from TestHarness: $source_rel" >&2
        exit 1
    fi
    printf '%s\n' "$ROOT_DIR/$source_rel"
}

lexer_tool_source_path() {
    tool_source_path_from_manifest lexer "$LEXER_PATH_MANIFEST"
}

parser_tool_source_path() {
    tool_source_path_from_manifest parser "$PARSER_PATH_MANIFEST"
}

semantic_tool_source_path() {
    tool_source_path_from_manifest semantic "$SEMANTIC_PATH_MANIFEST"
}

codegen_tool_source_path() {
    tool_source_path_from_manifest codegen "$CODEGEN_PATH_MANIFEST"
}

compile_tool() {
    local label="$1"
    local source="$2"
    local build_subdir="$3"
    local bin="$BUILD_DIR/$build_subdir/main.exe"
    local log="$BUILD_DIR/$build_subdir/compile.log"

    mkdir -p "$BUILD_DIR/$build_subdir"

    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1); then
        echo "[self-host-completeness] $label build failed" >&2
        cat "$log" >&2
        exit 1
    fi
    printf '%s\n' "$bin"
}

LEXER_BIN=""
PARSER_BIN=""
SEMANTIC_BIN=""
CODEGEN_BIN=""

if stage_selected lexer; then
    LEXER_BIN="$(compile_tool lexer "$(lexer_tool_source_path)" lexer)"
fi
if stage_selected parser; then
    PARSER_BIN="$(compile_tool parser "$(parser_tool_source_path)" parser)"
fi
if stage_selected semantic; then
    SEMANTIC_BIN="$(compile_tool semantic "$(semantic_tool_source_path)" semantic)"
fi
if stage_selected codegen; then
    # Codegen completeness consumes AST text emitted by the self-host parser.
    # `pgy --ast` remains a separate oracle/parity target, not this stage's
    # producer.
    if [[ -z "$PARSER_BIN" ]]; then
        PARSER_BIN="$(compile_tool parser "$(parser_tool_source_path)" parser)"
    fi
    CODEGEN_BIN="$(compile_tool codegen "$(codegen_tool_source_path)" codegen)"
fi

run_source_check() {
    local stage="$1"
    local bin="$2"
    local src="$3"
    local check_src="${4:-$src}"
    local out="$BUILD_DIR/${stage}_${src//[^A-Za-z0-9_]/_}.out"
    local err="$BUILD_DIR/${stage}_${src//[^A-Za-z0-9_]/_}.err"

    (cd "$ROOT_DIR" && timeout "$CHECK_TIMEOUT_SEC" "$bin" --check "$check_src" >"$out" 2>"$err")
    local rc="$?"
    if [[ "$rc" -eq 0 ]]; then
        if grep -Fq 'Status: ok' "$out"; then
            return 0
        fi
        return 1
    fi
    if [[ "$rc" -eq "$TIMEOUT_EXIT_CODE" ]]; then
        echo "[self-host-completeness] $stage timed out after ${CHECK_TIMEOUT_SEC}s: $src" >&2
        return 2
    fi
    return 1
}

semantic_check_target_for() {
    local src="$1"
    local target
    target="$(awk -F '\t' -v src="$src" '$1 == src { print $2; exit }' "$SEMANTIC_TARGET_MANIFEST")"
    if [[ -z "$target" ]]; then
        printf '%s\n' "$src"
        return
    fi
    printf '%s\n' "$target"
}

run_codegen_check() {
    local src="$1"
    local safe="${src//[^A-Za-z0-9_]/_}"
    local ast_rel=".tmp/self_hosted/completeness/ast/${safe}.ast.txt"
    local ast_abs="$ROOT_DIR/$ast_rel"
    local ast_err="$BUILD_DIR/ast/${safe}.err"
    local out="$BUILD_DIR/codegen_${safe}.out"
    local err="$BUILD_DIR/codegen_${safe}.err"

    mkdir -p "$ROOT_DIR/.tmp/self_hosted/completeness/ast"
    (cd "$ROOT_DIR" && timeout "$CHECK_TIMEOUT_SEC" "$PARSER_BIN" "$src" \
        >"$ast_abs" 2>"$ast_err")
    local ast_rc="$?"
    if [[ "$ast_rc" -eq 0 ]]; then
        :
    else
        if [[ "$ast_rc" -eq "$TIMEOUT_EXIT_CODE" ]]; then
            echo "[self-host-completeness] ast export timed out after ${CHECK_TIMEOUT_SEC}s: $src" >&2
            return 2
        fi
        return 1
    fi
    (cd "$ROOT_DIR" && timeout "$CHECK_TIMEOUT_SEC" "$CODEGEN_BIN" --check "$ast_rel" >"$out" 2>"$err")
    local codegen_rc="$?"
    if [[ "$codegen_rc" -eq 0 ]]; then
        :
    else
        if [[ "$codegen_rc" -eq "$TIMEOUT_EXIT_CODE" ]]; then
            echo "[self-host-completeness] codegen timed out after ${CHECK_TIMEOUT_SEC}s: $src" >&2
            return 2
        fi
        return 1
    fi
    if grep -Fq 'Status: ok' "$out"; then
        return 0
    fi
    return 1
}

count_stage() {
    local stage="$1"
    local pass=0
    local fail=0
    local src
    local failure_manifest="$BUILD_DIR/${stage}_failures.txt"
    local pass_manifest="$BUILD_DIR/${stage}_passes.txt"

    local index=0
    local total="${#SOURCES[@]}"

    : >"$failure_manifest"
    : >"$pass_manifest"
    for src in "${SOURCES[@]}"; do
        index=$((index + 1))
        echo "[self-host-completeness] $stage checking $index/$total $src" >&2
        case "$stage" in
            lexer)
                if run_source_check lexer "$LEXER_BIN" "$src"; then
                    pass=$((pass + 1))
                    printf '%s\n' "$src" >>"$pass_manifest"
                else
                    local rc="$?"
                    if [[ "$rc" -eq 2 ]]; then
                        exit 1
                    fi
                    fail=$((fail + 1))
                    printf '%s\n' "$src" >>"$failure_manifest"
                fi
                ;;
            parser)
                if run_source_check parser "$PARSER_BIN" "$src"; then
                    pass=$((pass + 1))
                    printf '%s\n' "$src" >>"$pass_manifest"
                else
                    local rc="$?"
                    if [[ "$rc" -eq 2 ]]; then
                        exit 1
                    fi
                    fail=$((fail + 1))
                    printf '%s\n' "$src" >>"$failure_manifest"
                fi
                ;;
            semantic)
                local semantic_target
                semantic_target="$(semantic_check_target_for "$src")"
                if run_source_check semantic "$SEMANTIC_BIN" "$src" "$semantic_target"; then
                    pass=$((pass + 1))
                    printf '%s\n' "$src" >>"$pass_manifest"
                else
                    local rc="$?"
                    if [[ "$rc" -eq 2 ]]; then
                        exit 1
                    fi
                    fail=$((fail + 1))
                    printf '%s\n' "$src" >>"$failure_manifest"
                fi
                ;;
            codegen)
                if run_codegen_check "$src"; then
                    pass=$((pass + 1))
                    printf '%s\n' "$src" >>"$pass_manifest"
                else
                    local rc="$?"
                    if [[ "$rc" -eq 2 ]]; then
                        exit 1
                    fi
                    fail=$((fail + 1))
                    printf '%s\n' "$src" >>"$failure_manifest"
                fi
                ;;
            *)
                echo "[self-host-completeness] unknown stage: $stage" >&2
                exit 1
                ;;
        esac
    done
    printf '%s\tpass=%s\tfail=%s\n' "$stage" "$pass" "$fail"
}

print_stage_failures() {
    local stage="$1"
    local failure_manifest="$BUILD_DIR/${stage}_failures.txt"

    if [[ ! -s "$failure_manifest" ]]; then
        return 0
    fi
    echo "[self-host-completeness] $stage failures:" >&2
    sed 's/^/  - /' "$failure_manifest" >&2
}

SOURCE_COUNT="${#SOURCES[@]}"
if (( SOURCE_COUNT < SOURCE_MIN )); then
    echo "[self-host-completeness] source count regressed: $SOURCE_COUNT < $SOURCE_MIN" >&2
    exit 1
fi

LEDGER="$BUILD_DIR/ledger.tsv"
: >"$LEDGER"
for stage in "${STAGES[@]}"; do
    count_stage "$stage" | tee -a "$LEDGER"
done

stage_pass() {
    local stage="$1"
    local value
    value="$(grep -E "^${stage}[[:space:]]" "$LEDGER" | sed -E 's/.*pass=([0-9]+).*/\1/' || true)"
    if [[ -z "$value" ]]; then
        printf '0\n'
        return
    fi
    printf '%s\n' "$value"
}

pipeline_count() {
    local manifest="$1"
    if [[ ! -s "$manifest" ]]; then
        printf '0\n'
        return
    fi
    wc -l <"$manifest" | tr -d '[:space:]'
}

write_pipeline_manifests() {
    sort -u "$BUILD_DIR/lexer_passes.txt" >"$BUILD_DIR/lexer_passes.sorted"
    sort -u "$BUILD_DIR/parser_passes.txt" >"$BUILD_DIR/parser_passes.sorted"
    sort -u "$BUILD_DIR/semantic_passes.txt" >"$BUILD_DIR/semantic_passes.sorted"
    sort -u "$BUILD_DIR/codegen_passes.txt" >"$BUILD_DIR/codegen_passes.sorted"

    comm -12 "$BUILD_DIR/lexer_passes.sorted" "$BUILD_DIR/parser_passes.sorted" \
        >"$BUILD_DIR/lex_parse_passes.txt"
    comm -12 "$BUILD_DIR/lex_parse_passes.txt" "$BUILD_DIR/semantic_passes.sorted" \
        >"$BUILD_DIR/lex_parse_semantic_passes.txt"
    comm -12 "$BUILD_DIR/lex_parse_semantic_passes.txt" "$BUILD_DIR/codegen_passes.sorted" \
        >"$BUILD_DIR/full_pipeline_passes.txt"
}

check_pipeline_identity() {
    local label="$1"
    local baseline="$2"
    local current="$3"
    local missing="$BUILD_DIR/${label}_identity_missing.txt"
    local line

    : >"$missing"
    while IFS= read -r line; do
        [[ -n "$line" ]] || continue
        if ! grep -Fxq -- "$line" "$current"; then
            printf '%s\n' "$line" >>"$missing"
        fi
    done <"$baseline"

    if [[ -s "$missing" ]]; then
        echo "[self-host-completeness] $label pipeline identity regressed:" >&2
        sed 's/^/  - /' "$missing" >&2
        exit 1
    fi
}

LEXER_PASS="$(stage_pass lexer)"
PARSER_PASS="$(stage_pass parser)"
SEMANTIC_PASS="$(stage_pass semantic)"
CODEGEN_PASS="$(stage_pass codegen)"

if stage_selected lexer && (( LEXER_PASS < LEXER_PASS_MIN )); then
    echo "[self-host-completeness] lexer pass count regressed: $LEXER_PASS < $LEXER_PASS_MIN" >&2
    print_stage_failures lexer
    exit 1
fi
if stage_selected parser && (( PARSER_PASS < PARSER_PASS_MIN )); then
    echo "[self-host-completeness] parser pass count regressed: $PARSER_PASS < $PARSER_PASS_MIN" >&2
    print_stage_failures parser
    exit 1
fi
if stage_selected semantic && (( SEMANTIC_PASS < SEMANTIC_PASS_MIN )); then
    echo "[self-host-completeness] semantic pass count regressed: $SEMANTIC_PASS < $SEMANTIC_PASS_MIN" >&2
    print_stage_failures semantic
    exit 1
fi
if stage_selected codegen && (( CODEGEN_PASS < CODEGEN_PASS_MIN )); then
    echo "[self-host-completeness] codegen pass count regressed: $CODEGEN_PASS < $CODEGEN_PASS_MIN" >&2
    print_stage_failures codegen
    exit 1
fi

if [[ "$FULL_STAGE_RUN" -eq 1 ]]; then
    write_pipeline_manifests
    LEX_PARSE_PASS="$(pipeline_count "$BUILD_DIR/lex_parse_passes.txt")"
    LEX_PARSE_SEMANTIC_PASS="$(pipeline_count "$BUILD_DIR/lex_parse_semantic_passes.txt")"
    FULL_PIPELINE_PASS="$(pipeline_count "$BUILD_DIR/full_pipeline_passes.txt")"

    if (( LEX_PARSE_PASS < LEX_PARSE_PASS_MIN )); then
        echo "[self-host-completeness] lex+parse pipeline pass count regressed: $LEX_PARSE_PASS < $LEX_PARSE_PASS_MIN" >&2
        exit 1
    fi
    if (( LEX_PARSE_SEMANTIC_PASS < LEX_PARSE_SEMANTIC_PASS_MIN )); then
        echo "[self-host-completeness] lex+parse+semantic pipeline pass count regressed: $LEX_PARSE_SEMANTIC_PASS < $LEX_PARSE_SEMANTIC_PASS_MIN" >&2
        exit 1
    fi
    if (( FULL_PIPELINE_PASS < FULL_PIPELINE_PASS_MIN )); then
        echo "[self-host-completeness] full pipeline pass count regressed: $FULL_PIPELINE_PASS < $FULL_PIPELINE_PASS_MIN" >&2
        exit 1
    fi
    check_pipeline_identity "lex_parse" "$LEX_PARSE_BASELINE_MANIFEST" "$BUILD_DIR/lex_parse_passes.txt"
    check_pipeline_identity "lex_parse_semantic" "$LEX_PARSE_SEMANTIC_BASELINE_MANIFEST" "$BUILD_DIR/lex_parse_semantic_passes.txt"
    check_pipeline_identity "full_pipeline" "$FULL_PIPELINE_BASELINE_MANIFEST" "$BUILD_DIR/full_pipeline_passes.txt"

    echo "[self-host-completeness] ledger ok: sources=$SOURCE_COUNT lexer=$LEXER_PASS parser=$PARSER_PASS semantic=$SEMANTIC_PASS codegen=$CODEGEN_PASS lex_parse=$LEX_PARSE_PASS lex_parse_semantic=$LEX_PARSE_SEMANTIC_PASS full_pipeline=$FULL_PIPELINE_PASS"
    echo "[self-host-completeness] failure manifests: $BUILD_DIR/{lexer,parser,semantic,codegen}_failures.txt"
    echo "[self-host-completeness] pipeline manifests: $BUILD_DIR/{lex_parse,lex_parse_semantic,full_pipeline}_passes.txt"
else
    echo "[self-host-completeness] focused ledger ok: sources=$SOURCE_COUNT lexer=$LEXER_PASS parser=$PARSER_PASS semantic=$SEMANTIC_PASS codegen=$CODEGEN_PASS stages=${STAGES[*]}"
    echo "[self-host-completeness] focused failure manifests: $BUILD_DIR/{lexer,parser,semantic,codegen}_failures.txt"
fi

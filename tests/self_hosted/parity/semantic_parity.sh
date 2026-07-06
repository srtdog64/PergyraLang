#!/usr/bin/env bash
# Rung 2 parity for the Pergyra-origin semantic substitution slice.
# The Pergyra tool emits a bounded deterministic verdict while the C compiler
# remains the accept/reject oracle for the same fixtures.

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

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:semantic] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:semantic] missing compiler binary: $PGY" >&2
    exit 1
fi

PGY_EXPECTS_WINDOWS_PATHS=0
if pgy_binary_expects_windows_paths "$PGY"; then
    PGY_EXPECTS_WINDOWS_PATHS=1
fi

semantic_compiler_path() {
    local path="$1"
    if [[ "$PGY_EXPECTS_WINDOWS_PATHS" -eq 1 ]]; then
        pgy_path_for_windows_tool "$path"
        return 0
    fi
    printf '%s\n' "$path"
}

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/semantic}"
ARTIFACT_COMPARE_BUILD_DIR="$PERGYRA_TOOL_BUILD_DIR/artifact_owner"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/semantic_harness_paths.txt"
SEMANTIC_FIXTURE_MANIFEST_FILE="$PERGYRA_TOOL_BUILD_DIR/semantic_fixture_manifest.txt"
SEMANTIC_COMPARATOR_BIN=""
PERGYRA_TOOL_SOURCE=""
PERGYRA_TOOL_ARG=""
COMPARATOR_SOURCE=""
FIXTURE_DIR=""
FIXTURE_DIR_REL=""
EXPECTED_DIR=""
DIAGNOSTIC_CODE_OWNER=""
DIAGNOSTIC_RENDERER_OWNER=""
SEMANTIC_SOURCE_DIR=""

SOURCE_PAIRS=()

known_semantic_codes() {
    awk '
        /^func SemanticDiagnosticCodeKnown\(/ { inside=1; next }
        inside && /^func[[:space:]]/ { inside=0 }
        inside {
            while (match($0, /code == "[a-z0-9_]+"/)) {
                code=substr($0, RSTART, RLENGTH)
                sub(/^code == "/, "", code)
                sub(/"$/, "", code)
                print code
                $0=substr($0, RSTART + RLENGTH)
            }
        }
    ' "$DIAGNOSTIC_CODE_OWNER" |
        sort
}

contains_line() {
    local haystack="$1"
    local needle="$2"
    local line

    while IFS= read -r line; do
        [[ "$line" == "$needle" ]] && return 0
    done <<<"$haystack"
    return 1
}

semantic_code_known() {
    local code="$1"
    local known

    known="$(known_semantic_codes)"
    contains_line "$known" "$code"
}

semantic_oracle_code_for() {
    local code="$1"
    awk -v target="$code" '
        /^func SemanticDiagnosticOracleCode\(/ { inside=1; next }
        inside && /^func[[:space:]]/ { inside=0 }
        inside && $0 ~ "code == \"" target "\"" { found=1; next }
        inside && found && match($0, /return "[^"]*"/) {
            value=substr($0, RSTART, RLENGTH)
            sub(/^return "/, "", value)
            sub(/"$/, "", value)
            print value
            exit
        }
    ' "$DIAGNOSTIC_CODE_OWNER"
}

json_code_from_output() {
    tr -d '\r' |
        grep -oE '"code"[[:space:]]*:[[:space:]]*"[^"]*"' |
        head -n 1 |
        sed -E 's/.*"code"[[:space:]]*:[[:space:]]*"([^"]*)".*/\1/'
}

compare_semantic_verdict_with_owner() {
    local backend="$1"
    local base="$2"
    local expected_file="$3"
    local actual_text="$4"
    local safe_label="${base//[^A-Za-z0-9_]/_}_${backend}"
    local expected_norm="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}_expected.diag"
    local actual_norm="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}_actual.diag"
    local cmp_out="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}.compare.out"
    local cmp_err="$ARTIFACT_COMPARE_BUILD_DIR/${safe_label}.compare.err"
    local expected_rel
    local actual_rel

    pgy_selfhost_normalize_text_artifact < "$expected_file" > "$expected_norm"
    printf '%s' "$actual_text" | pgy_selfhost_normalize_text_artifact > "$actual_norm"
    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected_norm")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual_norm")"

    if ! (cd "$ROOT_DIR" && "$SEMANTIC_COMPARATOR_BIN" \
        "$expected_rel" "$actual_rel" 0 2 diagnostics \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[self-host-parity:semantic] backend=$backend $base: diagnostics artifact parity FAIL" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

check_semantic_diagnostic_code_surface() {
    local owner="$DIAGNOSTIC_CODE_OWNER"
    local renderer="$DIAGNOSTIC_RENDERER_OWNER"
    local known total unique declared_count

    if [[ ! -f "$owner" ]]; then
        echo "[self-host-parity:semantic] missing diagnostic code owner: $owner" >&2
        exit 1
    fi

    known="$(known_semantic_codes)"
    if [[ -z "$known" ]]; then
        echo "[self-host-parity:semantic] diagnostic code owner has no codes" >&2
        exit 1
    fi

    total="$(printf '%s\n' "$known" | sed '/^$/d' | wc -l | tr -d ' ')"
    unique="$(printf '%s\n' "$known" | sed '/^$/d' | sort -u | wc -l | tr -d ' ')"
    if [[ "$total" -ne "$unique" ]]; then
        echo "[self-host-parity:semantic] duplicate diagnostic code in owner" >&2
        printf '%s\n' "$known" >&2
        exit 1
    fi

    declared_count="$(grep -oE 'return[[:space:]]+[0-9]+;' "$owner" |
        tail -n 1 |
        grep -oE '[0-9]+' || true)"
    if [[ "$declared_count" != "$unique" ]]; then
        echo "[self-host-parity:semantic] diagnostic code count drift (declared=$declared_count actual=$unique)" >&2
        exit 1
    fi

    while IFS= read -r code; do
        [[ -n "$code" ]] || continue
        if ! semantic_code_known "$code"; then
            echo "[self-host-parity:semantic] expected fixture uses unregistered diagnostic code: $code" >&2
            exit 1
        fi
        if ! grep -Fq "code == \"$code\"" "$renderer"; then
            echo "[self-host-parity:semantic] diagnostic code lacks reason/fix renderer branch: $code" >&2
            exit 1
        fi
        if [[ -z "$(semantic_oracle_code_for "$code")" ]]; then
            echo "[self-host-parity:semantic] diagnostic code lacks C oracle mapping: $code" >&2
            exit 1
        fi
    done < <(
        grep -h '^Code: ' "$EXPECTED_DIR"/*.diag |
            tr -d '\r' |
            sed -E 's/^Code: //'
    )

    while IFS= read -r code; do
        [[ -n "$code" ]] || continue
        if ! semantic_code_known "$code"; then
            echo "[self-host-parity:semantic] call site emits unregistered diagnostic code: $code" >&2
            exit 1
        fi
    done < <(
        grep -RhoE 'SemanticError[A-Za-z0-9_]*\("[a-z0-9_]+"' \
            "$SEMANTIC_SOURCE_DIR" |
            sed -E 's/.*"([^"]+)".*/\1/' |
            sort -u
    )

    echo "[self-host-parity:semantic] diagnostic code vocabulary ok ($unique codes)"
}

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:semantic" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "semantic-parity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 7 ]]; then
    echo "[self-host-parity:semantic] TestHarness manifest expected 7 semantic paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"
COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"
FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"
FIXTURE_DIR_REL="${harness_paths[2]}"
EXPECTED_DIR="$ROOT_DIR/${harness_paths[3]}"
DIAGNOSTIC_CODE_OWNER="$ROOT_DIR/${harness_paths[4]}"
DIAGNOSTIC_RENDERER_OWNER="$ROOT_DIR/${harness_paths[5]}"
SEMANTIC_SOURCE_DIR="$ROOT_DIR/${harness_paths[6]}"

for path in "$PERGYRA_TOOL_SOURCE" "$COMPARATOR_SOURCE" "$DIAGNOSTIC_CODE_OWNER" "$DIAGNOSTIC_RENDERER_OWNER"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:semantic] missing TestHarness input: $path" >&2
        exit 1
    fi
done
for dir in "$FIXTURE_DIR" "$EXPECTED_DIR" "$SEMANTIC_SOURCE_DIR"; do
    if [[ ! -d "$dir" ]]; then
        echo "[self-host-parity:semantic] missing TestHarness directory: $dir" >&2
        exit 1
    fi
done

pgy_selfhost_compile_backend_output_comparator \
    "self-host-parity:semantic" "$ARTIFACT_COMPARE_BUILD_DIR" "$COMPARATOR_SOURCE"
SEMANTIC_COMPARATOR_BIN="$(pgy_selfhost_backend_output_comparator_bin "$ARTIFACT_COMPARE_BUILD_DIR")"

check_c_oracle() {
    local base="$1"
    local expected_class="$2"
    local source="$FIXTURE_DIR/${base}.pgy"
    local exe="$PERGYRA_TOOL_BUILD_DIR/${base}.c-oracle.exe"
    local output
    local rc

    set +e
    output="$(cd "$ROOT_DIR" && "$PGY" "$(semantic_compiler_path "$source")" \
        --backend=c --error-format=json \
        -o "$(semantic_compiler_path "$exe")" 2>&1)"
    rc=$?
    set -e

    if [[ "$expected_class" == "ok" && "$rc" -ne 0 ]]; then
        echo "[self-host-parity:semantic] C oracle rejected valid fixture: $base" >&2
        printf '%s\n' "$output" | sed -n '1,40p' >&2
        exit 1
    fi
    if [[ "$expected_class" == "error" && "$rc" -eq 0 ]]; then
        echo "[self-host-parity:semantic] C oracle accepted invalid fixture: $base" >&2
        exit 1
    fi
    if [[ "$expected_class" == "error" ]]; then
        local expected_self_code
        local expected_oracle_code
        local actual_oracle_code
        expected_self_code="$(sed -n 's/^Code: //p' "$EXPECTED_DIR/${base}.diag" |
            tr -d '\r' |
            head -n 1)"
        expected_oracle_code="$(semantic_oracle_code_for "$expected_self_code")"
        actual_oracle_code="$(printf '%s\n' "$output" | json_code_from_output)"
        if [[ -z "$actual_oracle_code" ]]; then
            echo "[self-host-parity:semantic] C oracle emitted no JSON code for invalid fixture: $base" >&2
            printf '%s\n' "$output" | sed -n '1,40p' >&2
            exit 1
        fi
        if [[ "$actual_oracle_code" != "$expected_oracle_code" ]]; then
            echo "[self-host-parity:semantic] C oracle code drift for $base (expected=$expected_oracle_code actual=$actual_oracle_code self=$expected_self_code)" >&2
            printf '%s\n' "$output" | sed -n '1,40p' >&2
            exit 1
        fi
    fi
}

compile_semantic_backend() {
    local backend="$1"
    local tool_bin="$2"

    echo "[self-host-parity:semantic] compiling semantic backend=$backend..."
    (cd "$ROOT_DIR" && "$PGY" \
        "$PERGYRA_TOOL_ARG" \
        --backend="$backend" \
        -o "$(semantic_compiler_path "$tool_bin")" >/dev/null)
}

read_semantic_fixture_manifest() {
    local manifest_bin="$PERGYRA_TOOL_BUILD_DIR/main_manifest.exe"
    local line

    compile_semantic_backend c "$manifest_bin"
    if ! (cd "$ROOT_DIR" && "$manifest_bin" --fixture-manifest >"$SEMANTIC_FIXTURE_MANIFEST_FILE"); then
        echo "[self-host-parity:semantic] fixture manifest emission failed" >&2
        exit 1
    fi

    SOURCE_PAIRS=()
    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        if [[ "$line" != *:* ]]; then
            echo "[self-host-parity:semantic] malformed fixture manifest row: $line" >&2
            exit 1
        fi
        case "${line##*:}" in
            ok|error) ;;
            *)
                echo "[self-host-parity:semantic] unknown fixture expectation in row: $line" >&2
                exit 1
                ;;
        esac
        SOURCE_PAIRS+=("$line")
    done <"$SEMANTIC_FIXTURE_MANIFEST_FILE"

    if [[ "${#SOURCE_PAIRS[@]}" -ne 108 ]]; then
        echo "[self-host-parity:semantic] fixture manifest count drifted: ${#SOURCE_PAIRS[@]} != 108" >&2
        exit 1
    fi
}

run_semantic_backend() {
    local backend="$1"
    local tool_bin="$2"

    for pair in "${SOURCE_PAIRS[@]}"; do
        local base="${pair%%:*}"
        local source="$FIXTURE_DIR/${base}.pgy"
        local expected_file="$EXPECTED_DIR/${base}.diag"
        local pergyra_out
        local rc

        if [[ ! -f "$source" ]]; then
            echo "[self-host-parity:semantic] missing source: $source" >&2
            exit 1
        fi
        if [[ ! -f "$expected_file" ]]; then
            echo "[self-host-parity:semantic] missing expected: $expected_file" >&2
            exit 1
        fi

        set +e
        pergyra_out="$(cd "$ROOT_DIR" && "$tool_bin" \
            "${FIXTURE_DIR_REL}/${base}.pgy" 2>/dev/null \
            | tr -d '\r')"
        rc=$?
        set -e

        if [[ "$rc" -ne 0 ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: exit-code FAIL ($rc)" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if [[ "$pergyra_out" == SEMANTIC\ * ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: raw semantic text leaked" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if [[ "$pergyra_out" == \{* ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: JSON semantic output leaked" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if ! grep -Fq 'Diagnostic: pgy.selfhost.semantic.v1' <<<"$pergyra_out"; then
            echo "[self-host-parity:semantic] backend=$backend $base: semantic diagnostic header missing" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if [[ "${pair##*:}" == "ok" ]]; then
            if ! grep -Fq 'Status: ok' <<<"$pergyra_out"; then
                echo "[self-host-parity:semantic] backend=$backend $base: ok status missing" >&2
                printf '%s\n' "$pergyra_out" >&2
                exit 1
            fi
        else
            if ! grep -Fq 'Status: error' <<<"$pergyra_out" || ! grep -Fq 'Code: ' <<<"$pergyra_out"; then
                echo "[self-host-parity:semantic] backend=$backend $base: error diagnostic shape missing" >&2
                printf '%s\n' "$pergyra_out" >&2
                exit 1
            fi
        fi

        compare_semantic_verdict_with_owner "$backend" "$base" "$expected_file" "$pergyra_out"
    done

    echo "[self-host-parity:semantic] backend=$backend verdicts ok (${#SOURCE_PAIRS[@]} fixtures)"
}

read_semantic_fixture_manifest
check_semantic_diagnostic_code_surface

for pair in "${SOURCE_PAIRS[@]}"; do
    check_c_oracle "${pair%%:*}" "${pair##*:}"
done

BACKENDS="${PGY_SELFHOST_SEMANTIC_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    tool_bin="$PERGYRA_TOOL_BUILD_DIR/main_${backend}.exe"
    compile_semantic_backend "$backend" "$tool_bin"
    run_semantic_backend "$backend" "$tool_bin"
done

echo "[self-host-parity:semantic] rung-2 parity ok (${#SOURCE_PAIRS[@]} fixtures; backends=$BACKENDS)"

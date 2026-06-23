#!/usr/bin/env bash
# Gates compiler-stage self-host substitute contracts.
#
# Heavy parity scripts prove behavior. This smoke proves the self-hosted
# compiler-stage surface itself is wired correctly: each active stage has an
# intent-verification pair, a Makefile target, and fixture/expected files that
# are actually listed by its parity harness.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SELF_HOST_DIR="$ROOT_DIR/src/self_hosted"
PARITY_DIR="$SELF_HOST_DIR/parity"

fail() {
    echo "[self-host-component-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_dir() {
    local rel="$1"
    [[ -d "$ROOT_DIR/$rel" ]] || fail "missing directory $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

reject_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel must not contain retired term: $term"
    fi
}

contains_line() {
    local haystack="$1"
    local needle="$2"
    printf '%s\n' "$haystack" | grep -Fxq -- "$needle"
}

extract_shell_array_items() {
    local file="$1"
    local array_name="$2"
    awk -v array_name="$array_name" '
        $0 ~ "^" array_name "=\\(" { inside = 1; next }
        inside && $0 ~ "^[[:space:]]*\\)" { inside = 0; next }
        inside {
            line = $0
            sub(/[[:space:]]*#.*/, "", line)
            gsub(/"/, "", line)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
            if (line != "") {
                print line
            }
        }
    ' "$file"
}

require_owner_surface() {
    local stage="$1"
    shift
    local stage_dir="$SELF_HOST_DIR/$stage"
    local count
    count="$(find "$stage_dir" -maxdepth 1 -type f -name '*.pgy' | wc -l | tr -d ' ')"
    [[ "$count" -gt 1 ]] ||
        fail "$stage must not be a monolithic main.pgy-only compiler stage"

    local owner
    for owner in "$@"; do
        require_file "src/self_hosted/$stage/$owner"
        require_text "src/self_hosted/$stage/main.pgy" "import \"$owner\";"
    done
}

for stage in lexer parser semantic codegen; do
    require_dir "src/self_hosted/$stage"
    require_file "src/self_hosted/$stage/main.pgy"
    require_file "src/self_hosted/$stage/README.md"
    require_file "src/self_hosted/$stage/intent.md"
    require_dir "src/self_hosted/$stage/fixture"
    require_dir "src/self_hosted/$stage/expected"
    require_file "src/self_hosted/parity/${stage}_parity.sh"

    for anchor in '## Intent' '## Input Contract' '## Output Contract' '## Oracle'; do
        require_text "src/self_hosted/$stage/intent.md" "$anchor"
    done

    require_text "src/self_hosted/parity/${stage}_parity.sh" "set -euo pipefail"
    require_text "Makefile" "self-host-${stage}-parity-test-smoke"
    require_text "Makefile" "src/self_hosted/parity/${stage}_parity.sh"
done

require_dir "src/self_hosted/mir_lower"
require_file "src/self_hosted/mir_lower/main.pgy"
require_file "src/self_hosted/mir_lower/README.md"
require_file "src/self_hosted/mir_lower/intent.md"
require_dir "src/self_hosted/mir_lower/fixture"
require_file "src/self_hosted/parity/mir_json_parity.sh"
for anchor in '## Intent' '## Input Contract' '## Output Contract' '## Oracle'; do
    require_text "src/self_hosted/mir_lower/intent.md" "$anchor"
done
require_text "src/self_hosted/parity/mir_json_parity.sh" "set -euo pipefail"
require_text "Makefile" "self-host-mir-json-parity-test-smoke"
require_text "Makefile" "src/self_hosted/parity/mir_json_parity.sh"

require_owner_surface lexer \
    "char_owner.pgy" \
    "token_owner.pgy" \
    "scan_owner.pgy" \
    "source_input_owner.pgy"
require_owner_surface parser \
    "error_owner.pgy" \
    "cursor_owner.pgy" \
    "source_path_owner.pgy" \
    "tree_text_owner.pgy" \
    "decl_dispatch_owner.pgy"
require_owner_surface semantic \
    "text_scan_owner.pgy" \
    "source_bundle_owner.pgy" \
    "diagnostic_owner.pgy" \
    "env_owner.pgy" \
    "program_check_owner.pgy"
require_owner_surface codegen \
    "text_owner.pgy" \
    "type_env.pgy" \
    "stmt_emit.pgy" \
    "program_emit.pgy"
require_owner_surface mir_lower \
    "error_owner.pgy" \
    "json_fact_read.pgy" \
    "stmt_render.pgy" \
    "routine_lower.pgy" \
    "decl_lower.pgy"

require_text "src/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/semantic/main.pgy"'
require_text "src/self_hosted/parity/selfcheck_sources.sh" '"src/self_hosted/lexer/main.pgy"'
reject_text "src/self_hosted/parity/selfcheck_sources.sh" "lexer_selfcheck_unit"
reject_text "src/self_hosted/parity/selfcheck_sources.sh" "grep -h -v '^import '"
reject_text "src/self_hosted/lexer/main.pgy" "fixture/source.txt"

semantic_items="$(extract_shell_array_items "$PARITY_DIR/semantic_parity.sh" SOURCE_PAIRS | sed 's/:.*//')"
[[ -n "$semantic_items" ]] || fail "semantic parity SOURCE_PAIRS is empty"

while IFS= read -r fixture; do
    base="$(basename "$fixture" .pgy)"
    contains_line "$semantic_items" "$base" ||
        fail "semantic fixture not listed in SOURCE_PAIRS: $base"
    require_file "src/self_hosted/semantic/expected/${base}.diag"
done < <(find "$SELF_HOST_DIR/semantic/fixture" -maxdepth 1 -type f -name '*.pgy' | sort)

while IFS= read -r expected; do
    base="$(basename "$expected" .diag)"
    contains_line "$semantic_items" "$base" ||
        fail "semantic expected not listed in SOURCE_PAIRS: $base"
    require_file "src/self_hosted/semantic/fixture/${base}.pgy"
done < <(find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.diag' | sort)

if find "$SELF_HOST_DIR/semantic/expected" -maxdepth 1 -type f -name '*.json' -print -quit | grep -q .; then
    fail "semantic verdict fixtures must stay diagnostic blocks (*.diag), not JSON"
fi

codegen_items="$(extract_shell_array_items "$PARITY_DIR/codegen_parity.sh" FIXTURES)"
[[ -n "$codegen_items" ]] || fail "codegen parity FIXTURES is empty"
contains_line "$codegen_items" "hello" ||
    fail "codegen no-argument golden fixture must stay listed: hello"
require_text "src/self_hosted/codegen/README.md" "Golden/platform contract"
require_text "src/self_hosted/codegen/README.md" "PGY_SELFHOST_CODEGEN_BACKENDS=c"
require_text "src/self_hosted/parity/codegen_parity.sh" 'run_native_capture()'
require_text "src/self_hosted/parity/codegen_parity.sh" 'pgy_binary_is_runnable_here "$bin"'
require_text "src/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe" "${run_args[@]}"'
require_text "src/self_hosted/parity/codegen_parity.sh" 'run_native_capture "$ROOT_DIR" "$run_raw" "$run_err" "$self_exe" "${run_args[@]}"'

while IFS= read -r fixture; do
    base="$(basename "$fixture" .pgy)"
    contains_line "$codegen_items" "$base" ||
        fail "codegen fixture not listed in FIXTURES: $base"
    require_file "src/self_hosted/codegen/expected/${base}_stdout.txt"
done < <(find "$SELF_HOST_DIR/codegen/fixture" -maxdepth 1 -type f -name '*.pgy' | sort)

while IFS= read -r expected; do
    name="$(basename "$expected")"
    base="${name%_stdout.txt}"
    [[ "$name" != "$base" ]] || fail "codegen expected must end with _stdout.txt: $name"
    contains_line "$codegen_items" "$base" ||
        fail "codegen expected not listed in FIXTURES: $base"
    require_file "src/self_hosted/codegen/fixture/${base}.pgy"
done < <(find "$SELF_HOST_DIR/codegen/expected" -maxdepth 1 -type f -name '*_stdout.txt' | sort)

echo "[self-host-component-contract] compiler-stage contracts ok"

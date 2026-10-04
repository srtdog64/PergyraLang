#!/usr/bin/env bash
# Tests checker mechanics only, not compiler behavior or closure.
set -euo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OWNER="$REPO_DIR/tests/self_hosted_component_contract_smoke.sh"
SOURCE_SIZE_COUNTER="$REPO_DIR/scripts/source_size_count.py"
mkdir -p "$REPO_DIR/.tmp/self_hosted/component_checker"
ROOT_DIR="$(mktemp -d "$REPO_DIR/.tmp/self_hosted/component_checker/run.XXXXXX")"

# Exercise the owner's actual functions without running the full inventory.
# Definitions end at a column-zero brace; embedded awk programs are indented.
definitions="$(awk '
    /^(fail|load_text_cache|function_body_text|load_function_body_cache|require_function_text|reject_function_text|require_max_lines|run_line_cap_checks|check_match_pattern_consumer_placement|check_artifact_comparison_transport_placement|check_replacement_frontier_build_graph|reject_regex_under|run_regex_scope_checks)\(\) \{/ { capture = 1 }
    capture { print }
    /^}/ { capture = 0 }
' "$OWNER")"
eval "$definitions"
for checker in fail load_text_cache function_body_text load_function_body_cache \
        require_function_text reject_function_text require_max_lines run_line_cap_checks \
        check_match_pattern_consumer_placement check_artifact_comparison_transport_placement \
        check_replacement_frontier_build_graph reject_regex_under run_regex_scope_checks; do
    declare -F "$checker" >/dev/null || {
        echo "[component-checker] missing checker function: $checker" >&2
        exit 1
    }
done

expect_rejection() {
    local label="$1" diagnostic="$2"; shift 2
    local status
    if ("$@") >"$ROOT_DIR/$label.out" 2>&1; then
        fail "checker accepted $label"
    else
        status=$?
    fi
    [[ "$status" -eq 1 ]] || fail "$label returned unexpected exit $status"
    grep -Fq -- "$diagnostic" "$ROOT_DIR/$label.out" ||
        fail "$label lost its diagnostic: $diagnostic"
}

# A dry-run is not side-effect-free: GNU make evaluates $(file ...) while
# expanding recipes. Exercise the actual graph checker without build products.
# Production supports GNU make 3.x too; its fixture uses a shell expansion for
# the same response-file isolation obligation because $(file ...) is absent.
frontier_fixture_dir="$ROOT_DIR/frontier-build-graph"
mkdir -p "$frontier_fixture_dir"
write_replacement_frontier_make_fixture() {
    printf 'FIXTURE_CASE := %s\n' "$1" >"$frontier_fixture_dir/Makefile"
    printf '%s\n' \
        'BUILD_DIR ?= build' \
        'PGY_SELF_HOST_COMPILER_ADMITTED ?= 0' \
        '.PHONY: native-fixture self-host-compiler self-host-replacement-frontier-installed-test-smoke self-host-replacement-frontier-test-smoke' \
        'native-fixture:' \
        'ifneq ($(filter 3.%,$(MAKE_VERSION)),)' \
        $'\t$(shell printf "fixture-owned-response\\n" > "$(BUILD_DIR)/compiler_sources.rsp")' \
        'else' \
        $'\t$(file >$(BUILD_DIR)/compiler_sources.rsp,fixture-owned-response)' \
        'endif' \
        $'\t@printf "executed\\n" >> "$(CURDIR)/recipes.executed"' \
        'self-host-replacement-frontier-installed-test-smoke: native-fixture' \
        'ifeq ($(FIXTURE_CASE),installed-bootstrap)' \
        $'\t@echo tests/self_hosted/parity/self_host_compiler_build.sh' \
        'endif' \
        'ifeq ($(FIXTURE_CASE),installed-make-failure)' \
        $'\t$(error fixture installed make failure)' \
        'endif' \
        $'\t@printf "executed\\n" >> "$(CURDIR)/recipes.executed"' \
        'self-host-compiler:' \
        'ifeq ($(PGY_SELF_HOST_COMPILER_ADMITTED),1)' \
        'ifeq ($(FIXTURE_CASE),admitted-rebuild)' \
        $'\t@echo tests/self_hosted/parity/self_host_compiler_build.sh' \
        'endif' \
        'ifeq ($(FIXTURE_CASE),admitted-make-failure)' \
        $'\t$(error fixture admitted make failure)' \
        'endif' \
        'else' \
        'ifneq ($(FIXTURE_CASE),standalone-missing)' \
        $'\t@echo tests/self_hosted/parity/self_host_compiler_build.sh' \
        'endif' \
        'ifeq ($(FIXTURE_CASE),standalone-duplicate)' \
        $'\t@echo tests/self_hosted/parity/self_host_compiler_build.sh' \
        'endif' \
        'ifeq ($(FIXTURE_CASE),standalone-make-failure)' \
        $'\t$(error fixture standalone make failure)' \
        'endif' \
        'endif' \
        $'\t@printf "executed\\n" >> "$(CURDIR)/recipes.executed"' \
        'self-host-replacement-frontier-test-smoke: self-host-compiler self-host-replacement-frontier-installed-test-smoke' \
        $'\t@printf "executed\\n" >> "$(CURDIR)/recipes.executed"' \
        >>"$frontier_fixture_dir/Makefile"
}
check_replacement_frontier_fixture() (
    ROOT_DIR="$frontier_fixture_dir"
    case "${1:-ordinary}" in
        exported-admitted) export PGY_SELF_HOST_COMPILER_ADMITTED=1 ;;
        missing-make) PATH="$frontier_fixture_dir/no-make" ;;
    esac
    check_replacement_frontier_build_graph
)
write_replacement_frontier_make_fixture baseline
[[ ! -d "$frontier_fixture_dir/build" ]] || fail 'frontier fixture is not unbuilt'
check_replacement_frontier_fixture
[[ ! -d "$frontier_fixture_dir/build" ]] ||
    fail 'frontier dry-run created the caller build directory'
frontier_responses=("$frontier_fixture_dir"/.tmp/self_hosted/frontier_graph.*/compiler_sources.rsp)
[[ "${#frontier_responses[@]}" -eq 1 && -f "${frontier_responses[0]}" ]] ||
    fail 'frontier response file was not isolated in its dry-run directory'
grep -Fxq 'fixture-owned-response' "${frontier_responses[0]}" ||
    fail 'frontier fixture did not exercise the make response-file expansion'
mkdir -p "$frontier_fixture_dir/build"
printf 'caller-owned response\n' >"$frontier_fixture_dir/build/compiler_sources.rsp"
cp "$frontier_fixture_dir/build/compiler_sources.rsp" "$frontier_fixture_dir/caller-before.rsp"
check_replacement_frontier_fixture exported-admitted
for variant in installed-bootstrap standalone-missing standalone-duplicate admitted-rebuild \
        installed-make-failure standalone-make-failure admitted-make-failure; do
    write_replacement_frontier_make_fixture "$variant"
    case "$variant" in
        installed-bootstrap) diagnostic='installed replacement frontier must be bootstrap-free' ;;
        standalone-missing|standalone-duplicate) diagnostic='standalone replacement frontier must bootstrap exactly once' ;;
        admitted-rebuild) diagnostic='an admitted compiler pair must not be rebuilt' ;;
        installed-make-failure) diagnostic='installed replacement frontier dry-run failed' ;;
        standalone-make-failure) diagnostic='standalone replacement frontier dry-run failed' ;;
        admitted-make-failure) diagnostic='admitted replacement frontier dry-run failed' ;;
    esac
    expect_rejection "frontier-$variant" "$diagnostic" check_replacement_frontier_fixture
done
write_replacement_frontier_make_fixture baseline
expect_rejection frontier-missing-make 'replacement frontier inventory requires make' \
    check_replacement_frontier_fixture missing-make
cmp -s "$frontier_fixture_dir/caller-before.rsp" "$frontier_fixture_dir/build/compiler_sources.rsp" ||
    fail 'frontier dry-run changed a caller-owned response file'
[[ ! -e "$frontier_fixture_dir/recipes.executed" ]] ||
    fail 'frontier graph inventory executed a fixture recipe'

# Bash 3.2 treats an empty array's unguarded expansion as unset under nounset.
[[ "$definitions" == *'${checked_dirs[@]+"${checked_dirs[@]}"}'* ]] ||
    fail 'regex scope checker must guard its empty array for Bash 3.2'

LINE_CAP_REQUESTS=()
FUNCTION_BODY_CACHE_REL=""
FUNCTION_BODY_CACHE_SIGNATURE=""
FUNCTION_BODY_CACHE_CONTENT=""
FUNCTION_BODY_EXTRACTIONS=0
FUNCTION_BODY_REUSES=0

check_one_cap() {
    LINE_CAP_REQUESTS=()
    require_max_lines "$1" "$2"
    run_line_cap_checks
}

check_duplicate_caps() {
    LINE_CAP_REQUESTS=()
    require_max_lines two.txt 9
    require_max_lines two.txt 1
    run_line_cap_checks
}

printf '' >"$ROOT_DIR/empty.txt"
printf 'one' >"$ROOT_DIR/unterminated.txt"
printf 'one\n\n' >"$ROOT_DIR/two.txt"
printf 'one\r\ntwo\r\n' >"$ROOT_DIR/crlf.txt"
printf 'a\0b' >"$ROOT_DIR/nul.txt"
printf 'one\n' >"$ROOT_DIR/path with spaces.txt"
check_one_cap empty.txt 0
check_one_cap unterminated.txt 1
check_one_cap two.txt 2
check_one_cap crlf.txt 2
check_one_cap nul.txt 1
check_one_cap 'path with spaces.txt' 1
expect_rejection unterminated-over 'has 1 lines; cap is 0' check_one_cap unterminated.txt 0
expect_rejection blank-line-over 'has 2 lines; cap is 1' check_one_cap two.txt 1
expect_rejection crlf-over 'has 2 lines; cap is 1' check_one_cap crlf.txt 1
expect_rejection nul-over 'has 1 lines; cap is 0' check_one_cap nul.txt 0
expect_rejection strict-duplicate 'has 2 lines; cap is 1' check_duplicate_caps
expect_rejection missing-file 'missing line-count input' check_one_cap absent.txt 1
expect_rejection invalid-cap 'invalid line-count request' check_one_cap two.txt nope
expect_rejection noncanonical-cap 'invalid line-count request' check_one_cap two.txt 02
LINE_CAP_REQUESTS=()
expect_rejection empty-batch 'no line-count requests' run_line_cap_checks
LINE_CAP_REQUESTS=($'1\tabsent.txt')
expect_rejection missing-at-consumer 'unreadable line-count input' run_line_cap_checks
LINE_CAP_REQUESTS=($'1\ttwo.txt' $'1\tabsent.txt' $'0\tunterminated.txt' $'1\tpath with spaces.txt')
expect_rejection independent-cap-failures 'two.txt has 2 lines; cap is 1' run_line_cap_checks
for diagnostic in 'unreadable line-count input: absent.txt' \
        'unterminated.txt has 1 lines; cap is 0'; do
    grep -Fq -- "$diagnostic" "$ROOT_DIR/independent-cap-failures.out" ||
        fail "line-cap batch hid a later failure: $diagnostic"
done

# The focused identity gate must consume the shared cap, including a tighter
# value, and reject missing/duplicate/malformed authority instead of guessing.
identity_cap_definition="$(awk '
    /^require_scalar_identity_owner_caps\(\) \{/ { capture = 1 }
    capture { print }
    /^}/ { capture = 0 }
' "$REPO_DIR/tests/self_hosted/parity/expression_graph_identity_carriage_owner.sh")"
eval "$identity_cap_definition"
declare -F require_scalar_identity_owner_caps >/dev/null || fail 'missing shared identity cap consumer'
mkdir -p "$ROOT_DIR/src/self_hosted/compiler" "$ROOT_DIR/tests/self_hosted/parity"
shared_caps="$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv"
identity_cap_rows=()
for basename in expression_admission leaf_identity_fact call_expression_admission call_with_arguments_admission; do
    identity_rel="src/self_hosted/compiler/direct_mir_scalar_program_${basename}_owner.pgy"
    printf 'one\ntwo\nthree' >"$ROOT_DIR/$identity_rel"
    identity_cap_rows+=("$identity_rel|3")
done
printf '%s\r\n' "${identity_cap_rows[@]}" >"$shared_caps"
require_scalar_identity_owner_caps
printf '%s\n' "${identity_cap_rows[@]:1}" >"$shared_caps"
expect_rejection absent-shared-cap 'missing or invalid shared owner cap' require_scalar_identity_owner_caps
printf '%s\n' "${identity_cap_rows[@]}" "${identity_cap_rows[0]}" >"$shared_caps"
expect_rejection duplicate-shared-cap 'missing or invalid shared owner cap' require_scalar_identity_owner_caps
printf '%s\n' "${identity_cap_rows[0]%|*}|invalid" "${identity_cap_rows[@]:1}" >"$shared_caps"
expect_rejection malformed-shared-cap 'missing or invalid shared owner cap' require_scalar_identity_owner_caps
printf '%s\n' "${identity_cap_rows[0]%|*}|2" "${identity_cap_rows[@]:1}" >"$shared_caps"
expect_rejection tighter-shared-cap 'owner hard cap exceeded' require_scalar_identity_owner_caps

printf '%s\n' 'func Alpha() {' '    return ALPHA;' '}' \
    'func Beta() {' '    return BETA;' '}' 'func Inline() { return INLINE; }' \
    >"$ROOT_DIR/one.pgy"
printf '%s\n' 'func Alpha() {' '    return OTHER;' '}' >"$ROOT_DIR/other.pgy"
require_function_text one.pgy 'func Alpha(' ALPHA
reject_function_text one.pgy 'func Alpha(' BETA
[[ "$FUNCTION_BODY_EXTRACTIONS" -eq 1 && "$FUNCTION_BODY_REUSES" -eq 1 ]] ||
    fail 'identical adjacent function checks were not reused'
expect_rejection required-absent 'missing term: BETA' \
    require_function_text one.pgy 'func Alpha(' BETA
expect_rejection forbidden-present 'retired term: ALPHA' \
    reject_function_text one.pgy 'func Alpha(' ALPHA
require_function_text one.pgy 'func Beta(' BETA
reject_function_text one.pgy 'func Beta(' ALPHA
require_function_text other.pgy 'func Alpha(' OTHER
reject_function_text other.pgy 'func Alpha(' ALPHA
require_function_text one.pgy 'func Inline(' INLINE
reject_function_text one.pgy 'func Inline(' BETA
expect_rejection missing-function 'missing function: func Missing(' \
    require_function_text one.pgy 'func Missing(' ALPHA
expect_rejection missing-function-input 'missing function input' \
    require_function_text absent.pgy 'func Alpha(' ALPHA

SELF_HOST_DIR="$ROOT_DIR/placement"
mkdir -p "$SELF_HOST_DIR/hir" "$SELF_HOST_DIR/semantic" "$SELF_HOST_DIR/other/hir"
printf '%s\n' 'AstMatchCasePatternFactFromText(' 'AstMatchCasePatternFactFromReadyArtifact(' \
    >"$SELF_HOST_DIR/hir/ast_match_pattern_fact_owner.pgy"
printf '%s\n' 'AstMatchCasePatternFactFromReadyArtifact(' \
    >"$SELF_HOST_DIR/semantic/ast_statement_fact_owner.pgy"
check_match_pattern_consumer_placement
printf '%s\n' 'AstMatchCasePatternFactFromText(' \
    >"$SELF_HOST_DIR/other/hir/ast_match_pattern_fact_owner.pgy"
expect_rejection same-basename-owner 'text parse escaped its HIR owner' \
    check_match_pattern_consumer_placement
printf '%s\n' 'AstMatchCasePatternFactFromReadyArtifact(' \
    >"$SELF_HOST_DIR/other/hir/ast_match_pattern_fact_owner.pgy"
expect_rejection foreign-artifact-consumer 'artifact read escaped statement admission' \
    check_match_pattern_consumer_placement
printf '%s\n' '// no match-pattern read' \
    >"$SELF_HOST_DIR/other/hir/ast_match_pattern_fact_owner.pgy"
printf '%s\n' 'AstMatchCasePatternFactFromText(' \
    >"$SELF_HOST_DIR/semantic/ast_statement_fact_owner.pgy"
expect_rejection statement-text-parse 'text parse escaped its HIR owner' \
    check_match_pattern_consumer_placement
SELF_HOST_DIR="$ROOT_DIR/absent-placement"
expect_rejection missing-placement-root 'placement scan failed' check_match_pattern_consumer_placement

# A filtered recursive scan may report no matches before opening a missing
# operand. Root validity belongs to the checker, not to grep's filter order.
check_placement_with_scan_status() (
    local placement_scan_status="$1"
    grep() { return "$placement_scan_status"; }
    check_match_pattern_consumer_placement
)
expect_rejection filtered-missing-placement-root 'placement scan failed' \
    check_placement_with_scan_status 1
SELF_HOST_DIR="$ROOT_DIR/empty.txt"
expect_rejection nondirectory-placement-root 'placement scan failed' \
    check_match_pattern_consumer_placement
SELF_HOST_DIR="$ROOT_DIR/empty-placement"
mkdir -p "$SELF_HOST_DIR"
check_match_pattern_consumer_placement
expect_rejection placement-scan-error 'placement scan failed' \
    check_placement_with_scan_status 2

mkdir -p "$ROOT_DIR/regex-a" "$ROOT_DIR/regex-b" "$ROOT_DIR/regex with spaces"
printf '%s\n' 'allowed call' >"$ROOT_DIR/regex-a/main.pgy"
printf '%s\n' 'retired_token' >"$ROOT_DIR/regex-a/ignored.txt"
printf '%s\n' 'allowed call' >"$ROOT_DIR/regex-b/main.pgy"
printf '%s\n' 'allowed call' >"$ROOT_DIR/regex with spaces/main.pgy"
REGEX_SCOPE_DIRS=()
REGEX_SCOPE_PATTERNS=()
reject_regex_under regex-a 'retired_token|retired_other'
reject_regex_under regex-a '(open|close)\('
reject_regex_under regex-b 'different_forbidden'
reject_regex_under 'regex with spaces' 'different_forbidden'
run_regex_scope_checks
printf '%s\n' 'close(' >"$ROOT_DIR/regex-a/main.pgy"
expect_rejection second-regex-retained 'must not match retired regex requests' run_regex_scope_checks
printf '%s\n' 'allowed call' >"$ROOT_DIR/regex-a/main.pgy"
printf '%s\n' 'different_forbidden' >"$ROOT_DIR/regex-b/main.pgy"
expect_rejection second-root-retained 'regex-b must not match' run_regex_scope_checks
REGEX_SCOPE_DIRS=()
REGEX_SCOPE_PATTERNS=()
reject_regex_under regex-a '['
expect_rejection invalid-regex 'directory regex scan failed' run_regex_scope_checks
expect_rejection missing-regex-root 'missing regex input directory' reject_regex_under absent-dir token
REGEX_SCOPE_DIRS=()
REGEX_SCOPE_PATTERNS=()
expect_rejection missing-regex-requests 'missing or inconsistent directory regex requests' run_regex_scope_checks
REGEX_SCOPE_DIRS=(regex-a)
expect_rejection missing-regex-patterns 'missing or inconsistent directory regex requests' run_regex_scope_checks
REGEX_SCOPE_DIRS=()
REGEX_SCOPE_PATTERNS=(token)
expect_rejection missing-regex-directories 'missing or inconsistent directory regex requests' run_regex_scope_checks
REGEX_SCOPE_DIRS=(regex-a regex-b)
REGEX_SCOPE_PATTERNS=(token)
expect_rejection inconsistent-regex-requests 'missing or inconsistent directory regex requests' run_regex_scope_checks

# A scoped byte precheck must not legalize other shell parity decisions.
transport_rel=tests/self_hosted/parity/llvm_leg_helpers.sh
transport_file="$ROOT_DIR/$transport_rel"
transport_baseline="$ROOT_DIR/transport-baseline.txt"
printf '%s\n' \
    'pgy_selfhost_compare_expected_text_artifact_file_with_owner() {' \
    '    cmp -s -- "$expected_file" "$actual_file" || byte_compare_status=$?' \
    '    "$comparator_bin" "$expected_rel" "$actual_rel" 0 2 "$artifact_kind"' \
    '}' >"$transport_baseline"
check_transport_snapshot() {
    # Each mutation is a new snapshot, not an immutable cached source tree.
    TEXT_CACHE_REL=""
    TEXT_CACHE_CONTENT=""
    FUNCTION_BODY_CACHE_REL=""
    FUNCTION_BODY_CACHE_SIGNATURE=""
    FUNCTION_BODY_CACHE_CONTENT=""
    check_artifact_comparison_transport_placement
}
cp "$transport_baseline" "$transport_file"
check_transport_snapshot
printf '%s\n' 'foreign_comparison() {' '    cmp -s left right' '}' >>"$transport_file"
expect_rejection foreign-artifact-comparison 'escaped its file-transport owner' check_transport_snapshot
cat "$transport_baseline" "$transport_baseline" >"$transport_file"
expect_rejection duplicate-artifact-transport 'escaped its file-transport owner' check_transport_snapshot
sed 's/0 2/0 1/' "$transport_baseline" >"$transport_file"
expect_rejection missing-artifact-owner-call 'missing term:' check_transport_snapshot
sed '/    cmp -s/d' "$transport_baseline" >"$transport_file"
expect_rejection missing-artifact-byte-precheck 'missing term:' check_transport_snapshot
# Keep the same early-success mutant without GNU sed's one-line i extension.
awk 'NR == 2 { print "    return 0" } { print }' "$transport_baseline" >"$transport_file"
expect_rejection artifact-shell-early-success 'retired term: return 0' check_transport_snapshot
sed 's/^pgy_selfhost_compare_expected_text_artifact_file_with_owner/Other/' \
    "$transport_baseline" >"$transport_file"
expect_rejection missing-artifact-transport 'missing function:' check_transport_snapshot
(
    ROOT_DIR="$REPO_DIR"
    check_transport_snapshot
)

python3 "$REPO_DIR/tests/source_size_count_test.py" LexicalMetricTests
echo '[component-checker] line caps, lexical source sizes, missing inputs, selected function identity, isolated frontier graph and negative predicates: PASS'

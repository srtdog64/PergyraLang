#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[dir-domain-identity] missing $file contract: $text" >&2
        exit 1
    fi
}

reject_text() {
    local file="$1"
    local text="$2"
    if grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[dir-domain-identity] forbidden fallback remains in $file: $text" >&2
        exit 1
    fi
}

require_text "src/compiler/dir.h" "uint32_t    source_syntax_id;"
require_text "src/compiler/dir.h" "uint32_t    owner_source_syntax_id;"
require_text "src/compiler/dir.h" "source_program_syntax_id"
require_text "src/compiler/dir.h" "domain_graph_id"
require_text "src/compiler/dir_storage.c" "ast_node_stable_id(ast)"
require_text "src/compiler/dir_collect.c" "dir_collect_nodes_from_hir"
require_text "src/compiler/dir_collect.c" "dir_collect_edges_and_intents_from_hir"
require_text "src/compiler/dir.c" "dir_lower_with_hir_resource_flow_facts"
require_text "src/compiler/dir.c" "dir_domain_graph_anchor"
require_text "src/compiler/dir_validate.c" \
    "DIR domain graph is missing its anchored source identity"
require_text "src/compiler/dir_validate.c" \
    "DIR slot-contract node '%s' is missing source or owner syntax identity"
require_text "src/compiler/dir_validate_internal.h" \
    "bool dir_validate_domain_topology("
require_text "src/compiler/dir_validate_internal.h" \
    "bool dir_validate_domain_runtime_facts("
require_text "src/compiler/dir_validate_internal.h" \
    "bool dir_validate_intents("
require_text "src/compiler/dir_validate_domain_topology.c" \
    "DIR domain topology row[%llu] has incomplete stable identity"
require_text "src/compiler/dir_validate_domain_runtime.c" \
    "DIR domain participant-role fact[%llu] has incomplete exact identity"
require_text "src/compiler/dir_validate_intent.c" \
    "DIR typed intent[%llu] terminal coverage drifted"
require_text "src/compiler/dir_validate.c" \
    "dir_validate_domain_topology(dir, error_message)"
require_text "src/compiler/dir_validate.c" \
    "dir_validate_domain_runtime_facts(dir, error_message)"
require_text "src/compiler/dir_validate.c" \
    "dir_validate_intents(dir, error_message)"
reject_text "src/compiler/dir_validate.c" \
    "dir_domain_topology_slot_matches("
reject_text "src/compiler/dir_validate.c" \
    "dir_domain_runtime_find_node_by_source("
reject_text "src/compiler/dir_validate.c" \
    "dir_intent_branch_matches_ast("
require_text "src/compiler/rir_validation_dir.c" \
    "rir_find_domain_scope_for_source_id"
require_text "src/compiler/rir_validation_dir.c" \
    "missing owner source identity"
require_text "src/test_rir.c" \
    "RIR/DIR validation rejects a slot with missing owner identity"
# Enforcement gate label: DIR source/owner identity and RIR negative gate.
reject_text "src/compiler/rir_validation_dir.c" "rir_split_qualified_name"
reject_text "src/compiler/rir_validation_dir.c" "rir_scope_name_matches"
reject_text "src/compiler/rir_validation_dir.c" "rir_find_domain_scope_for_owner"

RIR_TEST="${RIR_TEST_BIN:-$ROOT_DIR/bin/test_rir}"
if [[ "$RIR_TEST" != *.exe && -x "${RIR_TEST}.exe" ]]; then
    RIR_TEST="${RIR_TEST}.exe"
fi
if [[ ! -x "$RIR_TEST" ]]; then
    echo "[dir-domain-identity] missing RIR test binary: $RIR_TEST" >&2
    exit 1
fi
pgy_require_runnable_binary_here "dir-domain-identity" "$RIR_TEST"

output="$($RIR_TEST 2>&1)"
printf '%s\n' "$output"
if ! grep -Eq '=== Results: [0-9]+ passed, 0 failed ===' <<<"$output"; then
    echo "[dir-domain-identity] RIR identity negative gate failed" >&2
    exit 1
fi

echo "[dir-domain-identity] DIR source/owner identity is required for RIR validation"

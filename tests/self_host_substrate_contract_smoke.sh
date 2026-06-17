#!/usr/bin/env bash
# Gates the three systems-substrate contracts that self-hosted compiler passes
# depend on: AST-like tree representation, scoped raw/FFI boundaries, and
# deterministic collection iteration.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-substrate-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains stale term: $term"
    fi
}

# 1. Arbitrary data tree representation.
# Existing fixtures prove user-owned record/tree shapes and nested generic
# containers through the parser and backend. The progress doc must not upgrade
# that into a false claim that a self-hosted compiler AST model already exists.
require_file "tests/cases/backend_compare/node_traversal_sum/main.pgy"
require_file "tests/cases/backend_compare/tree_walk_recursive/main.pgy"
require_file "tests/cases/backend_compare/tree_grow_recursive/main.pgy"
require_file "tests/cases/backend_compare/nested_generic_containers/main.pgy"
require_file "src/self_hosted/parser/fixture/deep_generic_type.pgy"
require_text "tests/cases/backend_compare/node_traversal_sum/main.pgy" "class Node"
require_text "tests/cases/backend_compare/tree_walk_recursive/main.pgy" "class Tree"
require_text "tests/cases/backend_compare/tree_grow_recursive/main.pgy" "func WithHeight"
require_text "tests/cases/backend_compare/nested_generic_containers/main.pgy" "List<HashMap<String, Int>>"
require_text "src/self_hosted/parser/fixture/deep_generic_type.pgy" "HashMap<String, List<HashMap<Int, Array<String>>>>"
require_text "src/self_hosted/PROGRESS.md" "not yet a self-hosted compiler AST model"
require_text "src/self_hosted/PROGRESS.md" "mixed tree shapes are parser/backend evidence"
forbid_text "src/self_hosted/PROGRESS.md" "not exercised yet by self-host code"

# 2. Low-level FFI and raw escape.
# Raw/system escape is intentionally rejected for normal code today. FFI may
# become a boundary later, but self-hosted passes must not depend on a hidden
# raw pointer bridge while the scoped capability contract is still closed.
require_file "tests/raw_escape_contract_smoke.sh"
require_file "tests/cases/backend_compare/unsafe_scoped/main.pgy"
require_text "Makefile" "raw-escape-contract-test-smoke"
require_text "tests/raw_escape_contract_smoke.sh" "PGY_SEM_RAW_ESCAPE_UNSTABLE"
require_text "tests/raw_escape_contract_smoke.sh" "system-tier raw escape is explicitly rejected"
require_text "tests/cases/backend_compare/unsafe_scoped/main.pgy" "unsafe(raw)"
require_text "tests/cases/backend_compare/unsafe_scoped/main.pgy" "unsafe(ffi, raw)"
require_text "docs/132_unsafe_capability_scope.md" "Unsafe is a scoped capability, not a mode bit."
require_text "docs/self_hosted/02_required_language_surface.md" "Stable FFI boundary"
require_text "docs/self_hosted/02_required_language_surface.md" "Stable scoped unsafe/raw escape policy"
require_text "src/self_hosted/PROGRESS.md" "FFI remains intentionally absent from the compiler-pass path"

# 3. Deterministic iteration.
# Compiler-facing maps/sets must produce stable snapshots. The Stage 4 fixture
# covers scalar compiler keys plus canonical symbol, record, and handle keys.
require_file "tests/stage4_determinism_smoke.sh"
require_file "tests/cases/stage4_determinism/collection_iteration/main.pgy"
require_text "Makefile" "stage4-determinism-test-smoke"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "HashMap<String, Int>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "HashMap<Int, Int>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "HashMap<Long, Int>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "HashMap<Bool, Int>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "Set<String>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "Set<Int>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "Set<Long>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "Set<Bool>"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "MapKeys"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "SetValues"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "CheckCanonicalSymbolKeysForward"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "CheckCanonicalRecordKeysForward"
require_text "tests/cases/stage4_determinism/collection_iteration/main.pgy" "CheckCanonicalHandleKeysForward"
require_text "docs/self_hosted/07_hard_self_host_scorecard.md" "canonical scalar IDs"
require_text "src/self_hosted/PROGRESS.md" "raw aggregate keys as a second collection truth"

echo "[self-host-substrate-contract] tree/raw/determinism substrate contracts ok"

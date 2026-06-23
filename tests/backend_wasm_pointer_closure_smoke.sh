#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[backend-wasm-pointer-closure] $*" >&2
    exit 1
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden overclaim: $term"
    fi
}

doc="docs/135_backend_wasm_pointer_closure.md"

require_text "$doc" "verified subset plus named remaining debt"
require_text "$doc" "The C-backend route to WebAssembly is verified end to end."
require_text "$doc" "LLVM-to-wasm route is a runtime-link debt."
require_text "$doc" "direct wasm backend is post-beta."
require_text "$doc" "runtime-abi-lifetime-test-smoke"
require_text "$doc" "static scratch pointers"
require_text "$doc" "cross-arena references use index or stable handle"
require_text "$doc" "not a universal pointer/lifetime proof"
require_text "$doc" "Windows strict beta wording is C backend only."
require_text "$doc" "LLVM_ENABLED=0"
require_text "$doc" "Windows LLVM remains toolchain/parity debt"

require_text "docs/62_llvm_backend_debt_ledger.md" "MIR-only completion debt"
require_text "docs/62_llvm_backend_debt_ledger.md" "expression type exactness debt"
require_text "docs/62_llvm_backend_debt_ledger.md" "local placement / escape debt"
require_text "docs/62_llvm_backend_debt_ledger.md" "representation debt"
require_text "docs/wasm_target_todo.md" "Status: the C-backend route to WebAssembly is verified end to end."
require_text "docs/wasm_target_todo.md" "The IR route currently fails at link"
require_text "docs/107_beta_stable_subset.md" "native WASM backend"
require_text "docs/128_pointer_risk_register.md" "Any pointer not fitting one of these classes is beta debt."
require_text "docs/128_pointer_risk_register.md" "runtime-abi-lifetime-test-smoke"
require_text "docs/94_arena_index_lifetime_plan.md" "cross-arena reference"
require_text "docs/94_arena_index_lifetime_plan.md" "index/handle"

forbid_text "$doc" "fully pointer-safe"
forbid_text "$doc" "all lifetime bugs"
forbid_text "$doc" "LLVM support is fully complete"
forbid_text "$doc" "direct wasm backend is beta"
forbid_text "$doc" "Windows LLVM is beta-stable"
forbid_text "$doc" "All platforms support the same C/LLVM backend matrix"

echo "[backend-wasm-pointer-closure] backend, wasm, and pointer wording guard ok"

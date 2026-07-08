#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[beta-readiness-checklist] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_max_lines() {
    local rel="$1"
    local max_lines="$2"
    local line_count

    line_count="$(wc -l < "$ROOT_DIR/$rel")"
    [[ "$line_count" -le "$max_lines" ]] ||
        fail "$rel has $line_count lines; expected <= $max_lines"
}

require_text() {
    local rel="$1"
    local term="$2"
    local candidate

    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        for candidate in \
            "docs/100_beta_readiness_checklist.md" \
            "docs/100a_beta_active_status.md" \
            "docs/100b_beta_p0_semantics_systems_air.md" \
            "docs/100c_beta_dag_mir_abi_runtime.md" \
            "docs/100d_beta_execution_log.md"; do
            if grep -Fq -- "$term" "$ROOT_DIR/$candidate"; then
                return 0
            fi
        done
        fail "$rel shards missing term: $term"
    fi

    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden term: $term"
    fi
}

require_terms() {
    local rel="$1"
    local term

    while IFS= read -r term; do
        [[ -z "$term" ]] && continue
        require_text "$rel" "$term"
    done
}

required_files=(
    "docs/100_beta_readiness_checklist.md"
    "docs/100a_beta_active_status.md"
    "docs/100b_beta_p0_semantics_systems_air.md"
    "docs/100c_beta_dag_mir_abi_runtime.md"
    "docs/100d_beta_execution_log.md"
    "docs/19_design_philosophy.md"
    "docs/107_beta_stable_subset.md"
    "docs/108_stdlib_beta_freeze.md"
    "docs/109_package_module_resolver_contract.md"
    "docs/110_string_unicode_policy.md"
    "docs/111_beta_test_suite_freeze.md"
    "docs/112_observability_trace_schema.md"
    "docs/113_memory_concurrency_model.md"
    "docs/114_async_model_positioning.md"
    "docs/166_production_bar_review_2026_07.md"
    "docs/self_hosted/05_compiler_core_gap_analysis.md"
    "README.md"
    "docs/74_slot_pinning_caching.md"
    "docs/106_ownership_model_comparison.md"
    "src/semantic/diag_codes.h"
    "docs/72_diagnostic_codes.md"
    "src/runtime/pgy_abi_spec.h"
    "src/runtime/pgy_abi_spec_asserts.h"
    ".github/workflows/ci.yml"
    "Makefile"
    "examples/wasm_hello/main.pgy"
    "examples/wasm_hello/README.md"
)

for rel in "${required_files[@]}"; do
    require_file "$rel"
done

require_max_lines "docs/100_beta_readiness_checklist.md" 120
for rel in \
    "docs/100a_beta_active_status.md" \
    "docs/100b_beta_p0_semantics_systems_air.md" \
    "docs/100c_beta_dag_mir_abi_runtime.md" \
    "docs/100d_beta_execution_log.md"; do
    require_max_lines "$rel" 2600
done

require_terms "docs/100_beta_readiness_checklist.md" <<'EOF'
## 0. Formal Semantics / Proof Obligations
## 0a. Systems Language Baseline Closure
## 0b. Function CFG / Body Dataflow Closure
## 0c. Core Language Semantic Closure
## 0d. Runtime Panic And Secure Authority Invariants
## 0e. User-Facing Beta Quality Gates
## 0f. AIR Abstraction Safety Closure
Intent closure:
Zone/world/authority/handoff closure:
Runtime panic/unwinding policy that must be frozen:
Secure slot / authority invariant obligations:
Diagnostic quality gate:
Cross-platform support matrix:
Stdlib beta freeze:
Tooling beta conformance:
Package/module resolver beta surface:
Test quality gate:
Performance gate:
Observability/tracing schema gate:
Docs freeze:
Memory/concurrency model gate:
String/unicode policy:
Source of truth: `docs/104_air_compiler_architecture.md`
Source of truth: `docs/19_design_philosophy.md`
Pergyra is a systems language with domain extensions
no mandatory GC, predictable memory, C FFI, ABI
stability, raw escape, optional runtime, and compile-time determinism
system-tier raw escape contract
PGY_SEM_RAW_ESCAPE_UNSTABLE
Freeze unsafe as scoped capability, not a mode bit.
docs/132_unsafe_capability_scope.md
unsafe(raw) { ... }
make raw-escape-contract-test-smoke
Self-host boundary
post-beta consumer of the language spine
not a beta source-of-truth owner
CFG/AIR/DAG/MIR/ABI language trust first
docs/self_hosted/05_compiler_core_gap_analysis.md
make self-host-preparation-test-smoke
`--runtime=none`
intent/zone/world changes must
not break C FFI ABI
deterministic codegen evidence
make codegen-determinism-test-smoke
make runtime-none-contract-test-smoke
make air-drift-test-smoke
docs/74_slot_pinning_caching.md
Pin/Lease is a typed lexical lease
the system-tier raw escape
Pin/Lease
Option C ownership lift
`WriteView<T>` exclusive
NoEscape(view, region)
NoSuspend(view, region)
WriteExclusive(slot, region)
DropOnce(owner, all_cfg_exits)
ReleaseAfterUnpin(slot, all_cfg_exits)
NoUnsupportedTokenTransport(token, boundary)
direct named `spawn`
channel send/receive/close
`Cancel(...)`
diagnostics-json-test-smoke
generic param ownership classifier
PinnedView<T>
Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable
LLVM builtin lowering parity
shared ownership stable subset requires C/LLVM lifecycle parity
explicitly rejected in semantic analysis
Linux CI now installs `coq`
SecureSlot token ABI is now build-mode stable
Pergyra does not expose memory as address ownership
modular resource boundary
replaceable backend handle
Canonical semantic split
static rejection covers unsafe transition across a
known boundary
runtime validation covers dynamic existence/state of a
resource handle
Pergyra does not statically predict every business object's
lifetime
old release-mode SecureSlot macro has been removed
Non-pin handle expiration is a layered contract
Zone-Bound Handle typing
`BORROW_TRACKED` / anchored-handle conservative rejection
docs/108_stdlib_beta_freeze.md
docs/109_package_module_resolver_contract.md
docs/110_string_unicode_policy.md
docs/111_beta_test_suite_freeze.md
docs/112_observability_trace_schema.md
docs/113_memory_concurrency_model.md
docs/114_async_model_positioning.md
make async-model-positioning-test-smoke
decomposes coloring
`await` is a completion join only
`Future<T>` / `RemoteFuture<T>` are
make stdlib-test-smoke
make package-module-resolver-test-smoke
make unicode-policy-test-smoke
make beta-test-suite-freeze-test-smoke
make observability-schema-test-smoke
make async-model-positioning-test-smoke
make memory-concurrency-model-test-smoke
make perf-contract-test-smoke
make tooling-conformance-test-smoke
dogfood-first path
make dogfood-webgl-test-smoke
Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge
not freeze WebGL APIs
pgy.render.webgl
post-beta consumer of the language spine
language spine
LSP beta-stable: initialize capability response, keyword hover, and keyword completion
Debugger beta-stable: CLI `pgy debug <file>` parse + semantic gate and interactive quit path
DAP, binary breakpoints, variable watch, multi-file workspace indexing
beta readiness
strict beta readiness is now about 83%
Do not call this beta-complete until
EOF

require_text "TODO.md" "strict beta readiness is now about 83%"
require_text "TODO.md" "Historical note: this old 60% readiness anchor is superseded"
require_text "TODO.md" "Production-bar review routing"
require_text "docs/50_language_completion_board.md" "Strict beta readiness is about 83%"
require_text "docs/INDEX.md" "166_production_bar_review_2026_07.md"

require_terms "docs/166_production_bar_review_2026_07.md" <<'EOF'
Production Bar Review
Gate-less claim = FAIL
Partial executable coverage = PARTIAL
Accepted P0 Blockers
Compatibility evolution gate
compatibility_evolution_owner.pgy
Obsolete migration gate
MIR-owned ABI layout
Backend dumb-emitter gate
LLVM runtime bitcode integration
Precise `BoundaryCaptureFact` producer coverage
`ExecutionLane` negative regression coverage
AIR/backend access lint
Sandbox capability and frame-budget gate
Stdlib L2 doctrine pass
Non-Overclaim Rules
Do not claim native WASM, WIT, NPU, GPU, or dataflow backend readiness
171 real self-hosted sources
SELF-HOSTING OK
compatibility_evolution_manifest.pgy
self-host-compatibility-evolution-parity-test-smoke
sources=172
seed source/ABI/diagnostic breaking-change corpus
EOF

require_terms "docs/19_design_philosophy.md" <<'EOF'
Pergyra is a systems language with domain extensions
The systems-language baseline
no GC
predictable memory
C FFI
stability, raw escape
raw escape
optional runtime
compile-time determinism
intent / zone / world
Slot이 시스템-tier raw escape 없음
Runtime이 default로 들어감
ABI 안정성이 도메인 primitive에 의해 흔들림
Compile-time determinism
pgyc --runtime=none main.pgy
intent/zone/world의 어떤 변경도 C FFI ABI를 깨면 안 된다
EOF

require_terms "docs/107_beta_stable_subset.md" <<'EOF'
Beta Stable Subset Contract
beta-freeze-source-of-truth
syntax -> semantic -> runtime ->
Pergyra is a systems language with domain extensions
docs/19_design_philosophy.md
raw escape
optional runtime
compile-time determinism
System-tier raw pointer escape is not beta-stable
SlotRawPointer(...)
PGY_SEM_RAW_ESCAPE_UNSTABLE
`--runtime=none` is beta-gated
PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED
make runtime-none-contract-test-smoke
make codegen-determinism-test-smoke
Core Stable Surface
Generic Contract Stable Subset
Ownership Stable Subset
Slot is the stable source-level modular resource boundary
Option C ownership lift
Non-pin handle expiration is not claimed as a single-mechanism proof
First-class Zone-Bound Handle typing
conservative `BORROW_TRACKED`
Collections Stable Subset
FileExists(String) -> Bool
FileOpen(String, String) -> Int
FileRead(Int) -> String
FileWrite(Int, String) -> Void
FileClose(Int) -> Void
DirWalk(String) -> Array<String>
Intent / Zone / World / AIR Stable Subset
Backend And Tooling Contract
Async/concurrency stable subset is decomposition-based
`await` owns completion join only
docs/114_async_model_positioning.md
HashMap<String, T>
HashMap<Int, T>
HashMap<Long, T>
HashMap<Bool, T>
Token<T>` transport
SecureSlot<T>` token ABI is beta-stable across build modes and backends
WriteView<T>` is exclusive
pin slot as view { ... }
PgyPinnedSlotView_
PgyPinnedSecureSlotView_
pgy_pin_read_
pgy_pin_write_
pgy_unpin_
AIR Phase 1
Tooling beta-stable subset is exactly the `make tooling-conformance-test-smoke`
make air-strict-backend-compare-test-smoke
make stdlib-test-smoke
make package-module-resolver-test-smoke
make unicode-policy-test-smoke
make beta-test-suite-freeze-test-smoke
make observability-schema-test-smoke
make memory-concurrency-model-test-smoke
make async-model-positioning-test-smoke
make tooling-conformance-test-smoke
macOS: C-only CI preflight
EOF

require_terms "docs/108_stdlib_beta_freeze.md" <<'EOF'
Stdlib Beta Freeze
beta-freeze-source-of-truth
Stable Builtin Stdlib Surface
Stable `use` Modules
Known But Experimental Modules
`datetime`
`money`
`timer`
`versioning`
`ledger`
`obligation`
`device_adapter`
`http`: transport adapter draft
`storage`: persistence adapter draft
`page`: UI/page adapter draft
`spray`: GPU/Spray design placeholder
EOF

require_terms "docs/109_package_module_resolver_contract.md" <<'EOF'
Package And Module Resolver Beta Contract
beta-freeze-source-of-truth
import "relative/path.pgy";
relative to the importing file
Only manifest scaffolding is beta-stable
`pgy install`
Dependency version solving
supply-chain integrity
remote imports
JSON diagnostics for module-load failures
make package-module-resolver-test-smoke
EOF

require_terms "docs/110_string_unicode_policy.md" <<'EOF'
String And Unicode Beta Policy
beta-freeze-source-of-truth
UTF-8 string payloads
StringLength` is byte-length for beta
byte-exact and normalization-blind
Unicode identifiers are not beta-stable
Locale-sensitive comparison
Grapheme-cluster iteration
make unicode-policy-test-smoke
EOF

require_terms "docs/111_beta_test_suite_freeze.md" <<'EOF'
Beta Test Suite Freeze
beta-freeze-source-of-truth
Mandatory Pre-Beta Gates
Platform Gates
Explicitly Not Claimed Yet
Fuzz testing is out-of-beta
Property-based testing is out-of-beta
Coverage percentage is not yet a beta acceptance metric
make beta-test-suite-freeze-test-smoke
EOF

require_terms "examples/wasm_hello/README.md" <<'EOF'
Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge
does not require or claim a native LLVM wasm backend
not a stable WebGL language surface
pgy.render.webgl
make dogfood-webgl-test-smoke
EOF

require_terms "examples/wasm_hello/main.pgy" <<'EOF'
extern "C"
pgy_host_log
pgy_webgl_frame
EOF

require_terms "docs/112_observability_trace_schema.md" <<'EOF'
Observability And Trace Schema Beta Contract
beta-freeze-source-of-truth
IntentLast*
IntentHistory*
IntentActive*
IntentRecent*
runtime-borrowed strings
authority-token-mismatch
newest-first
General event streaming schema
Structured JSON trace export
make observability-schema-test-smoke
EOF

require_terms "docs/113_memory_concurrency_model.md" <<'EOF'
Memory And Concurrency Model Beta Contract
beta-freeze-source-of-truth
parallel` is the core execution primitive
Named `spawn Worker(args...)`
coloring decomposition
`await` is a completion join only
user-level effect system
A `parallel { ... }` block joins before control continues
Shared `ref`/`ref` reads
`ref`/`own` and `own`/`own` task-boundary conflicts are rejected
Non-blocking/timeout receive is copy-only for beta
ChannelClose(Channel<T>)` is copy-only for beta
Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are copy-only
Full weak-memory ordering vocabulary
make memory-concurrency-model-test-smoke
EOF

require_terms "docs/114_async_model_positioning.md" <<'EOF'
Async Model Positioning
coloring decomposition
Each concern has an owner
Widening one cell must not silently widen the others
For beta, await is a completion join for checked futures
Future<T> and RemoteFuture<T> are typed completion handles
not a general user-level effect system
visibility-high / decomposition-high
AIR Phase 1 sync/async drift detection
EOF

require_terms "docs/74_slot_pinning_caching.md" <<'EOF'
Slot Pinning / Lease
scope-entry capability lease
PgyPinnedView
manual raw pointer API
C / LLVM
pin slot as view: ReadView<T>|WriteView<T>
DeviceSlot<T>
PGY_SEM_PIN_ESCAPE
PGY_SEM_PIN_PARALLEL_CONFLICT
PGY_SEM_PIN_AWAIT_BOUNDARY
PGY_SEM_PIN_QUBIT_REJECT
PGY_SEM_PIN_TOKEN_INVALID
Evidence View Cache Policy
evidence-amortization path
cacheable only as an acceleration cache over MIR facts
mir_block_has_pin_guard_amortization_region
EOF

pin_diag_terms=$(cat <<'EOF'
PGY_SEM_PIN_ESCAPE
PGY_SEM_PIN_PARALLEL_CONFLICT
PGY_SEM_PIN_AWAIT_BOUNDARY
PGY_SEM_PIN_QUBIT_REJECT
PGY_SEM_PIN_TOKEN_INVALID
semantic:pin:escape
semantic:pin:parallel_conflict
semantic:pin:await_boundary
semantic:pin:qubit_reject
semantic:pin:token_invalid
keep-pin-view-local
serialize-pin-access
end-pin-before-await
do-not-pin-qubit
provide-valid-pin-token
EOF
)
while IFS= read -r term; do
    [[ -z "$term" ]] && continue
    require_text "src/semantic/diag_codes.h" "$term"
    require_text "docs/72_diagnostic_codes.md" "$term"
done <<< "$pin_diag_terms"

require_terms "docs/106_ownership_model_comparison.md" <<'EOF'
Project Verona
Mojo
Swift ARC
Vale
C# `fixed`
Generic + Ownership Interaction
Async Ownership
Option C Ownership Lift
generic param ownership classifier
Rust `Pin<&mut T>`
Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable
semantic/runtime/C/LLVM/lifecycle regressions
semantic rejects
EOF

require_terms "src/runtime/pgy_abi_spec.h" <<'EOF'
Current runtime ABI note:
SecureSlot<T> keeps the same token layout and hard-fail checks across
typedef struct { uint64_t id; bool can_write; bool can_read; } pgy_abi_token_int;
typedef struct { int64_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_long;
typedef struct { float   value; bool occupied; uint64_t token; } pgy_abi_secure_slot_float;
typedef struct { double  value; bool occupied; uint64_t token; } pgy_abi_secure_slot_double;
typedef struct { bool    value; bool occupied; uint64_t token; } pgy_abi_secure_slot_bool;
Debug/release mode is a build policy, not an ABI type-name dimension.
uint32 strong/weak counts plus an alive bit
beta-stable shared ownership subset
uint32_t strong_count;
uint32_t weak_count;
bool     alive;
Ordinary Channel<T> lowering in the current beta implementation still uses
default-zeroed inside aggregate fields
ZoneChannel/WorldChannel ABI target
EOF

require_terms "src/runtime/pgy_abi_spec_asserts.h" <<'EOF'
secure_slot_string_token_after_value
token_int_min_size_16
token_int_can_write_after_id
token_int_can_read_after_can_write
rc_ctrl_int_alive_at_8
EOF

require_terms ".github/workflows/ci.yml" <<'EOF'
sudo apt-get install -y gcc make llvm-dev llvm coq
make ci-linux
mingw-w64-ucrt-x86_64-python
build-macos-c-only
make ci-macos
EOF

require_terms "Makefile" <<'EOF'
WINDOWS_LLVM_READY :=
"$(LLVM_CONFIG)" --libs core
ci-macos:
check-macos-toolchain:
perf-contract-test-smoke:
evidence-guard-amortization-test-smoke:
tooling-conformance-test-smoke:
package-module-resolver-test-smoke:
unicode-policy-test-smoke:
beta-test-suite-freeze-test-smoke:
observability-schema-test-smoke:
memory-concurrency-model-test-smoke:
dogfood-webgl-test-smoke:
self-host-preparation-test-smoke:
EOF

require_terms "scripts/ci_windows_steps.sh" <<'EOF'
ci-windows: LLVM toolchain detected; running LLVM smoke and backend compare
ci-windows: LLVM toolchain not detected; skipping Windows LLVM smoke/backend compare
evidence-guard-amortization-test-smoke
EOF

require_terms "scripts/ci_linux_steps.sh" <<'EOF'
evidence-guard-amortization-test-smoke
EOF

require_terms "scripts/ci_macos_steps.sh" <<'EOF'
evidence-guard-amortization-test-smoke
EOF

forbid_text "Makefile" 'WINDOWS_LLVM_READY := $(shell if [ -n "$(LLVM_CONFIG)" ] ||'
forbid_text "Makefile" '/c/Program\ Files/LLVM/lib'
forbid_text "Makefile" 'C:/Program Files/LLVM/lib'

require_terms "README.md" <<'EOF'
Current CI support matrix:
Linux: C backend + LLVM backend regression coverage
Windows: C backend regression coverage always; LLVM smoke + backend compare run only when executable `llvm-config --libs core` evidence is present
macOS: C-only CI preflight through `make ci-macos`; macOS LLVM/backend parity remains out-of-beta
EOF

echo "beta readiness checklist smoke: ok"

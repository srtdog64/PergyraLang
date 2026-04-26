#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "missing python for beta readiness checklist smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
stable_subset_path = root / "docs" / "107_beta_stable_subset.md"
stdlib_freeze_path = root / "docs" / "108_stdlib_beta_freeze.md"
package_module_path = root / "docs" / "109_package_module_resolver_contract.md"
unicode_policy_path = root / "docs" / "110_string_unicode_policy.md"
test_suite_path = root / "docs" / "111_beta_test_suite_freeze.md"
observability_schema_path = root / "docs" / "112_observability_trace_schema.md"
memory_concurrency_path = root / "docs" / "113_memory_concurrency_model.md"
async_positioning_path = root / "docs" / "114_async_model_positioning.md"
readme_path = root / "README.md"
slot_pin_path = root / "docs" / "74_slot_pinning_caching.md"
ownership_path = root / "docs" / "106_ownership_model_comparison.md"
diag_header_path = root / "src" / "semantic" / "diag_codes.h"
diag_doc_path = root / "docs" / "72_diagnostic_codes.md"
abi_spec_path = root / "src" / "runtime" / "pgy_abi_spec.h"
ci_path = root / ".github" / "workflows" / "ci.yml"
makefile_path = root / "Makefile"
if not checklist_path.exists():
    raise SystemExit("missing docs/100_beta_readiness_checklist.md")
if not stable_subset_path.exists():
    raise SystemExit("missing docs/107_beta_stable_subset.md")
if not stdlib_freeze_path.exists():
    raise SystemExit("missing docs/108_stdlib_beta_freeze.md")
if not package_module_path.exists():
    raise SystemExit("missing docs/109_package_module_resolver_contract.md")
if not unicode_policy_path.exists():
    raise SystemExit("missing docs/110_string_unicode_policy.md")
if not test_suite_path.exists():
    raise SystemExit("missing docs/111_beta_test_suite_freeze.md")
if not observability_schema_path.exists():
    raise SystemExit("missing docs/112_observability_trace_schema.md")
if not memory_concurrency_path.exists():
    raise SystemExit("missing docs/113_memory_concurrency_model.md")
if not async_positioning_path.exists():
    raise SystemExit("missing docs/114_async_model_positioning.md")
if not readme_path.exists():
    raise SystemExit("missing README.md")
if not slot_pin_path.exists():
    raise SystemExit("missing docs/74_slot_pinning_caching.md")
if not ownership_path.exists():
    raise SystemExit("missing docs/106_ownership_model_comparison.md")
if not diag_header_path.exists():
    raise SystemExit("missing src/semantic/diag_codes.h")
if not diag_doc_path.exists():
    raise SystemExit("missing docs/72_diagnostic_codes.md")
if not abi_spec_path.exists():
    raise SystemExit("missing src/runtime/pgy_abi_spec.h")
if not ci_path.exists():
    raise SystemExit("missing .github/workflows/ci.yml")
if not makefile_path.exists():
    raise SystemExit("missing Makefile")

text = checklist_path.read_text(encoding="utf-8")
stable_subset = stable_subset_path.read_text(encoding="utf-8")
stdlib_freeze = stdlib_freeze_path.read_text(encoding="utf-8")
package_module = package_module_path.read_text(encoding="utf-8")
unicode_policy = unicode_policy_path.read_text(encoding="utf-8")
test_suite = test_suite_path.read_text(encoding="utf-8")
observability_schema = observability_schema_path.read_text(encoding="utf-8")
memory_concurrency = memory_concurrency_path.read_text(encoding="utf-8")
async_positioning = async_positioning_path.read_text(encoding="utf-8")
readme = readme_path.read_text(encoding="utf-8")
slot_pin = slot_pin_path.read_text(encoding="utf-8")
ownership = ownership_path.read_text(encoding="utf-8")
diag_header = diag_header_path.read_text(encoding="utf-8")
diag_doc = diag_doc_path.read_text(encoding="utf-8")
abi_spec = abi_spec_path.read_text(encoding="utf-8")
ci = ci_path.read_text(encoding="utf-8")
makefile = makefile_path.read_text(encoding="utf-8")

required_sections = [
    "## 0. Formal Semantics / Proof Obligations",
    "## 0b. Function CFG / Body Dataflow Closure",
    "## 0c. Core Language Semantic Closure",
    "## 0d. Runtime Panic And Secure Authority Invariants",
    "## 0e. User-Facing Beta Quality Gates",
    "## 0f. AIR Abstraction Safety Closure",
]

required_terms = [
    "Intent closure:",
    "Zone/world/authority/handoff closure:",
    "Runtime panic/unwinding policy that must be frozen:",
    "Secure slot / authority invariant obligations:",
    "Diagnostic quality gate:",
    "Cross-platform support matrix:",
    "Stdlib beta freeze:",
    "Tooling beta conformance:",
    "Package/module resolver beta surface:",
    "Test quality gate:",
    "Performance gate:",
    "Observability/tracing schema gate:",
    "Docs freeze:",
    "Memory/concurrency model gate:",
    "String/unicode policy:",
    "Source of truth: `docs/104_air_compiler_architecture.md`",
    "make air-drift-test-smoke",
    "docs/74_slot_pinning_caching.md",
    "Pin/Lease",
    "Option C ownership lift",
    "`WriteView<T>` exclusive",
    "generic param ownership classifier",
    "PinnedView<T>",
    "Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable",
    "LLVM builtin lowering parity",
    "shared ownership stable subset requires C/LLVM lifecycle parity",
    "explicitly rejected in semantic analysis",
    "Linux CI now installs `coq`",
    "SecureSlot token ABI is now build-mode stable",
    "old release-mode SecureSlot macro has been removed",
    "docs/108_stdlib_beta_freeze.md",
    "docs/109_package_module_resolver_contract.md",
    "docs/110_string_unicode_policy.md",
    "docs/111_beta_test_suite_freeze.md",
    "docs/112_observability_trace_schema.md",
    "docs/113_memory_concurrency_model.md",
    "docs/114_async_model_positioning.md",
    "make async-model-positioning-test-smoke",
    "decomposes coloring",
    "`await` is a completion join only",
    "`Future<T>` / `RemoteFuture<T>` are",
    "make stdlib-test-smoke",
    "make package-module-resolver-test-smoke",
    "make unicode-policy-test-smoke",
    "make beta-test-suite-freeze-test-smoke",
    "make observability-schema-test-smoke",
    "make async-model-positioning-test-smoke",
    "make memory-concurrency-model-test-smoke",
    "make perf-contract-test-smoke",
    "make tooling-conformance-test-smoke",
    "LSP beta-stable: initialize capability response, keyword hover, and keyword completion",
    "Debugger beta-stable: CLI `pgy debug <file>` parse + semantic gate and interactive quit path",
    "DAP, binary breakpoints, variable watch, multi-file workspace indexing",
]

missing_sections = [section for section in required_sections if section not in text]
if missing_sections:
    raise SystemExit("beta checklist missing section(s): " + ", ".join(missing_sections))

missing_terms = [term for term in required_terms if term not in text]
if missing_terms:
    raise SystemExit("beta checklist missing gate term(s): " + ", ".join(missing_terms))

stable_subset_terms = [
    "Beta Stable Subset Contract",
    "beta-freeze-source-of-truth",
    "syntax -> semantic -> runtime ->",
    "Core Stable Surface",
    "Generic Contract Stable Subset",
    "Ownership Stable Subset",
    "Option C ownership lift",
    "Collections Stable Subset",
    "Intent / Zone / World / AIR Stable Subset",
    "Backend And Tooling Contract",
    "Async/concurrency stable subset is decomposition-based",
    "`await` owns completion join only",
    "docs/114_async_model_positioning.md",
    "HashMap<String, T>",
    "HashMap<Int, T>",
    "HashMap<Long, T>",
    "HashMap<Bool, T>",
    "Token<T>` transport",
    "SecureSlot<T>` token ABI is beta-stable across build modes and backends",
    "WriteView<T>` is exclusive",
    "pin slot as view { ... }",
    "AIR Phase 1",
    "Tooling beta-stable subset is exactly the `make tooling-conformance-test-smoke`",
    "make air-strict-backend-compare-test-smoke",
    "make stdlib-test-smoke",
    "make package-module-resolver-test-smoke",
    "make unicode-policy-test-smoke",
    "make beta-test-suite-freeze-test-smoke",
    "make observability-schema-test-smoke",
    "make memory-concurrency-model-test-smoke",
    "make async-model-positioning-test-smoke",
    "make tooling-conformance-test-smoke",
    "macOS: C-only CI preflight",
]
missing_stable_subset_terms = [
    term for term in stable_subset_terms if term not in stable_subset
]
if missing_stable_subset_terms:
    raise SystemExit(
        "stable subset doc missing term(s): "
        + ", ".join(missing_stable_subset_terms)
    )

stdlib_freeze_terms = [
    "Stdlib Beta Freeze",
    "beta-freeze-source-of-truth",
    "Stable Builtin Stdlib Surface",
    "Stable `use` Modules",
    "Known But Experimental Modules",
    "`datetime`",
    "`money`",
    "`timer`",
    "`versioning`",
    "`ledger`",
    "`obligation`",
    "`device_adapter`",
    "`http`: transport adapter draft",
    "`storage`: persistence adapter draft",
    "`page`: UI/page adapter draft",
    "`spray`: GPU/Spray design placeholder",
]
missing_stdlib_terms = [
    term for term in stdlib_freeze_terms if term not in stdlib_freeze
]
if missing_stdlib_terms:
    raise SystemExit(
        "stdlib freeze doc missing term(s): "
        + ", ".join(missing_stdlib_terms)
    )

package_module_terms = [
    "Package And Module Resolver Beta Contract",
    "beta-freeze-source-of-truth",
    "import \"relative/path.pgy\";",
    "relative to the importing file",
    "Only manifest scaffolding is beta-stable",
    "`pgy install`",
    "Dependency version solving",
    "supply-chain integrity",
    "remote imports",
    "JSON diagnostics for module-load failures",
    "make package-module-resolver-test-smoke",
]
missing_package_module_terms = [
    term for term in package_module_terms if term not in package_module
]
if missing_package_module_terms:
    raise SystemExit(
        "package/module resolver doc missing term(s): "
        + ", ".join(missing_package_module_terms)
    )

unicode_policy_terms = [
    "String And Unicode Beta Policy",
    "beta-freeze-source-of-truth",
    "UTF-8 string payloads",
    "StringLength` is byte-length for beta",
    "byte-exact and normalization-blind",
    "Unicode identifiers are not beta-stable",
    "Locale-sensitive comparison",
    "Grapheme-cluster iteration",
    "make unicode-policy-test-smoke",
]
missing_unicode_policy_terms = [
    term for term in unicode_policy_terms if term not in unicode_policy
]
if missing_unicode_policy_terms:
    raise SystemExit(
        "unicode policy doc missing term(s): "
        + ", ".join(missing_unicode_policy_terms)
    )

test_suite_terms = [
    "Beta Test Suite Freeze",
    "beta-freeze-source-of-truth",
    "Mandatory Pre-Beta Gates",
    "Platform Gates",
    "Explicitly Not Claimed Yet",
    "Fuzz testing is out-of-beta",
    "Property-based testing is out-of-beta",
    "Coverage percentage is not yet a beta acceptance metric",
    "make beta-test-suite-freeze-test-smoke",
]
missing_test_suite_terms = [
    term for term in test_suite_terms if term not in test_suite
]
if missing_test_suite_terms:
    raise SystemExit(
        "beta test suite freeze doc missing term(s): "
        + ", ".join(missing_test_suite_terms)
    )

observability_schema_terms = [
    "Observability And Trace Schema Beta Contract",
    "beta-freeze-source-of-truth",
    "IntentLast*",
    "IntentHistory*",
    "IntentActive*",
    "IntentRecent*",
    "runtime-borrowed strings",
    "authority-token-mismatch",
    "newest-first",
    "General event streaming schema",
    "Structured JSON trace export",
    "make observability-schema-test-smoke",
]
missing_observability_schema_terms = [
    term for term in observability_schema_terms if term not in observability_schema
]
if missing_observability_schema_terms:
    raise SystemExit(
        "observability schema doc missing term(s): "
        + ", ".join(missing_observability_schema_terms)
    )

memory_concurrency_terms = [
    "Memory And Concurrency Model Beta Contract",
    "beta-freeze-source-of-truth",
    "parallel` is the core execution primitive",
    "Named `spawn Worker(args...)`",
    "coloring decomposition",
    "`await` is a completion join only",
    "user-level effect system",
    "A `parallel { ... }` block joins before control continues",
    "Shared `ref`/`ref` reads",
    "`ref`/`own` and `own`/`own` task-boundary conflicts are rejected",
    "Non-blocking/timeout receive is copy-only for beta",
    "ChannelClose(Channel<T>)` is copy-only for beta",
    "Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are copy-only",
    "Full weak-memory ordering vocabulary",
    "make memory-concurrency-model-test-smoke",
]
missing_memory_concurrency_terms = [
    term for term in memory_concurrency_terms if term not in memory_concurrency
]
if missing_memory_concurrency_terms:
    raise SystemExit(
        "memory/concurrency model doc missing term(s): "
        + ", ".join(missing_memory_concurrency_terms)
    )

async_positioning_terms = [
    "Async Model Positioning",
    "coloring decomposition",
    "Each concern has an owner",
    "Widening one cell must not silently widen the others",
    "For beta, await is a completion join for checked futures",
    "Future<T> and RemoteFuture<T> are typed completion handles",
    "not a general user-level effect system",
    "visibility-high / decomposition-high",
    "AIR Phase 1 sync/async drift detection",
]
missing_async_positioning_terms = [
    term for term in async_positioning_terms if term not in async_positioning
]
if missing_async_positioning_terms:
    raise SystemExit(
        "async positioning doc missing term(s): "
        + ", ".join(missing_async_positioning_terms)
    )

if "현재 공식 beta readiness는 약 50%" not in text:
    raise SystemExit("beta readiness checklist must keep strict 50% readiness wording")

slot_pin_terms = [
    "Slot Pinning / Lease",
    "scope-entry capability lease",
    "PgyPinnedView",
    "manual raw pointer API",
    "C / LLVM",
    "Pin/Lease syntax is not beta-stable yet",
    "DeviceSlot<T>",
    "PGY_SEM_PIN_ESCAPE",
    "PGY_SEM_PIN_PARALLEL_CONFLICT",
    "PGY_SEM_PIN_AWAIT_BOUNDARY",
    "PGY_SEM_PIN_QUBIT_REJECT",
    "PGY_SEM_PIN_TOKEN_INVALID",
]
missing_slot_pin_terms = [term for term in slot_pin_terms if term not in slot_pin]
if missing_slot_pin_terms:
    raise SystemExit(
        "slot pinning doc missing term(s): " + ", ".join(missing_slot_pin_terms)
    )

pin_diag_terms = [
    "PGY_SEM_PIN_ESCAPE",
    "PGY_SEM_PIN_PARALLEL_CONFLICT",
    "PGY_SEM_PIN_AWAIT_BOUNDARY",
    "PGY_SEM_PIN_QUBIT_REJECT",
    "PGY_SEM_PIN_TOKEN_INVALID",
    "semantic:pin:escape",
    "semantic:pin:parallel_conflict",
    "semantic:pin:await_boundary",
    "semantic:pin:qubit_reject",
    "semantic:pin:token_invalid",
    "keep-pin-view-local",
    "serialize-pin-access",
    "end-pin-before-await",
    "do-not-pin-qubit",
    "provide-valid-pin-token",
]
for label, content in (
    ("diag_codes.h", diag_header),
    ("docs/72_diagnostic_codes.md", diag_doc),
):
    missing_pin_terms = [term for term in pin_diag_terms if term not in content]
    if missing_pin_terms:
        raise SystemExit(
            f"{label} missing Pin/Lease diagnostic term(s): "
            + ", ".join(missing_pin_terms)
        )

ownership_terms = [
    "Project Verona",
    "Mojo",
    "Swift ARC",
    "Vale",
    "C# `fixed`",
    "Generic + Ownership Interaction",
    "Async Ownership",
    "Option C Ownership Lift",
    "generic param ownership classifier",
    "Rust `Pin<&mut T>`",
    "Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable",
    "semantic/runtime/C/LLVM/lifecycle regressions",
    "semantic rejects",
]
missing_ownership_terms = [term for term in ownership_terms if term not in ownership]
if missing_ownership_terms:
    raise SystemExit(
        "ownership comparison doc missing term(s): "
        + ", ".join(missing_ownership_terms)
    )

abi_terms = [
    "Current runtime ABI note:",
    "SecureSlot<T> keeps the same token layout and hard-fail checks across",
    "typedef struct { uint64_t id; bool can_write; bool can_read; } pgy_abi_token_int_rel;",
    "typedef struct { int64_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_long_rel;",
    "typedef struct { float   value; bool occupied; uint64_t token; } pgy_abi_secure_slot_float_rel;",
    "typedef struct { double  value; bool occupied; uint64_t token; } pgy_abi_secure_slot_double_rel;",
    "typedef struct { bool    value; bool occupied; uint64_t token; } pgy_abi_secure_slot_bool_rel;",
    "secure_slot_string_rel_same_token_offset_as_dbg",
    "token_int_rel_same_size_as_dbg",
    "token_int_rel_can_write_same_offset_as_dbg",
    "token_int_rel_can_read_same_offset_as_dbg",
    "uint32 strong/weak counts plus an alive bit",
    "beta-stable shared ownership subset",
    "uint32_t strong_count;",
    "uint32_t weak_count;",
    "bool     alive;",
    "rc_ctrl_int_alive_at_8",
]
missing_abi_terms = [term for term in abi_terms if term not in abi_spec]
if missing_abi_terms:
    raise SystemExit(
        "ABI spec missing beta ownership/shape term(s): "
        + ", ".join(missing_abi_terms)
    )

ci_terms = [
    "sudo apt-get install -y gcc make llvm-dev llvm coq",
    "make ci-linux",
    "build-macos-c-only",
    "make ci-macos",
]
missing_ci_terms = [term for term in ci_terms if term not in ci]
if missing_ci_terms:
    raise SystemExit(
        "CI workflow missing formal-semantics proof gate term(s): "
        + ", ".join(missing_ci_terms)
    )

makefile_terms = [
    "WINDOWS_LLVM_READY :=",
    '"$(LLVM_CONFIG)" --libs core',
    "ci-macos:",
    "check-macos-toolchain:",
    "perf-contract-test-smoke:",
    "tooling-conformance-test-smoke:",
    "package-module-resolver-test-smoke:",
    "unicode-policy-test-smoke:",
    "beta-test-suite-freeze-test-smoke:",
    "observability-schema-test-smoke:",
    "memory-concurrency-model-test-smoke:",
    "ci-windows: LLVM toolchain detected; running LLVM smoke and backend compare",
    "ci-windows: LLVM toolchain not detected; skipping Windows LLVM smoke/backend compare",
]
missing_makefile_terms = [term for term in makefile_terms if term not in makefile]
if missing_makefile_terms:
    raise SystemExit(
        "Makefile missing Windows LLVM support-matrix guard term(s): "
        + ", ".join(missing_makefile_terms)
    )
if "WINDOWS_LLVM_READY := $(shell if [ -n \"$(LLVM_CONFIG)\" ] ||" in makefile:
    raise SystemExit("WINDOWS_LLVM_READY must not treat LLVM library folders as runnable LLVM support")
if "/c/Program\\ Files/LLVM/lib" in makefile or "C:/Program Files/LLVM/lib" in makefile:
    raise SystemExit("Windows LLVM support detection must not rely on Program Files library folders")

readme_support_terms = [
    "Current CI support matrix:",
    "Linux: C backend + LLVM backend regression coverage",
    "Windows: C backend regression coverage always; LLVM smoke + backend compare run only when executable `llvm-config --libs core` evidence is present",
    "macOS: C-only CI preflight through `make ci-macos`; macOS LLVM/backend parity remains out-of-beta",
]
missing_readme_terms = [term for term in readme_support_terms if term not in readme]
if missing_readme_terms:
    raise SystemExit(
        "README support matrix missing term(s): "
        + ", ".join(missing_readme_terms)
    )

print("beta readiness checklist smoke: ok")
PY

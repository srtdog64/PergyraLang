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
slot_pin_path = root / "docs" / "74_slot_pinning_caching.md"
ownership_path = root / "docs" / "106_ownership_model_comparison.md"
diag_header_path = root / "src" / "semantic" / "diag_codes.h"
diag_doc_path = root / "docs" / "72_diagnostic_codes.md"
abi_spec_path = root / "src" / "runtime" / "pgy_abi_spec.h"
if not checklist_path.exists():
    raise SystemExit("missing docs/100_beta_readiness_checklist.md")
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

text = checklist_path.read_text(encoding="utf-8")
slot_pin = slot_pin_path.read_text(encoding="utf-8")
ownership = ownership_path.read_text(encoding="utf-8")
diag_header = diag_header_path.read_text(encoding="utf-8")
diag_doc = diag_doc_path.read_text(encoding="utf-8")
abi_spec = abi_spec_path.read_text(encoding="utf-8")

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
]

missing_sections = [section for section in required_sections if section not in text]
if missing_sections:
    raise SystemExit("beta checklist missing section(s): " + ", ".join(missing_sections))

missing_terms = [term for term in required_terms if term not in text]
if missing_terms:
    raise SystemExit("beta checklist missing gate term(s): " + ", ".join(missing_terms))

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
        "ABI spec missing Rc/Weak beta/shape term(s): "
        + ", ".join(missing_abi_terms)
    )

print("beta readiness checklist smoke: ok")
PY

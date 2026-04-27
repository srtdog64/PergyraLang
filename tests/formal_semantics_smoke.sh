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
        echo "missing python for formal semantics smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
index_path = root / "docs" / "102_formal_semantics_and_proof_obligations.md"
proof_dir = root / "docs" / "semantics"
rigor_audit_path = root / "docs" / "118_slot_model_rigor_audit.md"
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
semantic_design_path = root / "docs" / "14_semantic_analyzer_design.md"
slot_manager_path = root / "src" / "runtime" / "slot_manager.h"
slot_macros_path = root / "src" / "runtime" / "pgy_runtime_slot_macros.h"
todo_path = root / "TODO.md"
ci_path = root / ".github" / "workflows" / "ci.yml"

if not index_path.exists():
    raise SystemExit("missing docs/102_formal_semantics_and_proof_obligations.md")
if not proof_dir.is_dir():
    raise SystemExit("missing docs/semantics proof folder")
if not rigor_audit_path.exists():
    raise SystemExit("missing docs/118_slot_model_rigor_audit.md")
if not semantic_design_path.exists():
    raise SystemExit("missing docs/14_semantic_analyzer_design.md")
if not slot_manager_path.exists():
    raise SystemExit("missing src/runtime/slot_manager.h")
if not slot_macros_path.exists():
    raise SystemExit("missing src/runtime/pgy_runtime_slot_macros.h")
if not ci_path.exists():
    raise SystemExit("missing .github/workflows/ci.yml")

index_doc = index_path.read_text(encoding="utf-8")
checklist = checklist_path.read_text(encoding="utf-8")
rigor_audit = rigor_audit_path.read_text(encoding="utf-8")
semantic_design = semantic_design_path.read_text(encoding="utf-8")
slot_manager = slot_manager_path.read_text(encoding="utf-8")
slot_macros = slot_macros_path.read_text(encoding="utf-8")
todo = todo_path.read_text(encoding="utf-8")
ci = ci_path.read_text(encoding="utf-8")

required_files = {
    "README.md": [
        "Status: `beta-proof-obligation`",
        "Every stable beta feature must be represented in this folder",
        "Stable proof scope:",
        "Out of beta proof scope:",
        "Regression tests, smoke tests, and backend compare runs are proof evidence, not proof itself.",
        "Borrow-checker-equivalent safety: only through the combined ownership",
        "Slot alone is not advertised as a borrow checker.",
        "08_slot_capability_calculus.md",
        "proofs/SlotCalculus.v",
        "not beta-closure evidence unless a CI",
    ],
    "00_proof_contract.md": [
        "## Semantic Domains",
        "## Core Judgments",
        "### Type Preservation",
        "### Progress",
        "### Failure Separation",
        "### Backend Observational Equivalence",
        "`PanicState`",
    ],
    "01_intent_world_zone.md": [
        "Keywords: `intent`, `world`, `zone`, `subject`, `authority`, `handoff`.",
        "## Theorem: Authority Soundness",
        "## Theorem: Intent Step Progress",
        "## Theorem: World/Zone Frontier Termination",
    ],
    "02_relation_effect_projection.md": [
        "Keywords: `relation`, `effect`, `projection`, `refresh`, `publish`, `bind`.",
        "## Theorem: Projection Freshness",
        "## Theorem: Effect Conflict Soundness",
        "## Theorem: Projection Diagnostic Completeness",
    ],
    "03_generics_modules_dag.md": [
        "Keywords and surfaces: `where`, `ability`, generic parameters, default type arguments, module imports/exports, type-resolution DAG.",
        "## Theorem: Generic Contract Soundness",
        "## Theorem: DAG Soundness",
        "## Theorem: Module Visibility Non-Interference",
    ],
    "04_ownership_abi.md": [
        "Keywords and surfaces: `own`, `ref`, anchored slot handles, slot boundaries, runtime ABI ownership.",
        "## Theorem Boundary: Slot Runtime Safety Is Not Borrow Safety",
        "`Slot runtime safety`",
        "`Borrow-checker-equivalent safety`",
        "## Theorem: Anchored Ownership Safety",
        "## Theorem: Secure Token Unforgeability",
        "## Theorem: Authority Transfer Single-Owner",
        "## Theorem: Arena Lifetime Non-Escape",
        "## Theorem: ABI Ownership Parity",
    ],
    "05_parallel_execution.md": [
        "Keywords: `parallel`",
        "## Theorem: Parallel Conflict Soundness",
        "## Theorem: Execution Backend Parity",
    ],
    "06_backend_parity.md": [
        "Surfaces: MIR, declaration inventory, C backend, LLVM backend, runtime ABI.",
        "## Theorem: MIR Source-of-Truth",
        "## Theorem: Backend Observational Equivalence",
        "## Theorem: Runtime Panic Parity",
        "## Theorem: Structured Backend Failure",
    ],
    "07_air_abstraction_safety.md": [
        "Stable surface: AIR (Abstraction Intent Representation)",
        "## Theorem: AIR Synthesis Read-Only",
        "## Theorem: Intent Node Coverage",
        "## Theorem: Boundary Closure",
        "## Theorem: Drift Detection Soundness",
        "## Theorem: Codegen Non-Impact",
        "PGY_SEM_INTENT_BOUNDARY_DRIFT",
    ],
    "08_slot_capability_calculus.md": [
        "Status: `IN PROGRESS / PROOF-SKETCH`",
        "## Stable Surface",
        "## Negative Claim: Slot Is Not A Borrow Checker",
        "Slot = runtime capability + generation + token + pin-state safety.",
        "borrow-checker-equivalent is the static ownership/CFG layer above Slot.",
        "## Semantic Domains",
        "## Theorem: ABA Safety",
        "## Theorem: Token Unforgeability",
        "## Theorem: Pin Non-Eviction",
        "## Bridge Obligation: Borrow-Checker-Equivalent Safety",
        "NoEscape(view, region)",
        "NoSuspend(view, region)",
        "WriteExclusive(slot, region)",
        "proof sketch, not completed",
        "Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` now reaches",
    ],
}

for filename, required_terms in required_files.items():
    path = proof_dir / filename
    if not path.exists():
        raise SystemExit(f"missing proof document: docs/semantics/{filename}")
    text = path.read_text(encoding="utf-8")
    missing = [term for term in required_terms if term not in text]
    if missing:
        raise SystemExit(
            f"proof document docs/semantics/{filename} missing required term(s): "
            + ", ".join(missing)
        )

required_scope_terms = [
    "Generic contracts",
    "Ownership: anchored slot-handle boundary subset only.",
    "Runtime observability",
    "Backends: MIR-equivalent C and LLVM behavior",
    "AIR abstraction safety",
    "Slot capability calculus",
    "Full quantum resource model.",
    "Higher-kinded types and full FP functor/applicative/monad laws.",
    "GPU/Spray, Skia/render graph",
]
readme = (proof_dir / "README.md").read_text(encoding="utf-8")
missing_scope = [term for term in required_scope_terms if term not in readme]
if missing_scope:
    raise SystemExit(
        "formal semantics scope missing term(s): " + ", ".join(missing_scope)
    )

ref = "docs/102_formal_semantics_and_proof_obligations.md"
folder_ref = "docs/semantics/"
if ref not in checklist:
    raise SystemExit("beta readiness checklist does not reference formal semantics doc")
if ref not in todo:
    raise SystemExit("TODO does not reference formal semantics doc")
if folder_ref not in checklist:
    raise SystemExit("beta readiness checklist does not reference docs/semantics/")
if folder_ref not in todo:
    raise SystemExit("TODO does not reference docs/semantics/")
if "docs/semantics/README.md" not in index_doc:
    raise SystemExit("formal semantics index does not point at proof pack README")
if "docs/semantics/08_slot_capability_calculus.md" not in index_doc:
    raise SystemExit("formal semantics index does not point at slot capability calculus")
if "docs/semantics/proofs/SlotCalculus.v" not in index_doc:
    raise SystemExit("formal semantics index does not point at SlotCalculus.v")
if "docs/118_slot_model_rigor_audit.md" not in index_doc:
    raise SystemExit("formal semantics index does not point at Slot rigor audit")

if "Do not advertise mechanized proof for beta" not in checklist:
    raise SystemExit("checklist must forbid advertising mechanized proof before an artifact exists")
if "proof evidence" not in todo or "beta proof line honest" not in todo:
    raise SystemExit("TODO must preserve proof-evidence boundary wording")

slot_coq = proof_dir / "proofs" / "SlotCalculus.v"
if not slot_coq.exists():
    raise SystemExit("missing docs/semantics/proofs/SlotCalculus.v")
slot_coq_text = slot_coq.read_text(encoding="utf-8")
for forbidden in [
    "Status: Beta",
    "Level 4 Mechanized Proof",
    "Safe Core Mechanized Proof",
]:
    if forbidden in slot_coq_text:
        raise SystemExit(
            "SlotCalculus.v overclaims mechanized proof status: " + forbidden
        )
for required in [
    "Status: proof-sketch; not beta-closure evidence unless checked by CI",
    "Negative scope: this file does not prove Rust-style borrow checking",
    "Require Import Coq.Arith.PeanoNat.",
    "Lemma stale_handle_read_impossible",
    "Lemma handle_read_requires_issued_token",
    "Lemma unissued_token_read_impossible",
    "Lemma handle_pin_requires_issued_token",
    "Lemma unissued_token_pin_impossible",
    "Lemma pin_non_eviction",
    "Qed.",
]:
    if required not in slot_coq_text:
        raise SystemExit("SlotCalculus.v missing required proof boundary term: " + required)

for filename in [
    "00_proof_contract.md",
    "08_slot_capability_calculus.md",
    "README.md",
]:
    text = (proof_dir / filename).read_text(encoding="utf-8")
    for forbidden in [
        "proof sketch for one invariant",
        "proof sketch for two invariants",
        "minimal mechanized sketch for two invariants",
    ]:
        if forbidden in text:
            raise SystemExit(
                f"docs/semantics/{filename} has stale SlotCalculus scope wording: "
                + forbidden
        )

if 'Do not advertise "Slot as borrow checker"' not in checklist:
    raise SystemExit("checklist must preserve Slot/borrow-checker claim boundary")

for required in [
    "Slot Resource-Boundary Analyzer",
    "Slot analysis is not Rust-style lifetime analysis.",
    "Slot is the source-level modular resource boundary.",
]:
    if required not in semantic_design:
        raise SystemExit("semantic analyzer design missing Slot boundary term: " + required)

for label, source in [
    ("slot_manager.h", slot_manager),
    ("pgy_runtime_slot_macros.h", slot_macros),
]:
    for required in [
        "source-level resource boundary",
        "pointer/address ownership",
        "backend",
    ]:
        if required not in source:
            raise SystemExit(f"{label} missing Slot runtime boundary term: {required}")

for required in [
    "Slot Is Not a Borrow Checker",
    "Slot Is A Modular Resource Boundary",
    "Pergyra does not expose memory as address ownership.",
    "Pergyra exposes memory as a modular resource boundary.",
    "A Slot is the stable language-level boundary; the backend handle below it is replaceable.",
    "Slot = address abstraction + ownership boundary + capability gate + replaceable backend handle",
    "The borrow-checker-equivalent in Pergyra is not Slot",
    "Current `WriteView<T>` same-slot exclusivity is enforced",
    "source-level typed-view pin blocks reject suspension and transport boundaries",
    "Active source-level typed-view pin blocks; `docs/74`; diagnostics + backend compare",
    "The block-scoped source `pin` surface is active for typed views.",
    "pin_read_view_block",
    "pin_secure_read_view_block",
    "pin_mixed_read_view_sequence",
    "pin_write_view_block",
    "pin_secure_write_view_block",
    "straight-line typed-view read/write parity across",
    "Static strength comparable to Rust 1.0 at launch",
]:
    if required not in rigor_audit:
        raise SystemExit("Slot rigor audit missing required boundary term: " + required)

allowed_borrow_claim_docs = {
    checklist_path,
    rigor_audit_path,
    proof_dir / "08_slot_capability_calculus.md",
    proof_dir / "README.md",
}
for path in [root / "README.md", root / "TODO.md", *sorted((root / "docs").rglob("*.md"))]:
    if not path.exists() or path in allowed_borrow_claim_docs:
        continue
    text = path.read_text(encoding="utf-8")
    for forbidden in [
        "Slot Lifetime Analyzer",
        "Slot<T>`: 명시적 수명 관리",
        "slot의 생명주기",
        "슬롯 생명주기",
        "Slot is Pergyra's borrow checker",
        "Slot proves borrow safety",
        "Slot proves Rust-style borrow checking",
        "Rust-level memory safety",
        "pin blocks reject crossing await",
        "pin blocks statically reject crossing await",
        "WriteView<T> exclusive is not enforced",
        "WriteView<T> is not enforced",
    ]:
        if forbidden in text:
            rel = path.relative_to(root).as_posix()
            raise SystemExit(
                f"{rel} overclaims Slot/borrow-checker safety: {forbidden}"
            )

for required in [
    "sudo apt-get install -y gcc make llvm-dev llvm coq",
    "make ci-linux",
]:
    if required not in ci:
        raise SystemExit("CI workflow missing formal semantics Coq gate term: " + required)

print("formal semantics smoke: ok")
PY

if command -v coqc >/dev/null 2>&1; then
    (cd "$ROOT_DIR" && coqc docs/semantics/proofs/SlotCalculus.v)
    echo "formal semantics Coq smoke: ok"
else
    echo "formal semantics Coq smoke: skipped (coqc not found)"
fi

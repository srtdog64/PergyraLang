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
        echo "[documentation-quality] missing python" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
index = root / "docs" / "INDEX.md"
audit = root / "docs" / "116_documentation_quality_audit.md"
guide = root / "docs" / "05_async_concurrency.md"
contract = root / "docs" / "113_memory_concurrency_model.md"
positioning = root / "docs" / "114_async_model_positioning.md"
async_demo = root / "examples" / "async_demo.pgy"
air_semantics = root / "docs" / "semantics" / "07_air_abstraction_safety.md"
remote_future_example = root / "examples" / "remote_future_result.pgy"
grammar_syntax = root / "docs" / "grammar" / "01_syntax.md"
grammar_rules = root / "docs" / "grammar" / "02_grammar.md"

for path in [
    index,
    audit,
    guide,
    contract,
    positioning,
    async_demo,
    air_semantics,
    remote_future_example,
    grammar_syntax,
    grammar_rules,
]:
    if not path.exists():
        raise SystemExit(f"missing documentation quality input: {path.relative_to(root)}")

index_text = index.read_text(encoding="utf-8")
audit_text = audit.read_text(encoding="utf-8")
guide_text = guide.read_text(encoding="utf-8")
contract_text = contract.read_text(encoding="utf-8")
positioning_text = positioning.read_text(encoding="utf-8")
async_demo_text = async_demo.read_text(encoding="utf-8")
air_semantics_text = air_semantics.read_text(encoding="utf-8")
remote_future_text = remote_future_example.read_text(encoding="utf-8")
grammar_syntax_text = grammar_syntax.read_text(encoding="utf-8")
grammar_rules_text = grammar_rules.read_text(encoding="utf-8")

text_roots = [
    root / "docs",
    root / "examples",
]
text_suffixes = {".md", ".pgy"}
for base in text_roots:
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in text_suffixes:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            raise SystemExit(
                f"{path.relative_to(root)} is not valid UTF-8: {exc}"
            ) from exc
        if "\ufffd" in text:
            raise SystemExit(
                f"{path.relative_to(root)} contains Unicode replacement characters"
            )

for label, text in [
    ("docs/INDEX.md", index_text),
    ("docs/116_documentation_quality_audit.md", audit_text),
    ("docs/semantics/07_air_abstraction_safety.md", air_semantics_text),
    ("examples/remote_future_result.pgy", remote_future_text),
]:
    if "\ufffd" in text:
        raise SystemExit(f"{label} contains Unicode replacement characters")

index_terms = [
    "PergyraLang Documentation Index",
    "Beta Closure Source Of Truth",
    "Async, Parallel, And Memory",
    "116_documentation_quality_audit.md",
    "Current Documentation Policy",
]
missing_index = [term for term in index_terms if term not in index_text]
if missing_index:
    raise SystemExit("docs/INDEX.md missing term(s): " + ", ".join(missing_index))

audit_terms = [
    "Documentation Quality Audit",
    "beta-closure support note",
    "Async Documentation Position",
    "capture-bearing detached async block stability",
    "Avoid using \"experimental\" as a dumping ground",
]
missing_audit = [term for term in audit_terms if term not in audit_text]
if missing_audit:
    raise SystemExit(
        "docs/116_documentation_quality_audit.md missing term(s): "
        + ", ".join(missing_audit)
    )

guide_terms = [
    "Use named `spawn Worker(args...)` for beta-stable task creation",
    "RemoteFuture<T> -> await -> Result<T>",
    "let result: Result<Int> = await pending;",
    "capture-bearing detached async block stability",
]
missing_guide = [term for term in guide_terms if term not in guide_text]
if missing_guide:
    raise SystemExit("docs/05_async_concurrency.md missing term(s): " + ", ".join(missing_guide))

contract_terms = [
    "Detached",
    "anonymous async blocks with local captures are not the stable task-creation",
    "Capture-bearing detached async block stability",
]
missing_contract = [term for term in contract_terms if term not in contract_text]
if missing_contract:
    raise SystemExit(
        "docs/113_memory_concurrency_model.md missing term(s): "
        + ", ".join(missing_contract)
    )

positioning_terms = [
    "explicit named task creation plus",
    "let ordersTask: Future<OrderList> = spawn GetOrders(user.id);",
    "Capture-bearing detached async blocks as the stable task creation model",
]
missing_positioning = [term for term in positioning_terms if term not in positioning_text]
if missing_positioning:
    raise SystemExit(
        "docs/114_async_model_positioning.md missing term(s): "
        + ", ".join(missing_positioning)
    )

for forbidden in [
    "coloring avoidance",
    "hides suspension",
    "async is the umbrella",
]:
    if forbidden in positioning_text:
        raise SystemExit(f"async positioning contains forbidden simplification: {forbidden}")

if "async {" in async_demo_text:
    raise SystemExit("examples/async_demo.pgy must not use capture-bearing anonymous async block")
if "spawn Inc(8)" not in async_demo_text:
    raise SystemExit("examples/async_demo.pgy must demonstrate named spawn for the second async value")

design_sketch_examples = {
    root / "examples" / "party_system_demo.pgy",
}
for path in sorted((root / "examples").rglob("*.pgy")):
    if path in design_sketch_examples:
        text = path.read_text(encoding="utf-8")
        if "Status: design sketch" not in text:
            raise SystemExit(
                f"{path.relative_to(root)} may use future async syntax only with a design-sketch banner"
            )
        continue
    text = path.read_text(encoding="utf-8")
    if "async {" in text:
        raise SystemExit(
            f"{path.relative_to(root)} uses capture-bearing anonymous async block outside a design sketch"
        )

grammar_terms = [
    "베타 안정 표면",
    "named `spawn Worker(args...)`",
    "capture-bearing detached `async { ... }`",
    "베타 안정 태스크 생성 표면이 아니다",
]
missing_grammar_terms = [
    term
    for term in grammar_terms
    if term not in grammar_syntax_text or term not in grammar_rules_text
]
if missing_grammar_terms:
    raise SystemExit(
        "grammar docs missing async stability warning term(s): "
        + ", ".join(missing_grammar_terms)
    )

for required in [
    "DIR -> step -> intent_node",
    "intent_step_ast -> execution_boundary -> boundary_node",
    "AIR -> no_drift",
]:
    if required not in air_semantics_text:
        raise SystemExit(
            "docs/semantics/07_air_abstraction_safety.md missing readable judgment: "
            + required
        )

for required in [
    "RemoteFuture<T> -> await -> Result<T>",
    "let result: Result<Int> = await pending;",
    "let value: Int = Unwrap(result);",
]:
    if required not in remote_future_text:
        raise SystemExit(
            "examples/remote_future_result.pgy missing beta RemoteFuture pattern: "
            + required
        )

print("[documentation-quality] UTF, docs index, async wording, and executable example surface ok")
PY

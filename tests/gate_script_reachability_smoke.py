#!/usr/bin/env python3
"""Ratchet literal test-script execution edges and explicit manual exceptions.

This is a source-graph check, not proof that a scheduled gate ran or passed.
Dynamic shell dispatch is deliberately not guessed; it needs an exact manual
declaration until an executable owner makes its route inspectable.
"""

from __future__ import annotations

from collections import defaultdict, deque
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT / "tests" / "manual_gate_inventory.tsv"
SCRIPT_PATH = re.compile(r"(?:tests|scripts)/[A-Za-z0-9_./-]+\.sh")
COMMAND = re.compile(r"\b(?:bash|source|exec|run|sh)\b")
NONEXECUTING = re.compile(
    r"\b(?:grep|rg|require_text|reject_text|echo|printf)\b"
)


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def execution_edges(path: Path) -> set[str]:
    edges: set[str] = set()
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        text = line.lstrip()
        if not text or text.startswith("#") or NONEXECUTING.search(text):
            continue
        if not (COMMAND.search(text) or "$(BASH)" in text or "$BASH" in text
                or text.startswith(("tests/", "scripts/", "./tests/", "./scripts/"))):
            continue
        for match in SCRIPT_PATH.finditer(text):
            target = match.group().removeprefix("./")
            if (ROOT / target).is_file():
                edges.add(target)
    return edges


def inventory_errors(tests: set[str], reached: set[str],
                     manual: dict[str, str]) -> tuple[list[str], list[str], list[str]]:
    return (sorted(tests - reached - manual.keys()),
            sorted(manual.keys() - tests),
            sorted(manual.keys() & reached))


def main() -> int:
    # Negative self-checks prevent a permissive set operation from making this
    # governance gate green while a new, stale or dual-owned script is present.
    assert inventory_errors({"tests/new.sh"}, set(), {})[0] == ["tests/new.sh"]
    assert inventory_errors(set(), set(), {"tests/old.sh": "manual"})[1] == ["tests/old.sh"]
    assert inventory_errors({"tests/dual.sh"}, {"tests/dual.sh"},
                            {"tests/dual.sh": "manual"})[2] == ["tests/dual.sh"]
    tests = {relative(path) for path in (ROOT / "tests").rglob("*.sh")}
    sources = [ROOT / "Makefile"]
    sources.extend((ROOT / ".github" / "workflows").glob("*.yml"))
    for folder in (ROOT / "scripts", ROOT / "tests"):
        for suffix in ("*.sh", "*.py", "*.ps1"):
            sources.extend(folder.rglob(suffix))

    roots: set[str] = set()
    graph: dict[str, set[str]] = defaultdict(set)
    for source in sources:
        edges = execution_edges(source)
        if source.name == "Makefile" or ".github/workflows/" in relative(source):
            roots.update(edges)
        else:
            graph[relative(source)].update(edges)

    reached = set(roots)
    queue = deque(roots)
    while queue:
        for child in graph[queue.popleft()]:
            if child not in reached:
                reached.add(child)
                queue.append(child)

    manual: dict[str, str] = {}
    if not MANUAL.is_file():
        print("[gate-reachability] missing manual inventory", file=sys.stderr)
        return 1
    for number, raw in enumerate(MANUAL.read_text(encoding="utf-8").splitlines(), 1):
        if not raw or raw.startswith("#"):
            continue
        fields = raw.split("\t", 1)
        if len(fields) != 2 or not fields[1].strip() or fields[0] in manual:
            print(f"[gate-reachability] invalid manual row {number}", file=sys.stderr)
            return 1
        manual[fields[0]] = fields[1]

    undeclared, stale, dual = inventory_errors(tests, reached, manual)
    registry = (ROOT / "docs" / "semantics" /
                "sot_owner_spine_registry.md").read_text(encoding="utf-8")
    cited_manual = sorted(path for path in manual if path in registry)
    unsupported_evidence = [path for path in cited_manual
                            if "registry citation is historical" not in manual[path]]
    for label, paths in (("unreached", undeclared), ("stale manual", stale),
                         ("manual also target-reachable", dual),
                         ("manual registry evidence lacks historical scope",
                          unsupported_evidence)):
        for path in paths:
            print(f"[gate-reachability] {label}: {path}", file=sys.stderr)
    print(f"[gate-reachability] scripts={len(tests)} target-reachable="
          f"{len(tests & reached)} manual={len(manual)} "
          f"manual-registry-historical={len(cited_manual)} "
          f"undeclared={len(undeclared)} stale={len(stale)} dual={len(dual)}")
    return 1 if undeclared or stale or dual or unsupported_evidence else 0


if __name__ == "__main__":
    raise SystemExit(main())

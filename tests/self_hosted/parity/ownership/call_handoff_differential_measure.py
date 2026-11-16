#!/usr/bin/env python3
"""Compare the call hand-off rows of the native and self-host MIR (G2, S2).

Arguments: ROOT WORK_DIR INPUTS_FILE OUT_DIR. WORK_DIR holds, per input key,
<key>.native.json/.native.rc and <key>.self.json/.self.rc written by
call_handoff_differential_measure.sh. Front ends allocate different syntax
identities, so rows are joined by routine and binding names, callee, formal
ordinal and the occurrence order of equal keys. The output is counts
(summary.json) and one line per difference (differences.tsv); nothing here
passes or fails on a count.
"""

from __future__ import annotations

from collections import Counter, defaultdict
import json
from pathlib import Path
import sys

ROOT, WORK, INPUTS, OUT = (Path(arg) for arg in sys.argv[1:5])


def group_of(source: str) -> str:
    return "lifetime" if "/parity/fixture/" in source else "cases"


def load(key: str, side: str):
    rc_path = WORK / f"{key}.{side}.rc"
    doc_path = WORK / f"{key}.{side}.json"
    if not rc_path.is_file() or rc_path.read_text().strip() != "0":
        return None
    if not doc_path.is_file() or doc_path.stat().st_size == 0:
        return None
    try:
        document = json.loads(doc_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise SystemExit(f"{doc_path}: {error}")
    if document.get("schema") != "pgy.mir.v1":
        raise SystemExit(f"{doc_path}: not a pgy.mir.v1 document")
    return document


def routines(document):
    """Routine key -> routine; a repeated key takes an ordinal."""
    seen: Counter = Counter()
    out = {}
    for routine in document.get("routines", []):
        name = routine.get("name") or "?"
        if routine.get("owner"):
            name = f"{routine['owner']}.{name}"
        seen[name] += 1
        out[name if seen[name] == 1 else f"{name}#{seen[name]}"] = routine
    return out


def binding_names(routine):
    names = {}
    for local in routine.get("source_locals") or []:
        names.setdefault(local.get("binding_syntax_id"), local.get("name"))
    for param in routine.get("params") or []:
        names.setdefault(param.get("source_syntax_id"), param.get("name"))
    return names


def handoff(routine):
    rows = routine.get("call_handoff")
    if not isinstance(rows, dict):
        raise SystemExit(f"routine {routine.get('name')} has no call_handoff object")
    return rows


def kind_of(members):
    members = set(members)
    if "UNPROVED" in members:
        return "UNPROVED"
    if not members - {"READ"}:
        return "READ_ONLY"
    if "RETURN" in members and not members - {"READ", "RETURN"}:
        return "RETURN_ONLY"
    if "KEEP" in members and not members - {"READ", "KEEP"}:
        return "KEEP_ONLY"
    return "MOVING"


def keyed(rows):
    """(key, ordinal) -> value for a list of (key, value) in order."""
    seen: Counter = Counter()
    out = {}
    for key, value in rows:
        seen[key] += 1
        out[(key, seen[key])] = value
    return out


def program_rows(document):
    handed_rows, handed_actions, actions, classes, clones = set(), set(), {}, {}, Counter()
    incomplete = 0
    for name, routine in routines(document).items():
        names = binding_names(routine)
        rows = handoff(routine)
        incomplete += 0 if rows.get("complete") else 1
        for fact in routine.get("collection_ownership_facts") or []:
            if fact.get("disposition") == "handed-off":
                handed_rows.add((name, names.get(fact["binding_syntax_id"], "?")))
        listed = []
        for row in rows.get("actions") or []:
            binding = names.get(row["binding_syntax_id"], "?")
            if row["row"] == "handed-off":
                handed_actions.add((name, binding))
            listed.append(((name, binding, row["callee"], row["parameter_index"]),
                           (row["action"], row["compiler_clone"], row["row"],
                            row["origin"], row["later"], row["context"],
                            tuple(row["members"]), row["divergence"])))
        actions.update(keyed(listed))
        for row in rows.get("classes") or []:
            classes[(name, row["parameter_index"])] = (tuple(row["members"]), row["divergence"])
        for row in rows.get("clones") or []:
            binding = names.get(row["binding_syntax_id"], "<expression>") \
                if row["binding_syntax_id"] else "<expression>"
            clones[(name, row["callee"], row["argument_index"], binding)] += 1
    return handed_rows, handed_actions, actions, classes, clones, incomplete


def clone_text_excess(document, source: Path) -> int:
    """S0's measure: `Clone(` in the MIR statement texts beyond the source's."""
    count = 0
    for routine in document.get("routines", []):
        for block in routine.get("blocks") or []:
            for instruction in block.get("instructions") or []:
                if isinstance(instruction.get("ast"), str):
                    count += instruction["ast"].count("Clone(")
    return max(0, count - source.read_text(encoding="utf-8").count("Clone("))


def main() -> int:
    sources = [line.strip() for line in INPUTS.read_text().splitlines() if line.strip()]
    summary = defaultdict(lambda: defaultdict(Counter))
    differences = []
    for source in sources:
        group = group_of(source)
        key = source.replace("/", "_")
        native, self_host = load(key, "native"), load(key, "self")
        summary["inputs"][group]["programs"] += 1
        summary["admitted"][group]["native"] += native is not None
        summary["admitted"][group]["self"] += self_host is not None
        if native is None or self_host is None:
            continue
        summary["admitted"][group]["both"] += 1
        n = program_rows(native)
        s = program_rows(self_host)
        summary["incomplete_routines"][group]["native"] += n[5]
        summary["incomplete_routines"][group]["self"] += s[5]
        for label, index in (("handoff_rows", 0), ("handoff_actions", 1)):
            counts = summary[label][group]
            counts["agree"] += len(n[index] & s[index])
            counts["native_only"] += len(n[index] - s[index])
            counts["self_only"] += len(s[index] - n[index])
            counts["programs_with_difference"] += n[index] != s[index]
            for row in sorted(n[index] ^ s[index]):
                differences.append((group, source, label, *row,
                                    "handed-off" if row in n[index] else "-",
                                    "handed-off" if row in s[index] else "-"))
        for side, rows in (("native", n), ("self", s)):
            consistency = summary["handoff_row_action_consistency"][group]
            consistency[f"{side}_mismatch"] += len(rows[0] ^ rows[1])
        counts = summary["action_rows"][group]
        counts["native"] += len(n[2])
        counts["self"] += len(s[2])
        for site in sorted(set(n[2]) | set(s[2]), key=repr):
            left, right = n[2].get(site), s[2].get(site)
            if left is None or right is None:
                counts["native_only" if right is None else "self_only"] += 1
            elif left[:3] == right[:3]:
                counts["same_action"] += 1
                continue
            else:
                counts["different_action"] += 1
                summary["action_pairs"][group][f"{left[0]}/{right[0]}"] += 1
            differences.append((group, source, "action", *site[0], site[1],
                                json.dumps(left), json.dumps(right)))
        counts = summary["class_rows"][group]
        counts["native"] += len(n[3])
        counts["self"] += len(s[3])
        for formal in sorted(set(n[3]) | set(s[3]), key=repr):
            left, right = n[3].get(formal), s[3].get(formal)
            if left is None or right is None:
                counts["native_only" if right is None else "self_only"] += 1
            elif left[0] == right[0]:
                counts["same_members"] += 1
                continue
            else:
                counts["same_kind" if kind_of(left[0]) == kind_of(right[0])
                       else "different_kind"] += 1
            differences.append((group, source, "class", *formal,
                                json.dumps(left), json.dumps(right)))
        counts = summary["clone_plan"][group]
        counts["native_clones"] += sum(n[4].values())
        counts["self_clones"] += sum(s[4].values())
        counts["native_programs"] += bool(n[4])
        counts["self_programs"] += bool(s[4])
        counts["agree"] += sum((n[4] & s[4]).values())
        counts["native_only"] += sum((n[4] - s[4]).values())
        counts["self_only"] += sum((s[4] - n[4]).values())
        for copy in sorted(set(n[4]) | set(s[4]), key=repr):
            if n[4][copy] != s[4][copy]:
                differences.append((group, source, "clone", *copy,
                                    str(n[4][copy]), str(s[4][copy])))
        excess = clone_text_excess(native, ROOT / source)
        counts = summary["clone_text_s0_method"][group]
        counts["native_programs"] += excess > 0
        counts["native_excess"] += excess
        excess = clone_text_excess(self_host, ROOT / source)
        counts["self_programs"] += excess > 0
        counts["self_excess"] += excess
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n",
                                      encoding="utf-8")
    with (OUT / "differences.tsv").open("w", encoding="utf-8") as handle:
        for row in differences:
            handle.write("\t".join(str(field) for field in row) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

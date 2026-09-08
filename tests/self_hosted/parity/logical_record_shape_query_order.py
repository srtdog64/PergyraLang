"""Source-order ratchet only; runtime and refusal parity belong to the shell gate."""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
checks = {
    "DirectMirScalarProgramLogicalRecordMemberMarkerReady": [
        "if node < 0 ||",
        "node >= ArrayLength(sequence.arena.topology.node_kinds)",
        "sequence.arena.topology.node_kinds[node] != AstExpressionNodeLeaf()",
        "return false;",
        "let parent: Int = node + 1;",
        "parent >= ArrayLength(sequence.arena.topology.node_kinds)",
        "sequence.arena.topology.node_kinds[parent] != AstExpressionNodeMemberAccess()",
        "return false;",
        "!DirectMirScalarProgramLogicalRecordFactReady(record)",
        "!record.present",
        "DirectMirScalarProgramLogicalRecordFieldOrdinal(",
        "sequence.arena.topology.right_children[parent]",
    ],
    "DirectMirScalarProgramLogicalRecordMemberFromGraph": [
        "if node < 0 ||",
        "node >= ArrayLength(sequence.arena.topology.node_kinds)",
        "sequence.arena.topology.node_kinds[node] != AstExpressionNodeMemberAccess()",
        "return missing;",
        "!DirectMirScalarProgramLogicalRecordFactReady(record)",
        "!record.present",
        "sequence.arena.topology.left_children[node]",
        "sequence.arena.topology.right_children[node]",
    ],
}
for name, ordered in checks.items():
    match = re.search(r"^func " + name + r"\(.*?^}", source, re.M | re.S)
    if match is None:
        raise SystemExit(f"[record-shape-order] missing function: {name}")
    body = " ".join(match.group().split())
    position = 0
    for token in ordered:
        found = body.find(token, position)
        if found < 0:
            raise SystemExit(f"[record-shape-order] {name}: missing/out-of-order {token}")
        position = found + len(token)
print("[record-shape-order] guarded shapes precede table validation; candidate checks retained: PASS")
